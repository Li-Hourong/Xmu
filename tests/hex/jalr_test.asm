# jalr_test.asm
# 测试 JALR 指令（opcode 0x67）
# 验证：跳转成功、rd 返回地址正确、ret 能返回、目标地址 &~1u 清最低位
#
# 内存布局：
#   0x00-0x10  主程序
#   0x60-0x68  被调函数
#
# 期望落点：
#   mem[0x14] = 0x22  函数体写入
#   mem[0x18] = 0x11  返回后写入

    addi x6, x0, 0x60         # x6 = 函数地址 0x60
    .word 0x001300e7         # jalr x1, x6, 1  （手写编码：imm=1，测 &~1u 清最低位）
    addi x10, x0, 0x11        # 返回后：x10 = 0x11
    sw   x10, 0x18(x0)        # mem[0x18] = 0x11
    ebreak

    .org 0x60
    addi x10, x0, 0x22        # 函数入口：x10 = 0x22
    sw   x10, 0x14(x0)        # mem[0x14] = 0x22
    .word 0x00008067         # ret = jalr x0, x1, 0
