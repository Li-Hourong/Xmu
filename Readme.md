# RISC-V Virtual CPU

本项目用于使用 C 语言实现一个 RISC-V 虚拟 CPU。当前执行模型为 `Fetch -> Execute`：`Fetch` 从内存读取 32 位指令并推进 `pc`，`Execute` 完成解码、执行、访存和写回。

## RISC-V 资料

官方资料：

- RISC-V International 规范页：<https://riscv.org/specifications/>
- RISC-V ISA Manual：<https://github.com/riscv/riscv-isa-manual>
- RISC-V opcode 数据库：<https://github.com/riscv/riscv-opcodes>

实现普通整数指令时，优先参考 **Unprivileged ISA**。异常、中断、CSR、特权级、页表等内容属于 **Privileged ISA**，可以等基础指令跑通后再实现。

建议第一版先实现 **RV32I**，也就是 32 位整数基础指令集。跑通后再考虑加入 `M` 扩展，也就是乘除法指令，形成 `RV32IM`。

## Decode 速查

RISC-V 基础指令长度为 32 bit。Decode 时最常用的字段如下：

```c
uint32_t opcode = inst & 0x7f;         // [6:0]
uint32_t rd     = (inst >> 7)  & 0x1f; // [11:7]
uint32_t funct3 = (inst >> 12) & 0x07; // [14:12]
uint32_t rs1    = (inst >> 15) & 0x1f; // [19:15]
uint32_t rs2    = (inst >> 20) & 0x1f; // [24:20]
uint32_t funct7 = (inst >> 25) & 0x7f; // [31:25]
```

寄存器编号范围是 `x0` 到 `x31`。其中 `x0` 永远为 0，写入 `x0` 的结果必须被丢弃。

## RV32I 指令格式

### R-Type

用于寄存器-寄存器运算，例如 `add`、`sub`、`and`。

```text
31      25 24   20 19   15 14    12 11    7 6      0
funct7     rs2     rs1     funct3    rd     opcode
```

### I-Type

用于立即数运算、load、`jalr`、`ecall`、`ebreak` 等。

```text
31          20 19   15 14    12 11    7 6      0
imm[11:0]      rs1     funct3    rd     opcode
```

### S-Type

用于 store 指令，例如 `sb`、`sh`、`sw`。

```text
31      25 24   20 19   15 14    12 11       7 6      0
imm[11:5]  rs2     rs1     funct3    imm[4:0]  opcode
```

### B-Type

用于条件分支，例如 `beq`、`bne`、`blt`。

```text
31       30    25 24   20 19   15 14    12 11       8 7        6      0
imm[12]  imm[10:5] rs2    rs1    funct3   imm[4:1]  imm[11]  opcode
```

### U-Type

用于 `lui`、`auipc`。

```text
31          12 11    7 6      0
imm[31:12]     rd     opcode
```

### J-Type

用于 `jal`。

```text
31       30      21 20       19      12 11    7 6      0
imm[20]  imm[10:1] imm[11]  imm[19:12] rd     opcode
```

## 立即数解码

RISC-V 立即数通常需要符号扩展。可以先准备一个通用的符号扩展函数：

```c
static int32_t sign_extend(uint32_t value, int bits) {
    uint32_t sign_bit = 1u << (bits - 1);
    return (int32_t)((value ^ sign_bit) - sign_bit);
}
```

各格式立即数拼接方式：

```c
int32_t imm_i = sign_extend(inst >> 20, 12);

int32_t imm_s = sign_extend(((inst >> 25) << 5) |
                            ((inst >> 7) & 0x1f), 12);

int32_t imm_b = sign_extend(((inst >> 31) << 12) |
                            (((inst >> 7) & 0x01) << 11) |
                            (((inst >> 25) & 0x3f) << 5) |
                            (((inst >> 8) & 0x0f) << 1), 13);

int32_t imm_u = (int32_t)(inst & 0xfffff000u);

int32_t imm_j = sign_extend(((inst >> 31) << 20) |
                            (((inst >> 12) & 0xff) << 12) |
                            (((inst >> 20) & 0x01) << 11) |
                            (((inst >> 21) & 0x3ff) << 1), 21);
```

