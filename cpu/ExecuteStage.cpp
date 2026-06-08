#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>

void CPU::execute() {
    using namespace RiscV;
    if (ID_EX_REG.empty()) return;
    
    ID_EX_Latch latch = ID_EX_REG.front();
    ID_EX_REG.pop();
    
    EX_MEM_Latch nextlatch{};
    nextlatch.rd_idx = latch.rd_idx;
    nextlatch.ctrl = latch.ctrl;
    nextlatch.store_val = latch.val_rs2;
    nextlatch.pc = latch.pc;
    
    uint32_t alu_in1 = 0;
    uint32_t alu_in2 = 0;
    
    switch (latch.ctrl.src1_sel) {
        case ALU_SRC1::RS1:  alu_in1 = latch.val_rs1; break;
        case ALU_SRC1::PC:   alu_in1 = latch.pc; break;
        case ALU_SRC1::ZERO: alu_in1 = 0; break;
    }
    switch (latch.ctrl.src2_sel) {
        case ALU_SRC2::RS2: alu_in2 = latch.val_rs2; break;
        case ALU_SRC2::IMM: alu_in2 = static_cast<uint32_t>(latch.imm); break;
    }
    
    // --- フォワーディングロジック ---
    if (latch.ctrl.src1_sel == ALU_SRC1::RS1) alu_in1 = registers[latch.rs1_idx];
    if (latch.ctrl.src2_sel == ALU_SRC2::RS2) alu_in2 = registers[latch.rs2_idx];
    if (latch.ctrl.mem_write) nextlatch.store_val = registers[latch.rs2_idx];
    
    if (!MEM_WB_REG.empty()) {
        auto mem_wb_latch = MEM_WB_REG.back();
        if (mem_wb_latch.ctrl.reg_write && mem_wb_latch.rd_idx != 0) {
            uint32_t fw_val = 0;
            if (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::ALU) fw_val = mem_wb_latch.alu_result;
            else if (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::PC4) fw_val = mem_wb_latch.pc + 4;
            else fw_val = mem_wb_latch.mem_read_data;
            
            if (latch.ctrl.src1_sel == ALU_SRC1::RS1 && mem_wb_latch.rd_idx == latch.rs1_idx) alu_in1 = fw_val;
            if (latch.ctrl.src2_sel == ALU_SRC2::RS2 && mem_wb_latch.rd_idx == latch.rs2_idx) alu_in2 = fw_val;
            if (latch.ctrl.mem_write && mem_wb_latch.rd_idx == latch.rs2_idx) nextlatch.store_val = fw_val;
        }
    }
    
    // --- ALU 計算 ---
    uint32_t alu_result = 0;
    switch (latch.ctrl.alu_op) {
        case ALU_OPS::ADD:  alu_result = alu_in1 + alu_in2; break;
        case ALU_OPS::SUB:  alu_result = alu_in1 - alu_in2; break;
        case ALU_OPS::AND:  alu_result = alu_in1 & alu_in2; break;
        case ALU_OPS::OR:   alu_result = alu_in1 | alu_in2; break;
        case ALU_OPS::XOR:  alu_result = alu_in1 ^ alu_in2; break;
        case ALU_OPS::SLL:  alu_result = alu_in1 << (alu_in2 & 0x1F); break;
        case ALU_OPS::SRL:  alu_result = alu_in1 >> (alu_in2 & 0x1F); break;
        case ALU_OPS::SRA:  alu_result = static_cast<uint32_t>(static_cast<int32_t>(alu_in1) >> (alu_in2 & 0x1F)); break;
        case ALU_OPS::SLT:  alu_result = (static_cast<int32_t>(alu_in1) < static_cast<int32_t>(alu_in2)) ? 1 : 0; break;
        case ALU_OPS::SLTU: alu_result = (alu_in1 < alu_in2) ? 1 : 0; break;
    }
    nextlatch.alu_result = alu_result;
    
    // --- 分岐判定と予測答え合わせ ---
    bool take_branch = false;
    uint32_t target_pc = 0;
    if (latch.ctrl.is_jump) {
        take_branch = true;
        target_pc = alu_result;
        if (latch.ctrl.wb_src == WB_SRC::PC4) target_pc &= ~1;
    } else if (latch.ctrl.is_branch) {
        int32_t r1 = static_cast<int32_t>(registers[latch.rs1_idx]);
        int32_t r2 = static_cast<int32_t>(registers[latch.rs2_idx]);
        if(!MEM_WB_REG.empty()){
            auto mem_wb_latch = MEM_WB_REG.back();
            if (mem_wb_latch.ctrl.reg_write && mem_wb_latch.rd_idx != 0) {
                uint32_t fw_val = (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::ALU) ? mem_wb_latch.alu_result : 
                                  (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::PC4) ? mem_wb_latch.pc + 4 : mem_wb_latch.mem_read_data;
                if(latch.rs1_idx == mem_wb_latch.rd_idx) r1 = fw_val;
                if(latch.rs2_idx == mem_wb_latch.rd_idx) r2 = fw_val; 
            }
        }
        switch (latch.ctrl.branch_op) {
            case 0x0: take_branch = (r1 == r2); break;
            case 0x1: take_branch = (r1 != r2); break;
            case 0x4: take_branch = (r1 < r2); break;
            case 0x5: take_branch = (r1 >= r2); break;
            case 0x6: take_branch = (static_cast<uint32_t>(r1) < static_cast<uint32_t>(r2)); break;
            case 0x7: take_branch = (static_cast<uint32_t>(r1) >= static_cast<uint32_t>(r2)); break;
        }
        if (take_branch) target_pc = latch.pc + latch.imm;
    }
    
    if (latch.ctrl.is_branch || latch.ctrl.is_jump) {
        bool actually_taken = take_branch;
        uint32_t actual_target = take_branch ? target_pc : latch.pc + 4;
    
        auto& pred = branch_predictor[latch.pc];
        if (actually_taken) {
            if (pred.first < 3) pred.first++;
            pred.second = target_pc;
        } else {
            if (pred.first > 0) pred.first--;
        }
    
        if ((latch.predicted_taken != actually_taken) || (actually_taken && latch.predicted_target != actual_target)) {
            pc = actual_target; 
            flush_pipeline(); 
        }
    }
    
    if (latch.ctrl.is_ecall) {
        if (registers[17] == 93) {
            std::cout << "ECALL: Exit requested. Status: " << registers[10] << std::endl;
            exit(0);
        }
    }
    EX_MEM_REG.push(nextlatch);
}