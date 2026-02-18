#ifndef TINKER_DEFS_H
#define TINKER_DEFS_H

#include <stdint.h>

struct tinker_file_header {
    uint64_t file_type;
    uint64_t code_seg_begin;
    uint64_t code_seg_size;
    uint64_t data_seg_begin;
    uint64_t data_seg_size;
};

// Operation Codes
typedef enum {
    // Logic
    OP_AND = 0x00, OP_OR = 0x01, OP_XOR = 0x02, OP_NOT = 0x03,
    // Shifts
    OP_SHFTR = 0x04, OP_SHFTRI = 0x05, OP_SHFTL = 0x06, OP_SHFTLI = 0x07,
    // Control
    OP_BR = 0x08, OP_BRR_R = 0x09, OP_BRR_L = 0x0a, OP_BRNZ = 0x0b,
    OP_CALL = 0x0c, OP_RET = 0x0d, OP_BRGT = 0x0e, OP_PRIV = 0x0f,
    // Data Movement
    OP_MOV_ML = 0x10, // mov rd, (rs)(L) - Load from Mem
    OP_MOV_RR = 0x11, // mov rd, rs
    OP_MOV_L = 0x12,  // mov rd, L (Sets upper bits)
    OP_MOV_SM = 0x13, // mov (rd)(L), rs - Store to Mem
    // Float
    OP_ADDF = 0x14, OP_SUBF = 0x15, OP_MULF = 0x16, OP_DIVF = 0x17,
    // Int
    OP_ADD = 0x18, OP_ADDI = 0x19, OP_SUB = 0x1a, OP_SUBI = 0x1b,
    OP_MUL = 0x1c, OP_DIV = 0x1d,

    MACRO_CLR, MACRO_HALT, MACRO_IN, MACRO_OUT, MACRO_LD, MACRO_PUSH, MACRO_POP,
    OP_UNKNOWN
} OperationCode;



#endif