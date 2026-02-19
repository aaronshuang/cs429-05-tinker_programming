#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include "tinker_defs.h"
#include "symbol_table.h"

int EXIT_STATUS = 0;

static const char *tmp_inter = NULL;
static const char *tmp_out   = NULL;

void error_exit(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    if (tmp_inter) remove(tmp_inter);
    if (tmp_out) remove(tmp_out);
    exit(1);
}

void trim_line(char *line) {
    char *p = strchr(line, ';');
    if (p) *p = '\0';

    size_t n = strlen(line);
    if (n == 0) return;

    p = line + n - 1;
    while (p >= line && isspace((unsigned char)*p)) {
        *p = '\0';
        if (p == line) break;
        p--;
    }
}

static int line_has_non_ws(const char *s) {
    for (int i = 0; s[i]; i++) {
        if (!isspace((unsigned char)s[i])) return 1;
    }
    return 0;
}

static void enforce_leading_space_rule(const char *raw_line) {
    if (raw_line && raw_line[0] == ' ') {
        error_exit("Leading spaces are invalid (line must start with tab for statements)");
    }
}

static void enforce_tab_rule_if_statement(const char *raw_line, const char *ptr_trimmed) {
    if (!ptr_trimmed || *ptr_trimmed == '\0') return;
    if (strncmp(ptr_trimmed, ".code", 5) == 0) return;
    if (strncmp(ptr_trimmed, ".data", 5) == 0) return;
    if (*ptr_trimmed == ':') return;

    if (raw_line[0] != '\t') {
        error_exit("Statement line must begin with a tab");
    }
}

static int is_valid_label_name(const char *s) {
    if (!s || !*s) return 0;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    for (int i = 1; s[i]; i++) {
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_')) return 0;
    }
    return 1;
}

static void enforce_label_only(const char *ptr) {
    const char *p = ptr + 1;

    // skip label token
    if (!(isalpha((unsigned char)*p) || *p == '_')) error_exit("Invalid label name");
    p++;
    while (isalnum((unsigned char)*p) || *p == '_') p++;

    // after label, only whitespace allowed
    while (*p) {
        if (!isspace((unsigned char)*p)) error_exit("Label must be alone on its line");
        p++;
    }
}

int parse_register(const char *reg) {
    if (!reg || reg[0] != 'r') return -1;
    if (!isdigit((unsigned char)reg[1])) return -1;

    int val = 0;
    for (int i = 1; reg[i]; i++) {
        if (!isdigit((unsigned char)reg[i])) return -1;
        val = val * 10 + (reg[i] - '0');
        if (val > 31) return -1;
    }
    return val;
}

int get_opcode(char *mnem) {
    if (!strcmp(mnem, "add")) return OP_ADD;
    if (!strcmp(mnem, "addi")) return OP_ADDI;
    if (!strcmp(mnem, "addf")) return OP_ADDF;
    if (!strcmp(mnem, "sub")) return OP_SUB;
    if (!strcmp(mnem, "subi")) return OP_SUBI;
    if (!strcmp(mnem, "subf")) return OP_SUBF;
    if (!strcmp(mnem, "mul")) return OP_MUL;
    if (!strcmp(mnem, "mulf")) return OP_MULF;
    if (!strcmp(mnem, "div")) return OP_DIV;
    if (!strcmp(mnem, "divf")) return OP_DIVF;
    if (!strcmp(mnem, "and")) return OP_AND;
    if (!strcmp(mnem, "or")) return OP_OR;
    if (!strcmp(mnem, "xor")) return OP_XOR;
    if (!strcmp(mnem, "not")) return OP_NOT;
    if (!strcmp(mnem, "shftr")) return OP_SHFTR;
    if (!strcmp(mnem, "shftri")) return OP_SHFTRI;
    if (!strcmp(mnem, "shftl")) return OP_SHFTL;
    if (!strcmp(mnem, "shftli")) return OP_SHFTLI;
    if (!strcmp(mnem, "br")) return OP_BR;
    if (!strcmp(mnem, "brr")) return OP_BRR_L; // check arg later
    if (!strcmp(mnem, "brnz")) return OP_BRNZ;
    if (!strcmp(mnem, "call")) return OP_CALL;
    if (!strcmp(mnem, "return") || !strcmp(mnem, "ret")) return OP_RET;
    if (!strcmp(mnem, "brgt")) return OP_BRGT;
    if (!strcmp(mnem, "priv")) return OP_PRIV;
    if (!strcmp(mnem, "mov")) return OP_MOV_RR;

    if (!strcmp(mnem, "clr")) return MACRO_CLR;
    if (!strcmp(mnem, "halt")) return MACRO_HALT;
    if (!strcmp(mnem, "in")) return MACRO_IN;
    if (!strcmp(mnem, "out")) return MACRO_OUT;
    if (!strcmp(mnem, "ld")) return MACRO_LD;
    if (!strcmp(mnem, "push")) return MACRO_PUSH;
    if (!strcmp(mnem, "pop")) return MACRO_POP;

    return OP_UNKNOWN;
}

