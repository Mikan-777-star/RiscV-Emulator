#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>

// 1. フォワーディングの解決（最新の値をバイパス）
// void CPU::resolve_forwarding(const RiscV::ID_EX_Latch& latch, uint32_t& alu_in1, uint32_t& alu_in2, uint32_t& store_val) {
//     std::cout << "DEBUG EX: latch.rs2_idx = " << (int)latch.rs1_idx 
//           << ", latch.val_rs2 = 0x" << std::hex << latch.val_rs2 << std::dec << std::endl;
//     using namespace RiscV;
//     std::cout << "Resolving forwarding for EX stage: RS1=" << static_cast<int>(latch.rs1_idx) 
//               << " RS2=" << static_cast<int>(latch.rs2_idx) 
//               << " RD=" << static_cast<int>(latch.rd_idx) << std::endl;
//     // 初期値のセット
//     switch (latch.ctrl.src1_sel) {
//         case ALU_SRC1::RS1:  alu_in1 = latch.val_rs1; break;
//         case ALU_SRC1::PC:   alu_in1 = latch.pc; break;
//         case ALU_SRC1::ZERO: alu_in1 = 0; break;
//     }
//     switch (latch.ctrl.src2_sel) {
//         case ALU_SRC2::RS2: alu_in2 = latch.val_rs2; break;
//         case ALU_SRC2::IMM: alu_in2 = static_cast<uint32_t>(latch.imm); break;
//     }
//     store_val = latch.val_rs2;

//     // 優先度1: WBステージ（すでに registers に書き込まれている最新値の取得）
//     if (latch.ctrl.src1_sel == ALU_SRC1::RS1) alu_in1 = registers[latch.rs1_idx];
//     if (latch.ctrl.src2_sel == ALU_SRC2::RS2) alu_in2 = registers[latch.rs2_idx];
//     if (latch.ctrl.mem_write) store_val = registers[latch.rs2_idx];

//     // 優先度2: MEMステージ (MEM_WB_REG の最新結果で上書き)
//     if (!MEM_WB_REG.empty()) {
//         auto mem_wb_latch = MEM_WB_REG.back();
//         if (mem_wb_latch.ctrl.reg_write && mem_wb_latch.rd_idx != 0) {
//             uint32_t fw_val = 0;
//             if (mem_wb_latch.ctrl.wb_src == WB_SRC::ALU) fw_val = mem_wb_latch.alu_result;
//             else if (mem_wb_latch.ctrl.wb_src == WB_SRC::PC4) fw_val = mem_wb_latch.pc + 4;
//             else fw_val = mem_wb_latch.mem_read_data;

//             if (latch.ctrl.src1_sel == ALU_SRC1::RS1 && mem_wb_latch.rd_idx == latch.rs1_idx) alu_in1 = fw_val;
//             if (latch.ctrl.src2_sel == ALU_SRC2::RS2 && mem_wb_latch.rd_idx == latch.rs2_idx) alu_in2 = fw_val;
//             if (latch.ctrl.mem_write && mem_wb_latch.rd_idx == latch.rs2_idx) store_val = fw_val;
//         }
//     }
// }
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
    if (!MEM_WB_REG.empty()) {
        auto wb_latch = MEM_WB_REG.front();
        if (wb_latch.ctrl.reg_write && wb_latch.rd_idx != 0) {
            uint32_t wb_data = 0;
            switch (wb_latch.ctrl.wb_src) {
                case WB_SRC::ALU: wb_data = wb_latch.alu_result; break;
                case WB_SRC::MEM: wb_data = wb_latch.mem_read_data; break;
                case WB_SRC::PC4: wb_data = wb_latch.pc + 4; break;
            }
            if (wb_latch.rd_idx == latch.rs1_idx) alu_in1 = wb_data;
            if (wb_latch.rd_idx == latch.rs2_idx) {
                if (latch.ctrl.src2_sel == ALU_SRC2::RS2) alu_in2 = wb_data;
                store_val = wb_data; // ✨修正: ストアデータもフォワーディング
            }
        }
    }

    // 優先度1: MEMステージからのフォワーディング (1つ前の命令・こちらが最新なので上書きする)
    if (!EX_MEM_REG.empty()) {
        auto mem_latch = EX_MEM_REG.front(); 
        if (mem_latch.ctrl.reg_write && mem_latch.rd_idx != 0 && !mem_latch.ctrl.mem_read) {

            // フォワーディングするべき正しいデータを判定する
            uint32_t forward_data = mem_latch.alu_result;
            if (mem_latch.ctrl.wb_src == WB_SRC::PC4) {
                forward_data = mem_latch.pc + 4; // ✨ JAL/JALRの戻り先アドレス（PC+4）を正しくフォワーディング
            }

            if (!mem_latch.ctrl.mem_read) { // LOAD命令以外の場合
                if (mem_latch.rd_idx == latch.rs1_idx) {
                    alu_in1 = forward_data; // ✨ 正しい戻り先アドレスがJALRのベースに入る！
                }
                if (mem_latch.rd_idx == latch.rs2_idx) {
                    if (latch.ctrl.src2_sel == ALU_SRC2::RS2) alu_in2 = forward_data;
                    store_val = forward_data;
                }
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
        if (!MEM_WB_REG.empty()) {
            auto mem_wb_latch = MEM_WB_REG.front();
            if (mem_wb_latch.ctrl.reg_write && mem_wb_latch.rd_idx != 0) {
                uint32_t fw_val = (mem_wb_latch.ctrl.wb_src == WB_SRC::ALU) ? mem_wb_latch.alu_result :
                                  (mem_wb_latch.ctrl.wb_src == WB_SRC::PC4) ? mem_wb_latch.pc + 4 : mem_wb_latch.mem_read_data;
                if (latch.rs1_idx == mem_wb_latch.rd_idx) r1 = fw_val;
                if (latch.rs2_idx == mem_wb_latch.rd_idx) r2 = fw_val;
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
            flush_pipeline();
        }
    }
}

// 司令塔となるメインの execute ステージ
void CPU::execute() {
    if (ID_EX_REG.empty()) return;

    RiscV::ID_EX_Latch latch = ID_EX_REG.front();
    ID_EX_REG.pop();

    RiscV::EX_MEM_Latch nextlatch{};
    nextlatch.rd_idx = latch.rd_idx;
    nextlatch.ctrl = latch.ctrl;
    nextlatch.pc = latch.pc;

    uint32_t alu_in1 = 0;
    uint32_t alu_in2 = 0;

    // 1. フォワーディングの解決
    resolve_forwarding(latch, alu_in1, alu_in2, nextlatch.store_val);

    // 2. ALU演算の実行
    //std::cout << "EX: PC=0x" << std::hex << latch.pc << " ALU_IN1=0x" << alu_in1 << " ALU_IN2=0x" << alu_in2 << std::endl;
    nextlatch.alu_result = calculate_alu(latch.ctrl.alu_op, alu_in1, alu_in2);

    // 3. 分岐判定と予測処理
    bool take_branch = false;
    uint32_t target_pc = 0;
    evaluate_branch_and_predict(latch, nextlatch.alu_result, take_branch, target_pc);

    // システムコール(ECALL)の処理
    if (latch.ctrl.is_ecall) {
        if (registers[17] == 93) {
            std::cout << "ECALL: Exit requested. Status: " << registers[10] << std::endl;
            exit(0);
        }
    }

    EX_MEM_REG.push(nextlatch);
}