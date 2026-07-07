# trap_mret_test.asm
# 测试 trap 进/出闭环：ecall -> handler -> mret -> 返回主流程
#
# 验证：
#   1. ecall 触发 trap，跳到 mtvec
#   2. handler 能 csrr 读 mcause (=11=ECALL_M) 和 mepc (=0x10=ecall 地址)
#   3. mepc += 4 跨过 ecall，mret 后 pc <- mepc，回到 0x14 继续执行
#
# 内存布局：
#   0x00-0x1c  主程序
#   0x80-0x94  trap handler
#
# 期望落点：
#   mem[0x40] = 0x11  ecall 前写入
#   mem[0x48] = 0x0b  handler 读出的 mcause
#   mem[0x44] = 0x22  mret 返回后写入（证明成功返回）

    addi x1, x0, 0x80         # x1 = handler 地址
    csrw mtvec, x1            # mtvec = 0x80

    addi x2, x0, 0x11
    sw   x2, 0x40(x0)         # mem[0x40] = 0x11  (ecall 前执行)

    ecall                     # -> trap，跳到 mtvec=0x80

    addi x3, x0, 0x22         # 返回后：x3 = 0x22
    sw   x3, 0x44(x0)         # mem[0x44] = 0x22  (mret 成功返回)

    ebreak

    .org 0x80
handler:
    csrr x5, mcause           # x5 = 11
    sw   x5, 0x48(x0)         # mem[0x48] = 0x0B  (证明进了 handler)
    csrr x6, mepc             # x6 = 0x10 (ecall 的地址)
    addi x6, x6, 4            # x6 = 0x14 (跳过 ecall)
    csrw mepc, x6             # mepc = 0x14
    mret                      # pc <- mepc = 0x14
