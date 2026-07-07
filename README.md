# RISC-V (RV32I) 5-Stage Pipeline Emulator

A cycle-accurate RISC-V (RV32I) 5-stage pipeline instruction set simulator written in pure C++ from scratch. 
This project is developed to deeply understand computer architecture, pipeline control, and hardware/software interfaces.

---

## 🚀 Features & Current Status

- **Architecture:** RV32I (32-bit base integer instruction set)
- **Pipeline Structure:** Complete 5-stage pipeline simulation: `IF` (Fetch) -> `ID` (Decode) -> `EX` (Execute) -> `MEM` (Memory) -> `WB` (Write Back).
- **Advanced Hazard Handling:**
  - **Data Hazards:** Resolved using a dedicated **Forwarding Unit** (bypassing logic) from `MEM` and `WB` stages directly to the ALU inputs.
  - **Strict MUX Selection Logic:** Implemented precise operand evaluation to prevent incorrect forwarding during instructions that utilize immediates (`IMM`) or the zero register (`x0`).
- **Status:** Under active development. Core execution, pipeline logic, and data hazard controls are fully functional.

---

## 🛠️ Supported Instructions

- [x] **R-type:** `add`, `sub`, `sll`, `slt`, `sltu`, `xor`, `srl`, `sra`, `or`, `and`
- [x] **I-type:** `addi`, `slti`, `sltiu`, `xori`, `ori`, `andi`, `slli`, `srli`, `srai`
- [x] **Load/Store:** `lw`, `sw`, etc. (Memory access instructions)
- [x] **Branch/Jump:** `beq`, `bne`, `jal`, `jalr`, etc. (Control flow instructions)

---

## 🗺️ Architecture Overview & Hazard Control

The simulator maintains microarchitectural states using pipeline registers between each stage (`if_id`, `id_ex`, `ex_mem`, `mem_wb`).

```

[IF Stage] -> (if_id) -> [ID Stage] -> (id_ex) -> [EX Stage] -> (ex_mem) -> [MEM Stage] -> (mem_wb) -> [WB Stage]
                                                ^                                                |
                                                |================== Forwarding Path =============|

```

### Technical Highlight: Forwarding Logic for Immediates
In textbook pipeline concepts, forwarding conditions often assume register-to-register operations. In this implementation, to prevent control signals from mistakenly overwriting immediate operands (`IMM`) or fixed zero sources (`ZERO`) with bypassed register data, the Forwarding Unit strictly verifies the source type (`id_ex.src_type == SRC_REG`) before activating the bypass network. 

---

## 💻 How to Build & Run

### Prerequisites
- C++17 compatible compiler (GCC or Clang)
- CMake (3.15+) or Make

### Build
```bash
make

```

### Run

```bash
./rv32i-emu [Path to RISC-V Binary/Hex file]

```

---

## 🗺️ Roadmap & TODO

* [ ] **Control Hazards:** Implement pipeline flushing and basic branch prediction.
* [ ] **CSR (Control and Status Registers):** Enhance system register implementation and basic exception handling architecture.
* [ ] **Verification:** Create a comprehensive test suite and integrate with official `riscv-tests`.
* [ ] **Extensions:** Add support for RV32M (Multiplication and Division extension) in the future.

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome!
Feel free to check the Issues page if you want to contribute or give feedback.

---

## 👤 Author

* **Yoshida Seiji (吉田誠司)**
* Vocational school student at Nagoya Kogakuin College (名古屋工学院専門学校)
* Focus: Low-level software/hardware development, Computer Architecture, Network Infrastructure.

