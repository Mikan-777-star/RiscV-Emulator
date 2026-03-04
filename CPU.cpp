#include "RiscV.hpp"
#include "CPU.hpp"
#include <cstdint>
#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <iomanip>
#include <array>
#include <fstream>
#include <iterator>
#include <unordered_map>

CPU::CPU() : memory(1024 * 1024 * 2, 0), registers({0}), pc(0x80000000) {}
bool CPU::is_halted(){
    return halted;
}
// テスト用のインターフェース（内部状態を外からいじるためのもの）
void CPU::write_memory_word(uint32_t load_addr, uint32_t inst){
    load_addr = get_phys_addr(load_addr);
    memory[load_addr] = inst & 0xff;
    memory[load_addr + 1] = (inst >> 8) & 0xff;
    memory[load_addr + 2] = (inst >> 16) & 0xff;
    memory[load_addr + 3] = (inst >> 24) & 0xff;
}
void CPU::inject_instruction(uint32_t inst) {RiscV::IF_ID_Latch latch = {inst, pc};pc+=4 ;IF_ID_REG.push(latch); }
void CPU::set_register(uint8_t idx, uint32_t val) { if (idx != 0) registers[idx] = val; }
void CPU::set_pc(uint32_t new_pc) { pc = new_pc; }
//uint32_t get_phys_addr(uint32_t addr){ return addr & 0x001FFFFF;}

