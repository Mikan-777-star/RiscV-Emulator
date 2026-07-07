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
bool flag = false;
void run_load_binary_test(const std::string& binary_path) {
    CPU cpu = CPU();
    cpu.memory.load_binary(binary_path, 0x80000000);
    std::string tmp;
    int cycle = 0;
    while (!cpu.is_halted() ) {
        cycle++;
        //std::cout << "Cycle " << std::setw(2) << cycle << "\n";
        cpu.tick();
        if(flag){
            std::cin >> tmp; // Enterで次のサイクルへ進む
             std::cout << std::hex << "Cycle " << std::setw(2) << cycle 
            << "\n | PC: 0x" << std::setw(8) << cpu.get_pc() << " "<< cpu.disassemble_riscv(cpu.memory.read_byte(cpu.get_pc()) | (cpu.memory.read_byte(cpu.get_pc() + 1) << 8) | (cpu.memory.read_byte(cpu.get_pc() + 2) << 16) | (cpu.memory.read_byte(cpu.get_pc() + 3) << 24))
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
        }
       
           // std::cin >> tmp; // Enterで次のサイクルへ進む
    } 
    double ipc = static_cast<double>(cpu.retired_inst_count) / cycle;
    std::cout << "Execution finished in " << std::dec << cycle << " cycles.\n";
    std::cout << "Retired Instructions: " << cpu.retired_inst_count << "\n";
    std::cout << "IPC: " << std::fixed << std::setprecision(3) << ipc << "\n";
}
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "[ERROR] Usage: " << argv[0] << " <path_to_binary.bin>\n";
        return 1;
    }
    std::string binary_path = argv[1];
    run_load_binary_test(binary_path);
    return 0;
}