#ifndef UART_H
#define UART_H
#include "common.h"

#define UART_BASE 0x10000000
#define UART_SIZE 0x100

#define UART_THR 0x00
#define UART_LSR 0x05
#define UART_LSR_THRE 0x20
#define UART_LSR_TEMT 0x40

typedef struct UART {
    
} UART;

UART* Create_UART(void);
void Free_UART(UART* uart);

// Bus 调用：offset = addr - UART_BASE
uint8_t uart_read8 (UART* uart, uint32_t offset);
void uart_write8(UART* uart, uint32_t offset, uint8_t value);

#endif
