#ifndef MEMORY_H
#define MEMORY_H
#include "common.h"

struct Memory {
    uint8_t* data;
    uint32_t size;
};

typedef struct Memory Memory;

Memory* Create_Memory(uint32_t size);
void Free_Memory(Memory* mem);

// read
uint8_t  memory_load8(Memory *mem, uint32_t addr);
uint16_t memory_load16(Memory *mem, uint32_t addr);
uint32_t memory_load32(Memory *mem, uint32_t addr);

// write
void memory_store8(Memory *mem, uint32_t addr, uint8_t value);
void memory_store16(Memory *mem, uint32_t addr, uint16_t value);
void memory_store32(Memory *mem, uint32_t addr, uint32_t value);

//debug
void memory_dump(Memory *mem, uint32_t start, uint32_t end);


#endif