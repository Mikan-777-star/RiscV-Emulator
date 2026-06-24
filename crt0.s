.global _start
.section .text

_start:
    # スタックポインタの初期化
    li sp, 0x801FFFF0
    
    # main 関数を呼び出し
    call main

halt:
    # シャットダウン通知
    li t0, 0x10000004
    li t1, 1
    sb t1, 0(t0)
halt_loop:
    wfi
    j halt_loop
