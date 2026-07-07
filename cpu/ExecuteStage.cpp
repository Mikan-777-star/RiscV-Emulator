#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>
#include <iomanip>



void CPU::resolve_forwarding(const RiscV::ID_EX_Latch& latch, uint32_t& alu_in1, uint32_t& alu_in2, uint32_t& store_val) {
    using namespace RiscV;
    
    // 1. 初期値のセット
    switch (latch.ctrl.src1_sel) {
        case ALU_SRC1::RS1:  alu_in1 = registers[latch.rs1_idx]; break;
        case ALU_SRC1::PC:   alu_in1 = latch.pc; break;
        case ALU_SRC1::ZERO: alu_in1 = 0; break;
    }
    switch (latch.ctrl.src2_sel) {
        case ALU_SRC2::RS2: alu_in2 = registers[latch.rs2_idx]; break;
        case ALU_SRC2::IMM: alu_in2 = static_cast<uint32_t>(latch.imm); break;
    }
    store_val = registers[latch.rs2_idx]; // デフォルト値

    // --- ここからフォワーディング (最新の命令から順に適用すべきなのでMEM -> WBの順、または上書き順を意識) ---

    // 優先度2: WBステージからのフォワーディング (2つ前の命令)
    // ※具体的な変数名（MEM_WB_REGのフロントなど）はお使いのコードに合わせてください
    
    auto wb_latch = current_MEM_WB_REG;
    if (wb_latch.ctrl.reg_write && wb_latch.rd_idx != 0) {
        uint32_t wb_data = 0;
        switch (wb_latch.ctrl.wb_src) {
            case WB_SRC::ALU: wb_data = wb_latch.alu_result; break;
            case WB_SRC::MEM: wb_data = wb_latch.mem_read_data; break;
            case WB_SRC::PC4: wb_data = wb_latch.pc + 4; break;
            
        }
        if (latch.ctrl.src1_sel == ALU_SRC1::RS1 && wb_latch.rd_idx == latch.rs1_idx) alu_in1 = wb_data;
        if (latch.ctrl.src2_sel == ALU_SRC2::RS2 && wb_latch.rd_idx == latch.rs2_idx) {
            alu_in2 = wb_data;
        }
        if (wb_latch.rd_idx == latch.rs2_idx) {
            store_val = wb_data; // ✨修正: ストアデータもフォワーディング
        }
    }
    

    // 優先度1: MEMステージからのフォワーディング (1つ前の命令・こちらが最新なので上書きする)
    
    auto mem_latch = current_EX_MEM_REG; 
    if (mem_latch.ctrl.reg_write && mem_latch.rd_idx != 0 && !mem_latch.ctrl.mem_read) {
        // フォワーディングするべき正しいデータを判定する
        uint32_t forward_data = mem_latch.alu_result;
        if (mem_latch.ctrl.wb_src == WB_SRC::PC4) {
            forward_data = mem_latch.pc + 4; // ✨ JAL/JALRの戻り先アドレス（PC+4）を正しくフォワーディング
        }
        if (!mem_latch.ctrl.mem_read) { // LOAD命令以外の場合
            if (latch.ctrl.src1_sel == ALU_SRC1::RS1 && mem_latch.rd_idx == latch.rs1_idx) {
                alu_in1 = forward_data; // ✨ 正しい戻り先アドレスがJALRのベースに入る！
            }
            if (latch.ctrl.src2_sel == ALU_SRC2::RS2 && mem_latch.rd_idx == latch.rs2_idx) {
                alu_in2 = forward_data;
            }
            if (mem_latch.rd_idx == latch.rs2_idx) {
                store_val = forward_data;
            }
        }
    }
}

