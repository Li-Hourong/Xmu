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

CSR 指令属于 `Zicsr` 扩展，不属于最小 RV32I 执行核心的必要部分（详见后文「CSR 与特权级」章节）。

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

## CSR 与特权级（Privileged ISA）

基础整数指令属于 Unprivileged ISA。CSR 寄存器、异常/中断、`mret` 等属于 Privileged ISA。本项目当前已实现同步异常的入口（`raise_trap` 设置 `mcause`/`mepc`/`mtval` 并维护 `mstatus`）、6 条 CSR 指令与 `mret`，trap 可进可出；异步中断（`mie`/`mip` 检测）尚未实现。

### CSR 地址编码

CSR 地址是 12 位（`0x000`–`0xFFF`），但**不是从 0 递增的编号，而是带标签的结构化字段**：高位是分类标签，只有低位是组内序号，因此地址看起来零散、不从 0 开始。

| 字段 | 含义 | 取值 |
|---|---|---|
| `[11:10]` | 访问权限 | `11`=只读；`00/01/10`=可读写 |
| `[9:8]` | 最低可访问特权级 | `00`=User, `01`=Supervisor, `10`=Hypervisor, `11`=Machine |
| `[7:4]` | 组别 | 标准寄存器一般为 `0` |
| `[3:0]` | 组内序号 | 真正递增的部分 |

`[11:10]=11` 表示只读，可由地址自证：用户只读影子计数器 `cycle`（`0xC00`，`[11:10]=11`），而机器可写的 `mcycle`（`0xB00`，`[11:10]=10`）。

解码示例：

| CSR | 地址（12 位） | `[11:10]` | `[9:8]` | 解读 |
|---|---|---|---|---|
| `mstatus` | `0x300` = `0011 0000 0000` | `00` RW | `11` M | 标准、机器态、可读写 |
| `mie` | `0x304` | `00` | `11` | 序号 4 |
| `mtvec` | `0x305` | `00` | `11` | 序号 5 |
| `mscratch` | `0x340` | `00` | `11` | 序号 0x40 |
| `mhartid` | `0xF14` = `1111 0001 0100` | `11` 只读 | `11` M | 只读、机器态 |

所有可读写的 M-mode 寄存器集中在 `0x3xx`，因为 `0x3 = 0011 = [11:10][9:8] = 可读写(00) + Machine(11)`。桶内大量留白留给规范中可选/未实现的寄存器（`medeleg`/`mideleg`、PMP、`mcounteren` 等），所以地址不连续。

### CSR 指令（Zicsr 扩展）

6 条指令，`opcode = SYSTEM (0x73)`，用 `funct3` 区分，操作对象是 12 位 CSR 地址 `inst[31:20]`：

| funct3 | 指令 | 操作 |
|---|---|---|
| `001` | `csrrw`  | `t=csr; csr=rs1;        rd=t` |
| `010` | `csrrs`  | `t=csr; if(rs1)  csr |= rs1;  rd=t` |
| `011` | `csrrc`  | `t=csr; if(rs1)  csr &= ~rs1; rd=t` |
| `101` | `csrrwi` | 同 `csrrw`，源操作数为 `zimm`（5 位零扩展） |
| `110` | `csrrsi` | 同 `csrrs`，源操作数为 `zimm` |
| `111` | `csrrci` | 同 `csrrc`，源操作数为 `zimm` |

三条语义要点：

- **零即不写**：`csrrs/c/csi/ci` 当 `rs1`（或 `zimm`）为 0 时**只读不写**。
- **`rd=x0` 即不读**：`csrrw/wi` 当 `rd=x0` 时**不读**（避免只读 CSR 的读副作用）。
- 访问**不存在**的 CSR，或**写只读** CSR（`[11:10]=11`）→ `illegal instruction`。

伪指令：`csrr rd, csr` = `csrrs rd, csr, x0`；`csrw csr, rs` = `csrrw x0, csr, rs`。

### M-mode 关键 CSR 寄存器

trap 状态（本项目在 `CPU` 结构中维护）：

| 地址 | 名 | 用途 | 读写 |
|---|---|---|---|
| `0x300` | `mstatus` | 全局状态，关键是中断位 | RW |
| `0x304` | `mie` | 中断使能：bit3=MSIE、bit7=MTIE、bit11=MEIE | RW |
| `0x305` | `mtvec` | trap 入口基址；低 2 位=模式（`0`=direct，`1`=vectored） | RW |
| `0x340` | `mscratch` | M-mode 专用 scratch | RW |
| `0x341` | `mepc` | trap 返回的 PC | RW |
| `0x342` | `mcause` | trap 原因：bit31=1 中断 / =0 异常；低 31 位=code | RW |
| `0x343` | `mtval` | trap 附加值（非法指令编码 / 出错地址） | RW |
| `0x344` | `mip` | 中断挂起：bit3=MSIP、bit7=MTIP、bit11=MEIP | 部分 RW |

只读常量：

| 地址 | 名 | 返回值 |
|---|---|---|
| `0x301` | `misa` | 支持的扩展（最小实现返回 `0`） |
| `0xf14` | `mhartid` | hart 编号（单核返回 `0`） |

> S-mode / 分页相关（`satp=0x180`、`sstatus`、`scause` 等）属于后续阶段。

### mstatus 关键位

| 位 | 名 | 含义 |
|---|---|---|
| `3` | `MIE` | M 全局中断使能 |
| `7` | `MPIE` | trap 前的 `MIE`（进 trap 时被保存） |
| `12:11` | `MPP` | trap 前的特权级（本项目仅 M-mode，恒 `11`） |

### trap 进出语义

进入 trap（`raise_trap`）：

```text
mepc    <- fault_pc          （异常=出错指令；中断=下一条）
mcause  <- code | （中断? 0x80000000 : 0）
mtval   <- tval
MPIE    <- MIE               （保存）
MIE     <- 0                 （关中断）
MPP     <- 11
pc      <- mtvec
```

退出 trap（`mret`，编码 `0x30200073`）：

```text
pc      <- mepc
MIE     <- MPIE              （恢复）
MPIE    <- 1
```

### mie / mip 中断位与中断 cause

| bit | `mie`（使能） | `mip`（挂起） | 中断类型 | cause（中断） |
|---|---|---|---|---|
| `3` | MSIE | MSIP | M 软件中断 | `3` |
| `7` | MTIE | MTIP | M 定时器中断 | `7` |
| `11` | MEIE | MEIP | M 外部中断 | `11` |

异步中断的检测点在 `Run_CPU` 循环中 `fetch` 之前：若 `(mip & mie)` 非空且 `mstatus.MIE==1`，按优先级（MEI > MSI > MTI）选一个中断，`mcause = 0x80000000 | code` 后走 trap 流程（`mepc` 存"下一条"未执行指令）。

异常 cause code 见 `common.h` 的 `TrapCause` 枚举。

## 程序停止约定

当前虚拟 CPU 使用 RISC-V 标准 `EBREAK` 指令作为停机指令：

```text
assembly: ebreak
machine:  0x00100073
bytes:    73 00 10 00
```

外部程序文件被加载到内存后，`Run_CPU` 会循环执行 `fetch` 和 `execute`，直到 `Execute` 遇到 `0x00100073` 并设置 `cpu->halted = 1`。因此每个测试程序都应该在最后放一条 `ebreak`，否则 CPU 会继续从后续内存取指。

默认运行方式：

```sh
./build/xmu program.bin
```

如果没有传入文件名，程序会尝试读取当前目录下的 `program.bin`。
