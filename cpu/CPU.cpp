#include "CPU.hpp"
#include <iostream>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>


CPU::CPU() : registers({0}),  pc(0x80000000), memory() {
    uint32_t mem_size = 1024 * 1024 * 2; // あなたの現在のメモリサイズ
    registers[2] = 0x80000000 + mem_size; // スタックポインタ初期化
}

bool CPU::is_halted() { return halted; }

void CPU::flush_pipeline() {
    IF_ID_REG = std::queue<RiscV::IF_ID_Latch>();
    ID_EX_REG = std::queue<RiscV::ID_EX_Latch>();
    //std::cout << "Pipeline flushed due to mispredicted branch.\n";
}

void CPU::reset_pipeline() {
    IF_ID_REG = std::queue<RiscV::IF_ID_Latch>();
    ID_EX_REG = std::queue<RiscV::ID_EX_Latch>();
    EX_MEM_REG = std::queue<RiscV::EX_MEM_Latch>();
    MEM_WB_REG = std::queue<RiscV::MEM_WB_Latch>();
    registers.fill(0);
    uint32_t mem_size = 1024 * 1024 * 2; // あなたの現在のメモリサイズ
    registers[2] = 0x80000000 + mem_size;
    pc = 0x80000000;
}

void CPU::tick() {
    write_back();
    //std::cout << "After write_back: PC=" << std::hex << pc << std::dec << std::endl;
    memory_access();
    //std::cout << "After memory_access: PC=" << std::hex << pc << std::dec << std::endl;
    execute();
    //std::cout << "After execute: PC=" << std::hex << pc << std::dec << std::endl;
    decode();
    //std::cout << "After decode: PC=" << std::hex << pc << std::dec << std::endl;
    fetch();
    //std::cout << "After fetch: PC=" << std::hex << pc << std::dec << std::endl;
}

// テスト用・ヘルパー関数
void CPU::write_memory_word(uint32_t load_addr, uint32_t inst) {
    memory.write_byte(load_addr, inst & 0xff);
    memory.write_byte(load_addr + 1, (inst >> 8) & 0xff);
    memory.write_byte(load_addr + 2, (inst >> 16) & 0xff);
    memory.write_byte(load_addr + 3, (inst >> 24) & 0xff);
}
void CPU::inject_instruction(uint32_t inst) { RiscV::IF_ID_Latch latch = {inst, pc}; pc += 4; IF_ID_REG.push(latch); }
void CPU::set_register(uint8_t idx, uint32_t val) { if (idx != 0) registers[idx] = val; }
void CPU::set_pc(uint32_t new_pc) { pc = new_pc; }
bool CPU::is_id_ex_empty() const { return ID_EX_REG.empty(); }
RiscV::ID_EX_Latch CPU::get_id_ex_latch() const { 
    return ID_EX_REG.empty() ? RiscV::ID_EX_Latch{} : ID_EX_REG.front(); 
}
RiscV::EX_MEM_Latch CPU::get_ex_mem_latch() const { 
    return EX_MEM_REG.empty() ? RiscV::EX_MEM_Latch{} : EX_MEM_REG.front(); 
}
RiscV::MEM_WB_Latch CPU::get_mem_wb_latch() const { 
    return MEM_WB_REG.empty() ? RiscV::MEM_WB_Latch{} : MEM_WB_REG.front(); 
}
uint32_t CPU::get_register(uint8_t rs) { return registers[rs]; }

