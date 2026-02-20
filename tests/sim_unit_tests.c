#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>

#define main hw5_sim_main
#include "simulator.c"
#undef main

#define EXPECT_DEATH(statement) do { \
    pid_t pid = fork(); \
    if (pid == 0) { \
        int fd = open("/dev/null", O_WRONLY); \
        dup2(fd, STDERR_FILENO); \
        dup2(fd, STDOUT_FILENO); \
        close(fd); \
        statement; \
        exit(0); \
    } else { \
        int status; \
        waitpid(pid, &status, 0); \
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 1); \
    } \
} while(0)

uint32_t make_instr(int op, int rd, int rs, int rt, uint32_t lit) {
    return ((op & 0x1F) << 27) | ((rd & 0x1F) << 22) | ((rs & 0x1F) << 17) | ((rt & 0x1F) << 12) | (lit & 0xFFF);
}

void wrap_check8_unaligned() { check8(1); }
void wrap_check8_oob() { check8(MEM_SIZE); }
void wrap_check4_unaligned() { check4(1); }
void wrap_check4_oob() { check4(MEM_SIZE); }

void test_memory_checks() {
    check8(0);
    check8(8);
    check4(0);
    check4(4);
    EXPECT_DEATH(wrap_check8_unaligned());
    EXPECT_DEATH(wrap_check8_oob());
    EXPECT_DEATH(wrap_check4_unaligned());
    EXPECT_DEATH(wrap_check4_oob());
}

void wrap_read_u64_neg() {
    FILE *f = fopen("test_in.txt", "w");
    fprintf(f, "-5\n");
    fclose(f);
    freopen("test_in.txt", "r", stdin);
    read_u64_strict();
}

void wrap_read_u64_alpha() {
    FILE *f = fopen("test_in.txt", "w");
    fprintf(f, "abc\n");
    fclose(f);
    freopen("test_in.txt", "r", stdin);
    read_u64_strict();
}

void wrap_read_u64_erange() {
    FILE *f = fopen("test_in.txt", "w");
    fprintf(f, "999999999999999999999999\n");
    fclose(f);
    freopen("test_in.txt", "r", stdin);
    read_u64_strict();
}

void test_read_u64() {
    FILE *f = fopen("test_in.txt", "w");
    fprintf(f, "12345\n");
    fclose(f);
    
    int fd_in = dup(STDIN_FILENO);
    freopen("test_in.txt", "r", stdin);
    uint64_t val = read_u64_strict();
    assert(val == 12345);
    
    dup2(fd_in, STDIN_FILENO);
    close(fd_in);
    
    EXPECT_DEATH(wrap_read_u64_neg());
    EXPECT_DEATH(wrap_read_u64_alpha());
    EXPECT_DEATH(wrap_read_u64_erange());
    remove("test_in.txt");
}

void test_execute_math() {
    reset();
    registers[1] = 10;
    registers[2] = 3;
    
    execute(make_instr(OP_ADD, 3, 1, 2, 0));
    assert(registers[3] == 13);
    
    execute(make_instr(OP_SUB, 3, 1, 2, 0));
    assert(registers[3] == 7);
    
    execute(make_instr(OP_MUL, 3, 1, 2, 0));
    assert(registers[3] == 30);
    
    execute(make_instr(OP_DIV, 3, 1, 2, 0));
    assert(registers[3] == 3);
    
    execute(make_instr(OP_ADDI, 1, 0, 0, 5));
    assert(registers[1] == 15);
    
    execute(make_instr(OP_SUBI, 1, 0, 0, 5));
    assert(registers[1] == 10);
}

void test_execute_logical() {
    reset();
    registers[1] = 0xC;
    registers[2] = 0xA;
    
    execute(make_instr(OP_AND, 3, 1, 2, 0));
    assert(registers[3] == 0x8);
    
    execute(make_instr(OP_OR, 3, 1, 2, 0));
    assert(registers[3] == 0xE);
    
    execute(make_instr(OP_XOR, 3, 1, 2, 0));
    assert(registers[3] == 0x6);
    
    execute(make_instr(OP_NOT, 3, 1, 0, 0));
    assert(registers[3] == ~0xCULL);
}

void test_execute_shifts() {
    reset();
    registers[1] = 16;
    registers[2] = 2;
    
    execute(make_instr(OP_SHFTR, 3, 1, 2, 0));
    assert(registers[3] == 4);
    
    execute(make_instr(OP_SHFTL, 3, 1, 2, 0));
    assert(registers[3] == 64);
    
    registers[3] = 16;
    execute(make_instr(OP_SHFTRI, 3, 0, 0, 2));
    assert(registers[3] == 4);
    
    execute(make_instr(OP_SHFTLI, 3, 0, 0, 2));
    assert(registers[3] == 16);
}

