#include "Bus.h"

Bus* Create_Bus(Memory* mem, UART* uart)
{
    Bus* bus = (Bus*)malloc(sizeof(Bus));
    if (!bus) {
        return NULL;
    }
    bus->mem = mem;
    bus->uart = uart;
    return bus;
}

void Free_Bus(Bus* bus)
{
    if (bus) {
        free(bus);
    }
}

// ---- load ----

int bus_load8(Bus* bus, uint32_t addr, uint8_t* out)
{
    if (bus->uart && addr >= UART_BASE && addr < UART_BASE + UART_SIZE) {
        *out = uart_read8(bus->uart, addr - UART_BASE);
        return 0;
    }
    return memory_load8_checked(bus->mem, addr, out);
}

int bus_load16(Bus* bus, uint32_t addr, uint16_t* out)
{
    // 串口寄存器是字节宽度，软件只用 lbu 访问；16 位访问取起始字节并零扩展
    if (bus->uart && addr >= UART_BASE && addr < UART_BASE + UART_SIZE) {
        *out = (uint16_t)uart_read8(bus->uart, addr - UART_BASE);
        return 0;
    }
    return memory_load16_checked(bus->mem, addr, out);
}

int bus_load32(Bus* bus, uint32_t addr, uint32_t* out)
{
    if (bus->uart && addr >= UART_BASE && addr < UART_BASE + UART_SIZE) {
        *out = (uint32_t)uart_read8(bus->uart, addr - UART_BASE);
        return 0;
    }
    return memory_load32_checked(bus->mem, addr, out);
}

// ---- store ----

int bus_store8(Bus* bus, uint32_t addr, uint8_t value)
{
    if (bus->uart && addr >= UART_BASE && addr < UART_BASE + UART_SIZE) {
        uart_write8(bus->uart, addr - UART_BASE, value);
        return 0;
    }
    return memory_store8_checked(bus->mem, addr, value);
}

int bus_store16(Bus* bus, uint32_t addr, uint16_t value)
{
    if (bus->uart && addr >= UART_BASE && addr < UART_BASE + UART_SIZE) {
        uart_write8(bus->uart, addr - UART_BASE, (uint8_t)(value & 0xFF));
        return 0;
    }
    return memory_store16_checked(bus->mem, addr, value);
}

int bus_store32(Bus* bus, uint32_t addr, uint32_t value)
{
    if (bus->uart && addr >= UART_BASE && addr < UART_BASE + UART_SIZE) {
        uart_write8(bus->uart, addr - UART_BASE, (uint8_t)(value & 0xFF));
        return 0;
    }
    return memory_store32_checked(bus->mem, addr, value);
}
