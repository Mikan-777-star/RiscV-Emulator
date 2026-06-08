#include "CPU.hpp"
#include "RiscV.hpp"

uint32_t CPU::get_phys_addr(uint32_t addr) const {
    const uint32_t BASE_ADDR = 0x80000000;
    if (addr >= BASE_ADDR && addr < BASE_ADDR + memory.size()) {
        return addr - BASE_ADDR;
    }
    return 0; // 範囲外アクセス
}

void CPU::fetch() {
    if (pc < 0x80000000 || pc >= 0x80000000 + memory.size() - 3) return;
    if (last_stall_flag) return; 

    uint32_t p_addr = get_phys_addr(pc);
    uint32_t inst = memory.read_byte(pc) |
                    (memory.read_byte(pc + 1) << 8) |
                    (memory.read_byte(pc + 2) << 16) |
                    (memory.read_byte(pc + 3) << 24);
    bool pred_taken = false;
    uint32_t pred_target = pc + 4; 

    auto it = branch_predictor.find(pc);
    if (it != branch_predictor.end() && it->second.first >= 2) {
        pred_taken = true;
        pred_target = it->second.second;
    }

    RiscV::IF_ID_Latch latch = { inst, pc, pred_taken, pred_target };
    IF_ID_REG.push(latch);
    pc = pred_target;
}