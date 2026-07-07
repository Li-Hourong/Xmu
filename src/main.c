#include <stdio.h>
#include "common.h"
#include "CPU.h"
#include "Memory.h"
#include "UART.h"
#include "Bus.h"

int main(int argc, char *argv[]) {
    CPU* cpu = Create_CPU();
    Memory* mem = Create_Memory(1024); // Create 1KB of memory
    UART* uart = Create_UART();
    Bus* bus = Create_Bus(mem, uart);

    const char *program = argc > 1 ? argv[1] : "program.bin";
    if (load_memory(program, mem) != 0) {
        Free_Bus(bus);
        Free_UART(uart);
        Free_Memory(mem);
        Free_CPU(cpu);
        return 1;
    }
    Run_CPU(cpu, bus); // Run until EBREAK
    dump_memory(mem, 0, 256); // Dump the first 256 bytes of memory for debugging
    Free_Bus(bus);
    Free_UART(uart);
    Free_Memory(mem);
    Free_CPU(cpu);
    return 0;
}
