#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>
void CPU::fetch() {
    if (pc < 0x80000000 || pc >= 0x80000000 + memory.size() - 3) return;
    

    //get_phys_addr is unnecessary here 
    //because Memory class already handles the address translation and range checking. 
    //We can directly read from memory using the virtual address (pc) and let the Memory class take care of it.
    //uint32_t p_addr = get_phys_addr(pc);
    uint32_t inst = memory.read_byte(pc) |
                    static_cast<uint32_t>(memory.read_byte(pc + 1) << 8) |
                    static_cast<uint32_t>(memory.read_byte(pc + 2) << 16) |
                    static_cast<uint32_t>(memory.read_byte(pc + 3) << 24);
    ///std::cout <<"PC : "<<std::hex << pc << std::dec << " asm : "<< disassemble_riscv(inst) << std::endl; // デバッグ用に命令を文字列化して表示（必要に応じてコメントアウト可）
    bool pred_taken = false;
    uint32_t pred_target = pc + 4; 
    auto it = branch_predictor.find(pc);
    if (it != branch_predictor.end() && it->second.first >= 2) {
        pred_taken = true;
        pred_target = it->second.second;
    }

    RiscV::IF_ID_Latch latch = {true, inst, pc, pred_taken, pred_target};
    //std::cout << "IF: PC=0x" << std::hex << pc << " INST=0x" << inst 
    //        << " PRED=" << (pred_taken ? "T" : "N") 
    //        << " PRED_TGT=0x" << pred_target << std::dec << std::endl;
    next_IF_ID_REG = latch;
    next_pc = pred_target;
}