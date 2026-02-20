#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define main hw5_asm_main
#include "assembler.c"
#undef main

#define EXPECT_DEATH(statement) do { \
    pid_t pid = fork(); \
    if (pid == 0) { \
        int fd = open("/dev/null", O_WRONLY); \
        dup2(fd, STDERR_FILENO); \
        close(fd); \
        statement; \
        exit(0); \
    } else { \
        int status; \
        waitpid(pid, &status, 0); \
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 1); \
    } \
} while(0)

void test_trim_line() {
    char buf1[] = "addi r1, 10 ; comment";
    trim_line(buf1);
    assert(strcmp(buf1, "addi r1, 10") == 0);

    char buf2[] = "   \t";
    trim_line(buf2);
    assert(strlen(buf2) == 0);

    char buf3[] = "code  ";
    trim_line(buf3);
    assert(strcmp(buf3, "code") == 0);
}

void test_line_has_non_ws() {
    assert(line_has_non_ws("   \t  ") == 0);
    assert(line_has_non_ws(" a ") == 1);
    assert(line_has_non_ws("") == 0);
}

void test_enforce_leading_space_rule() {
    enforce_leading_space_rule("\taddi r1, 10");
    enforce_leading_space_rule(".code");
    EXPECT_DEATH(enforce_leading_space_rule(" addi r1, 10"));
}

void test_enforce_tab_rule_if_statement() {
    enforce_tab_rule_if_statement("\taddi r1, 10", "addi r1, 10");
    enforce_tab_rule_if_statement(".code", ".code");
    enforce_tab_rule_if_statement(":label", ":label");
    EXPECT_DEATH(enforce_tab_rule_if_statement("addi r1, 10", "addi r1, 10"));
}

void test_is_valid_label_name() {
    assert(is_valid_label_name("Valid_Label1") == 1);
    assert(is_valid_label_name("_valid") == 1);
    assert(is_valid_label_name("1invalid") == 0);
    assert(is_valid_label_name("") == 0);
}

void test_enforce_label_only() {
    enforce_label_only(":valid_label   \t ");
    EXPECT_DEATH(enforce_label_only(":invalid_label addi"));
    EXPECT_DEATH(enforce_label_only(":1bad"));
}

void test_parse_register() {
    assert(parse_register("r0") == 0);
    assert(parse_register("r31") == 31);
    assert(parse_register("r32") == -1);
    assert(parse_register("x1") == -1);
    assert(parse_register("r") == -1);
}

void test_get_opcode() {
    assert(get_opcode("add") == OP_ADD);
    assert(get_opcode("halt") == MACRO_HALT);
    assert(get_opcode("mov") == OP_MOV_RR);
    assert(get_opcode("fake") == OP_UNKNOWN);
}

void test_resolve_value() {
    int64_t val;
    SymbolTable *t = create_table();
    insert_label(t, "target", 8192);

    assert(resolve_value("100", t, &val) == 0);
    assert(val == 100);

    assert(resolve_value("-50", t, &val) == 0);
    assert(val == -50);

    assert(resolve_value(":target", t, &val) == 0);
    assert(val == 8192);

    assert(resolve_value(":missing", t, &val) == 1);
    assert(resolve_value("invalid", t, &val) == 1);
}

void test_resolve_u64_decimal() {
    uint64_t val;
    SymbolTable *t = create_table();
    insert_label(t, "target", 8192);

    assert(resolve_u64_decimal("100", t, &val) == 0);
    assert(val == 100);

    assert(resolve_u64_decimal(":target", t, &val) == 0);
    assert(val == 8192);

    assert(resolve_u64_decimal("-1", t, &val) == 1);
    assert(resolve_u64_decimal("invalid", t, &val) == 1);
}

void test_parse_int64_strict() {
    int64_t val;
    assert(parse_int64_strict("123", &val) == 0);
    assert(val == 123);

    assert(parse_int64_strict("-123", &val) == 0);
    assert(val == -123);

    assert(parse_int64_strict("123bad", &val) == 1);
    assert(parse_int64_strict("", &val) == 1);
}

void test_check_bounds_signed() {
    check_bounds_signed(2047, 12, "msg");
    check_bounds_signed(-2048, 12, "msg");
    EXPECT_DEATH(check_bounds_signed(2048, 12, "msg"));
    EXPECT_DEATH(check_bounds_signed(-2049, 12, "msg"));
}

void test_check_bounds_unsigned() {
    check_bounds_unsigned(4095, 12, "msg");
    check_bounds_unsigned(0, 12, "msg");
    EXPECT_DEATH(check_bounds_unsigned(4096, 12, "msg"));
    EXPECT_DEATH(check_bounds_unsigned(-1, 12, "msg"));
}

void test_parse_mem_operand() {
    int base_reg;
    int64_t lit;
    SymbolTable *t = create_table();
    
    assert(parse_mem_operand("(r1)(10)", &base_reg, &lit, t) == 0);
    assert(base_reg == 1);
    assert(lit == 10);

    assert(parse_mem_operand("(r2)(-5)", &base_reg, &lit, t) == 0);
    assert(base_reg == 2);
    assert(lit == -5);

    assert(parse_mem_operand("(r33)(10)", &base_reg, &lit, t) == 1);
    assert(parse_mem_operand("r1(10)", &base_reg, &lit, t) == 1);
}


int main() {
    test_trim_line();
    test_line_has_non_ws();
    test_enforce_leading_space_rule();
    test_enforce_tab_rule_if_statement();
    test_is_valid_label_name();
    test_enforce_label_only();
    test_parse_register();
    test_get_opcode();
    test_resolve_value();
    test_resolve_u64_decimal();
    test_parse_int64_strict();
    test_check_bounds_signed();
    test_check_bounds_unsigned();
    test_parse_mem_operand();

    printf("ALL TESTS PASSED\n");
    return 0;
}