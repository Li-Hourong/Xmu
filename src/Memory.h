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
int load_memory(const char* filename, Memory* mem);

// read
uint8_t  memory_load8(Memory *mem, uint32_t addr);
uint16_t memory_load16(Memory *mem, uint32_t addr);
uint32_t memory_load32(Memory *mem, uint32_t addr);

int memory_load8_checked(Memory *mem, uint32_t addr, uint8_t *out);
int memory_load16_checked(Memory *mem, uint32_t addr, uint16_t *out);
int memory_load32_checked(Memory *mem, uint32_t addr, uint32_t *out);

// write
void memory_store8(Memory *mem, uint32_t addr, uint8_t value);
void memory_store16(Memory *mem, uint32_t addr, uint16_t value);
void memory_store32(Memory *mem, uint32_t addr, uint32_t value);

int memory_store8_checked(Memory *mem, uint32_t addr, uint8_t value);
int memory_store16_checked(Memory *mem, uint32_t addr, uint16_t value);
int memory_store32_checked(Memory *mem, uint32_t addr, uint32_t value);

//debug
void dump_memory(Memory *mem, uint32_t start, uint32_t end);


#endif