bool CPU::is_id_ex_empty() const { return ID_EX_REG.empty(); }
RiscV::ID_EX_Latch CPU::get_id_ex_latch() const { return ID_EX_REG.front(); }
uint32_t CPU::get_register(uint8_t rs){
    return registers[rs];
}
void  CPU::flush_pipeline(){
    //if(DEBUG)std::cout << "pipeline_flushed" << std::endl;
    IF_ID_REG = std::queue<RiscV::IF_ID_Latch>();
    ID_EX_REG = std::queue<RiscV::ID_EX_Latch>();
}
void CPU::reset_pipeline() {
    IF_ID_REG = std::queue<RiscV::IF_ID_Latch>();
    ID_EX_REG = std::queue<RiscV::ID_EX_Latch>();
    EX_MEM_REG = std::queue<RiscV::EX_MEM_Latch>();
    registers.fill(0);
    pc = 0x80000000;
}
void CPU::decode() {
    using namespace RiscV;
    if (IF_ID_REG.empty()) return;
    last_stall_flag = false;
    auto firstlatch = IF_ID_REG.front();
    ID_EX_Latch latch{}; // ゼロ初期化
    uint32_t inst = firstlatch.inst;
    uint8_t opcode = inst & 0x7f;
    uint8_t funct3 = (inst >> 12) & 0x7;
    uint8_t funct7 = (inst >> 25);
    latch.rs1_idx = (inst >> 15) & 0x1f;
    latch.rs2_idx = (inst >> 20) & 0x1f;
    latch.rd_idx = (inst >> 7) & 0x1f;
    
    latch.val_rs1 = registers[latch.rs1_idx];
    latch.val_rs2 = registers[latch.rs2_idx];
    latch.pc = firstlatch.pc;
    latch.predicted_taken = firstlatch.predicted_taken;
    latch.predicted_target = firstlatch.predicted_target;
    switch (opcode) {
    case OP_RType:
        latch.ctrl.reg_write = true;
        latch.ctrl.wb_src = WB_SRC::ALU;
        switch (funct3) {
            case 0x0: latch.ctrl.alu_op = (funct7 == 0) ? ALU_OPS::ADD : ALU_OPS::SUB; break;
            case 0x1: latch.ctrl.alu_op = ALU_OPS::SLL; break;
            case 0x2: latch.ctrl.alu_op = ALU_OPS::SLT; break;
            case 0x3: latch.ctrl.alu_op = ALU_OPS::SLTU; break;
            case 0x4: latch.ctrl.alu_op = ALU_OPS::XOR; break;
            case 0x5: latch.ctrl.alu_op = (funct7 == 0) ? ALU_OPS::SRL : ALU_OPS::SRA; break;
            case 0x6: latch.ctrl.alu_op = ALU_OPS::OR; break;
            case 0x7: latch.ctrl.alu_op = ALU_OPS::AND; break;
        }
        break;
    case OP_IMM:
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
            case 0x5: latch.ctrl.alu_op = (funct7 == 0) ? ALU_OPS::SRL : ALU_OPS::SRA; latch.imm &= 0x1F; break;
        }
        break;
    case OP_LOAD:
        latch.ctrl.wb_src = WB_SRC::MEM;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.mem_read = true;
        latch.ctrl.reg_write = true;
        latch.imm = sign_extension(inst >> 20, 12);
        latch.ctrl.mem_size = static_cast<MEM_SIZE>(funct3 & 0x3);
        latch.ctrl.mem_unsigned = (funct3 & 0x4) != 0;
        break;
    case OP_STORE:
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
        latch.ctrl.src1_sel = ALU_SRC1::ZERO;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.ctrl.reg_write = true;
        latch.imm = inst & 0xFFFFF000;
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
        break;
    case OP_JALR:
        latch.ctrl.wb_src = WB_SRC::PC4;
        latch.ctrl.is_jump = true;
        latch.ctrl.reg_write = true;
        latch.ctrl.src2_sel = ALU_SRC2::IMM;
        latch.imm = sign_extension(inst >> 20, 12);
        break;
    case OP_SYSTEM:
        if (funct3 == 0x0) {
            switch (inst >> 20) {
                case 0x000: latch.ctrl.is_ecall = true; break;
                case 0x001: latch.ctrl.is_ebreak = true; break;
            }
        }
        break;
    case OP_FENCE:
        latch.ctrl.is_fence = true;
        break;
    }
    
    // ストール（データハザード）判定
   // --- 最小化されたストール判定 (Load-Use Hazardのみ) ---
    bool stall = false;
    if (!EX_MEM_REG.empty()) {
        auto ex_latch = EX_MEM_REG.back();
        // 直前の命令がメモリ読み込み(LOAD)で、かつ今回そのレジスタを使うなら1サイクル待つ
        if (ex_latch.ctrl.mem_read && ex_latch.rd_idx != 0) {
            bool use_rs1 = (latch.ctrl.src1_sel == ALU_SRC1::RS1 || latch.ctrl.is_branch || latch.ctrl.is_jump);
            bool use_rs2 = (latch.ctrl.src2_sel == ALU_SRC2::RS2 || latch.ctrl.is_branch || latch.ctrl.mem_write);
            if (use_rs1 && ex_latch.rd_idx == latch.rs1_idx) stall = true;
            if (use_rs2 && ex_latch.rd_idx == latch.rs2_idx) stall = true;
        }
    }
    if (stall) {
        //std::cout << "[STALL] NOP inserted" << std::endl;
        last_stall_flag = true;
        ID_EX_REG.push(ID_EX_Latch{}); // NOP（気泡）を挿入して待つ
        return;
    }
    // -------------------------------------------------- 
    IF_ID_REG.pop();
    ID_EX_REG.push(latch);
}
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
    // --- フォワーディング (Bypassing) ユニット ---
    
    // 優先度1: WBステージ (2サイクル前の命令) からのフォワーディング
    // tick()の仕様上、すでにwrite_back()が完了してregistersに書き込まれているため、再取得する
    if (latch.ctrl.src1_sel == ALU_SRC1::RS1) alu_in1 = registers[latch.rs1_idx];
    if (latch.ctrl.src2_sel == ALU_SRC2::RS2) alu_in2 = registers[latch.rs2_idx];
    if (latch.ctrl.mem_write) nextlatch.store_val = registers[latch.rs2_idx];
    // 優先度2: MEMステージ (1サイクル前の命令) からのフォワーディング
    // tick()の仕様上、1サイクル前のEX結果は今 MEM_WB_REG に押し出されている
    if (!MEM_WB_REG.empty()) {
        auto mem_wb_latch = MEM_WB_REG.back();
        if (mem_wb_latch.ctrl.reg_write && mem_wb_latch.rd_idx != 0) {
            uint32_t fw_val = 0;
            if (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::ALU) fw_val = mem_wb_latch.alu_result;
            else if (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::PC4) fw_val = mem_wb_latch.pc + 4;
            else fw_val = mem_wb_latch.mem_read_data;
            // より新しい1サイクル前のデータがあれば、それで上書きする
            if (latch.ctrl.src1_sel == ALU_SRC1::RS1 && mem_wb_latch.rd_idx == latch.rs1_idx) alu_in1 = fw_val;
            if (latch.ctrl.src2_sel == ALU_SRC2::RS2 && mem_wb_latch.rd_idx == latch.rs2_idx) alu_in2 = fw_val;
            if (latch.ctrl.mem_write && mem_wb_latch.rd_idx == latch.rs2_idx) nextlatch.store_val = fw_val;
        }
    }
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
    bool take_branch = false;
    uint32_t target_pc = 0;
    if (latch.ctrl.is_jump) {
        take_branch = true;
        target_pc = alu_result;
        if (latch.ctrl.wb_src == WB_SRC::PC4) target_pc &= ~1;
    } else if (latch.ctrl.is_branch) {
        
        // ★修正: 分岐判定も古いlatch.val_rs1ではなく、最新レジスタから取得
        int32_t r1 = static_cast<int32_t>(registers[latch.rs1_idx]);
        int32_t r2 = static_cast<int32_t>(registers[latch.rs2_idx]);
        if(!MEM_WB_REG.empty()){
            auto mem_wb_latch = MEM_WB_REG.back();
            if (mem_wb_latch.ctrl.reg_write && mem_wb_latch.rd_idx != 0) {
                uint32_t fw_val = 0;
                if (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::ALU) fw_val = mem_wb_latch.alu_result;
                else if (mem_wb_latch.ctrl.wb_src == RiscV::WB_SRC::PC4) fw_val = mem_wb_latch.pc + 4;
                else fw_val = mem_wb_latch.mem_read_data;
                if(latch.rs1_idx == mem_wb_latch.rd_idx) r1 = fw_val;
                if(latch.rs2_idx == mem_wb_latch.rd_idx) r2 = fw_val; 
            }
        }
        //std::cout << std::hex <<"alu_in1: " << alu_in1 <<" alu_in2: " << alu_in2 << std::endl;
        switch (latch.ctrl.branch_op) {
            case 0x0: take_branch = (r1 == r2); break;
            case 0x1: take_branch = (r1 != r2); break;
            case 0x4: take_branch = (r1 < r2); break;
            case 0x5: take_branch = (r1 >= r2); break;
            case 0x6: take_branch = (static_cast<uint32_t>(r1) < static_cast<uint32_t>(r2)); break;
            case 0x7: take_branch = (static_cast<uint32_t>(r1) >= static_cast<uint32_t>(r2)); break;
        }
        if (take_branch) target_pc = latch.pc + latch.imm;
        //if(DEBUG && take_branch)std::cout << std::hex << "PC : " << latch.pc << " IMM: " << latch.imm <<" target_pc: " << target_pc <<  std::endl; 
    }
    // execute() の中、ALU計算などの後
    if (latch.ctrl.is_branch || latch.ctrl.is_jump) {
        bool actually_taken = take_branch;
        uint32_t actual_target = take_branch ? target_pc : latch.pc + 4;
    
        // --- 1. 予測器の学習 (SRAMの更新) ---
        auto& pred = branch_predictor[latch.pc]; // 参照(&)で取得
        if (actually_taken) {
            if (pred.first < 3) pred.first++; // 飽和カウンタ(+)
            pred.second = target_pc;          // 飛び先を記録
        } else {
            if (pred.first > 0) pred.first--; // 飽和カウンタ(-)
        }
    
        // --- 2. 答え合わせ (ミスプレディクトの検出と修復) ---
        // 「予測と事実が違う」または「飛んだけどターゲットPCの計算が間違っていた」場合
        if ((latch.predicted_taken != actually_taken) || 
            (actually_taken && latch.predicted_target != actual_target)) {
            
            //std::cout << "[MISPREDICT] PC: " << hex_str(latch.pc) << std::endl;
            pc = actual_target; // 正しい未来へPCを修正
            flush_pipeline();   // 間違った未来の命令をすべて破棄
        }
    }
    if (latch.ctrl.is_ecall) {
        uint32_t syscall_num = registers[17];
        if (syscall_num == 93) {
            std::cout << "ECALL: Exit requested. Status: " << registers[10] << std::endl;
            exit(0);
        }
    }
    EX_MEM_REG.push(nextlatch);
}
void CPU::memory_access() {
    if(EX_MEM_REG.empty())return;
    auto latch = EX_MEM_REG.front();
    EX_MEM_REG.pop();
    RiscV::MEM_WB_Latch nextlatch{};
    nextlatch.rd_idx = latch.rd_idx;
    nextlatch.ctrl = latch.ctrl;
    nextlatch.alu_result = latch.alu_result;
    nextlatch.pc = latch.pc;
    if(latch.ctrl.mem_read || latch.ctrl.mem_write) {
        uint32_t addr = get_phys_addr(latch.alu_result);
        if (latch.ctrl.mem_write) {
            if (latch.alu_result == 0x10000000) {
                // アーキテクチャの厳密な制約：UARTへの書き込みは BYTE (SB) のみ許可
                if (latch.ctrl.mem_size == RiscV::MEM_SIZE::BYTE) {
                    std::cout << static_cast<char>(latch.store_val & 0xFF) << std::flush;
                } else {
                    std::cout << "[WARN] Invalid access size to UART at PC: " << std::hex << latch.pc << std::endl;
                }
            } else if (latch.alu_result == 0x10000004) {
                // ★新規: システム停止シグナル
                std::cout << "\n[SYSTEM] Halt signal received. Shutting down..." << std::endl;
                halted = true;
            } else{
                switch(latch.ctrl.mem_size) {
                    case RiscV::MEM_SIZE::BYTE:
                        memory[addr] = latch.store_val & 0xff;
                        break;
                    case RiscV::MEM_SIZE::HALF:
                        memory[addr] = latch.store_val & 0xff;
                        memory[addr + 1] = (latch.store_val >> 8) & 0xff;
                        break;
                    case RiscV::MEM_SIZE::WORD:
                        memory[addr] = latch.store_val & 0xFF;
                        memory[addr+1] = (latch.store_val >> 8) & 0xFF;
                        memory[addr+2] = (latch.store_val >> 16) & 0xFF;
                        memory[addr+3] = (latch.store_val >> 24) & 0xFF;
                        break; 
                }
            }
        }else if (latch.ctrl.mem_read) {
            uint32_t raw_data = 0;
            switch (latch.ctrl.mem_size) {
                case RiscV::MEM_SIZE::BYTE:
                    raw_data = memory[addr];
                    if (!latch.ctrl.mem_unsigned) raw_data = RiscV::sign_extension(raw_data, 8);
                    break;
                case RiscV::MEM_SIZE::HALF:
                    raw_data = memory[addr] | (memory[addr+1] << 8);
                    if (!latch.ctrl.mem_unsigned) raw_data = RiscV::sign_extension(raw_data, 16);
                    break;
                case RiscV::MEM_SIZE::WORD:
                    raw_data = memory[addr] | (memory[addr+1] << 8) | (memory[addr+2] << 16) | (memory[addr+3] << 24);
                    // 32bitアーキテクチャならWORDの符号拡張は不要よ
                    break;
            }
            nextlatch.mem_read_data = raw_data;
        }
    }
    MEM_WB_REG.push(nextlatch);
}
void CPU::write_back() {
    if (MEM_WB_REG.empty()) return;
    auto latch = MEM_WB_REG.front();
    MEM_WB_REG.pop();
    
    // ゼロレジスタ(x0)への書き込みはハードウェア的に無視される。これを確実に表現すること。
    if (latch.ctrl.reg_write && latch.rd_idx != 0) {
        uint32_t write_data = 0;
        switch (latch.ctrl.wb_src) {
            case RiscV::WB_SRC::ALU: write_data = latch.alu_result; break;
            case RiscV::WB_SRC::MEM: write_data = latch.mem_read_data; break;
            case RiscV::WB_SRC::PC4: write_data = latch.pc + 4; break; // ここでついにPC+4が活きるわ
        }
        registers[latch.rd_idx] = write_data;
    }
    bool is_nop = (!latch.ctrl.reg_write && !latch.ctrl.mem_read && !latch.ctrl.mem_write && 
               !latch.ctrl.is_branch && !latch.ctrl.is_jump && !latch.ctrl.is_ecall);
    if (!is_nop) {
        retired_inst_count++;
    }
}