std::string CPU::disassemble_riscv(uint32_t inst) {
    uint8_t opcode = inst & 0x7F;
    uint8_t funct3 = (inst >> 12) & 0x07;
    uint8_t funct7 = (inst >> 25) & 0x7F;
    uint8_t rd     = (inst >> 7) & 0x1F;
    uint8_t rs1    = (inst >> 15) & 0x1F;
    uint8_t rs2    = (inst >> 20) & 0x1F;

    // NOPの即時判定（RISC-Vではaddi x0, x0, 0）
    if (inst == 0x00000013) {
        return "NOP";
    }
    if (inst == 0x00000000) {
        return "BUBBLE (ALL ZERO)";
    }

    std::stringstream ss;

    switch (opcode) {
        case 0x33: // OP_RType
            switch (funct3) {
                case 0x0: ss << ((funct7 == 0x20) ? "sub" : "add"); break;
                case 0x1: ss << "sll"; break;
                case 0x2: ss << "slt"; break;
                case 0x3: ss << "sltu"; break;
                case 0x4: ss << "xor"; break;
                case 0x5: ss << ((funct7 == 0x20) ? "sra" : "srl"); break;
                case 0x6: ss << "or"; break;
                case 0x7: ss << "and"; break;
                default:  ss << "unknown_R"; break;
            }
            ss << " x" << (int)rd << ", x" << (int)rs1 << ", x" << (int)rs2;
            break;

        case 0x13: // OP_IMM
            {
                int32_t imm = static_cast<int32_t>(inst) >> 20; // 12bit符号拡張
                switch (funct3) {
                    case 0x0: ss << "addi"; break;
                    case 0x2: ss << "slti"; break;
                    case 0x3: ss << "sltiu"; break;
                    case 0x4: ss << "xori"; break;
                    case 0x6: ss << "ori"; break;
                    case 0x7: ss << "andi"; break;
                    case 0x1: ss << "slli"; imm &= 0x1F; break;
                    case 0x5: ss << ((funct7 == 0x20) ? "srai" : "srli"); imm &= 0x1F; break;
                    default:  ss << "unknown_I"; break;
                }
                ss << " x" << (int)rd << ", x" << (int)rs1 << ", " << imm;
            }
            break;

        case 0x03: // OP_LOAD
            {
                int32_t imm = static_cast<int32_t>(inst) >> 20;
                switch (funct3) {
                    case 0x0: ss << "lb"; break;
                    case 0x1: ss << "lh"; break;
                    case 0x2: ss << "lw"; break;
                    case 0x4: ss << "lbu"; break;
                    case 0x5: ss << "lhu"; break;
                    default:  ss << "unknown_load"; break;
                }
                ss << " x" << (int)rd << ", " << imm << "(x" << (int)rs1 << ")";
            }
            break;

        case 0x23: // OP_STORE
            {
                int32_t imm = ((static_cast<int32_t>(inst) >> 25) << 5) | ((inst >> 7) & 0x1F);
                if (imm & 0x800) imm |= 0xFFFFF000; // 12bit符号拡張
                switch (funct3) {
                    case 0x0: ss << "sb"; break;
                    case 0x1: ss << "sh"; break;
                    case 0x2: ss << "sw"; break;
                    default:  ss << "unknown_store"; break;
                }
                ss << " x" << (int)rs2 << ", " << imm << "(x" << (int)rs1 << ")";
            }
            break;

        case 0x63: // OP_BRANCH
            {
                int32_t imm = ((inst >> 31) & 1) << 12 |
                              ((inst >> 7)  & 1) << 11 |
                              ((inst >> 25) & 0x3F) << 5 |
                              ((inst >> 8)  & 0x0F) << 1;
                if (imm & 0x1000) imm |= 0xFFFFE000; // 13bit符号拡張
                switch (funct3) {
                    case 0x0: ss << "beq"; break;
                    case 0x1: ss << "bne"; break;
                    case 0x4: ss << "blt"; break;
                    case 0x5: ss << "bge"; break;
                    case 0x6: ss << "bltu"; break;
                    case 0x7: ss << "bgeu"; break;
                    default:  ss << "unknown_branch"; break;
                }
                ss << " x" << (int)rs1 << ", x" << (int)rs2 << ", offset:" << imm;
            }
            break;

        case 0x37: // OP_LUI
            {
                uint32_t imm = inst & 0xFFFFF000;
                ss << "lui x" << (int)rd << ", 0x" << std::hex << (imm >> 12);
            }
            break;

        case 0x17: // OP_AUIPC
            {
                uint32_t imm = inst & 0xFFFFF000;
                ss << "auipc x" << (int)rd << ", 0x" << std::hex << (imm >> 12);
            }
            break;

        case 0x6F: // OP_JAL
            {
                int32_t imm = ((inst >> 31) & 1) << 20 |
                              ((inst >> 12) & 0xFF) << 12 |
                              ((inst >> 20) & 1) << 11 |
                              ((inst >> 21) & 0x3FF) << 1;
                if (imm & 0x100000) imm |= 0xFFE00000; // 21bit符号拡張
                ss << "jal x" << (int)rd << ", offset:" << imm;
            }
            break;

        case 0x67: // OP_JALR
            {
                int32_t imm = static_cast<int32_t>(inst) >> 20;
                ss << "jalr x" << (int)rd << ", " << imm << "(x" << (int)rs1 << ")";
            }
            break;

        case 0x73: // OP_SYSTEM
            if (funct3 == 0x0) {
                uint32_t sys_imm = inst >> 20;
                if (sys_imm == 0x000) return "ecall";
                if (sys_imm == 0x001) return "ebreak";
            }
            ss << "system_unknown";
            break;

        default:
            ss << "unknown (0x" << std::hex << std::setw(8) << std::setfill('0') << inst << ")";
            break;
    }

    return ss.str();
}