void test_execute_control() {
    reset();
    registers[1] = 0x3000;
    execute(make_instr(OP_BR, 1, 0, 0, 0));
    assert(program_counter == 0x3000);

    program_counter = 0x2004;
    registers[1] = 8;
    execute(make_instr(OP_BRR_R, 1, 0, 0, 0));
    assert(program_counter == 0x2008);

    program_counter = 0x2004;
    execute(make_instr(OP_BRR_L, 0, 0, 0, 8));
    assert(program_counter == 0x2008);
    
    program_counter = 0x2004;
    execute(make_instr(OP_BRR_L, 0, 0, 0, 0xFF8));
    assert(program_counter == 0x1FF8);

    program_counter = 0x2004;
    registers[1] = 1;
    registers[2] = 0x4000;
    execute(make_instr(OP_BRNZ, 2, 1, 0, 0));
    assert(program_counter == 0x4000);

    program_counter = 0x2004;
    registers[1] = 0;
    execute(make_instr(OP_BRNZ, 2, 1, 0, 0));
    assert(program_counter == 0x2004);

    program_counter = 0x2004;
    registers[1] = 10;
    registers[2] = 5;
    registers[3] = 0x5000;
    execute(make_instr(OP_BRGT, 3, 1, 2, 0));
    assert(program_counter == 0x5000);

    program_counter = 0x2004;
    execute(make_instr(OP_BRGT, 3, 2, 1, 0));
    assert(program_counter == 0x2004);

    reset();
    program_counter = 0x2004;
    registers[1] = 0x6000;
    execute(make_instr(OP_CALL, 1, 0, 0, 0));
    assert(program_counter == 0x6000);
    
    uint64_t saved_pc;
    memcpy(&saved_pc, &memory[MEM_SIZE - 8], 8);
    assert(saved_pc == 0x2004);
    
    execute(make_instr(OP_RET, 0, 0, 0, 0));
    assert(program_counter == 0x2004);
}

void test_execute_memory() {
    reset();
    registers[1] = 0x10000;
    registers[2] = 0xABCDEF;
    execute(make_instr(OP_MOV_SM, 1, 2, 0, 0));
    
    uint64_t mem_val;
    memcpy(&mem_val, &memory[0x10000], 8);
    assert(mem_val == 0xABCDEF);

    registers[3] = 0;
    execute(make_instr(OP_MOV_ML, 3, 1, 0, 0));
    assert(registers[3] == 0xABCDEF);

    execute(make_instr(OP_MOV_RR, 4, 2, 0, 0));
    assert(registers[4] == 0xABCDEF);

    execute(make_instr(OP_MOV_L, 5, 0, 0, 0xFFF));
    assert(registers[5] == 0xFFF);
}

void test_execute_float() {
    reset();
    double f1 = 5.5;
    double f2 = 2.0;
    double fres;
    
    memcpy(&registers[1], &f1, 8);
    memcpy(&registers[2], &f2, 8);
    
    execute(make_instr(OP_ADDF, 3, 1, 2, 0));
    memcpy(&fres, &registers[3], 8);
    assert(fres == 7.5);

    execute(make_instr(OP_SUBF, 3, 1, 2, 0));
    memcpy(&fres, &registers[3], 8);
    assert(fres == 3.5);

    execute(make_instr(OP_MULF, 3, 1, 2, 0));
    memcpy(&fres, &registers[3], 8);
    assert(fres == 11.0);

    execute(make_instr(OP_DIVF, 3, 1, 2, 0));
    memcpy(&fres, &registers[3], 8);
    assert(fres == 2.75);
}

void test_execute_priv() {
    reset();
    execute(make_instr(OP_PRIV, 0, 0, 0, 0));
    assert(halt_program == true);

    reset();
    FILE *f = fopen("test_in.txt", "w");
    fprintf(f, "999\n");
    fclose(f);
    int fd_in = dup(STDIN_FILENO);
    freopen("test_in.txt", "r", stdin);
    
    execute(make_instr(OP_PRIV, 1, 0, 0, 3));
    assert(registers[1] == 999);
    
    dup2(fd_in, STDIN_FILENO);
    close(fd_in);
    remove("test_in.txt");

    registers[1] = 1;
    registers[2] = 123;
    execute(make_instr(OP_PRIV, 1, 2, 0, 4));

    registers[1] = 3;
    registers[2] = 65;
    execute(make_instr(OP_PRIV, 1, 2, 0, 4));
}

void wrap_div_zero() {
    registers[1] = 10;
    registers[2] = 0;
    execute(make_instr(OP_DIV, 3, 1, 2, 0));
}

