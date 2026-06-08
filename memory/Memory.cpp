#include "Memory.hpp"
#include <fstream>
#include <iostream>

Memory::Memory(size_t size) : mem_array(size, 0) {}

uint32_t Memory::get_phys_addr(uint32_t addr) const {
    if (addr >= BASE_ADDR && addr < BASE_ADDR + mem_array.size()) {
        return addr - BASE_ADDR;
    }
    // 範囲外アクセスの場合は0を返す（本来はトラップや例外を投げるべき箇所よ）
    return 0; 
}

uint8_t Memory::read_byte(uint32_t addr) const {
    uint32_t p_addr = get_phys_addr(addr);
    return mem_array[p_addr];
}

void Memory::write_byte(uint32_t addr, uint8_t val) {
    uint32_t p_addr = get_phys_addr(addr);
    // x0への書き込み拒否はCPU側の責任だけど、メモリの範囲外書き込みはここでガードできるわ
    if (addr >= BASE_ADDR && addr < BASE_ADDR + mem_array.size()) {
        mem_array[p_addr] = val;
    }
}

bool Memory::load_binary(const std::string& filename, uint32_t load_addr) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "[ERROR] Failed to open file: " << filename << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    uint32_t p_addr = get_phys_addr(load_addr);
    if (p_addr + static_cast<uint32_t>(size) > mem_array.size()) {
        std::cerr << "[ERROR] Binary size (" << size << ") exceeds memory limit." << std::endl;
        return false;
    }

    if (file.read(reinterpret_cast<char*>(&mem_array[p_addr]), size)) {
        std::cout << "[INFO] Loaded " << size << " bytes to " << std::hex << load_addr << std::endl;
        return true;
    }
    return false;
}