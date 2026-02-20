#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>

#include "tinker_defs.h"

#define MEM_SIZE 524288 // 512 * 1024

// States
uint64_t registers[32] = { [31] = MEM_SIZE };
uint64_t program_counter = 0x2000;
uint8_t memory[MEM_SIZE];
bool halt_program = false;

// Error Out
void error_exit(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void check8(uint64_t addr) {
    if (addr > MEM_SIZE - 8) error_exit("Simulation error");
    if (addr & 7) error_exit("Simulation error");
}
static void check4(uint64_t addr) {
    if (addr > MEM_SIZE - 4) error_exit("Simulation error");
    if (addr & 3) error_exit("Simulation error");
}

static uint64_t read_u64_strict(void) {
    char buf[256];
    if (scanf(" %255s", buf) != 1) error_exit("Simulation error");
    if (buf[0] == '-' || buf[0] == '+') error_exit("Simulation error");

    errno = 0;
    char *end = NULL;
    unsigned long long v = strtoull(buf, &end, 10);

    if (errno == ERANGE || end == buf || *end != '\0') error_exit("Simulation error");
    //if (v > UINT64_MAX) error_exit("Simulation error");

    return (uint64_t) v;
}


// Reset
void reset() {
    halt_program = false;
    memset(registers, 0, sizeof(registers));
    program_counter = 0x2000;
    registers[31] = MEM_SIZE;
}

// Execute a single line of instruction
void execute(uint64_t instr) {
    uint64_t current_pc = program_counter - 4;
    int op = (instr >> 27) & 0x1F;
    int rd = (instr >> 22) & 0x1F;
    int rs = (instr >> 17) & 0x1F;
    int rt = (instr >> 12) & 0x1F;

    int32_t litS = ((int32_t) (instr & 0xFFF) << 20) >> 20;   // signed
    uint32_t lit  = instr & 0xFFF;

    switch (op) {
        // Logical
        case OP_AND:
            registers[rd] = registers[rs] & registers[rt]; break;
        case OP_OR:
            registers[rd] = registers[rs] | registers[rt]; break;
        case OP_XOR:
            registers[rd] = registers[rs] ^ registers[rt]; break;
        case OP_NOT:
            registers[rd] = ~registers[rs]; break;

        // Shifts
        case OP_SHFTR:
            registers[rd] = registers[rs] >> registers[rt]; break;
        case OP_SHFTRI:
            registers[rd] = registers[rd] >> lit; break;
        case OP_SHFTL:
            registers[rd] = registers[rs] << registers[rt]; break;
        case OP_SHFTLI:
            registers[rd] = registers[rd] << lit; break;

        // Control
        case OP_BR:
            program_counter = registers[rd]; break;
        case OP_BRR_R:
            program_counter = current_pc + registers[rd]; break;
        case OP_BRR_L:
            program_counter = current_pc + litS; break;
        case OP_BRNZ:
            if (registers[rs] != 0) program_counter = registers[rd]; 
            break;
        case OP_CALL: {
            if (registers[31] < 8) error_exit("Simulation error");
            check8(registers[31] - 8);
            uint64_t address = program_counter;
            memcpy(&memory[registers[31] - 8], &address, 8);
            program_counter = registers[rd]; break;
        }
        case OP_RET: {
            check8(registers[31] - 8);
            uint64_t address;
            memcpy(&address, &memory[registers[31] - 8], 8);
            program_counter = address; break;
        }
        case OP_BRGT:
            if (registers[rs] > registers[rt]) program_counter = registers[rd];
            break;
        case OP_PRIV: {
            switch(lit) {
                case 0x0: {
                    // Halt
                    halt_program = true;
                    return;
                }
                case 0x3: {
                    // Input Instruction
                    uint64_t input;
                    registers[rd] = read_u64_strict();
                    break;
                }
                case 0x4:
                    // Output Instruction
                    uint64_t port = registers[rd];
                    if (port == 1) {
                        printf("%" PRIu64 "\n", registers[rs]);
                    } else if (port == 3) {
                        printf("%c", (char)registers[rs]);
                    }
                    break;
                default:
                    error_exit("Simulation error");
                    break;
            }
            break;
        }

        // Data Movement
        case OP_MOV_ML: {
            int64_t addr_s = (int64_t)registers[rs] + (int64_t)litS;
            if (addr_s < 0) error_exit("Simulation error");
            uint64_t address = (uint64_t)addr_s;
            if (address > MEM_SIZE - 8) error_exit("Simulation error");
            // check8(address);
            // if (address % 8 != 0) error_exit("Simulation error");
            memcpy(&registers[rd], &memory[address], 8); break;
        }
        case OP_MOV_RR:
            registers[rd] = registers[rs]; break;
        case OP_MOV_L: {
            uint64_t mask = 0xFFFULL;
            registers[rd] = (registers[rd] & ~mask) | (lit & mask); break;
        }
        case OP_MOV_SM: {
            int64_t addr_s = (int64_t)registers[rd] + (int64_t)litS;
            if (addr_s < 0) error_exit("Simulation error");
            uint64_t address = (uint64_t)addr_s;
            check8(address);
            memcpy(&memory[address], &registers[rs], 8); break;
        }
        // Float
        case OP_ADDF: {
            double a, b, res;
            memcpy(&a, &registers[rs], 8);
            memcpy(&b, &registers[rt], 8);
            res = a + b;
            memcpy(&registers[rd], &res, 8); break;
        }
        case OP_SUBF: {
            double a, b, res;
            memcpy(&a, &registers[rs], 8);
            memcpy(&b, &registers[rt], 8);
            res = a - b;
            memcpy(&registers[rd], &res, 8); break;
        }
        case OP_MULF: {
            double a, b, res;
            memcpy(&a, &registers[rs], 8);
            memcpy(&b, &registers[rt], 8);
            res = a * b;
            memcpy(&registers[rd], &res, 8); break;
        }
        case OP_DIVF: {
            double a, b, res;
            memcpy(&a, &registers[rs], 8);
            memcpy(&b, &registers[rt], 8);
            if (b == 0.0) error_exit("Simulation error");
            res = a / b;
            memcpy(&registers[rd], &res, 8); break;
        }
        
        // Int
        case OP_ADD:
            registers[rd] = registers[rs] + registers[rt]; break;
        case OP_ADDI:
            registers[rd] += lit; break;
        case OP_SUB:
            registers[rd] = registers[rs] - registers[rt]; break;
        case OP_SUBI:
            registers[rd] -= lit; break;
        case OP_MUL:
            registers[rd] = registers[rs] * registers[rt]; break;
        case OP_DIV:
        if (registers[rt] == 0) error_exit("Simulation error");
            registers[rd] = registers[rs] / registers[rt]; break;
    }
    return;
}

// Get Instruction
uint32_t fetch() {
    check4(program_counter);

    uint32_t instr; memcpy(&instr, &memory[program_counter], 4);

    program_counter += 4;
    
    return instr;
}

// Run Loop
void run() {
    while (!halt_program) {
        uint32_t instr = fetch();
        execute(instr);
    }
}

// Read the file
void read_binary(const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        error_exit("Invalid tinker filepath");
    }

    struct tinker_file_header header;

    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        error_exit("Invalid header");
    }

    if (header.code_seg_size > 0) {
        if (header.code_seg_begin + header.code_seg_size > MEM_SIZE) error_exit("Invalid tinker filepath");
        fread(&memory[header.code_seg_begin], 1, header.code_seg_size, file);
    }

    // 3. Load the Data Segment
    if (header.data_seg_size > 0) {
        if (header.data_seg_begin + header.data_seg_size > MEM_SIZE) error_exit("Invalid tinker filepath");
        fread(&memory[header.data_seg_begin], 1, header.data_seg_size, file);
    }

    program_counter = header.code_seg_begin;

    // size_t n = fread(memory + 0x1000, 1, MEM_SIZE - 0x1000, file);
    // if (n == 0) error_exit("Invalid tinker filepath");
    fclose(file);
}

int main(int argc, char** argv) {
    if (argc < 2) error_exit("Invalid tinker filepath");

    const char *dot = strrchr(argv[1], '.');

    if (!dot || strcmp(dot, ".tko") != 0) {
        error_exit("Invalid tinker filepath");
    }

    read_binary(argv[1]);
    run();
    return 0;
}