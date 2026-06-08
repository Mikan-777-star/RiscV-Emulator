#pragma once
#include <vector>
#include <cstdint>
#include <string>

class Memory {
private:
    std::vector<uint8_t> mem_array;
    static const uint32_t BASE_ADDR = 0x80000000;

public:
    // 初期サイズを指定してメモリを確保（デフォルトは2MB）
    Memory(size_t size = 1024 * 1024 * 2);

    // 仮想（論理）アドレスから、生配列の物理インデックスへの変換
    uint32_t get_phys_addr(uint32_t addr) const;

    // 範囲チェック付きの安全な読み書きインターフェース
    uint8_t  read_byte(uint32_t addr) const;
    void     write_byte(uint32_t addr, uint8_t val);

    // バイナリファイルを特定の論理アドレスへロードする機能
    bool load_binary(const std::string& filename, uint32_t load_addr);

    // デバッグやサイズ取得用
    size_t size() const { return mem_array.size(); }
};