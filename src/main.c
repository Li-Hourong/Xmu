#include <stdio.h>
#include "common.h"
#include "CPU.h"
#include "Memory.h"

int main() {
    CPU* cpu = Create_CPU();
    
    Shutdown_CPU(cpu);
    return 0;
}