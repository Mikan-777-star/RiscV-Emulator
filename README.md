# RiscV-Emulator

A lightweight pipeline Risc-V architecture emulator written in pure C++ from scratch. 
This project is developed to deeply understand computer architecture  and low-level software/hardware interfaces.

## 🚀 Features & Current Status

- **Architecture:** RV32I (32-bit base integer instruction set)
- **Status:** Under active development 
  - Base integer instruction decoding and execution loop

## 🛠️ Supported Instructions

- [x] R-type (add, sub, sll, slt, sltu, xor, srl, sra, or, and)
- [x] I-type (addi, slti, sltiu, xori, ori, andi, slli, srli, srai)
- [x] Load/Store instructions (lw, sw, etc.) [※注5]
- [x] Branch/Jump instructions (beq, bne, jal, jalr, etc.) [※注5]

## 💻 How to Build & Run

### Prerequisites
- C++17 compatible compiler (GCC or Clang)
- Make or CMake (whichever you use)

## Run
```
./rv32i-emu
```

## 🗺️ Roadmap & TODO
- [ ] Implement remaining RV32I instructions (Branching and Memory Access).

- [ ] Enhance CSR (Control and Status Registers) implementation.

- [ ] Create a comprehensive test suite using RISC-V architectural tests.

- [ ] Add support for RV32M (Multiplication and Division extension) in the future.

## 🤝 Contributing
Contributions, issues, and feature requests are welcome! 
Feel free to check the Issues page if you want to contribute or give feedback.

