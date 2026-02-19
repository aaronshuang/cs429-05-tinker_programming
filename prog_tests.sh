ASM="./hw5-asm"
SIM="./hw5-sim"

FIBO_FILE="fibonacci.tk"
BSEARCH_FILE="binary_search.tk"
MATMUL_FILE="matrix_multiplication.tk"

TMP_TKO="app_test.tko"

PASS=0
FAIL=0

run_app_test() {
    local name="$1"
    local source_file="$2"
    local input_data="$3"
    local expected_output="$4"

    if [ ! -f "$source_file" ]; then
        echo "SKIP: $name ($source_file not found)"
        return
    fi

    $ASM "$source_file" "$TMP_TKO" > /dev/null 2>&1
    
    if [ ! -f "$TMP_TKO" ]; then
        echo "FAIL: $name (Assembler failed)"
        ((FAIL++))
        return
    fi

    local actual_output
    actual_output=$(echo "$input_data" | $SIM "$TMP_TKO" 2>&1 | xargs)
    
    local expected_norm
    expected_norm=$(echo "$expected_output" | xargs)

    if [ "$actual_output" == "$expected_norm" ]; then
        echo "PASS: $name"
        ((PASS++))
    else
        echo "FAIL: $name"
        echo "   Expected : $expected_norm"
        echo "   Got      : $actual_output"
        ((FAIL++))
    fi

    rm -f "$TMP_TKO"
}

echo "Starting Application Tests"

## Fibonacci
run_app_test "Fibo N=1"  "$FIBO_FILE" "1"  "0"
run_app_test "Fibo N=2"  "$FIBO_FILE" "2"  "1"
run_app_test "Fibo N=3"  "$FIBO_FILE" "3"  "1"
run_app_test "Fibo N=10" "$FIBO_FILE" "10" "34"

## Binary Search
run_app_test "BS Found Mid" "$BSEARCH_FILE" "5 10 20 30 40 50 30" "found"
run_app_test "BS Found End" "$BSEARCH_FILE" "5 10 20 30 40 50 50" "found"
run_app_test "BS Not Found" "$BSEARCH_FILE" "5 10 20 30 40 50 99" "not found"

## Matrix Multiplication
run_app_test "3x3 Identity" "$MATMUL_FILE" \
    "3 4607182418800017408 4624633867356078080 4613937818241073152 4616189618054758400 13856381001095905280 4617315517961592832 4619567317775278080 4617315517961592832 4621256167635542016 4607182418800017408 0 0 0 4607182418800017408 0 0 0 4607182418800017408" \
    "4607182418800017408 4624633867356078080 4613937818241073152 4616189618054758400 13856381001095905280 4617315517961592832 4619567317775278080 4617315517961592832 4621256167635542016"

echo "Results"
echo "Total: $((PASS + FAIL))"
echo "Passed: $PASS"
echo "Failed: $FAIL"

exit $FAIL