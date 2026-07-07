#ifndef CPU_H
#define CPU_H

#include "common.h"
#include "Bus.h"


struct CPU {
    uint32_t pc;
    uint32_t reg[32];
    uint32_t inst;
    uint32_t addr;

    uint32_t mcause;
    uint32_t mepc;
    uint32_t mtvec;
    uint32_t mtval;
    uint32_t mstatus;
    uint32_t mie;
    uint32_t mip;
    uint32_t mscratch;
    uint8_t trap_pending;

    uint8_t halted;
    uint8_t halt_on_ebreak;

    void (*fetch)(struct CPU *cpu, Bus *bus);
    void (*execute)(struct CPU *cpu, Bus *bus);
};

typedef struct CPU CPU;

struct CPU* Create_CPU();
void Free_CPU(CPU* cpu);
void Run_CPU(CPU* cpu, Bus* bus);
void Fetch(CPU *cpu, Bus *bus);
void Execute(CPU *cpu, Bus *bus);

void raise_trap(CPU *cpu, uint32_t cause, uint32_t tval);


#endif