int resolve_value(char *token, SymbolTable *t, int64_t *out_val) {
    size_t n = strlen(token);
    while (n && isspace((unsigned char)token[n-1])) token[--n] = '\0';
    if (n && (token[n-1] == 'u' || token[n-1] == 'U')) {
        token[n-1] = '\0';
        n--;
    }

    if (token[0] == ':') {
        if (!t) return 1;
        if (!is_valid_label_name(token + 1)) return 1;
        int64_t addr = lookup_label(t, token + 1);
        if (addr == -1) return 1;
        *out_val = addr;
        return 0;
    }
    char *end;
    errno = 0;
    *out_val = strtoll(token, &end, 0);
    if (errno != 0) return 1;
    if (*end != '\0') return 1;
    return 0;
}

static int resolve_u64_decimal(char *token, SymbolTable *t, uint64_t *out) {
    size_t n = strlen(token);
    while (n && isspace((unsigned char)token[n-1])) token[--n] = '\0';

    if (token[0] == '-') return 1;

    if (token[0] == ':') {
        if (!t) return 1;
        if (!is_valid_label_name(token + 1)) return 1;
        int64_t addr = lookup_label(t, token + 1);
        if (addr == -1) return 1;
        *out = (uint64_t)addr;
        return 0;
    }

    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(token, &end, 10);
    if (errno == ERANGE) return 1;
    if (!end || *end != '\0') return 1;
    *out = (uint64_t)v;
    return 0;
}

static int parse_int64_strict(const char *s, int64_t *out) {
    if (!s || !*s) return 1;
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno != 0) return 1;
    if (!end || *end != '\0') return 1;
    *out = (int64_t)v;
    return 0;
}

static void check_bounds_signed(int64_t v, int bits, const char *msg) {
    if (bits <= 0 || bits >= 63) return;
    int64_t max =  (1LL << (bits - 1)) - 1;
    int64_t min = -(1LL << (bits - 1));
    if (v < min || v > max) error_exit(msg);
}

static void check_bounds_unsigned(int64_t v, int bits, const char *msg) {
    if (bits <= 0 || bits >= 63) return;
    if (v < 0) error_exit(msg);
    uint64_t max = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    if ((uint64_t)v > max) error_exit(msg);
}

static int parse_mem_operand(const char *s, int *base_reg, int64_t *lit, SymbolTable *t);

struct tinker_file_header pass_one(const char *input, const char *interfile, SymbolTable *t) {
    struct tinker_file_header header;
    header.file_type = 0;
    header.code_seg_begin = 0x2000;
    header.code_seg_size = 0;
    header.data_seg_begin = 0x10000;
    header.data_seg_size = 0;


    FILE *in = fopen(input, "r");
    if (!in) error_exit("Cannot open input file");

