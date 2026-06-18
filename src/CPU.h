#ifndef CPU_H
#define CPU_H

#include "common.h"
#include "Memory.h"

struct CPU {
    uint32_t pc;
    uint32_t reg[32];
    uint32_t inst;
    uint32_t addr;
    void (*fetch)(struct CPU *cpu, Memory *mem);
    void (*execute)(struct CPU *cpu, Memory *mem);
};

typedef struct CPU CPU;

struct CPU* Create_CPU();
void Shutdown_CPU(CPU* cpu);
void Fetch(CPU *cpu, Memory *mem);
void Execute(CPU *cpu, Memory *mem);




#endif
