#ifndef CPU_H
#define CPU_H
#include "common.h"

struct CPU {
    uint32_t pc;
    uint32_t regs[32];
    void (*fetch)(struct CPU *cpu);
    void (*decode)(struct CPU *cpu);
    void (*execute)(struct CPU *cpu);
    void (*memory_access)(struct CPU *cpu);
    void (*write_back)(struct CPU *cpu);
};

typedef struct CPU CPU;

struct CPU* Create_CPU();
void Shutdown_CPU(CPU* cpu);
void Fetch(CPU *cpu);
void Decode(CPU *cpu);
void Execute(CPU *cpu);
void Memory_Access(CPU *cpu);
void Write_Back(CPU *cpu);



#endif