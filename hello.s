.global _start
.section .text

_start:
    # スタックポインタの初期化 (2MBメモリの末尾)
    li sp, 0x80200000
    
    # 文字列の先頭アドレスをロード
    la s0, hello_str
    li s1, 0x10000000  # UART_THRのアドレス

loop:
    lb a0, 0(s0)       # 文字をロード
    beqz a0, halt      # 0(NULL)なら終了
    sb a0, 0(s1)       # UARTに書き出し
    addi s0, s0, 1     # 次の文字へ
    j loop

halt:
    # シャットダウン通知
    li t0, 0x10000004
    li t1, 1
    sb t1, 0(t0)
    
inf_loop:
    wfi
    j inf_loop

.section .rodata
hello_str:
    .string "Hello, RISC-V Pipeline!\n"