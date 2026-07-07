#include "CPU.h"

// M-mode CSR 地址（地址编码规则见 Readme「CSR 与特权级」）
#define CSR_MSTATUS   0x300
#define CSR_MISA      0x301
#define CSR_MIE       0x304
#define CSR_MTVEC     0x305
#define CSR_MSCRATCH  0x340
#define CSR_MEPC      0x341
#define CSR_MCAUSE    0x342
#define CSR_MTVAL     0x343
#define CSR_MIP       0x344
#define CSR_MHARTID   0xF14

// mstatus 关键位（详见 Readme「mstatus 关键位」）
#define MSTATUS_MIE   (1u << 3)    // M 全局中断使能
#define MSTATUS_MPIE  (1u << 7)    // trap 前的 MIE（进 trap 时保存）
#define MSTATUS_MPP_M (3u << 11)   // MPP = Machine（单特权级恒 11）

CPU* Create_CPU(){
    struct CPU* cpu = (struct CPU*)malloc(sizeof(struct CPU));
    cpu->pc = 0;
    cpu->inst = 0;
    cpu->addr = 0;
    cpu->halted = 0;
    cpu->mcause = 0;
    cpu->mepc = 0;
    cpu->mtvec = 0;
    cpu->mtval = 0;
    cpu->mstatus = 0;
    cpu->mie = 0;
    cpu->mip = 0;
    cpu->mscratch = 0;
    cpu->halt_on_ebreak = 1;
    
    cpu->trap_pending = 0;
    memset(cpu->reg, 0, sizeof(cpu->reg));
    cpu->fetch = Fetch;
    cpu->execute = Execute;
    return cpu;
}

void Free_CPU(CPU* cpu){
    free(cpu);
}

void Run_CPU(CPU* cpu, Bus* bus) {
    while (!cpu->halted) {
        cpu->trap_pending = 0;
        cpu->fetch(cpu, bus);
        if (cpu->trap_pending) {
            continue;
        }
        cpu->execute(cpu, bus);
    }
}

//---------- utils ----------

static void raise_trap_at(CPU *cpu, uint32_t cause, uint32_t tval, uint32_t fault_pc) {
    cpu->mcause = cause;
    cpu->mtval = tval;
    cpu->mepc = fault_pc;
    // 进入 trap：MPIE ← MIE（保存），MIE ← 0（关中断），MPP ← Machine
    if (cpu->mstatus & MSTATUS_MIE) cpu->mstatus |= MSTATUS_MPIE;
    else                            cpu->mstatus &= ~MSTATUS_MPIE;
    cpu->mstatus &= ~MSTATUS_MIE;
    cpu->mstatus |= MSTATUS_MPP_M;
    cpu->pc = cpu->mtvec;
    cpu->trap_pending = 1;
}

void raise_trap(CPU *cpu, uint32_t cause, uint32_t tval) {
    raise_trap_at(cpu, cause, tval, cpu->pc - 4);
}

//------ CSR 访问 ------
// 返回值统一约定：0 = 成功；-1 = 失败（CSR 不存在或只读）。
// 读成功时把值写到 *out；写失败时调用方按 illegal instruction 处理。
// 由 SYSTEM opcode 的 CSR 指令分支调用。

static int csr_read(CPU *cpu, uint32_t csr, uint32_t *out) {
    switch (csr) {
        case CSR_MSTATUS:  *out = cpu->mstatus;  return 0;
        case CSR_MISA:     *out = 0;             return 0;  // 最小实现不报告扩展
        case CSR_MIE:      *out = cpu->mie;      return 0;
        case CSR_MTVEC:    *out = cpu->mtvec;    return 0;
        case CSR_MSCRATCH: *out = cpu->mscratch; return 0;
        case CSR_MEPC:     *out = cpu->mepc;     return 0;
        case CSR_MCAUSE:   *out = cpu->mcause;   return 0;
        case CSR_MTVAL:    *out = cpu->mtval;    return 0;
        case CSR_MIP:      *out = cpu->mip;      return 0;
        case CSR_MHARTID:  *out = 0;             return 0;  // 单核 hart id = 0
        default:           return -1;  // 不存在的 CSR
    }
}

static int csr_write(CPU *cpu, uint32_t csr, uint32_t val) {
    switch (csr) {
        case CSR_MSTATUS:  cpu->mstatus  = val; return 0;
        case CSR_MIE:      cpu->mie      = val; return 0;
        case CSR_MTVEC:    cpu->mtvec    = val; return 0;
        case CSR_MSCRATCH: cpu->mscratch = val; return 0;
        case CSR_MEPC:     cpu->mepc     = val; return 0;
        case CSR_MCAUSE:   cpu->mcause   = val; return 0;
        case CSR_MTVAL:    cpu->mtval    = val; return 0;
        case CSR_MIP:      cpu->mip      = val; return 0;  // 简化：不区分可写/只读位
        case CSR_MISA:     return -1;  // 只读
        case CSR_MHARTID:  return -1;  // 只读
        default:           return -1;  // 不存在
    }
}

