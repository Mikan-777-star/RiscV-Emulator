#pragma once
#include "RiscV.hpp"
#include "Memory.hpp"
#include <queue>
#include <vector>
#include <array>
#include <unordered_map>

class CPU {
private:
    // ExecuteStageから切り出す3つの戦略的ヘルパー関数
    void resolve_forwarding(const RiscV::ID_EX_Latch& latch, uint32_t& alu_in1, uint32_t& alu_in2, uint32_t& store_val);
    uint32_t calculate_alu(RiscV::ALU_OPS alu_op, uint32_t alu_in1, uint32_t alu_in2);
    void evaluate_branch_and_predict(const RiscV::ID_EX_Latch& latch, uint32_t alu_result, bool& take_branch, uint32_t& target_pc);
    uint32_t registers[32];
    uint32_t pc;
    uint32_t last_actual_target = 0; // 
    bool different_flag = false;
    uint32_t last_stall_pc = 0;
    bool last_stall_flag = false;
    bool halted = false;
    RiscV::IF_ID_Latch current_IF_ID_REG, next_IF_ID_REG;
    RiscV::ID_EX_Latch current_ID_EX_REG, next_ID_EX_REG;
    RiscV::EX_MEM_Latch current_EX_MEM_REG, next_EX_MEM_REG;
    RiscV::MEM_WB_Latch current_MEM_WB_REG, next_MEM_WB_REG;
    // キー: 分岐命令のPC
    // 値: {2ビットカウンタ(0~3), ターゲットPC}
    std::unordered_map<uint32_t, std::pair<uint8_t, uint32_t>> branch_predictor;
    std::unordered_map<uint32_t, uint32_t> csrs;
public:
    Memory memory;
    CPU();
    bool is_halted();
    // テスト用のインターフェース（内部状態を外からいじるためのもの）
    void write_memory_word(uint32_t load_addr, uint32_t inst);
    void set_register(uint8_t idx, uint32_t val);
    void set_pc(uint32_t new_pc) ;    
    uint32_t get_register(uint8_t rs);
    void  flush_pipeline();

    void decode();

    void execute();
    void memory_access();
    uint64_t retired_inst_count = 0;
    void write_back();
    
    //uint32_t get_phys_addr(uint32_t addr) const;
    void fetch() ;
    void tick() ;
    std::string disassemble_riscv(uint32_t inst);

    //bool load_binary(const std::string& filename, uint32_t load_addr) ;
};