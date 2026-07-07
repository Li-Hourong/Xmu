#include "UART.h"
#include <stdio.h>

UART* Create_UART(void) {
    UART* uart = (UART*)malloc(sizeof(UART));
    return uart;
}

void Free_UART(UART* uart) {
    if (uart) {
        free(uart);
    }
}

uint8_t uart_read8(UART* uart, uint32_t offset) {
    (void)uart;
    switch (offset) {
        case UART_LSR:
            // 简化：始终报告发送器空闲，软件可立即发下一字节
            return UART_LSR_THRE | UART_LSR_TEMT;
        default:
            return 0;  // 其它寄存器暂未实现
    }
}

void uart_write8(UART* uart, uint32_t offset, uint8_t value) {
    (void)uart;
    switch (offset) {
        case UART_THR:
            // 写 THR = 输出一个字节到宿主机终端
            putchar((int)value);
            fflush(stdout);
            break;
        default:
            break;
    }
}
