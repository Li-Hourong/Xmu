#include <stdio.h>
#include "common.h"
#include "CPU.h"
#include "Memory.h"

int main() {
    CPU* cpu = Create_CPU();
    Memory* mem = Create_Memory(1024); // Create 1KB of memory

    Free_Memory(mem);
    Shutdown_CPU(cpu);
    return 0;
}