uint32_t CPU::get_phys_addr(uint32_t addr) const {
    const uint32_t BASE_ADDR = 0x80000000;
    if (addr >= BASE_ADDR && addr < BASE_ADDR + memory.size()) {
        return addr - BASE_ADDR;
    }
    // 範囲外アクセスの場合はとりあえず0を返す（本来は例外やトラップを発生させるべき）
    return 0; 
}
void CPU::fetch() {
    if (pc < 0x80000000 || pc >= 0x80000000 + memory.size() - 3) return;
    if (last_stall_flag) return; // ストール時はPCも更新せず、フェッチもしない
    uint32_t p_addr = get_phys_addr(pc);
    uint32_t inst = memory[p_addr] | (memory[p_addr+1] << 8) | (memory[p_addr+2] << 16) | (memory[p_addr+3] << 24);
    bool pred_taken = false;
    uint32_t pred_target = pc + 4; // デフォルトは次の命令
    // ★ 鍵は必ずPC！
    auto it = branch_predictor.find(pc);
    if (it != branch_predictor.end() && it->second.first >= 2) {
        pred_taken = true;
        pred_target = it->second.second;
    }
    RiscV::IF_ID_Latch latch = { 
        inst, 
        pc,
        pred_taken, 
        pred_target
        };
    IF_ID_REG.push(latch);
    // 予測したアドレス（またはPC+4）へ強制的に進む
    pc = pred_target;
} 
void CPU::tick() {
    // 逆順に評価することで、1サイクル中のデータ突き抜けを防ぐ
    //printf("Now PC: %x\n", pc);
    write_back();
    memory_access();
    execute();
    decode();
    
    // ※分岐によるフラッシュ（ストール）が発生した場合はフェッチを止めるなどの
    // 制御ロジックが本来は必要だけど、今は基本のフローを通すわ。
    fetch();
}
bool CPU::load_binary(const std::string& filename, uint32_t load_addr) {
    // 1. ファイルをバイナリモードで開く
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "[ERROR] Failed to open file: " << filename << std::endl;
        return false;
    }
    // 2. ファイルサイズを確認（末尾まで移動して位置を取得）
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    // 3. 読み込む先（メモリ配列）の物理インデックスを計算
    uint32_t p_addr = get_phys_addr(load_addr);
    // 4. メモリの範囲チェック（境界を越えないか確認するのは戦略的リスク管理よ）
    if (p_addr + static_cast<uint32_t>(size) > memory.size()) {
        std::cerr << "[ERROR] Binary size (" << size << ") exceeds memory limit." << std::endl;
        return false;
    }
    // 5. ファイルの内容を memory 配列に直接流し込む
    if (file.read(reinterpret_cast<char*>(&memory[p_addr]), size)) {
        std::cout << "[INFO] Loaded " << size << " bytes to " << std::hex << load_addr << std::endl;
        return true;
    }
    return false;
}