void wrap_divf_zero() {
    double f1 = 5.0;
    double f2 = 0.0;
    memcpy(&registers[1], &f1, 8);
    memcpy(&registers[2], &f2, 8);
    execute(make_instr(OP_DIVF, 3, 1, 2, 0));
}

void wrap_call_oob() {
    registers[31] = 0;
    execute(make_instr(OP_CALL, 0, 0, 0, 0));
}

void wrap_mov_ml_neg() {
    registers[1] = 0;
    execute(make_instr(OP_MOV_ML, 2, 1, 0, 0xFF8));
}

void wrap_mov_sm_neg() {
    registers[1] = 0;
    execute(make_instr(OP_MOV_SM, 1, 2, 0, 0xFF8));
}

void wrap_mov_ml_oob() {
    registers[1] = MEM_SIZE;
    execute(make_instr(OP_MOV_ML, 2, 1, 0, 0));
}

void wrap_priv_invalid() {
    execute(make_instr(OP_PRIV, 0, 0, 0, 99));
}

void test_execution_deaths() {
    EXPECT_DEATH(wrap_div_zero());
    EXPECT_DEATH(wrap_divf_zero());
    EXPECT_DEATH(wrap_call_oob());
    EXPECT_DEATH(wrap_mov_ml_neg());
    EXPECT_DEATH(wrap_mov_sm_neg());
    EXPECT_DEATH(wrap_mov_ml_oob());
    EXPECT_DEATH(wrap_priv_invalid());
}

void test_fetch_and_run() {
    reset();
    uint32_t inst = make_instr(OP_PRIV, 0, 0, 0, 0);
    memcpy(&memory[0x2000], &inst, 4);
    
    assert(fetch() == inst);
    assert(program_counter == 0x2004);
    
    reset();
    memcpy(&memory[0x2000], &inst, 4);
    run();
    assert(halt_program == true);
}

void wrap_bad_ext() {
    char *args[] = {"sim", "bad.txt"};
    hw5_sim_main(2, args);
}

void wrap_no_args() {
    char *args[] = {"sim"};
    hw5_sim_main(1, args);
}

void wrap_bad_file() {
    read_binary("does_not_exist.tko");
}

void wrap_bad_header() {
    FILE *f = fopen("bad_header.tko", "wb");
    int bad = 1;
    fwrite(&bad, 4, 1, f);
    fclose(f);
    read_binary("bad_header.tko");
}

void wrap_oob_code() {
    struct tinker_file_header h;
    memset(&h, 0, sizeof(h));
    h.code_seg_begin = MEM_SIZE - 2;
    h.code_seg_size = 4;
    FILE *f = fopen("oob_code.tko", "wb");
    fwrite(&h, sizeof(h), 1, f);
    fclose(f);
    read_binary("oob_code.tko");
}

void wrap_oob_data() {
    struct tinker_file_header h;
    memset(&h, 0, sizeof(h));
    h.data_seg_begin = MEM_SIZE - 2;
    h.data_seg_size = 8;
    FILE *f = fopen("oob_data.tko", "wb");
    fwrite(&h, sizeof(h), 1, f);
    fclose(f);
    read_binary("oob_data.tko");
}

void test_binary_loader() {
    EXPECT_DEATH(wrap_bad_ext());
    EXPECT_DEATH(wrap_no_args());
    EXPECT_DEATH(wrap_bad_file());
    EXPECT_DEATH(wrap_bad_header());
    EXPECT_DEATH(wrap_oob_code());
    EXPECT_DEATH(wrap_oob_data());
    
    struct tinker_file_header h;
    memset(&h, 0, sizeof(h));
    h.code_seg_begin = 0x2000;
    h.code_seg_size = 4;
    h.data_seg_begin = 0x10000;
    h.data_seg_size = 8;
    
    FILE *f = fopen("valid.tko", "wb");
    fwrite(&h, sizeof(h), 1, f);
    uint32_t inst = 0;
    fwrite(&inst, 4, 1, f);
    uint64_t data = 0;
    fwrite(&data, 8, 1, f);
    fclose(f);
    
    read_binary("valid.tko");
    assert(program_counter == 0x2000);
    remove("valid.tko");
    remove("bad_header.tko");
    remove("oob_code.tko");
    remove("oob_data.tko");
}

int main() {
    test_memory_checks();
    test_read_u64();
    test_execute_math();
    test_execute_logical();
    test_execute_shifts();
    test_execute_control();
    test_execute_memory();
    test_execute_float();
    test_execute_priv();
    test_execution_deaths();
    test_fetch_and_run();
    test_binary_loader();
    
    printf("ALL TESTS PASSED\n");
    return 0;
}