#include "CPU.hpp"
#include "RiscV.hpp"
#include <iostream>
void CPU::memory_access() {
    if(EX_MEM_REG.empty()) return;
    auto latch = EX_MEM_REG.front();
    EX_MEM_REG.pop();
    
    RiscV::MEM_WB_Latch nextlatch{};
    nextlatch.rd_idx = latch.rd_idx;
    nextlatch.ctrl = latch.ctrl;
    nextlatch.alu_result = latch.alu_result;
    nextlatch.pc = latch.pc;
    
    if(latch.ctrl.mem_read || latch.ctrl.mem_write) {
        //get_phys_addr is unnecessary because it is in the memory class, 
        uint32_t addr = /*get_phys_addr*/(latch.alu_result);
        //std::cout << "[MEMORY ACCESS] " << (latch.ctrl.mem_read ? "READ" : "WRITE") 
        //          << " Addr: 0x" << std::hex << latch.alu_result 
        //          << " (Phys: 0x" << addr << ")"
        //          << " Size: " << ((latch.ctrl.mem_size == RiscV::MEM_SIZE::BYTE) ? "BYTE" : (latch.ctrl.mem_size == RiscV::MEM_SIZE::HALF) ? "HALF" : "WORD")
        //          << std::dec << std::endl;
        if (latch.ctrl.mem_write) {
            
            if (latch.alu_result == 0x10000000) { // UART
                //std::cout << "[UART OUTPUT] latch.alu_result = 0x" << std::hex << latch.store_val << std::dec << std::endl;
                if (latch.ctrl.mem_size == RiscV::MEM_SIZE::BYTE) {
                    std::cout << static_cast<char>(latch.store_val & 0xFF) << std::flush;
                } else {
                    std::cout << "[WARN] Invalid access size to UART" << std::endl;
                }
                //std::string tmp;
                //std::cin >> tmp; // Enterで次のサイクルへ進む
            } else if (latch.alu_result >= 0x10000001 && latch.alu_result <= 0x1FFFFFFF) { // Halt
                std::cout << "\n[SYSTEM] Halt signal received. Shutting down..." << std::endl;
                halted = true;
            } else {
                switch(latch.ctrl.mem_size) {
                    case RiscV::MEM_SIZE::BYTE: 
                        memory.write_byte(addr,     latch.store_val & 0xFF);
                    break;
                    case RiscV::MEM_SIZE::HALF:
                        memory.write_byte(addr,     latch.store_val & 0xFF);
                        memory.write_byte(addr + 1, (latch.store_val >> 8) & 0xFF);
                        break;
                    case RiscV::MEM_SIZE::WORD:
                        // MemoryStage.cpp の中（WORDストアの例）
                        memory.write_byte(addr,     latch.store_val & 0xFF);
                        memory.write_byte(addr + 1, (latch.store_val >> 8) & 0xFF);
                        memory.write_byte(addr + 2, (latch.store_val >> 16) & 0xFF);
                        memory.write_byte(addr + 3, (latch.store_val >> 24) & 0xFF);
                        break; 
                }
                //std::cout << "[MEMORY WRITE] Addr: 0x" << std::hex << latch.alu_result 
                //          << " Data: 0x" << latch.store_val 
                //          << " Size: " << ((latch.ctrl.mem_size == RiscV::MEM_SIZE::BYTE) ? "BYTE" : (latch.ctrl.mem_size == RiscV::MEM_SIZE::HALF) ? "HALF" : "WORD")
                //          << std::dec << std::endl;
            }
        } else if (latch.ctrl.mem_read) {
            uint32_t raw_data = 0;
            switch (latch.ctrl.mem_size) {
                case RiscV::MEM_SIZE::BYTE:
                    raw_data = memory.read_byte(addr);
                    if (!latch.ctrl.mem_unsigned) raw_data = RiscV::sign_extension(raw_data, 8);
                    break;
                case RiscV::MEM_SIZE::HALF:
                    raw_data = memory.read_byte(addr) | (memory.read_byte(addr + 1) << 8);
                    if (!latch.ctrl.mem_unsigned) raw_data = RiscV::sign_extension(raw_data, 16);
                    break;
                case RiscV::MEM_SIZE::WORD:
                    raw_data =   memory.read_byte(addr) | 
                                (memory.read_byte(addr + 1) << 8) |
                                (memory.read_byte(addr + 2) << 16) | 
                                (memory.read_byte(addr + 3) << 24);
                    break;
                std::cout << "[MEMORY READ] Addr: 0x" << std::hex << latch.alu_result 
                          << " Data: 0x" << raw_data 
                          << " Size: " << ((latch.ctrl.mem_size == RiscV::MEM_SIZE::BYTE) ? "BYTE" : (latch.ctrl.mem_size == RiscV::MEM_SIZE::HALF) ? "HALF" : "WORD")
                          << std::dec << std::endl;
            }
            nextlatch.mem_read_data = raw_data;
        }
    }
    MEM_WB_REG.push(nextlatch);
}