    // uint64_t addr = 0x1000;
    uint64_t data_addr = header.data_seg_begin;
    uint64_t code_addr = header.code_seg_begin;

    bool in_code = true;
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), in)) {
        enforce_leading_space_rule(line);

        char clean[MAX_LINE];
        strcpy(clean, line);
        trim_line(clean);

        if (!line_has_non_ws(clean)) continue;

        char *ptr = clean;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') continue;

        if (strncmp(ptr, ".code", 5) == 0) { in_code = true;  continue; }
        if (strncmp(ptr, ".data", 5) == 0) { in_code = false; continue; }

        if (*ptr == ':') {
            enforce_label_only(ptr);
            char label[MAX_LABEL];
            if (sscanf(ptr+1, "%s", label) != 1) error_exit("Invalid label syntax");
            if (!is_valid_label_name(label)) error_exit("Invalid label name");
            uint64_t addr = in_code ? code_addr : data_addr;
            if (insert_label(t, label, addr) == 1) error_exit("duplicate label");
            continue;
        }

        enforce_tab_rule_if_statement(line, ptr);

        if (!in_code) {
            data_addr += 8;
            continue;
        }

        char mnem[64];
        sscanf(ptr, "%s", mnem);
        int op = get_opcode(mnem);

        if (op == MACRO_LD) code_addr += 48;
        else if (op == MACRO_PUSH || op == MACRO_POP) code_addr += 8;
        else code_addr += 4;
    }

    rewind(in);
    FILE *out = fopen(interfile, "w");
    if (!out) error_exit("Cannot open intermediate file");

    // addr = 0x1000;
    code_addr = header.code_seg_begin;
    data_addr = header.data_seg_begin;

    in_code = true;
    int last_section = -1;

    while (fgets(line, sizeof(line), in)) {
        enforce_leading_space_rule(line);

        char clean[MAX_LINE];
        strcpy(clean, line);
        trim_line(clean);

        if (!line_has_non_ws(clean)) continue;

        char *ptr = clean;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') continue;

        if (strncmp(ptr, ".code", 5) == 0) {
            in_code = true;
            if (last_section != 1) {
                fprintf(out, ".code\n");
                last_section = 1;
            }
            continue;
        }
        if (strncmp(ptr, ".data", 5) == 0) {
            in_code = false;
            if (last_section != 0) {
                fprintf(out, ".data\n");
                last_section = 0;
            }
            continue;
        }

        if (*ptr == ':') continue;

        enforce_tab_rule_if_statement(line, ptr);

        int cur = in_code ? 1 : 0;
        if (cur != last_section) {
            fprintf(out, in_code ? ".code\n" : ".data\n");
            last_section = cur;
        }

        if (!in_code) {
            uint64_t uval;
            if (resolve_u64_decimal(ptr, t, &uval) != 0) error_exit("Invalid data value");
            fprintf(out, "\t%llu\n", (unsigned long long)uval);
            data_addr += 8;
            continue;
        }

        char mnem[64], args[4][64];
        int arg_count = 0;

        char *token = strtok(ptr, " ,\t\n");
        if (!token) continue;
        strcpy(mnem, token);

        while ((token = strtok(NULL, " ,\t\n"))) {
            if (arg_count >= 4) {
                error_exit("Too many operants");
            }
            strcpy(args[arg_count++], token);
        }

        int op = get_opcode(mnem);
        if (op == OP_UNKNOWN) error_exit("Unknown instruction");



        if (op == MACRO_CLR) {
            if (arg_count != 1) error_exit("clr takes 1 arg");
            if (parse_register(args[0]) < 0) error_exit("clr requires a register");
            fprintf(out, "\txor %s, %s, %s\n", args[0], args[0], args[0]);
            code_addr += 4;
        }
        else if (op == MACRO_POP) {
            if (arg_count != 1) error_exit("pop takes 1 arg");
            if (parse_register(args[0]) < 0) error_exit("pop requires a register");
            fprintf(out, "\tmov %s, (r31)(0)\n", args[0]);
            fprintf(out, "\taddi r31, 8\n");
            code_addr += 8;
        }
        else if (op == MACRO_PUSH) {
            if (arg_count != 1) error_exit("push takes 1 arg");
            if (parse_register(args[0]) < 0) error_exit("push requires a register");
            fprintf(out, "\tmov (r31)(-8), %s\n", args[0]);
            fprintf(out, "\tsubi r31, 8\n");
            code_addr += 8;
        }
        else if (op == MACRO_HALT) {
            if (arg_count != 0) error_exit("halt takes 0 args");
            fprintf(out, "\tpriv r0, r0, r0, 0\n");
            code_addr += 4;
        }
        else if (op == MACRO_IN) {
            if (arg_count != 2) error_exit("in takes 2 args");
            if (parse_register(args[0]) < 0) error_exit("in requires a register");
            if (parse_register(args[1]) < 0) error_exit("in requires a register");
            fprintf(out, "\tpriv %s, %s, r0, 3\n", args[0], args[1]);
            code_addr += 4;
        }
        else if (op == MACRO_OUT) {
            if (arg_count != 2) error_exit("out takes 2 args");
            if (parse_register(args[0]) < 0) error_exit("out requires a register");
            if (parse_register(args[1]) < 0) error_exit("out requires a register");
            fprintf(out, "\tpriv %s, %s, r0, 4\n", args[0], args[1]);
            code_addr += 4;
        }
        else if (op == MACRO_LD) {
            if (arg_count != 2) error_exit("ld takes 2 args");

            uint64_t L;
            if (resolve_u64_decimal(args[1], t, &L) != 0) error_exit("Invalid literal/label in ld");

            const char *rd = args[0];
            if (parse_register(rd) < 0) error_exit("ld requires a register");

            fprintf(out, "\txor %s, %s, %s\n", rd, rd, rd);
            fprintf(out, "\taddi %s, %lu\n", rd, (unsigned long)((L >> 52) & 0xFFF));

            fprintf(out, "\tshftli %s, 12\n", rd);
            fprintf(out, "\taddi %s, %lu\n", rd, (unsigned long)((L >> 40) & 0xFFF));

            fprintf(out, "\tshftli %s, 12\n", rd);
            fprintf(out, "\taddi %s, %lu\n", rd, (unsigned long)((L >> 28) & 0xFFF));

            fprintf(out, "\tshftli %s, 12\n", rd);
            fprintf(out, "\taddi %s, %lu\n", rd, (unsigned long)((L >> 16) & 0xFFF));

            fprintf(out, "\tshftli %s, 12\n", rd);
            fprintf(out, "\taddi %s, %lu\n", rd, (unsigned long)((L >> 4) & 0xFFF));

            fprintf(out, "\tshftli %s, 4\n", rd);
            fprintf(out, "\taddi %s, %lu\n", rd, (unsigned long)(L & 0xF));
            code_addr += 48;
        }
        else {
            if (op == OP_BRR_L) {
                if (arg_count != 1) error_exit("brr requires 1 arg");
                if (args[0][0] == 'r') {
                    if (parse_register(args[0]) < 0) error_exit("brr r requires a register");
                    fprintf(out, "\tbrr %s\n", args[0]);
                }
                else {
                    int64_t L_inst = 0;
                    if (args[0][0] == ':') {
                        int64_t target = lookup_label(t, args[0] + 1);
                        if (target == -1) error_exit("Label not found");
                        // Distance in bytes, then divide by 4 to get instruction count L
                        L_inst = (target - (code_addr + 4)) / 4;
                    } else {
                        if (parse_int64_strict(args[0], &L_inst) != 0) error_exit("Invalid brr literal");
                    }
                    check_bounds_signed(L_inst, 12, "Branch offset too large for 12 bits");
                    fprintf(out, "\tbrr %lld\n", (long long)L_inst);
                }
            } 
            else {
                fprintf(out, "\t%s", mnem);
                for (int i = 0; i < arg_count; i++) {
                    if (strchr(args[i], '(')) { // Memory Operand
                        int base; int64_t disp;
                        if (parse_mem_operand(args[i], &base, &disp, t) != 0) error_exit("Bad mem op");
                        fprintf(out, " (r%d)(%lld)", base, (long long)disp);
                    } else if (args[i][0] == ':' || isdigit((unsigned char)args[i][0]) || args[i][0] == '-') {
                        int64_t val;
                        if (resolve_value(args[i], t, &val) != 0) error_exit("Invalid val");
                        if (op == OP_SHFTRI || op == OP_SHFTLI) {    
                            if (val > 4095 || val < 0) {
                                error_exit("Shift amount out of range");
                            }
                        }
                        fprintf(out, " %lld", (long long)val);
                    } else { // Register
                        fprintf(out, " %s", args[i]);
                    }
                    if (i < arg_count - 1) fprintf(out, ",");
                }
                fprintf(out, "\n");
            }
            code_addr += 4;
        }
    }

    header.code_seg_size = code_addr - header.code_seg_begin;
    header.data_seg_size = data_addr - header.data_seg_begin;

    fclose(in);
    fclose(out);

    return header;
}