static int is_aligned(uint32_t addr, uint32_t alignment) {
    return (addr & (alignment - 1)) == 0;
}

static int32_t sign_extend(uint32_t value, int bits) {
    uint32_t sign_bit = 1u << (bits - 1);
    return (int32_t)((value ^ sign_bit) - sign_bit);
}

static int32_t imm_i(uint32_t inst) {
    return sign_extend(inst >> 20, 12);
}

static int32_t imm_s(uint32_t inst) {
    return sign_extend(((inst >> 25) << 5) |
                       ((inst >> 7) & 0x1f), 12);
}

static int32_t imm_b(uint32_t inst) {
    return sign_extend(((inst >> 31) << 12) |
                       (((inst >> 7) & 0x01) << 11) |
                       (((inst >> 25) & 0x3f) << 5) |
                       (((inst >> 8) & 0x0f) << 1), 13);
}

static int32_t imm_u(uint32_t inst) {
    return (int32_t)(inst & 0xfffff000u);
}

static int32_t imm_j(uint32_t inst) {
    return sign_extend(((inst >> 31) << 20) |
                       (((inst >> 12) & 0xff) << 12) |
                       (((inst >> 20) & 0x01) << 11) |
                       (((inst >> 21) & 0x3ff) << 1), 21);
}


void Fetch(CPU *cpu, Bus *bus) {
    uint32_t inst;
    uint32_t fault_pc = cpu->pc;

    if (!is_aligned(cpu->pc, 4)) {
        raise_trap_at(cpu, TRAP_INST_MISALIGNED, cpu->pc, fault_pc);
        return;
    }

    if (bus_load32(bus, cpu->pc, &inst) != 0) {
        raise_trap_at(cpu, TRAP_INST_FAULT, cpu->pc, fault_pc);
        return;
    }

    cpu->inst = inst;
    cpu->pc += 4;
}

