#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>
void CPU::decode() {
    using namespace RiscV;
    last_stall_flag = false;
    //if (IF_ID_REG.empty()) return;
    auto firstlatch = current_IF_ID_REG;
    ID_EX_Latch latch{}; 
    uint32_t inst = firstlatch.inst;
    uint8_t opcode = inst & 0x7f;
    uint8_t funct3 = (inst >> 12) & 0x7;
    uint8_t funct7 = (inst >> 25);
    latch.valid = firstlatch.valid;
    latch.imm_unsigned = (inst >> 20) & 0xfff;
    latch.rs1_idx = (inst >> 15) & 0x1f;
    latch.rs2_idx = (inst >> 20) & 0x1f;
    latch.rd_idx = (inst >> 7) & 0x1f;
    latch.csr_addr = (inst >> 20) & 0xFFF;
    latch.pc = firstlatch.pc;
    latch.predicted_taken = firstlatch.predicted_taken;
    latch.predicted_target = firstlatch.predicted_target;
    latch.func3 = funct3;
   // if(latch.pc == 0x8000008c){
       //std::cout << latch.pc <<" : " << disassemble_riscv(inst) << std::endl;
      // std::exit(1);
    //}
    switch (opcode) {
        
    case OP_RType:
        latch.ctrl.reg_write = true;
        latch.ctrl.wb_src = WB_SRC::ALU;
        latch.ctrl.src1_sel = ALU_SRC1::RS1;
        latch.ctrl.src2_sel = ALU_SRC2::RS2;
        switch (funct3) {
            case 0x0: latch.ctrl.alu_op = (funct7 == 0) ? ALU_OPS::ADD : ALU_OPS::SUB; break;
            case 0x1: latch.ctrl.alu_op = ALU_OPS::SLL; break;
            case 0x2: latch.ctrl.alu_op = ALU_OPS::SLT; break;
            case 0x3: latch.ctrl.alu_op = ALU_OPS::SLTU; break;
            case 0x4: latch.ctrl.alu_op = ALU_OPS::XOR; break;
            case 0x5: latch.ctrl.alu_op = (funct7 == 0x20) ? ALU_OPS::SRA : ALU_OPS::SRL; break;
            case 0x6: latch.ctrl.alu_op = ALU_OPS::OR; break;
            case 0x7: latch.ctrl.alu_op = ALU_OPS::AND; break;
        }
        break;
    case OP_IMM:
        latch.ctrl.wb_src = WB_SRC::ALU;
        latch.ctrl.src1_sel = ALU_SRC1::RS1;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.reg_write = true;
        latch.imm = sign_extension(inst >> 20, 12);
        switch (funct3) {
            case 0x0: latch.ctrl.alu_op = ALU_OPS::ADD; break;
            case 0x2: latch.ctrl.alu_op = ALU_OPS::SLT; break;
            case 0x3: latch.ctrl.alu_op = ALU_OPS::SLTU; break;
            case 0x4: latch.ctrl.alu_op = ALU_OPS::XOR; break;
            case 0x6: latch.ctrl.alu_op = ALU_OPS::OR; break;
            case 0x7: latch.ctrl.alu_op = ALU_OPS::AND; break;
            case 0x1: latch.ctrl.alu_op = ALU_OPS::SLL; latch.imm &= 0x1F; break;
            case 0x5: latch.ctrl.alu_op = (funct7 == 0x20) ? ALU_OPS::SRA : ALU_OPS::SRL; latch.imm &= 0x1F; break;
        }
        break;
    case OP_LOAD:
        latch.ctrl.alu_op = ALU_OPS::ADD;
        latch.ctrl.src1_sel = ALU_SRC1::RS1;
        latch.ctrl.wb_src = WB_SRC::MEM;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.mem_read = true;
        latch.ctrl.reg_write = true;
        latch.imm = sign_extension(inst >> 20, 12);
        latch.ctrl.mem_size = static_cast<MEM_SIZE>(funct3 & 0x3);
        latch.ctrl.mem_unsigned = (funct3 & 0x4) != 0;
        break;
    case OP_STORE:
        latch.ctrl.alu_op = ALU_OPS::ADD;
        latch.ctrl.src1_sel = ALU_SRC1::RS1;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.mem_write = true;
        latch.imm = sign_extension(((inst >> 25) << 5) | ((inst >> 7) & 0x1f), 12);
        latch.ctrl.mem_size = static_cast<MEM_SIZE>(funct3 & 0x3);
        break;
    case OP_BRANCH:
        latch.ctrl.src1_sel = ALU_SRC1::PC;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.imm = sign_extension(((inst >> 31) << 12) | ((inst >> 7) & 1) << 11 | ((inst >> 25) & 0x3f) << 5 | ((inst >> 8) & 0xf) << 1, 13);
        latch.ctrl.is_branch = true;
        latch.ctrl.branch_op = funct3;
        break;
    case OP_LUI:
        latch.ctrl.wb_src = WB_SRC::ALU; // またはLUI専用のソース
        latch.ctrl.src1_sel = ALU_SRC1::ZERO;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.reg_write = true;
        latch.ctrl.alu_op = ALU_OPS::ADD; // 0 + IMM を計算させるため
        
        // 💡 修正：明確に int32_t にキャストしてから代入する
        // inst & 0xFFFFF000 の結果を確実に int32_t として確定させるのよ
        latch.imm = static_cast<int32_t>(inst & 0xFFFFF000);
        break;
    case OP_AUIPC:
        latch.ctrl.src1_sel = ALU_SRC1::PC;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.reg_write = true;
        latch.imm = inst & 0xFFFFF000;
        break;
    case OP_JAL:
        latch.ctrl.wb_src = WB_SRC::PC4;
        latch.ctrl.is_jump = true;
        latch.ctrl.src1_sel = ALU_SRC1::PC;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.reg_write = true;
        latch.imm = sign_extension(((inst >> 31) & 0x1) << 20 | ((inst >> 12) & 0xff) << 12 | ((inst >> 20) & 0x1) << 11 | ((inst >> 21) & 0x3ff) << 1, 21);
        //std::cout << "Decoded JAL: imm=" << std::hex << latch.imm << std::dec << std::endl;
        break;
    case OP_JALR:
        latch.ctrl.wb_src = WB_SRC::PC4;
        latch.ctrl.is_jump = true;
        latch.ctrl.reg_write = true;
        latch.ctrl.src1_sel = ALU_SRC1::RS1;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.imm = sign_extension(inst >> 20, 12);
        latch.ctrl.alu_op = ALU_OPS::ADD;
        //std::cout << "Decoded JALR: imm=" << std::hex << latch.imm  << " RS1=" << static_cast<int>(latch.rs1_idx) << std::dec << std::endl;
        break;
    case OP_SYSTEM:
        if (funct3 == 0x0) {
            switch (inst >> 20) {
                case 0x000: latch.ctrl.is_ecall = true; break;
                case 0x001: latch.ctrl.is_ebreak = true; break;
                case 0x302: latch.ctrl.is_mret = true; break;
            }
        }else{
            latch.ctrl.wb_src = RiscV::WB_SRC::ALU; // CSR命令の結果はALUの出力として扱う
            
            latch.ctrl.is_csrr = true;
        }
        break;
    case OP_FENCE:
        if (funct3 == 0x1) {
        latch.ctrl.is_fence_i = true; // ✨ FENCE.Iを厳密に識別
    } else {
        latch.ctrl.is_fence = true;   // 通常のFENCE（今回はNOP扱いでいいわ）
    }
        break;
    }
    // --- ストール判定 ---

    
    auto ex_latch = current_ID_EX_REG; // ※お使いの設計に合わせてfront/backは要確認
    if (ex_latch.ctrl.mem_read && ex_latch.rd_idx != 0) {         
        bool use_rs1 = (latch.ctrl.src1_sel == ALU_SRC1::RS1 || latch.ctrl.is_branch || opcode == OP_JALR); 
        bool use_rs2 = (latch.ctrl.src2_sel == ALU_SRC2::RS2 || latch.ctrl.is_branch || latch.ctrl.mem_write);
        if((use_rs1 && (latch.rs1_idx == ex_latch.rd_idx)) || (use_rs2 && (latch.rs2_idx == ex_latch.rd_idx))){
            next_ID_EX_REG = RiscV::ID_EX_Latch();
            last_stall_flag = true;
            return;
        }
    }

    bool is_pipeline_busy = (current_ID_EX_REG.valid || current_EX_MEM_REG.valid || current_MEM_WB_REG.valid);
    if(is_pipeline_busy && (latch.ctrl.is_ecall || latch.ctrl.is_ebreak || latch.ctrl.is_mret || latch.ctrl.is_fence_i || latch.ctrl.is_csrr)){
        next_ID_EX_REG = RiscV::ID_EX_Latch();
        ///std::cout << "stall"  << "valid ex men wb"  << current_ID_EX_REG.valid << " " <<  current_EX_MEM_REG.valid <<" " << current_MEM_WB_REG.valid << std::endl;
        last_stall_flag = true;
        return;
    }
    next_ID_EX_REG = latch;
}