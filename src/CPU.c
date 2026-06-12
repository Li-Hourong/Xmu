#include "CPU.h"
CPU* Create_CPU(){
    struct CPU* cpu = (struct CPU*)malloc(sizeof(struct CPU));
    cpu->pc = 0;
    memset(cpu->regs, 0, sizeof(cpu->regs));
    cpu->fetch = Fetch;
    cpu->decode = Decode;
    cpu->execute = Execute;
    cpu->memory_access = Memory_Access;
    cpu->write_back = Write_Back;
    return cpu;
}

void Shutdown_CPU(CPU* cpu){
    free(cpu);
}

// TODO: implement each pipeline stage
void Fetch(CPU *cpu) {
    
}

void Decode(CPU *cpu) {
    (void)cpu;
}

void Execute(CPU *cpu) {
    (void)cpu;
}

void Memory_Access(CPU *cpu) {
    (void)cpu;
}

void Write_Back(CPU *cpu) {
    (void)cpu;
}
