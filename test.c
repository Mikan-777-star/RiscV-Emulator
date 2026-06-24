// test.c

// あなたの実装に合わせたメモリマップの定義
#define UART_THR  ((volatile unsigned char*)0x10000000)
#define SYS_HALT  ((volatile unsigned int*)0x10000004)

// 1文字出力する関数（ロード待ちをしない、あなたのエミュレータ専用仕様よ）
void print_char(char c) {
    *UART_THR = c;
}

// 文字列を出力する関数
void print_str(const char* s) {
    while (*s) {
        print_char(*s);
        s++;
    }
}

// プログラムのエントリーポイント
void main() {
    print_str("Hello, RISC-V Pipeline!\n");

    // ループ条件とフォワーディングのテスト（1文字を5回出力）
    // インクリメントと条件分岐が激しく発生するから、ハザード制御のテストに最適よ
    for (int i = 0; i < 5; i++) {
        print_char('A' + i); // A, B, C, D, E と出力されるはずよ
    }
    print_char('\n');

    // 処理が正常に終了したら、あなたのエミュレータにシャットダウンを通知する
    *SYS_HALT = 1;

    
}