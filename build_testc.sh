# 1. アセンブラとコンパイラの実行
riscv64-unknown-elf-as -march=rv32i -mabi=ilp32 crt0.s -o crt0.o
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -ffreestanding -nostdlib -c test.c -o test.o

# 2. リンク（リンカスクリプトが必要よ）
# 0x80000000からコードを配置すると仮定してリンクしなさい
riscv64-unknown-elf-ld -m elf32lriscv -Ttext=0x80000000 crt0.o test.o -o firmware.elf

# 3. バイナリ（生の命令列）の抽出
riscv64-unknown-elf-objcopy -O binary firmware.elf firmware.bin