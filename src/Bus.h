#ifndef BUS_H
#define BUS_H
#include "common.h"
#include "Memory.h"
#include "UART.h"

typedef struct Bus {
    Memory* mem;
    UART* uart;
} Bus;

Bus* Create_Bus(Memory* mem, UART* uart);
void Free_Bus(Bus* bus);
int bus_load8(Bus* bus, uint32_t addr, uint8_t* out);
int bus_load16(Bus* bus, uint32_t addr, uint16_t* out);
int bus_load32(Bus* bus, uint32_t addr, uint32_t* out);
int bus_store8(Bus* bus, uint32_t addr, uint8_t value);
int bus_store16(Bus* bus, uint32_t addr, uint16_t value);
int bus_store32(Bus* bus, uint32_t addr, uint32_t value);

#endif