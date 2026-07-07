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
    current_IF_ID_REG = RiscV::IF_ID_Latch();
    current_ID_EX_REG = RiscV::ID_EX_Latch();
    //std::cout << "Pipeline flushed due to mispredicted branch.\n";
}



void CPU::tick() {
    decode();
    fetch();
    execute();
    write_back();
    memory_access();
    csrs[0xB00]++;

    bool interrupt_asserted = false;
    // もし外部タイマー(timer_irq)があり、mstatusで割り込みが許可されていたら
    if (this->timer_irq && (csrs[0x300] & (1 << 3))) { 
        interrupt_asserted = true;
    }

    // =================================================================
    // 4. クロックの立ち上がり (apply_clock_edge) のシミュレート
    // =================================================================
    
    // --- 優先度①：外部からの「割り込み」が発生した瞬間 ---
    if (interrupt_asserted) {
        csrs[0x341] = pc;               // 次に実行するはずだったPCを mepc に退避
        csrs[0x342] = (1ULL << 31) | 7; // mcause の最上位ビットを1にし、コード7(タイマー)をセット
        
        // パイプラインを全面フラッシュ（全ステージにバブルを強制注入）
        current_IF_ID_REG  = RiscV::IF_ID_Latch();
        current_ID_EX_REG  = RiscV::ID_EX_Latch();
        current_EX_MEM_REG = RiscV::EX_MEM_Latch();
        current_MEM_WB_REG = RiscV::MEM_WB_Latch();
        
        pc = csrs[0x305]; // 次のサイクルは mtvec (例外ハンドラ) からフェッチ
        return;           // このサイクルの通常更新はすべてスキップして終了
    }

    

    current_MEM_WB_REG = next_MEM_WB_REG;
    current_EX_MEM_REG = next_EX_MEM_REG;



    // --- 2. ID_EX ラッチの更新論理（優先順位：予測ミス > ストール > 通常） ---
    if (different_flag) {
        // 予測ミス時は、次にExecuteに進むはずだった命令を殺してバブルにする
        current_ID_EX_REG = RiscV::ID_EX_Latch();
    } else if (last_stall_flag) {
        // ロードハザード（ストール）時は、デコードを保留するためExecuteにはバブルを送り出す
        current_ID_EX_REG = RiscV::ID_EX_Latch();
    } else {
        // 通常時は、Decodeステージが生成した次の制御信号をそのまま受け入れる
        current_ID_EX_REG = next_ID_EX_REG;
    }

    // --- 3. IF_ID ラッチと PC の更新論理 ---
    if (different_flag) {
        // 予測ミス時は、フェッチ中だった間違った命令を破棄（バブル化）し、正しいターゲットへPCを飛ばす
        current_IF_ID_REG = RiscV::IF_ID_Latch();
        pc = last_actual_target;
        different_flag = false; // フラグクリア
    } else if (last_stall_flag) {
        // ストール時は、current_IF_ID_REG も PC も「今の状態を完全に維持（更新しない）」
        // next_IF_ID_REG をコピーしないことで、次のサイクルも同じ命令が [ID] に留まるわ
        last_stall_flag = false; // フラグクリア
    } else {
        // 通常時は、新しくフェッチした命令を受け入れ、PCを次のアドレスに進める
        current_IF_ID_REG = next_IF_ID_REG;
        pc = next_pc; // Fetchステージで計算しておいた次のPC（通常はpc+4、予測Takenなら予測先）
    }

}

// テスト用・ヘルパー関数
void CPU::write_memory_word(uint32_t load_addr, uint32_t inst) {
    memory.write_byte(load_addr, inst & 0xff);
    memory.write_byte(load_addr + 1, (inst >> 8) & 0xff);
    memory.write_byte(load_addr + 2, (inst >> 16) & 0xff);
    memory.write_byte(load_addr + 3, (inst >> 24) & 0xff);
}
void CPU::set_register(uint8_t idx, uint32_t val) { if (idx != 0) registers[idx] = val; }
void CPU::set_pc(uint32_t new_pc) { pc = new_pc; }

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
        case RiscV::OP_RType : // OP_RType
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

        case RiscV::OP_IMM: // OP_IMM
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

        case RiscV::OP_LOAD: // OP_LOAD
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

        case RiscV::OP_STORE: // OP_STORE
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

        case RiscV::OP_BRANCH: // OP_BRANCH
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

        case RiscV::OP_LUI: // OP_LUI
            {
                uint32_t imm = inst & 0xFFFFF000;
                ss << "lui x" << (int)rd << ", 0x" << std::hex << (imm >> 12);
            }
            break;

        case RiscV::OP_AUIPC: // OP_AUIPC
            {
                uint32_t imm = inst & 0xFFFFF000;
                ss << "auipc x" << (int)rd << ", 0x" << std::hex << (imm >> 12);
            }
            break;

        case RiscV::OP_JAL: // OP_JAL
            {
                int32_t imm = ((inst >> 31) & 1) << 20 |
                              ((inst >> 12) & 0xFF) << 12 |
                              ((inst >> 20) & 1) << 11 |
                              ((inst >> 21) & 0x3FF) << 1;
                if (imm & 0x100000) imm |= 0xFFE00000; // 21bit符号拡張
                ss << "jal x" << (int)rd << ", offset:" << imm;
            }
            break;

        case RiscV::OP_JALR: // OP_JALR
            {
                int32_t imm = static_cast<int32_t>(inst) >> 20;
                ss << "jalr x" << (int)rd << ", " << imm << "(x" << (int)rs1 << ")";
            }
            break;

        case RiscV::OP_SYSTEM: // OP_SYSTEM
            if (funct3 == 0x0) {
                uint32_t sys_imm = inst >> 20;
                if (sys_imm == 0x000) return "ecall";
                if (sys_imm == 0x001) return "ebreak";
                if (sys_imm == 0x302) return "mret";
            }else{
                uint32_t csr_addr = inst >> 20;
                ss << "csrrx x" << (int)rd << ", 0x" << std::hex << csr_addr << ", x" << (int)rs1;
            }
            break;

        default:
            ss << "unknown (0x" << std::hex << std::setw(8) << std::setfill('0') << inst << ")";
            break;
    }

    return ss.str();
}


std::array<uint32_t, 4096> csrs = {0}; 

void CPU::dump_active_csrs() {
    std::cout << "=== Active CSRs Dump (std::array) ===\n";
    
    for (size_t addr = 0; addr < csrs.size(); ++addr) {
        // 値が0以外のものを「現在有効なCSR」として扱う（初期値が0でないCSRがある場合は適宜調整）
        if (csrs[addr] != 0) {
            std::cout << "CSR [0x" 
                      << std::hex << std::setw(3) << std::setfill('0') << addr 
                      << "] = 0x" 
                      << std::setw(8) << csrs[addr] << std::dec << "\n";
        }
    }
    std::cout << "=====================================\n";
}