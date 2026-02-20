ASM="./hw5-asm"
TMP_TK="comprehensive.tk"
TMP_TKO="comprehensive.tko"
INTER="intermediate.tk"

PASS=0
FAIL=0

# Valid
test_valid() {
    local name="$1"
    local input_code="$2"
    local expected_inter="$3"

    printf "%b\n" "$input_code" > $TMP_TK
    
    $ASM $TMP_TK $TMP_TKO > /dev/null 2>&1

    if [ ! -f "$INTER" ]; then
        echo "FAIL: $name (intermediate.tk not generated)"
        ((FAIL++))
        return
    fi

    # Read intermediate, strip sections, trim whitespace, join lines with ';'
    local actual=$(grep -vE '^\.code|^\.data' $INTER | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' | paste -sd ';' -)
    local expected=$(echo "$expected_inter" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' | paste -sd ';' -)

    if [ "$actual" == "$expected" ]; then
        echo "PASS: $name"
        ((PASS++))
    else
        echo "FAIL: $name"
        echo "   Input   : $(echo "$input_code" | paste -sd ';' -)"
        echo "   Expected: $expected"
        echo "   Got     : $actual"
        ((FAIL++))
    fi
    rm -f $TMP_TKO $INTER
}

# Invalid
test_error() {
    local name="$1"
    local input_code="$2"
    local expected_err="$3"

    printf "%b\n" "$input_code" > $TMP_TK
    
    local output=$($ASM $TMP_TK $TMP_TKO 2>&1)
    
    if [[ "$output" == *"$expected_err"* ]]; then
        echo "PASS [ERR CATCH]: $name"
        ((PASS++))
    else
        echo "FAIL [ERR CATCH]: $name"
        echo "   Expected Error: $expected_err"
        echo "   Got Output    : $output"
        ((FAIL++))
    fi
    rm -f $TMP_TKO $INTER
}

echo "Tinker Assembler Tests"

# R-Type 3 Args
echo -e "\nCategory: R-Type 3 Args"
test_valid "ADD"   ".code\n\tadd r1, r2, r3"   "add r1, r2, r3"
test_valid "SUB"   ".code\n\tsub r31, r0, r15" "sub r31, r0, r15"
test_valid "MUL"   ".code\n\tmul r4, r5, r6"   "mul r4, r5, r6"
test_valid "DIV"   ".code\n\tdiv r7, r8, r9"   "div r7, r8, r9"
test_valid "AND"   ".code\n\tand r10, r11, r12" "and r10, r11, r12"
test_valid "OR"    ".code\n\tor r13, r14, r15" "or r13, r14, r15"
test_valid "XOR"   ".code\n\txor r16, r17, r18" "xor r16, r17, r18"
test_valid "SHFTR" ".code\n\tshftr r1, r2, r3" "shftr r1, r2, r3"
test_valid "SHFTL" ".code\n\tshftl r4, r5, r6" "shftl r4, r5, r6"
test_valid "ADDF"  ".code\n\taddf r1, r2, r3"  "addf r1, r2, r3"
test_valid "SUBF"  ".code\n\tsubf r1, r2, r3"  "subf r1, r2, r3"
test_valid "MULF"  ".code\n\tmulf r1, r2, r3"  "mulf r1, r2, r3"
test_valid "DIVF"  ".code\n\tdivf r1, r2, r3"  "divf r1, r2, r3"
test_valid "BRGT"  ".code\n\tbrgt r1, r2, r3"  "brgt r1, r2, r3"

# R-Type 2 Args
echo -e "\nR-Type 2 Args"
test_valid "NOT"   ".code\n\tnot r1, r2"   "not r1, r2"
test_valid "BRNZ"  ".code\n\tbrnz r1, r2"  "brnz r1, r2"

# I type
echo -e "\nI-Type"
test_valid "ADDI"    ".code\n\taddi r1, 4095"   "addi r1, 4095"
test_valid "SUBI"    ".code\n\tsubi r31, 0"    "subi r31, 0"
test_valid "SHFTRI"  ".code\n\tshftri r1, 4095" "shftri r1, 4095"
test_valid "SHFTLI"  ".code\n\tshftli r1, 12"   "shftli r1, 12"

# Data movement and memory
echo -e "\nData Movement"
test_valid "MOV_RR"  ".code\n\tmov r1, r2"          "mov r1, r2"
test_valid "MOV_L"   ".code\n\tmov r1, 4095"        "mov r1, 4095"
test_valid "MOV_ML"  ".code\n\tmov r1, (r2)(2047)"  "mov r1, (r2)(2047)"
test_valid "MOV_SM"  ".code\n\tmov (r1)(-2048), r2" "mov (r1)(-2048), r2"

# Control + Priv
echo -e "\nControl + Priv"
test_valid "BR"         ".code\n\tbr r1"          "br r1"
test_valid "BRR_REG"    ".code\n\tbrr r1"         "brr r1"
test_valid "BRR_LIT"    ".code\n\tbrr 2047"       "brr 2047"
test_valid "BRR_NEG"    ".code\n\tbrr -2048"      "brr -2048"
test_valid "CALL"       ".code\n\tcall r1"        "call r1"
test_valid "RETURN"     ".code\n\treturn"         "return"
test_valid "RET"        ".code\n\tret"            "ret"
test_valid "PRIV"       ".code\n\tpriv r1, r2, r3, 4095" "priv r1, r2, r3, 4095"

# Macro Expansions
echo -e "\nMacros Expansions"
test_valid "CLR"  ".code\n\tclr r1"       "xor r1, r1, r1"
test_valid "HALT" ".code\n\thalt"         "priv r0, r0, r0, 0"
test_valid "IN"   ".code\n\tin r1, r2"    "priv r1, r2, r0, 3"
test_valid "OUT"  ".code\n\tout r1, r2"   "priv r1, r2, r0, 4"
test_valid "PUSH" ".code\n\tpush r1"      "mov (r31)(-8), r1;subi r31, 8"
test_valid "POP"  ".code\n\tpop r1"       "mov r1, (r31)(0);addi r31, 8"

# LD Macro: Breaking down 64-bit maximum (18446744073709551615 = all 1s)
# Each 12 bit chunk is 4095, last 4 bits is 15.
test_valid "LD_MAX" ".code\n\tld r1, 18446744073709551615" \
"xor r1, r1, r1;addi r1, 4095;shftli r1, 12;addi r1, 4095;shftli r1, 12;addi r1, 4095;shftli r1, 12;addi r1, 4095;shftli r1, 12;addi r1, 4095;shftli r1, 4;addi r1, 15"

# PC relative
echo -e "\nPC relative"
# Forward: PC=0x2000, target=0x2004. (0x2004 - 0x2004)/4 = 0
test_valid "BRR_FWD_LBL" ".code\n\tbrr :lbl\n:lbl" "brr 0"

# Data
echo -e "\nData"
test_valid "DATA_SEGMENT" ".data\n\t12345\n\t67890\n.code\n\thalt" "12345;67890;priv r0, r0, r0, 0"

# Bounds + Syntax
echo -e "\nBounds + Syntax"
test_error "Invalid Register"   ".code\n\tadd r32, r1, r2"      "invalid rd"
test_error "Too Many Operands"  ".code\n\tadd r1, r2, r3, r4"   "r-type requires 3 args"
test_error "Missing R-Args"     ".code\n\tadd r1, r2"           "r-type requires 3 args"
test_error "Unsigned I-Type"    ".code\n\taddi r1, -1"          "Unsigned literal required"
test_error "Shift Out of Range" ".code\n\tshftli r1, 4096"      "Shift amount out of range"
test_error "Mem Offset Range"   ".code\n\tmov (r1)(2048), r2"   "Literal exceeds 12-bit signed range"
test_error "Label Not Alone"    ".code\n:lbl \tadd r1, r2, r3"  "Label must be alone on its line"

echo "----"
echo "Tests Completed: $((PASS + FAIL))"
echo "Passed: $PASS"
echo "Failed: $FAIL"

# Cleanup
rm -f $TMP_TK $TMP_TKO $INTER