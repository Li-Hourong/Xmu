#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TRAP_INST_MISALIGNED    = 0,
    TRAP_INST_FAULT         = 1,
    TRAP_ILLEGAL_INST       = 2,
    TRAP_BREAKPOINT         = 3,
    TRAP_LOAD_MISALIGNED    = 4,
    TRAP_LOAD_FAULT         = 5,
    TRAP_STORE_MISALIGNED   = 6,
    TRAP_STORE_FAULT        = 7,
    TRAP_ECALL_U            = 8,
    TRAP_ECALL_S            = 9,
    TRAP_ECALL_M            = 11
} TrapCause;


#endif