注意：

- B-Type 和 J-Type 的立即数最低位恒为 0，因为跳转目标至少 2 字节对齐。
- `imm_u` 不需要右移后再左移，直接保留高 20 位即可。
- 本项目的 `Fetch` 会先执行 `pc += 4`，所以 `Execute` 中分支和跳转目标应使用 `current_pc = cpu->pc - 4`。

## RV32I Opcode 表

```text
opcode   hex   指令组       常见指令
0110111  0x37  LUI          lui
0010111  0x17  AUIPC        auipc
1101111  0x6f  JAL          jal
1100111  0x67  JALR         jalr
1100011  0x63  BRANCH       beq, bne, blt, bge, bltu, bgeu
0000011  0x03  LOAD         lb, lh, lw, lbu, lhu
0100011  0x23  STORE        sb, sh, sw
0010011  0x13  OP-IMM       addi, slti, sltiu, xori, ori, andi, slli, srli, srai
0110011  0x33  OP           add, sub, sll, slt, sltu, xor, srl, sra, or, and
0001111  0x0f  MISC-MEM     fence
1110011  0x73  SYSTEM       ecall, ebreak
```

## RV32I 指令细分

### OP-IMM: `opcode = 0x13`

```text
funct3  指令
000     addi
010     slti
011     sltiu
100     xori
110     ori
111     andi
001     slli
101     srli / srai, 由 funct7 区分
```

`slli`、`srli`、`srai` 使用的位移量是 `rs2` 位置上的低 5 位，也就是 `shamt = (inst >> 20) & 0x1f`。

```text
funct3 = 101, funct7 = 0000000 -> srli
funct3 = 101, funct7 = 0100000 -> srai
```

### OP: `opcode = 0x33`

```text
funct3  funct7    指令
000     0000000   add
000     0100000   sub
001     0000000   sll
010     0000000   slt
011     0000000   sltu
100     0000000   xor
101     0000000   srl
101     0100000   sra
110     0000000   or
111     0000000   and
```

### BRANCH: `opcode = 0x63`

```text
funct3  指令
000     beq
001     bne
100     blt
101     bge
110     bltu
111     bgeu
```

### LOAD: `opcode = 0x03`

```text
funct3  指令  写回结果
000     lb    符号扩展 8 位
001     lh    符号扩展 16 位
010     lw    读取 32 位
100     lbu   零扩展 8 位
101     lhu   零扩展 16 位
```

### STORE: `opcode = 0x23`

```text
funct3  指令
000     sb
001     sh
010     sw
```

### SYSTEM: `opcode = 0x73`

基础实现可以先识别：

```text
inst = 0x00000073 -> ecall
inst = 0x00100073 -> ebreak
```

CSR 指令属于 `Zicsr` 扩展，不属于最小 RV32I 执行核心的必要部分。

## Execute 实现要点

当前项目没有再拆成五级流水阶段，`Execute` 直接完成：

- Decode：拆出 `opcode`、`rd`、`rs1`、`rs2`、`funct3`、`funct7`、立即数。
- Execute：完成 ALU、分支判断和跳转目标计算。
- Memory：load/store 通过 `Memory` 接口访问内存。
- Write back：通过 `write_reg` 写回寄存器，且忽略对 `x0` 的写入。

执行阶段需要特别注意：

- `jal` 写回地址是下一条指令地址，也就是 `cpu->pc`。
- `jalr` 目标地址必须清掉最低位：`(rs1 + imm) & ~1u`。
- `branch`、`jal` 的目标地址基于当前指令地址，不是已经加 4 后的 `pc`。
- 有符号比较使用 `(int32_t)`，无符号比较直接使用 `uint32_t`。

## 伪指令说明

汇编代码中常见的 `li`、`mv`、`nop` 等不是独立机器指令，而是汇编器展开出来的伪指令。例如：

```asm
nop
```

通常会被编码成：

```asm
addi x0, x0, 0
```

虚拟 CPU 只需要解码真实机器指令，不需要直接支持汇编伪指令。
