# uart_hello.asm
# 测试 MMIO + UART 输出
# 往 UART_BASE (0x10000000) 的 THR 寄存器连续写 4 个字节，
# 终端应输出 "Hi!"。
#
# UART 是 NS16550A 简化版：写 THR (offset 0) = putchar。
# 本例不轮询 LSR（简化版始终可发），直接写。

    lui x5, 0x10000           # x5 = 0x10000000 (UART_BASE)

    addi x6, x0, 'H'
    sb   x6, 0(x5)            # THR <- 'H'
    addi x6, x0, 'i'
    sb   x6, 0(x5)            # THR <- 'i'
    addi x6, x0, '!'
    sb   x6, 0(x5)            # THR <- '!'
    addi x6, x0, '\n'
    sb   x6, 0(x5)            # THR <- '\n'

    ebreak
