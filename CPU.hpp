#pragma once
#include "RiscV.hpp"
#include <queue>
#include <vector>
#include <array>
#include <unordered_map>

class CPU {
private:
    std::vector<uint8_t> memory;
    std::array<uint32_t, 32> registers;
    uint32_t pc;
    bool last_stall_flag = false;
    bool halted = false;
    std::queue<RiscV::IF_ID_Latch> IF_ID_REG;
    std::queue<RiscV::ID_EX_Latch> ID_EX_REG;
    std::queue<RiscV::EX_MEM_Latch> EX_MEM_REG;
    std::queue<RiscV::MEM_WB_Latch> MEM_WB_REG;
    // キー: 分岐命令のPC
    // 値: {2ビットカウンタ(0~3), ターゲットPC}
    std::unordered_map<uint32_t, std::pair<uint8_t, uint32_t>> branch_predictor;
    std::unordered_map<uint32_t, uint32_t> csrs;
public:
    CPU();
    bool is_halted();
    // テスト用のインターフェース（内部状態を外からいじるためのもの）
    void write_memory_word(uint32_t load_addr, uint32_t inst);
    void inject_instruction(uint32_t inst) ;
    void set_register(uint8_t idx, uint32_t val);
    void set_pc(uint32_t new_pc) ;    
    bool is_id_ex_empty() const;
    RiscV::ID_EX_Latch get_id_ex_latch() const;
    uint32_t get_register(uint8_t rs);
    void  flush_pipeline();
    void reset_pipeline() ;

    void decode();

    void execute();
    void memory_access();
    uint64_t retired_inst_count = 0;
    void write_back();
    
    uint32_t get_phys_addr(uint32_t addr) const;
    void fetch() ;
    void tick() ;

    bool load_binary(const std::string& filename, uint32_t load_addr) ;
};