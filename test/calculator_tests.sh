#!/bin/bash

# Function to run a test case
run_test() {
    operation=$1
    num1=$2
    num2=$3
    expected_result=$4

    result=$(../main/simple_calc "$num1" "$operation" "$num2")
    if [[ "$result" == "Result: $expected_result" ]] || [[ "$result" == "$expected_result" ]]; then
        echo "Test passed for operation $operation with inputs $num1 and $num2. Expected: $expected_result, Got: $result"
    else
        echo "Test failed for operation $operation with inputs $num1 and $num2. Expected: $expected_result, Got: $result"
        exit 1
    fi
}

# Addition
run_test "+" 5 3 "8.000000"

# Subtraction
run_test "-" 10 4 "6.000000"

# Multiplication
run_test "*" 7 6 "42.000000"

# Division
run_test "/" 20 5 "4.000000"

# Division by zero
run_test "/" 15 0 "Error: Division by zero"

# Invalid operation
run_test "&" 9 3 "Error: Invalid operation"

echo "All tests passed."
