# 1. アセンブル (オブジェクトファイル生成)
riscv64-unknown-elf-as -march=rv32i -mabi=ilp32 hello.s -o hello.o

# 2. リンク (0x80000000 に配置)
riscv64-unknown-elf-ld -m elf32lriscv -Ttext=0x80000000 hello.o -o hello.elf

# 3. バイナリ抽出 (エミュレータが読み込める生の命令列にする)
riscv64-unknown-elf-objcopy -O binary hello.elf hello.bin