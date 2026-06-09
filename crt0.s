.global _start
.section .text

_start:
    # スタックポインタの初期化
    li sp, 0x801FFFF0
    
    # main 関数を呼び出し
    call main

    # main から戻ってきたら終了処理へ
    # 0x10000004番地に1を書き込んでシャットダウン通知
    li t0, 0x10000004
    li t1, 1
    sb t1, 0(t0)

    # 念のための無限ループ（フォールバック）
    # wfi命令でCPUを待機状態にするのがベストプラクティスよ
halt_loop:
    wfi
    j halt_loop