void Execute(CPU *cpu, Bus *bus) {
    if (cpu->halted) {
        return;
    }
    uint32_t opcode = cpu->inst & 0x7f;        // [6:0]
    uint32_t rd     = (cpu->inst >> 7)  & 0x1f; // [11:7]
    uint32_t funct3 = (cpu->inst >> 12) & 0x07; // [14:12]
    uint32_t rs1    = (cpu->inst >> 15) & 0x1f; // [19:15]
    uint32_t rs2    = (cpu->inst >> 20) & 0x1f; // [24:20]
    uint32_t funct7 = (cpu->inst >> 25) & 0x7f; // [31:25]

    switch (opcode) {
        case 0b0110011: // R-type
            if (funct3 == 0x0 && funct7 == 0x00) {
                cpu->reg[rd] = cpu->reg[rs1] + cpu->reg[rs2]; // ADD
            } else if (funct3 == 0x0 && funct7 == 0x20) {
                cpu->reg[rd] = cpu->reg[rs1] - cpu->reg[rs2]; // SUB
            } else if (funct3 == 0x7 && funct7 == 0x00) {
                cpu->reg[rd] = cpu->reg[rs1] & cpu->reg[rs2]; // AND
            } else if (funct3 == 0x6 && funct7 == 0x00) {
                cpu->reg[rd] = cpu->reg[rs1] | cpu->reg[rs2]; // OR
            } else if (funct3 == 0x4 && funct7 == 0x00) {
                cpu->reg[rd] = cpu->reg[rs1] ^ cpu->reg[rs2]; // XOR
            }
        break;
        case 0b0010011: // I-type ALU
            switch (funct3) {
                case 0b000: // ADDI
                    cpu->reg[rd] = cpu->reg[rs1] + (uint32_t)imm_i(cpu->inst);
                break;
                case 0b010: // SLTI
                    cpu->reg[rd] = (int32_t)cpu->reg[rs1] < imm_i(cpu->inst);
                break;
                case 0b011: // SLTIU
                    cpu->reg[rd] = cpu->reg[rs1] < (uint32_t)imm_i(cpu->inst);
                break;
                case 0b100: // XORI
                    cpu->reg[rd] = cpu->reg[rs1] ^ (uint32_t)imm_i(cpu->inst);
                break;
                case 0b110: // ORI
                    cpu->reg[rd] = cpu->reg[rs1] | (uint32_t)imm_i(cpu->inst);
                break;
                case 0b111: // ANDI
                    cpu->reg[rd] = cpu->reg[rs1] & (uint32_t)imm_i(cpu->inst);
                break;
                case 0b001: // SLLI
                    if (funct7 == 0x00) {
                        uint32_t shamt = (cpu->inst >> 20) & 0x1f;
                        cpu->reg[rd] = cpu->reg[rs1] << shamt;
                    }
                break;
                case 0b101: {
                    uint32_t shamt = (cpu->inst >> 20) & 0x1f;
                    if (funct7 == 0x00) { // SRLI
                        cpu->reg[rd] = cpu->reg[rs1] >> shamt;
                    } else if (funct7 == 0x20) { // SRAI
                        cpu->reg[rd] = (uint32_t)((int32_t)cpu->reg[rs1] >> shamt);
                    }
                    break;
                }
                default:
                    raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                break;
            }
        break;
        case 0b0000011: // I-type load
            cpu->addr = cpu->reg[rs1] + (uint32_t)imm_i(cpu->inst);
            switch (funct3) {
                case 0b000: { // LB
                    uint8_t value;
                    if (bus_load8(bus, cpu->addr, &value) != 0) {
                        raise_trap(cpu, TRAP_LOAD_FAULT, cpu->addr);
                        return;
                    }
                    cpu->reg[rd] = (uint32_t)sign_extend(value, 8);
                break;
                }
                case 0b001: { // LH
                    uint16_t value;
                    if (!is_aligned(cpu->addr, 2)) {
                        raise_trap(cpu, TRAP_LOAD_MISALIGNED, cpu->addr);
                        return;
                    }
                    if (bus_load16(bus, cpu->addr, &value) != 0) {
                        raise_trap(cpu, TRAP_LOAD_FAULT, cpu->addr);
                        return;
                    }
                    cpu->reg[rd] = (uint32_t)sign_extend(value, 16);
                break;
                }
                case 0b010: { // LW
                    uint32_t value;
                    if (!is_aligned(cpu->addr, 4)) {
                        raise_trap(cpu, TRAP_LOAD_MISALIGNED, cpu->addr);
                        return;
                    }
                    if (bus_load32(bus, cpu->addr, &value) != 0) {
                        raise_trap(cpu, TRAP_LOAD_FAULT, cpu->addr);
                        return;
                    }
                    cpu->reg[rd] = value;
                break;
                }
                case 0b100: { // LBU
                    uint8_t value;
                    if (bus_load8(bus, cpu->addr, &value) != 0) {
                        raise_trap(cpu, TRAP_LOAD_FAULT, cpu->addr);
                        return;
                    }
                    cpu->reg[rd] = value;
                break;
                }
                case 0b101: { // LHU
                    uint16_t value;
                    if (!is_aligned(cpu->addr, 2)) {
                        raise_trap(cpu, TRAP_LOAD_MISALIGNED, cpu->addr);
                        return;
                    }
                    if (bus_load16(bus, cpu->addr, &value) != 0) {
                        raise_trap(cpu, TRAP_LOAD_FAULT, cpu->addr);
                        return;
                    }
                    cpu->reg[rd] = value;
                break;
                }
                default:
                    raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                break;
            }
        break;

        case 0b0100011: // S-type
            cpu->addr = cpu->reg[rs1] + (uint32_t)imm_s(cpu->inst);
            switch (funct3) {
                case 0b000: // SB
                    if (bus_store8(bus, cpu->addr, (uint8_t)(cpu->reg[rs2] & 0xFF)) != 0) {
                        raise_trap(cpu, TRAP_STORE_FAULT, cpu->addr);
                        return;
                    }
                break;
                case 0b001: // SH
                    if (!is_aligned(cpu->addr, 2)) {
                        raise_trap(cpu, TRAP_STORE_MISALIGNED, cpu->addr);
                        return;
                    }
                    if (bus_store16(bus, cpu->addr, (uint16_t)(cpu->reg[rs2] & 0xFFFF)) != 0) {
                        raise_trap(cpu, TRAP_STORE_FAULT, cpu->addr);
                        return;
                    }
                break;
                case 0b010: // SW
                    if (!is_aligned(cpu->addr, 4)) {
                        raise_trap(cpu, TRAP_STORE_MISALIGNED, cpu->addr);
                        return;
                    }
                    if (bus_store32(bus, cpu->addr, cpu->reg[rs2]) != 0) {
                        raise_trap(cpu, TRAP_STORE_FAULT, cpu->addr);
                        return;
                    }
                break;
                default:
                    raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                break;
            }
        break;

        case 0b1100011: // B-type
            switch (funct3) {
                case 0b000: // BEQ
                    if (cpu->reg[rs1] == cpu->reg[rs2]) {
                        cpu->pc += imm_b(cpu->inst) - 4;
                    }
                break;
                case 0b001: // BNE
                    if (cpu->reg[rs1] != cpu->reg[rs2]) {
                        cpu->pc += imm_b(cpu->inst) - 4;
                    }
                break;
                case 0b100: // BLT
                    if ((int32_t)cpu->reg[rs1] < (int32_t)cpu->reg[rs2]) {
                        cpu->pc += (uint32_t)imm_b(cpu->inst) - 4;
                    }
                break;
                case 0b101: // BGE
                    if ((int32_t)cpu->reg[rs1] >= (int32_t)cpu->reg[rs2]) {
                        cpu->pc += (uint32_t)imm_b(cpu->inst) - 4;
                    }
                break;
                case 0b110: // BLTU
                    if (cpu->reg[rs1] < cpu->reg[rs2]) {
                        cpu->pc += (uint32_t)imm_b(cpu->inst) - 4;
                    }
                break;
                case 0b111: // BGEU
                    if (cpu->reg[rs1] >= cpu->reg[rs2]) {
                        cpu->pc += (uint32_t)imm_b(cpu->inst) - 4;
                    }
                break;
                default:
                    raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                break;
            }
        break;
        case 0b0110111: // U-type LUI
            cpu->reg[rd] = imm_u(cpu->inst);
        break;
        case 0b0010111: // U-type AUIPC
            cpu->reg[rd] = cpu->pc - 4 + imm_u(cpu->inst);
        break;
        case 0b1100111: // I-type JALR
            cpu->reg[rd] = cpu->pc; // JALR
            cpu->pc = (cpu->reg[rs1] + (uint32_t)imm_i(cpu->inst)) & ~1u;
        break;
        case 0b1101111: // J-type
            cpu->reg[rd] = cpu->pc; // JAL
            cpu->pc += imm_j(cpu->inst) - 4;
        break;
        case 0b1110011: // SYSTEM
            if (funct3 == 0) {
                // funct3==0：ecall / ebreak / mret（wfi 等后续补）
                if (cpu->inst == 0x00100073u) { // EBREAK
                    if (cpu->halt_on_ebreak) {
                        cpu->halted = 1;
                    } else {
                        raise_trap(cpu, TRAP_BREAKPOINT, 0);
                    }
                    return;
                }
                if (cpu->inst == 0x00000073u) { // ECALL
                    raise_trap(cpu, TRAP_ECALL_M, 0);
                    return;
                }
                if (cpu->inst == 0x30200073u) { // MRET
                    cpu->pc = cpu->mepc;                 // 返回到 trap 前的位置
                    // MIE ← MPIE（恢复中断使能），MPIE ← 1
                    if (cpu->mstatus & MSTATUS_MPIE) cpu->mstatus |= MSTATUS_MIE;
                    else                              cpu->mstatus &= ~MSTATUS_MIE;
                    cpu->mstatus |= MSTATUS_MPIE;
                    return;
                }
                raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst); // wfi 等暂未实现
                return;
            }

            // funct3 != 0：6 条 CSR 指令（CSRRW/CSRRS/CSRRC 及其立即数变体）
            {
                uint32_t csr = (cpu->inst >> 20) & 0xfff;          // [31:20] CSR 地址
                // bit2=1 立即数版：源操作数取 rs1 字段低 5 位（zimm，零扩展）；
                // bit2=0 寄存器版：源操作数取 reg[rs1]。
                uint32_t src = (funct3 & 0b100) ? rs1 : cpu->reg[rs1];
                uint32_t old;
                if (csr_read(cpu, csr, &old) != 0) {                   // 访问不存在的 CSR
                    raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                    return;
                }
                switch (funct3 & 0b011) {
                    case 0b01: // CSRRW / CSRRWI —— 总是写
                        if (csr_write(cpu, csr, src) != 0) {
                            raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                            return;
                        }
                        break;
                    case 0b10: // CSRRS / CSRRSI —— rs1==0 不写
                        if (rs1 != 0 && csr_write(cpu, csr, old | src) != 0) {
                            raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                            return;
                        }
                        break;
                    case 0b11: // CSRRC / CSRRCI —— rs1==0 不写
                        if (rs1 != 0 && csr_write(cpu, csr, old & ~src) != 0) {
                            raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                            return;
                        }
                        break;
                    default:   // funct3 == 4 等保留编码
                        raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
                        return;
                }
                cpu->reg[rd] = old;   // 写回旧值（rd==x0 由末尾 reg[0]=0 兜底）
                break;
            }
        default:
            raise_trap(cpu, TRAP_ILLEGAL_INST, cpu->inst);
        break;
    }

    cpu->reg[0] = 0;
}
