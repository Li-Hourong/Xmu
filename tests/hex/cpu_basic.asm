# cpu_basic.asm
# RV32I 基础指令综合测试
# Run with: ./build/xmu tests/hex/cpu_basic.bin
# Expected memory after running:
# 0x80 = 0x0000000c  add result: 5 + 7
# 0x84 = 0x00000002  sub result: 7 - 5
# 0x88 = 0x00000014  slli result: 5 << 2
# 0x8c = 0xffffffff  srai result: -1 >> 1
# 0x90 = 0x00000001  branch result: BLT skipped addi 99
# 0x94 = 0x00000048  jal link address
# 0x98 = 0x0000000c  lw result loaded from 0x80

    addi x1, x0, 5
    addi x2, x0, 7
    add  x3, x1, x2
    sw   x3, 0x80(x0)
    lw   x11, 0x80(x0)
    sw   x11, 0x98(x0)
    sub  x4, x2, x1
    sw   x4, 0x84(x0)
    slli x5, x1, 2
    sw   x5, 0x88(x0)
    addi x6, x0, -1
    srai x7, x6, 1
    sw   x7, 0x8c(x0)
    blt  x1, x2, branch_ok
    addi x8, x0, 99
branch_ok:
    addi x8, x8, 1
    sw   x8, 0x90(x0)
    jal  x9, jal_ok
    addi x10, x0, 123
jal_ok:
    sw   x9, 0x94(x0)
    ebreak

