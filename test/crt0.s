.global _start
_start:
    # あなたのエミュレータのメモリサイズ(2MB)の末尾付近にスタックポインタ(x2)を設定
    li sp, 0x801FFFF0
    
    # C言語の main 関数へジャンプ (JAL)
    call main

    # mainから戻ってきたら、安全に無限ループして停止させる
inf_loop:
    j inf_loop