// 2. 純粋な ALU 演算処理
uint32_t CPU::calculate_alu(RiscV::ALU_OPS alu_op, uint32_t alu_in1, uint32_t alu_in2) {
    using namespace RiscV;
    uint32_t alu_result = 0;

    switch (alu_op) {
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
    return alu_result;
}

// 3. 分岐判定と予測器の更新・答え合わせ
void CPU::evaluate_branch_and_predict(const RiscV::ID_EX_Latch& latch, uint32_t alu_result, bool& take_branch, uint32_t& target_pc) {
    using namespace RiscV;
    take_branch = false;
    target_pc = 0;
    //std::cout << "EX: PC=0x" << std::hex << latch.pc << " ALU_RES=0x" << alu_result  << "latch.ctrl.is_jump=" << latch.ctrl.is_jump <<"latch.ctrl.is_branch=" << latch.ctrl.is_branch << std::endl;
    if (latch.ctrl.is_jump) {
        take_branch = true;
        target_pc = alu_result;
        if (latch.ctrl.wb_src == WB_SRC::PC4) target_pc &= ~1;
    } else if (latch.ctrl.is_branch) {
        int32_t r1 = static_cast<int32_t>(registers[latch.rs1_idx]);
        int32_t r2 = static_cast<int32_t>(registers[latch.rs2_idx]);

        // 分岐比較用のフォワーディング
        auto mem_latch = current_EX_MEM_REG; // かならず current_ を見ること
        if (mem_latch.ctrl.reg_write && mem_latch.rd_idx != 0) {
            uint32_t fw_val = mem_latch.alu_result;
            if (mem_latch.ctrl.wb_src == WB_SRC::PC4) {
                fw_val = mem_latch.pc + 4; // JAL/JALRの戻り先アドレス
            }
            
            // LOAD命令（mem_read）の場合はEXステージでデータが間に合わないので、
            // 本来はDecodeでストールさせる必要があるわ（バグ1で指摘した点よ）。
            if (!mem_latch.ctrl.mem_read) { 
                if (latch.rs1_idx == mem_latch.rd_idx) r1 = static_cast<int32_t>(fw_val);
                if (latch.rs2_idx == mem_latch.rd_idx) r2 = static_cast<int32_t>(fw_val);
            }
        }

        // =================================================================
        // 優先度2：2つ前の命令（WBステージ：current_MEM_WB_REG）からのフォワーディング
        // =================================================================
        // ※あなたの元のコード（next_MEM_WB_REGを見ていた部分）を current_ に修正したものよ
        auto wb_latch = current_MEM_WB_REG; 
        if (wb_latch.ctrl.reg_write && wb_latch.rd_idx != 0) {
            uint32_t fw_val = (wb_latch.ctrl.wb_src == WB_SRC::ALU) ? wb_latch.alu_result :
                              (wb_latch.ctrl.wb_src == WB_SRC::PC4) ? wb_latch.pc + 4 : wb_latch.mem_read_data;
            
            // 1つ前の命令（優先度1）で上書きされていない場合のみ、2つ前の値を適用する
            if (latch.rs1_idx == wb_latch.rd_idx && (current_EX_MEM_REG.rd_idx != latch.rs1_idx || current_EX_MEM_REG.ctrl.mem_read || !current_EX_MEM_REG.ctrl.reg_write)) {
                r1 = static_cast<int32_t>(fw_val);
            }
            if (latch.rs2_idx == wb_latch.rd_idx && (current_EX_MEM_REG.rd_idx != latch.rs2_idx || current_EX_MEM_REG.ctrl.mem_read || !current_EX_MEM_REG.ctrl.reg_write)) {
                r2 = static_cast<int32_t>(fw_val);
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
    //std::cout << "EX: PC=0x" << std::hex << latch.pc << " ALU_RES=0x" << alu_result 
    //          << " BRANCH=" << (take_branch ? "T" : "N") 
    //          << " TARGET=0x" << target_pc << std::endl;

    // 予測器の答え合わせとフラッシュ処理
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

        if ((latch.predicted_taken != actually_taken) || (actually_taken && (latch.predicted_target != actual_target))) {
            different_flag = true;
            last_actual_target = actual_target; // 予測が外れたときの正しいターゲットPCを保存
            //std::cout << "actual_target: " <<std::hex << actual_target << std::dec << "\n"; 
        }
    }
}

void CPU::csr_execute(const RiscV::ID_EX_Latch& latch,RiscV::EX_MEM_Latch& ex_mem_latch, uint32_t alu_in){
    if(!latch.ctrl.is_csrr){return;}
    else{
        //dump_active_csrs();
    }
    //std::cout << "EX: PC=0x" << std::hex << latch.pc << " CSR_EXECUTE" << std::dec << std::endl;
    uint32_t old_val = latch.csr_old_val; // IDステージが先読みしてくれた古い値
    bool is_imm = (latch.func3 & 0x4) != 0;
    // 操作対象のデータ（レジスタ値か、あるいは5bitのゼロ拡張即値か）
    uint32_t rs1_val = alu_in;
    
    // 5bitの即値（zimm）は rs1 フィールド（ビット15-19）に入っているから、rs1_idxそのものよ
    uint32_t zimm = latch.rs1_idx; 

    uint32_t op_val = is_imm ? zimm : rs1_val;
    uint32_t new_val = old_val; 

    // funct3の下位2ビットで操作を決定 (01: RW, 10: RS, 11: RC)
    uint32_t op_type = latch.func3 & 0x3; 

    if (op_type == 1) {        // csrrw / csrrwi
        new_val = op_val;
    } 
    else if (op_type == 2) {   // csrrs / csrrsi
        new_val = old_val | op_val;
    } 
    else if (op_type == 3) {   // csrrc / csrrci
        new_val = old_val & (~op_val);
    }

    ex_mem_latch.csr_write_data = new_val;
    ex_mem_latch.csr_write_addr = latch.csr_addr;
    //printf("EX: PC=0x%08X CSR[0x%03X] = 0x%08X (old: 0x%08X, op_val: 0x%08X)\n", latch.pc, latch.csr_addr, new_val, old_val, op_val);
    // 古い値をWBステージへ送り、rdに書き戻す
    ex_mem_latch.alu_result = old_val;
    
    ex_mem_latch.ctrl.mem_read  = false;
    ex_mem_latch.ctrl.mem_write = false;
}

// 司令塔となるメインの execute ステージ
void CPU::execute() {
    RiscV::ID_EX_Latch latch = current_ID_EX_REG;
    
    if (latch.valid == false) {
        next_EX_MEM_REG = RiscV::EX_MEM_Latch(); // 次のステージへバブルを流す
        //std::cout << "EX: PC=0x" << std::hex << latch.pc << " BUBBLE" << std::dec << std::endl;
        return; // 早期リターンして、古い分岐判定や registers[0] の誤評価を防ぐ！
    }
    RiscV::EX_MEM_Latch nextlatch{};
    nextlatch.valid = true;
    nextlatch.rd_idx = latch.rd_idx;
    nextlatch.ctrl = latch.ctrl;
    nextlatch.pc = latch.pc;

    uint32_t alu_in1 = 0;
    uint32_t alu_in2 = 0;

    // 1. フォワーディングの解決
    resolve_forwarding(latch, alu_in1, alu_in2, nextlatch.store_val);
    //もしかして割り込むならここ説
    if (latch.ctrl.is_csrr) {
        csr_execute(latch, nextlatch, alu_in1);
        
    }else{
        nextlatch.alu_result = calculate_alu(latch.ctrl.alu_op, alu_in1, alu_in2);
    }
    // 2. ALU演算の実行
    //std::cout << "EX: PC=0x" << std::hex << latch.pc << " ALU_IN1=0x" << alu_in1 << " ALU_IN2=0x" << alu_in2 << std::endl;
    
    // 3. 分岐判定と予測処理
    bool take_branch = false;
    uint32_t target_pc = 0;
    evaluate_branch_and_predict(latch, nextlatch.alu_result, take_branch, target_pc);

    // システムコール(ECALL)の処理
    if (latch.ctrl.is_ecall) {
        // 1. 現在の ecall 命令の PC を mepc (0x341) に保存
        // ※ 本来はWBステージでコミットすべきだけど、PCのジャンプと同時に行うならここで即時退避よ
        // std::cout << "EX: PC=0x" << std::hex << latch.pc << " ECALL"  << "\n"
        //            << " |  x1: " << std::setw(2) << get_register(1) 
        //            << " |  x2: " << std::setw(2) << get_register(2) 
        //            << " |  x3: " << std::setw(2) << get_register(3) << "\n"
        //            << " |  x4: " << std::setw(2) << get_register(4) 
        //            << " |  x5: " << std::setw(2) << get_register(5) 
        //            << " |  x6: " << std::setw(2) << get_register(6) << "\n"
        //            << " |  x7: " << std::setw(2) << get_register(7) 
        //            << " |  x8: " << std::setw(2) << get_register(8) 
        //            << " |  x9: " << std::setw(2) << get_register(9) << "\n"
        //            << " | x10: " << std::setw(2) << get_register(10) 
        //            << " | x11: " << std::setw(2) << get_register(11) 
        //            << " | x12: " << std::setw(2) << get_register(12) << "\n"
        //            << " | x13: " << std::setw(2) << get_register(13) 
        //            << " | x14: " << std::setw(2) << get_register(14) 
        //            << " | x15: " << std::setw(2) << get_register(15) << "\n"
        //            << " | x16: " << std::setw(2) << get_register(16) 
        //            << " | x17: " << std::setw(2) << get_register(17) 
        //            << " | x18: " << std::setw(2) << get_register(18) << "\n"
        //            << " | x19: " << std::setw(2) << get_register(19) 
        //            << " | x20: " << std::setw(2) << get_register(20) 
        //            << " | x21: " << std::setw(2) << get_register(21) << "\n"
        //            << " | x22: " << std::setw(2) << get_register(22) 
        //            << " | x23: " << std::setw(2) << get_register(23) 
        //            << " | x24: " << std::setw(2) << get_register(24) << "\n"
        //            << " | x25: " << std::setw(2) << get_register(25) 
        //            << " | x26: " << std::setw(2) << get_register(26) 
        //            << " | x27: " << std::setw(2) << get_register(27) << "\n"
        //            << " | x28: " << std::setw(2) << get_register(28) 
        //            << " | x29: " << std::setw(2) << get_register(29) 
        //            << " | x30: " << std::setw(2) << get_register(30) << "\n"
        //            << " | x31: " << std::setw(2) << get_register(31) <<std::dec<< std::endl;
        if (registers[17] == 93 || 
            (current_EX_MEM_REG.ctrl.wb_src == RiscV::WB_SRC::ALU && current_EX_MEM_REG.rd_idx == 17 && current_EX_MEM_REG.alu_result == 93) ||
            (current_MEM_WB_REG.ctrl.wb_src == RiscV::WB_SRC::ALU && current_MEM_WB_REG.rd_idx == 17 && current_MEM_WB_REG.alu_result == 93)) { // a7 == 93
            auto statuscode = registers[10]; // a0
            if(current_EX_MEM_REG.ctrl.wb_src == RiscV::WB_SRC::ALU && current_EX_MEM_REG.rd_idx == 10){
                statuscode = current_EX_MEM_REG.alu_result;
            }
            if(current_MEM_WB_REG.ctrl.wb_src == RiscV::WB_SRC::ALU && current_MEM_WB_REG.rd_idx == 10){
                statuscode = current_MEM_WB_REG.alu_result;
            }
            printf("Test finished with code: %d\n", statuscode); // a0
            halted = true;
            return;
        }
        csrs[0x341] = latch.pc;
        
        // 2. 例外原因を mcause (0x342) にセット (環境呼び出しは 8 または 11)
        csrs[0x342] = 8; 
        
        // 3. mtvec (0x305) に書き込まれているトラップハンドラのアドレスへジャンプ
        last_actual_target = csrs[0x305];
        //std::cout << "EX: PC=0x" << std::hex << latch.pc << " ECALL: Jumping to trap handler at 0x" << last_actual_target << std::dec << std::endl;
        different_flag = true; 
        // ecall自体はこれ以上後ろのステージで何もさせないためにバブルを流す
        next_EX_MEM_REG = RiscV::EX_MEM_Latch();
        return;
    }

    // --- ✨ mret 命令の処理 ---
    if (latch.ctrl.is_mret) {
        // 1. mepc (0x341) に保存されていた、トラップ発生元のPCを復元してそこへジャンプ
        last_actual_target = csrs[0x341];
        std::cout << "EX: PC=0x" << std::hex << latch.pc << " MRET: Returning to 0x" << last_actual_target << std::dec << std::endl;
        different_flag = true;
        next_EX_MEM_REG = RiscV::EX_MEM_Latch();
        return;
    }
    if (latch.ctrl.is_fence_i) {
        // FENCE.I命令の次のPC（latch.pc + 4）からフェッチをやり直させる
        last_actual_target = latch.pc + 4;
        different_flag = true; // 強制ジャンプ発動（IF_IDラッチのバブル化とPC更新をtickに要求）
        next_EX_MEM_REG = RiscV::EX_MEM_Latch(); // 自身は中身のないラッチを流す
        return;
    }
    next_EX_MEM_REG = nextlatch;
}