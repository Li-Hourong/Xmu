#include "CPU.h"

CPU* Create_CPU(){
    struct CPU* cpu = (struct CPU*)malloc(sizeof(struct CPU));
    cpu->pc = 0;
    cpu->inst = 0;
    cpu->addr = 0;
    cpu->halted = 0;
    memset(cpu->reg, 0, sizeof(cpu->reg));
    cpu->fetch = Fetch;
    cpu->execute = Execute;
    return cpu;
}

void Free_CPU(CPU* cpu){
    free(cpu);
}

void Run_CPU(CPU* cpu, Memory* mem) {
    while (!cpu->halted) {
        cpu->fetch(cpu, mem);
        cpu->execute(cpu, mem);
    }
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


void Fetch(CPU *cpu, Memory *mem) {
    cpu->inst = memory_load32(mem, cpu->pc);
    cpu->pc += 4;
}

void Execute(CPU *cpu, Memory *mem) {
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
                case 0b101:
                    uint32_t shamt = (cpu->inst >> 20) & 0x1f;
                    if (funct7 == 0x00) { // SRLI
                        cpu->reg[rd] = cpu->reg[rs1] >> shamt;
                    } else if (funct7 == 0x20) { // SRAI
                        cpu->reg[rd] = (uint32_t)((int32_t)cpu->reg[rs1] >> shamt);
                    }
                break;
                default:
                break;
            }
        break;
        case 0b0000011: // I-type load
            cpu->addr = cpu->reg[rs1] + (uint32_t)imm_i(cpu->inst);
            switch (funct3) {
                case 0b000: // LB
                    cpu->reg[rd] = (uint32_t)sign_extend(memory_load8(mem, cpu->addr), 8);
                break;
                case 0b001: // LH
                    cpu->reg[rd] = (uint32_t)sign_extend(memory_load16(mem, cpu->addr), 16);
                break;
                case 0b010: // LW
                    cpu->reg[rd] = memory_load32(mem, cpu->addr);
                break;
                case 0b100: // LBU
                    cpu->reg[rd] = memory_load8(mem, cpu->addr);
                break;
                case 0b101: // LHU
                    cpu->reg[rd] = memory_load16(mem, cpu->addr);
                break;
                default:
                break;
            }
        break;

        case 0b0100011: // S-type
            cpu->addr = cpu->reg[rs1] + (uint32_t)imm_s(cpu->inst);
            switch (funct3) {
                case 0b000: // SB
                    memory_store8(mem, cpu->addr, (uint8_t)(cpu->reg[rs2] & 0xFF));
                break;
                case 0b001: // SH
                    memory_store16(mem, cpu->addr, (uint16_t)(cpu->reg[rs2] & 0xFFFF));
                break;
                case 0b010: // SW
                    memory_store32(mem, cpu->addr, cpu->reg[rs2]);
                break;
                default:
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
                break;
            }
        break;
        case 0b0110111: // U-type LUI
            cpu->reg[rd] = imm_u(cpu->inst);
        break;
        case 0b0010111: // U-type AUIPC
            cpu->reg[rd] = cpu->pc - 4 + imm_u(cpu->inst);
        break;
        case 0b1101111: // J-type
            cpu->reg[rd] = cpu->pc; // JAL
            cpu->pc += imm_j(cpu->inst) - 4;
        break;
        case 0b1110011: // SYSTEM
            if (cpu->inst == 0x00100073u) { // EBREAK
                cpu->halted = 1;
            }
        break;
        default:
        break;
    }

    cpu->reg[0] = 0;
}