static int parse_mem_operand(const char *s, int *base_reg, int64_t *lit, SymbolTable *t) {
    if (!s || s[0] != '(') return 1;

    const char *p = s + 1;
    if (*p != 'r') return 1;

    char regbuf[16];
    int ri = 0;
    while (*p && *p != ')' && ri < (int)sizeof(regbuf)-1) regbuf[ri++] = *p++;
    regbuf[ri] = '\0';
    if (*p != ')') return 1;
    int r = parse_register(regbuf);
    if (r < 0) return 1;

    p++;
    if (*p != '(') return 1;
    p++;

    char valbuf[128];
    int vi = 0;
    while (*p && *p != ')' && vi < (int)sizeof(valbuf)-1) valbuf[vi++] = *p++;
    valbuf[vi] = '\0';
    if (*p != ')') return 1;
    p++;

    // if (t && valbuf[0] == ':') return 1;
    if (*p != '\0') return 1;

    int64_t v;
    if (resolve_value(valbuf, t, &v) != 0) return 1;

    *base_reg = r;
    *lit = v;
    return 0;
}

void pass_two(const char *interfile, const char *outfile, SymbolTable *t, struct tinker_file_header header) {
    FILE *in = fopen(interfile, "r");
    FILE *out = fopen(outfile, "wb");
    if (!in || !out) error_exit("File open error in Pass 2");

    fwrite(&header, sizeof(struct tinker_file_header), 1, out);

    char line[MAX_LINE];
    bool in_code = true;
    // uint64_t addr = 0x1000;

    uint64_t code_file_offset = sizeof(struct tinker_file_header);
    uint64_t data_file_offset = code_file_offset + header.code_seg_size;

    while (fgets(line, sizeof(line), in)) {
        char clean[MAX_LINE];
        strcpy(clean, line);
        trim_line(clean);

        if (!line_has_non_ws(clean)) continue;

        char *ptr = clean;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') continue;

        if (strncmp(ptr, ".code", 5) == 0) { in_code = true; continue; }
        if (strncmp(ptr, ".data", 5) == 0) { in_code = false; continue; }

        if (line[0] != '\t') error_exit("Intermediate file invalid: statement missing leading tab");

        if (!in_code) {
            char *end = NULL;
            errno = 0;
            unsigned long long sv = strtoull(ptr, &end, 10);
            if (errno == ERANGE) error_exit("Invalid data literal");
            if (!end || *end != '\0') error_exit("Invalid data literal");
            uint64_t bits = (uint64_t)sv;
            fseek(out, data_file_offset, SEEK_SET);
            fwrite(&bits, 8, 1, out);
            data_file_offset += 8;
            // addr += 8;
            continue;
        }

        char mnem[64], args[4][64];
        int arg_count = 0;
        char *token = strtok(ptr, " ,\t\n");
        if (!token) continue;
        strcpy(mnem, token);
        while ((token = strtok(NULL, " ,\t\n")) && arg_count < 4) strcpy(args[arg_count++], token);

        int op = get_opcode(mnem);
        uint32_t instr = 0;

        int rd = -1, rs = -1, rt = -1;
        int64_t lit = 0;

        if (op == OP_MOV_RR) {
            if (arg_count != 2) error_exit("mov requires 2 args");

            if (strchr(args[1], '(')) {
                op = OP_MOV_ML;
                rd = parse_register(args[0]);
                if (rd < 0) error_exit("Invalid rd in mov");
                if (parse_mem_operand(args[1], &rs, &lit, NULL) != 0) error_exit("Invalid mem operand");
            }
            else if (strchr(args[0], '(')) {
                op = OP_MOV_SM;
                rs = parse_register(args[1]);
                if (rs < 0) error_exit("Invalid rs in mov");
                if (parse_mem_operand(args[0], &rd, &lit, NULL) != 0) error_exit("Invalid mem operand");
            }
            else if (args[1][0] == 'r') {
                op = OP_MOV_RR;
                rd = parse_register(args[0]);
                rs = parse_register(args[1]);
                if (rd < 0 || rs < 0) error_exit("Invalid reg in mov");
            }
            else {
                op = OP_MOV_L;
                rd = parse_register(args[0]);
                if (rd < 0) error_exit("Invalid rd in mov");
                if (parse_int64_strict(args[1], &lit) != 0) error_exit("Bad mov literal");
                if (lit < 0) error_exit("mov rd, L requires unsigned");
            }
        }
        else if (op == OP_BR) {
            if (arg_count != 1) error_exit("br requires 1 arg");
            rd = parse_register(args[0]);
            if (rd < 0) error_exit("invalid rd");
        }
        else if (op == OP_BRR_L) {
            if (arg_count != 1) error_exit("brr requires 1 arg");
            if (args[0][0] == 'r') {
                op = OP_BRR_R; // brr rd format 
                rd = parse_register(args[0]);
                if (rd < 0) error_exit("brr r requires a register");
                lit = 0;
                rs = 0; rt = 0;
            } else {
                op = OP_BRR_L; // brr L format 
                int64_t L_count;
                if (parse_int64_strict(args[0], &L_count) != 0) error_exit("Bad brr literal");
                
                // Convert instruction count to byte offset 
                lit = L_count; 
                rd = 0;
                
                // Standardize to the 12-bit field shown in the manual 
                check_bounds_signed(lit, 12, "Branch offset too large for 12 bits");
            }
        }
        else if (op == OP_CALL) {
             if (arg_count != 1) error_exit("call requires 1 arg");
             rd = parse_register(args[0]);
             if (rd < 0) error_exit("invalid rd");
        }
        else if (op == OP_PRIV) {
             if (arg_count != 4) error_exit("priv requires 4 args");
             rd = parse_register(args[0]); if (rd < 0) error_exit("invalid rd");
             rs = parse_register(args[1]); if (rs < 0) error_exit("invalid rs");
             rt = parse_register(args[2]); if (rt < 0) error_exit("invalid rt");
             if (parse_int64_strict(args[3], &lit) != 0) error_exit("bad priv literal");
        }
        else if (op == OP_RET) {
            if (arg_count != 0) error_exit("return requires 0 args");
        }

        if (op == OP_UNKNOWN) {
            error_exit("unknown op");
        }
        else {
            if (op == OP_ADD || op == OP_SUB || op == OP_AND || op == OP_OR || op == OP_XOR ||
                op == OP_SHFTR || op == OP_SHFTL || op == OP_ADDF || op == OP_SUBF ||
                op == OP_MULF || op == OP_DIVF || op == OP_MUL || op == OP_DIV ||
                op == OP_BRGT || op == OP_BRNZ || op == OP_NOT) {

                if (op == OP_BRNZ || op == OP_NOT) {
                    if (arg_count != 2) error_exit("not/brnz requires 2 args");
                } else {
                    if (arg_count != 3) error_exit("r-type requires 3 args");
                }

                rd = parse_register(args[0]); if (rd < 0) error_exit("invalid rd");
                rs = parse_register(args[1]); if (rs < 0) error_exit("invalid rs");
                if (op != OP_BRNZ && op != OP_NOT) {
                    rt = parse_register(args[2]);
                    if (rt < 0) error_exit("invalid rt");
                }
            }
            else if (op == OP_ADDI || op == OP_SUBI || op == OP_SHFTRI || op == OP_SHFTLI) {
                if (arg_count != 2) error_exit("i-type requires 2 args");
                rd = parse_register(args[0]);
                rs = 0; 
                rt = 0;
                if (rd < 0) error_exit("invalid rd");
                if (resolve_value(args[1], t, &lit) != 0) {
                    error_exit("Invalid literal or label in i-type");
                }

                if (lit < 0) error_exit("Unsigned literal required");
                
                // Bounds check
                if ((op == OP_SHFTRI || op == OP_SHFTLI) && (lit > 4095)) {
                    error_exit("Shift amount out of range");
                }
            }
        }

        if (rd == -1) rd = 0;
        if (rs == -1) rs = 0;
        if (rt == -1) rt = 0;

        instr |= (op & 0x1F) << 27;
        instr |= (rd & 0x1F) << 22;
        instr |= (rs & 0x1F) << 17;
        instr |= (rt & 0x1F) << 12;

        if (op == OP_ADDI || op == OP_SUBI || op == OP_SHFTRI || op == OP_SHFTLI || 
            op == OP_MOV_L || op == OP_PRIV || op == OP_BRR_L || 
            op == OP_MOV_ML || op == OP_MOV_SM) {

            // Determine if the specific instruction expects a signed or unsigned 12-bit value
            if (op == OP_BRR_L || op == OP_MOV_ML || op == OP_MOV_SM) {
                // brr L and memory offsets allow negative values 
                check_bounds_signed(lit, 12, "Literal exceeds 12-bit signed range");
            } else {
                // addi, subi, shifts, and mov L use unsigned literals 
                check_bounds_unsigned(lit, 12, "Literal exceeds 12-bit unsigned range");
            }

            // Standardize packing: Literal always goes into the bottom 12 bits
            instr |= ((uint32_t)lit & 0xFFF); 
        }

        fseek(out, code_file_offset, SEEK_SET);
        fwrite(&instr, 4, 1, out);
        code_file_offset += 4;
        // addr += 4;
    }

    fclose(in);
    fclose(out);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.tk> <output.tko>\n", argv[0]);
        return 1;
    }

    char inter_tmp[512];
    char out_tmp[512];

    snprintf(inter_tmp, sizeof(inter_tmp), "%s.tmp", argv[1]);
    snprintf(out_tmp,   sizeof(out_tmp),   "%s.tmp", argv[2]);

    tmp_inter = inter_tmp;
    tmp_out   = out_tmp;

    SymbolTable *table = create_table();

    struct tinker_file_header header = pass_one(argv[1], inter_tmp, table);
    pass_two(inter_tmp, out_tmp, table, header);

    if (rename(inter_tmp, "intermediate.tk") != 0) error_exit("rename intermediate failed");
    if (rename(out_tmp, argv[2]) != 0) error_exit("rename output failed");

    tmp_inter = NULL;
    tmp_out = NULL;

    return 0;
}
