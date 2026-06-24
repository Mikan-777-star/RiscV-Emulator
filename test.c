// test.c
#include <stdint.h>
#include <stddef.h>

// 循環右シフトのマクロ（せいじのお気に入り）
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

// 初期ハッシュ値
static const uint32_t H[] = {
    0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
    0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
};

// ラウンド定数
static const uint32_t K[] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL, 0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

// 自製memcpy（依存排除）
static void my_memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// 自製memset（依存排除）
static void my_memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (char)c;
    }
}

static uint32_t big_endian_to_host_32(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8)  | ((uint32_t)bytes[3]);
}

static void host_to_big_endian_32(uint32_t val, unsigned char *bytes) {
    bytes[0] = (unsigned char)((val >> 24) & 0xFF);
    bytes[1] = (unsigned char)((val >> 16) & 0xFF);
    bytes[2] = (unsigned char)((val >> 8) & 0xFF);
    bytes[3] = (unsigned char)(val & 0xFF);
}

// 64バイトのブロックを処理するコア圧縮関数
static void sha256_transform(uint32_t h[8], const unsigned char *chunk) {
    uint32_t w[64];
    for (int j = 0; j < 16; ++j) {
        w[j] = big_endian_to_host_32(&chunk[j * 4]);
    }
    for (int j = 16; j < 64; ++j) {
        uint32_t s0 = ROTR(w[j-15], 7) ^ ROTR(w[j-15], 18) ^ (w[j-15] >> 3);
        uint32_t s1 = ROTR(w[j-2], 17) ^ ROTR(w[j-2], 19) ^ (w[j-2] >> 10);
        w[j] = w[j-16] + s0 + w[j-7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], _h = h[7];

    for (int j = 0; j < 64; ++j) {
        uint32_t S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = _h + S1 + ch + K[j] + w[j];

        uint32_t S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        _h = g; g = f; f = e;
        e = d + temp1;
        d = c; c = b; b = a;
        a = temp1 + temp2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += _h;
}

// メモリアロケーションを一切行わないベアメタル用SHA-256
void sha256_hash(const unsigned char *message, size_t len, unsigned char *output_hash) {
    uint32_t h[8];
    my_memcpy(h, H, sizeof(H));

    unsigned char chunk[64];
    size_t i = 0;
    size_t remaining = len;

    // 1. 64バイトきっちり埋まるブロックを先に処理
    while (remaining >= 64) {
        sha256_transform(h, &message[i]);
        i += 64;
        remaining -= 64;
    }

    // 2. 最後のパディングブロックの作成（mallocの完全排除）
    my_memset(chunk, 0, 64);
    my_memcpy(chunk, &message[i], remaining);
    chunk[remaining] = 0x80; // '1' ビットを追加

    // もし残りスペースが少なく、長さを入れる8バイトが足りない場合は、2ブロックに分ける
    if (remaining >= 56) {
        sha256_transform(h, chunk);
        my_memset(chunk, 0, 64);
    }

    // 末尾にビット長をビッグエンディアンで叩き込む
    uint64_t total_bits = (uint64_t)len * 8;
    chunk[56] = (unsigned char)((total_bits >> 56) & 0xFF);
    chunk[57] = (unsigned char)((total_bits >> 48) & 0xFF);
    chunk[58] = (unsigned char)((total_bits >> 40) & 0xFF);
    chunk[59] = (unsigned char)((total_bits >> 32) & 0xFF);
    chunk[60] = (unsigned char)((total_bits >> 24) & 0xFF);
    chunk[61] = (unsigned char)((total_bits >> 16) & 0xFF);
    chunk[62] = (unsigned char)((total_bits >> 8) & 0xFF);
    chunk[63] = (unsigned char)(total_bits & 0xFF);

    sha256_transform(h, chunk);

    // 最終結果の書き出し
    for (int j = 0; j < 8; ++j) {
        host_to_big_endian_32(h[j], &output_hash[j * 4]);
    }
}

// あなたの実装に合わせたメモリマップの定義
// UARTとシャットダウン用のレジスタアドレス
#define UART_THR      ((volatile char *)0x10000000)
#define SYS_SHUTDOWN  ((volatile char *)0x10000004)

// 1文字UARTへ出力
void uart_putchar(char c) {
    *UART_THR = c;
}

// 16進数をアスキー文字に変換して出力
void uart_print_hex(uint8_t val) {
    const char hex_chars[] = "0123456789abcdef";
    uart_putchar(hex_chars[(val >> 4) & 0x0F]);
    uart_putchar(hex_chars[val & 0x0F]);
}

void uart_print_str(const char *str) {
    while (*str) {
        uart_putchar(*str++);
    }
}

int main(void) {
    // テスト入力
    const char *test_data = "abc";
    unsigned char result[32];
    
    // 計算開始
    sha256_hash((const unsigned char *)test_data, 3, result);
    
    // 結果をシリアル出力
    uart_print_str("SHA-256 Result: ");
    for (int i = 0; i < 32; i++) {
        uart_print_hex(result[i]);
    }
    uart_print_str("\n");
    
    // シャットダウン通知（あなたのエミュレータ終了フラグ）
    *SYS_SHUTDOWN = 1;
    
    // フォールバック用の無限待機
    return 0;
}