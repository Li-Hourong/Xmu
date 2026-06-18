#include "Memory.h"
#include <stdio.h>

Memory* Create_Memory(uint32_t size) {
    Memory* mem = (Memory*)malloc(sizeof(Memory));
    mem->data = (uint8_t*)calloc(size, sizeof(uint8_t));
    mem->size = size;
    return mem;
}

void Free_Memory(Memory* mem) {
    if (mem) {
        free(mem->data);
        free(mem);
    }
}

int load_memory(const char* filename, Memory* mem) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return 0;
    }

    fread(mem->data, sizeof(uint8_t), mem->size, file);
    fclose(file);
    return 1;
}

static int memory_check_range(Memory *mem, uint32_t addr, uint32_t size) {
    if (!mem || !mem->data) {
        return 0;
    }

    return addr <= mem->size && size <= mem->size - addr;
}

// ---- read ----

uint8_t memory_load8(Memory *mem, uint32_t addr) {
    return mem->data[addr];
}

uint16_t memory_load16(Memory *mem, uint32_t addr) {
    return (uint16_t)mem->data[addr]
         | ((uint16_t)mem->data[addr + 1] << 8);
}

uint32_t memory_load32(Memory *mem, uint32_t addr) {
    return (uint32_t)mem->data[addr]
         | ((uint32_t)mem->data[addr + 1] << 8)
         | ((uint32_t)mem->data[addr + 2] << 16)
         | ((uint32_t)mem->data[addr + 3] << 24);
}

int memory_load8_checked(Memory *mem, uint32_t addr, uint8_t *out) {
    if (!out || !memory_check_range(mem, addr, 1)) {
        return 0;
    }
    *out = memory_load8(mem, addr);
    return 1;
}

int memory_load16_checked(Memory *mem, uint32_t addr, uint16_t *out) {
    if (!out || !memory_check_range(mem, addr, 2)) {
        return 0;
    }
    *out = memory_load16(mem, addr);
    return 1;
}

int memory_load32_checked(Memory *mem, uint32_t addr, uint32_t *out) {
    if (!out || !memory_check_range(mem, addr, 4)) {
        return 0;
    }
    *out = memory_load32(mem, addr);
    return 1;
}

// ---- write ----

void memory_store8(Memory *mem, uint32_t addr, uint8_t value) {
    mem->data[addr] = value;
}

void memory_store16(Memory *mem, uint32_t addr, uint16_t value) {
    mem->data[addr]     = (uint8_t)(value & 0xFF);
    mem->data[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
}

void memory_store32(Memory *mem, uint32_t addr, uint32_t value) {
    mem->data[addr]     = (uint8_t)(value & 0xFF);
    mem->data[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    mem->data[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
    mem->data[addr + 3] = (uint8_t)((value >> 24) & 0xFF);
}

int memory_store8_checked(Memory *mem, uint32_t addr, uint8_t value) {
    if (!memory_check_range(mem, addr, 1)) {
        return 0;
    }
    memory_store8(mem, addr, value);
    return 1;
}

int memory_store16_checked(Memory *mem, uint32_t addr, uint16_t value) {
    if (!memory_check_range(mem, addr, 2)) {
        return 0;
    }
    memory_store16(mem, addr, value);
    return 1;
}

int memory_store32_checked(Memory *mem, uint32_t addr, uint32_t value) {
    if (!memory_check_range(mem, addr, 4)) {
        return 0;
    }
    memory_store32(mem, addr, value);
    return 1;
}

// ---- debug ----

void dump_memory(Memory *mem, uint32_t start, uint32_t end) {
    for (uint32_t addr = start; addr < end; addr += 4) {
        uint32_t value = 0;
        uint32_t remaining = end - addr;
        uint32_t bytes = remaining < 4 ? remaining : 4;

        for (uint32_t offset = 0; offset < bytes; ++offset) {
            value |= (uint32_t)mem->data[addr + offset] << (offset * 8);
        }

        printf("%08x: %08x\n", addr, value);
    }
}
