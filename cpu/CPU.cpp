#include "CPU.hpp"
#include <iostream>

CPU::CPU() : registers({0}),  pc(0x80000000), memory() {
    uint32_t mem_size = 1024 * 1024 * 2; // あなたの現在のメモリサイズ
    registers[2] = 0x80000000 + mem_size; // スタックポインタ初期化
}

bool CPU::is_halted() { return halted; }

void CPU::flush_pipeline() {
    IF_ID_REG = std::queue<RiscV::IF_ID_Latch>();
    ID_EX_REG = std::queue<RiscV::ID_EX_Latch>();
    std::cout << "Pipeline flushed due to mispredicted branch.\n";
}

void CPU::reset_pipeline() {
    IF_ID_REG = std::queue<RiscV::IF_ID_Latch>();
    ID_EX_REG = std::queue<RiscV::ID_EX_Latch>();
    EX_MEM_REG = std::queue<RiscV::EX_MEM_Latch>();
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
RiscV::ID_EX_Latch CPU::get_id_ex_latch() const { return ID_EX_REG.front(); }
uint32_t CPU::get_register(uint8_t rs) { return registers[rs]; }