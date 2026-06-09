#include "CPU.hpp"
#include <iostream>
#include <iomanip>
// --- Test Driver ---
struct TestCase {
    std::string name;
    uint32_t inst;
    uint32_t rs1_val;
    uint32_t rs2_val;
    bool exp_reg_write;
    bool exp_mem_read;
    bool exp_mem_write;
    uint32_t exp_imm;
    RiscV::ALU_SRC1 exp_src1;
    RiscV::ALU_SRC2 exp_src2;
    RiscV::WB_SRC exp_wb;
};
void run_pipeline_test() {
    std::cout << "--- Starting End-to-End Pipeline Test ---\n";
    CPU cpu;

    // テストプログラムをメモリにロード (アドレス 0x80000000 から)
    // 1. ADDI x1, x0, 5    (x1 = 5)  : 0x00500093
    // 2. ADDI x2, x0, 10   (x2 = 10) : 0x00A00113
    // 3. ADD  x3, x1, x2   (x3 = x1 + x2 = 15) : 0x002081B3
    
    std::vector<uint32_t> program = {
        0x00500093,
        0x00A00113,
        0x002081B3
    };

    uint32_t load_addr = 0x80000000;
    for (uint32_t inst : program) {
        // 命令をメモリにリトルエンディアンで書き込むためのヘルパーメソッドをCPUクラスに用意するか、
        // ここでは便宜的に内部に注入したと仮定するロジックを組むこと。
        // （実際にはCPUクラスに load_program(vector, addr) のようなメソッドを作るべきよ）
        cpu.write_memory_word(load_addr, inst); 
        load_addr += 4;
    }

    // クロックサイクルを回して状態を観察する
    for (int cycle = 1; cycle <= 10; ++cycle) {
        cpu.tick();
        std::cout << "Cycle " << cycle 
                  << " | x1: " << std::setw(2) << cpu.get_register(1) 
                  << " | x2: " << std::setw(2) << cpu.get_register(2) 
                  << " | x3: " << std::setw(2) << cpu.get_register(3) << "\n";
    }
}
void run_tests() {
    using namespace RiscV;
    std::vector<TestCase> tests = {
        { "ADDI (Neg)", 0xFFF00513, 0, 0, true, false, false, 0xFFFFFFFF, ALU_SRC1::RS1, ALU_SRC2::IMM, WB_SRC::ALU },
        { "SW", 0x00A12223, 0x100, 0x55, false, false, true, 4, ALU_SRC1::RS1, ALU_SRC2::IMM, WB_SRC::ALU },
        { "JAL (Neg)", 0xFEDFF0EF, 0, 0, true, false, false, 0xFFFFFFEC, ALU_SRC1::PC, ALU_SRC2::IMM, WB_SRC::PC4 },
        { "JALR", 0xFFC500E7, 0x1000, 0, true, false, false, 0xFFFFFFFC, ALU_SRC1::RS1, ALU_SRC2::IMM, WB_SRC::PC4 },
        { "LUI", 0x12345537, 0, 0, true, false, false, 0x12345000, ALU_SRC1::ZERO, ALU_SRC2::IMM, WB_SRC::ALU }
    };

    std::cout << "--- Starting Decoder Tests ---\n";
    
    for (const auto& t : tests) {
        CPU cpu; // テストごとにクリーンなインスタンスを生成
        
        cpu.inject_instruction(t.inst);
        cpu.set_register((t.inst >> 15) & 0x1f, t.rs1_val);
        cpu.set_register((t.inst >> 20) & 0x1f, t.rs2_val);
        
        cpu.decode();

        if (cpu.is_id_ex_empty()) {
            std::cout << "[FAIL] " << t.name << ": No output generated.\n";
            continue;
        }
        
        ID_EX_Latch result = cpu.get_id_ex_latch();
        bool pass = true;

        if (result.ctrl.reg_write != t.exp_reg_write) pass = false;
        if (result.ctrl.mem_read != t.exp_mem_read) pass = false;
        if (result.ctrl.mem_write != t.exp_mem_write) pass = false;
        if (static_cast<uint32_t>(result.imm) != t.exp_imm) pass = false;
        if (result.ctrl.src1_sel != t.exp_src1) pass = false;
        if (result.ctrl.src2_sel != t.exp_src2) pass = false;
        if (result.ctrl.wb_src != t.exp_wb) pass = false;

        if (pass) {
            std::cout << "[PASS] " << t.name << "\n";
        } else {
            std::cout << "[FAIL] " << t.name << "\n";
            std::cout << "  Exp Imm: " << std::hex << t.exp_imm << " / Act: " << static_cast<uint32_t>(result.imm) << "\n";
            std::cout << std::boolalpha;
            std::cout << "  Exp RW: " << t.exp_reg_write << " / Act: " << result.ctrl.reg_write << "\n";
            // 残りの出力も同様に可能
        }
    }
}
void run_branch_test() {
    std::cout << "--- Starting Branch Hazard Test ---\n";
    CPU cpu;

    // 1. ADDI x1, x0, 1    (x1 = 1) 
    // 2. BEQ  x1, x1, 8    (x1 == x1 なのでPC+8へジャンプ。つまり3の命令を飛ばして4へ) 
    // 3. ADDI x2, x0, 99   (★実行されてはいけない！ x2 が 99 になったらフラッシュ失敗) 
    // 4. ADDI x3, x0, 2    (x3 = 2) 

    std::vector<uint32_t> program = {
        0x00100093, // ADDI x1, x0, 1
        0x00108463, // BEQ x1, x1, 8
        0x06300113, // ADDI x2, x0, 99
        0x00200193,  // ADDI x3, x0, 2
        0x00200193,
    };

    uint32_t load_addr = 0x80000000;
    for (uint32_t inst : program) {
        cpu.write_memory_word(load_addr, inst); 
        load_addr += 4;
    }

    for (int cycle = 1; cycle <= 15; ++cycle) {
        cpu.tick();
        std::cout << "Cycle " << std::setw(2) << cycle 
                  << " | x1: " << std::setw(2) << cpu.get_register(1) 
                  << " | x2: " << std::setw(2) << cpu.get_register(2) 
                  << " | x3: " << std::setw(2) << cpu.get_register(3) << "\n";
    }
}

