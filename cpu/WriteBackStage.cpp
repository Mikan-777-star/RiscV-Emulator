#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>
void CPU::write_back() {
    if (MEM_WB_REG.empty()) return;
    auto latch = MEM_WB_REG.front();
    MEM_WB_REG.pop();
    if (latch.ctrl.reg_write && latch.rd_idx != 0) {
        uint32_t write_data = 0;
        switch (latch.ctrl.wb_src) {
            case RiscV::WB_SRC::ALU: write_data = latch.alu_result; break;
            case RiscV::WB_SRC::MEM: write_data = latch.mem_read_data; break;
            case RiscV::WB_SRC::PC4: write_data = latch.pc + 4; break; 
        }
        //std::cout << "[WRITE BACK] RD: x" << std::dec << static_cast<int>(latch.rd_idx) 
        //          << " Data: 0x" << std::hex << write_data 
        //          << " Source: "  << ((latch.ctrl.wb_src == RiscV::WB_SRC::ALU) ? "ALU" : (latch.ctrl.wb_src == RiscV::WB_SRC::MEM) ? "MEM" : "PC+4")
        //          << std::dec << std::endl;
        registers[latch.rd_idx] = write_data;
    }
    
    bool is_nop = (!latch.ctrl.reg_write && !latch.ctrl.mem_read && !latch.ctrl.mem_write && 
                   !latch.ctrl.is_branch && !latch.ctrl.is_jump && !latch.ctrl.is_ecall);
    if (!is_nop) {
        retired_inst_count++;
    }
}