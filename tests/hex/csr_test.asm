# csr_test.asm
# 测试 6 条 CSR 指令（Zicsr 扩展）
#   csrrw / csrrs / csrrc / csrrwi / csrrsi / csrrci
#
# 用 mscratch (0x340) 作可读写测试对象，mhartid (0xF14) 作只读 CSR。
# 每条指令后把"旧值"或"新值"写入内存，方便核对。
#
# 期望落点（见每行注释）：
#   0x80=0x55  0x84=0x55  0x88=0x5f  0x8c=0x5f
#   0x90=0x58  0x94=0x58  0x98=0x40  0x9c=0x40
#   0xa0=0x1f  0xa4=0x00

    addi  x1, x0, 0x55
    csrrw x2, mscratch, x1        # mscratch = 0x55, x2 = 0（旧值）
    csrrs x3, mscratch, x0        # x3 = 0x55（读）
    sw    x3, 0x80(x0)            # mem[0x80] = 0x55

    csrrsi x4, mscratch, 0x0F    # mscratch = 0x55 | 0x0F = 0x5F, x4 = 0x55（旧值）
    sw     x4, 0x84(x0)           # mem[0x84] = 0x55
    csrrs  x5, mscratch, x0      # x5 = 0x5F
    sw     x5, 0x88(x0)          # mem[0x88] = 0x5F

    csrrci x6, mscratch, 0x07    # mscratch = 0x5F & ~0x07 = 0x58, x6 = 0x5F（旧值）
    sw     x6, 0x8c(x0)          # mem[0x8c] = 0x5F
    csrrs  x7, mscratch, x0      # x7 = 0x58
    sw     x7, 0x90(x0)          # mem[0x90] = 0x58

    addi  x10, x0, 0x18
    csrrc x9, mscratch, x10       # mscratch = 0x58 & ~0x18 = 0x40, x9 = 0x58（旧值）
    sw    x9, 0x94(x0)           # mem[0x94] = 0x58
    csrrs x11, mscratch, x0      # x11 = 0x40
    sw    x11, 0x98(x0)          # mem[0x98] = 0x40

    csrrwi x12, mscratch, 0x1F   # mscratch = 0x1F, x12 = 0x40（旧值）
    sw     x12, 0x9c(x0)         # mem[0x9c] = 0x40
    csrrs  x13, mscratch, x0    # x13 = 0x1F
    sw     x13, 0xa0(x0)         # mem[0xa0] = 0x1F

    csrrs  x8, mhartid, x0       # 只读 CSR：x8 = 0
    sw     x8, 0xa4(x0)          # mem[0xa4] = 0x00

    ebreak