void run_loop_test() {
    std::cout << "--- Starting Backward Loop Test ---\n";
    CPU cpu;

    std::vector<uint32_t> program = {
        0x00300093, // ADDI x1, x0, 3
        0xFFF08093, // ADDI x1, x1, -1
        0xFE009EE3  // BNE x1, x0, -4
    };

    uint32_t load_addr = 0x80000000;
    for (uint32_t inst : program) {
        cpu.write_memory_word(load_addr, inst); 
        load_addr += 4;
    }

    // 十分なサイクル数を回してループが終了するか観察する
    for (int cycle = 1; cycle <= 20; ++cycle) {
        cpu.tick();
        std::cout << "Cycle " << std::setw(2) << cycle 
                  << " | x1: " << std::setw(2) << cpu.get_register(1) << "\n";
    }
}

void run_load_binary_test(){
    CPU cpu;
    cpu.memory.load_binary("test.bin", 0x80000000);
    std::string tmp;
    int cycle = 0;
    while (!cpu.is_halted() ) {
        cycle++;
        cpu.tick();
       // /*
       std::cout << std::hex << "Cycle " << std::setw(2) << cycle 
                  << "\n |  x1: " << std::setw(2) << cpu.get_register(1) 
                  << " |  x2: " << std::setw(2) << cpu.get_register(2) 
                  << " |  x3: " << std::setw(2) << cpu.get_register(3) << "\n"
                  << " |  x4: " << std::setw(2) << cpu.get_register(4) 
                  << " |  x5: " << std::setw(2) << cpu.get_register(5) 
                  << " |  x6: " << std::setw(2) << cpu.get_register(6) << "\n"
                  << " |  x7: " << std::setw(2) << cpu.get_register(7) 
                  << " |  x8: " << std::setw(2) << cpu.get_register(8) 
                  << " |  x9: " << std::setw(2) << cpu.get_register(9) << "\n"
                  << " | x10: " << std::setw(2) << cpu.get_register(10) 
                  << " | x11: " << std::setw(2) << cpu.get_register(11) 
                  << " | x12: " << std::setw(2) << cpu.get_register(12) << "\n"
                  << " | x13: " << std::setw(2) << cpu.get_register(13) 
                  << " | x14: " << std::setw(2) << cpu.get_register(14) 
                  << " | x15: " << std::setw(2) << cpu.get_register(15) << "\n"
                  << " | x16: " << std::setw(2) << cpu.get_register(16) 
                  << " | x17: " << std::setw(2) << cpu.get_register(17) 
                  << " | x18: " << std::setw(2) << cpu.get_register(18) << "\n"
                  << " | x19: " << std::setw(2) << cpu.get_register(19) 
                  << " | x20: " << std::setw(2) << cpu.get_register(20) 
                  << " | x21: " << std::setw(2) << cpu.get_register(21) << "\n"
                  << " | x22: " << std::setw(2) << cpu.get_register(22) 
                  << " | x23: " << std::setw(2) << cpu.get_register(23) 
                  << " | x24: " << std::setw(2) << cpu.get_register(24) << "\n"
                  << " | x25: " << std::setw(2) << cpu.get_register(25) 
                  << " | x26: " << std::setw(2) << cpu.get_register(26) 
                  << " | x27: " << std::setw(2) << cpu.get_register(27) << "\n"
                  << " | x28: " << std::setw(2) << cpu.get_register(28) 
                  << " | x29: " << std::setw(2) << cpu.get_register(29) 
                  << " | x30: " << std::setw(2) << cpu.get_register(30) << "\n"
                  << " | x31: " << std::setw(2) << cpu.get_register(31) << "\n\n";
     // */ 
            std::cin >> tmp; // Enterで次のサイクルへ進む
    } 
    double ipc = static_cast<double>(cpu.retired_inst_count) / cycle;
    std::cout << "Execution finished in " << std::dec << cycle << " cycles.\n";
    std::cout << "Retired Instructions: " << cpu.retired_inst_count << "\n";
    std::cout << "IPC: " << std::fixed << std::setprecision(3) << ipc << "\n";
}
int main() {
    
    run_load_binary_test();
    return 0;
}