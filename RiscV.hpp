#pragma once
#include <cstdint>
#include <string>
#include <fstream>

namespace RiscV {
    // --- 定数定義 ---
    constexpr uint8_t OP_IMM    = 0x13;
    constexpr uint8_t OP_RType  = 0x33;
    constexpr uint8_t OP_LOAD   = 0x3;
    constexpr uint8_t OP_STORE  = 0x23;
    constexpr uint8_t OP_BRANCH = 0x63;
    constexpr uint8_t OP_JAL    = 0x6f;
    constexpr uint8_t OP_JALR   = 0x67;
    constexpr uint8_t OP_LUI    = 0x37;
    constexpr uint8_t OP_AUIPC  = 0x17;
    constexpr uint8_t OP_SYSTEM = 0x73;
    constexpr uint8_t OP_FENCE  = 0x0F;

    enum class ALU_OPS { ADD, SUB, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU };
    enum class ALU_SRC1 { RS1, PC, ZERO };
    enum class ALU_SRC2 { RS2, IMM };
    enum class WB_SRC { ALU, MEM, PC4 };
    enum class MEM_SIZE { BYTE, HALF, WORD };
    
   
    // --- 構造体 ---
    struct ControlSignals {
        ALU_OPS alu_op{ALU_OPS::ADD};
        ALU_SRC1 src1_sel{ALU_SRC1::RS1};
        ALU_SRC2 src2_sel{ALU_SRC2::RS2};
        bool mem_read{false};
        bool mem_write{false};
        bool reg_write{false};
        MEM_SIZE mem_size{MEM_SIZE::WORD};
        bool mem_unsigned{false};
        bool is_branch{false};
        uint8_t branch_op{0};
        bool is_jump{false};
        WB_SRC wb_src{WB_SRC::ALU};
        bool is_ecall{false};
        bool is_ebreak{false};
        bool is_fence{false};

    };
    struct IF_ID_Latch {
        uint32_t inst;
        uint32_t pc;   
        bool predicted_taken{false};
        uint32_t predicted_target{0};
    };
    struct ID_EX_Latch {
        uint32_t val_rs1{0};
        uint32_t val_rs2{0};
        int32_t imm{0};
        uint32_t imm_unsigned{0};
        uint8_t rs1_idx{0};
        uint8_t rs2_idx{0};
        uint8_t rd_idx{0};
        uint32_t pc{0};
        ControlSignals ctrl{};
        bool predicted_taken{false};
        uint32_t predicted_target{0};
    };

    struct EX_MEM_Latch {
        uint32_t alu_result{0};
        uint32_t store_val{0};
        uint8_t rd_idx{0};
        uint32_t pc{0};
        ControlSignals ctrl{};
        bool take_branch{false};
        uint32_t target_pc{0};
    };

    struct MEM_WB_Latch {
        uint32_t alu_result{0};
        uint32_t mem_read_data{0};
        uint8_t rd_idx{0};
        uint32_t pc{0};  
        ControlSignals ctrl{};
    };

    // 符号拡張ユーティリティ
    inline int32_t sign_extension(uint32_t buf, uint32_t bits) {
        if (bits >= 32) return static_cast<int32_t>(buf);
        
        // 一度、対象ビットの最上位が32ビット環境の符号ビット（bit 31）に来るまで左シフトする
        // その後、int32_t にキャストして右シフトすることで、コンパイラが自動で符号拡張（算術右シフト）を行う
        int32_t shift_amount = 32 - bits;
        return (static_cast<int32_t>(buf << shift_amount)) >> shift_amount;
    }
    // --- 各ラッチのデバッグ出力 ---
}