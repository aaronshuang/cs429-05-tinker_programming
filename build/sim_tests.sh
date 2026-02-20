# Configuration
ASM="./hw5-asm"
SIM="./hw5-sim"
TMP_TK="tester_tmp.tk"
TMP_TKO="tester_tmp.tko"

# Test counter
PASS=0
FAIL=0

run_test() {
    local name=$1
    local code=$2
    local expected=$3
    local input=$4

    printf ".code\n\tld r29, 1\n\tld r28, 3\n$code\n\thalt\n" > $TMP_TK
    
    # Run assembler
    $ASM $TMP_TK $TMP_TKO
    
    # Check if assembler actually created the file
    if [ ! -f "$TMP_TKO" ]; then
        echo "FAIL: $name (Assembler failed to create .tko file. Check your assembler logic!)"
        ((FAIL++))
        return
    fi

    local output
    if [ -n "$input" ]; then
        output=$(echo "$input" | $SIM $TMP_TKO 2>&1 | xargs)
    else
        output=$($SIM $TMP_TKO 2>&1 | xargs)
    fi

    if [ "$output" == "$expected" ]; then
        echo "PASS: $name"
        ((PASS++))
    else
        echo "FAIL: $name (Expected '$expected', got '$output')"
        ((FAIL++))
    fi
    
    # Clean up
    rm -f $TMP_TKO
}

echo "Tinker Simulator Tests"

# Integer Arithmetic
run_test "ADD" "\taddi r1, 20\n\taddi r2, 22\n\tadd r3, r1, r2\n\tout r29, r3" "42"
run_test "SUB" "\taddi r1, 50\n\taddi r2, 10\n\tsub r3, r1, r2\n\tout r29, r3" "40"
run_test "MUL" "\taddi r1, 6\n\taddi r2, 7\n\tmul r3, r1, r2\n\tout r29, r3" "42"
run_test "DIV" "\taddi r1, 100\n\taddi r2, 4\n\tdiv r3, r1, r2\n\tout r29, r3" "25"

# Logical Operations
run_test "AND" "\taddi r1, 15\n\taddi r2, 7\n\tand r3, r1, r2\n\tout r29, r3" "7"
run_test "OR"  "\taddi r1, 8\n\taddi r2, 4\n\tor r3, r1, r2\n\tout r29, r3" "12"
run_test "XOR" "\taddi r1, 10\n\taddi r2, 10\n\txor r3, r1, r2\n\tout r29, r3" "0"
run_test "NOT" "\tclr r1\n\tnot r2, r1\n\tout r29, r2" "18446744073709551615"

# Shifts
run_test "SHFTLI" "\taddi r1, 1\n\tshftli r1, 10\n\tout r29, r1" "1024"
run_test "SHFTRI" "\taddi r1, 1024\n\tshftri r1, 5\n\tout r29, r1" "32"

# Floating Point
run_test "ADDF" "\tld r1, 4607182418800017408\n\tld r2, 4607182418800017408\n\taddf r3, r1, r2\n\tout r29, r3" "4611686018427387904"
run_test "SUBF" "\tld r1, 4616189618054758400\n\tld r2, 4607182418800017408\n\tsubf r3, r1, r2\n\tout r29, r3" "4613937818241073152"

# Data Movement & Memory
run_test "LOAD_STORE" "\tld r1, 65536\n\taddi r2, 1234\n\tmov (r1)(0), r2\n\tmov r3, (r1)(0)\n\tout r29, r3" "1234"
run_test "MOV_L"      "\taddi r1, 0\n\taddi r1, 15\n\tout r29, r1" "15"

# Control Flow
run_test "BRNZ_TAKEN" "\taddi r1, 1\n\tld r2, :target\n\tbrnz r2, r1\n\taddi r3, 5\n:target\n\taddi r3, 10\n\tout r29, r3" "10"
run_test "BRGT_TAKEN" "\taddi r1, 10\n\taddi r2, 5\n\tld r3, :target\n\tbrgt r3, r1, r2\n\taddi r4, 1\n:target\n\taddi r4, 99\n\tout r29, r4" "99"

# Call and Return
run_test "CALL_RET" "
	ld r1, :sub
	call r1
	addi r2, 100
	out r29, r2
	halt
:sub
	addi r2, 50
	ret" "150"

# I/O Port
run_test "PORT_3_ASCII" "\tld r1, 72\n\tout r28, r1" "H"
run_test "PORT_0_IN"    "\tclr r5\n\tin r1, r5\n\tout r29, r1" "500" "500"

echo "----"
echo "Tests Completed: $((PASS + FAIL))"
echo "Passed: $PASS"
echo "Failed: $FAIL"

# Cleanup
rm -f $TMP_TK

if [ $FAIL -eq 0 ]; then
    exit 0
else
    exit 1
fi