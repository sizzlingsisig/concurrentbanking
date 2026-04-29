#!/bin/bash

# ============================================================================
# Concurrent Banking System - Test Runner
# ============================================================================
# This script runs all test cases for the concurrent banking system
# Usage: ./test_all.sh [--fast] [--verbose]
# ---
# Options:
#   --fast    Run with --tick-ms=10 for faster execution
#   --verbose Enable verbose logging
# ============================================================================

set -e  # Exit on first failure

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Paths
BINARY="./bankdb"
ACCOUNTS="tests/accounts.txt"
TRACE_DIR="tests"

# Options
VERBOSE=""
TICK_MS="100"
FAST=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose|-v)
            VERBOSE="--verbose"
            shift
            ;;
        --fast|-f)
            FAST="--tick-ms=10"
            TICK_MS="10"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo "=============================================================="
echo "  Concurrent Banking System - Test Suite"
echo "=============================================================="
echo "Tick interval: ${TICK_MS}ms"
echo "Verbose: ${VERBOSE:-no}"
echo "=============================================================="
echo ""

# Function to run a test
run_test() {
    local test_name="$1"
    local trace_file="$2"
    local deadlock_strat="$3"
    local expected="$4"
    
    echo "--------------------------------------------------------------"
    echo -e "${YELLOW}Running: ${test_name}${NC}"
    echo "Trace: ${trace_file}"
    echo "Expected: ${expected}"
    echo "--------------------------------------------------------------"
    
    if ${BINARY} --accounts=${ACCOUNTS} --trace=${TRACE_DIR}/${trace_file} --deadlock=${deadlock_strat} ${FAST} ${VERBOSE} 2>&1; then
        echo -e "${GREEN}✓ PASSED${NC}"
    else
        echo -e "${RED}✗ FAILED${NC}"
        exit 1
    fi
    echo ""
}

# Build first
echo "Building banking system..."
make clean > /dev/null 2>&1
make
echo ""

# ============================================================================
# Test 1: No Conflicts (Simple Sequential)
# ============================================================================
# Tests basic sequential operations
run_test "Test 1: No Conflicts" "trace_simple.txt" "prevention" "All operations succeed"

# ============================================================================
# Test 2: Concurrent Readers
# ============================================================================
# Tests reader-writer lock allows concurrent reads
run_test "Test 2: Concurrent Readers" "trace_readers.txt" "prevention" "4 concurrent reads complete"

# ============================================================================
# Test 3: Deadlock Prevention
# ============================================================================
# Tests deadlock prevention via lock ordering
run_test "Test 3: Deadlock Prevention" "trace_deadlock.txt" "prevention" "Both transfers succeed"

# ============================================================================
# Test 4: Deadlock Detection
# ============================================================================
# Tests deadlock detection and abort
run_test "Test 4: Deadlock Detection" "trace_deadlock.txt" "detection" "One aborts, one commits"

# ============================================================================
# Test 5: Insufficient Funds
# ============================================================================
# Tests transaction abort on insufficient funds
run_test "Test 5: Insufficient Funds" "trace_abort.txt" "prevention" "Transaction aborts"

# ============================================================================
# Test 6: Buffer Pool Saturation
# ============================================================================
# Tests bounded buffer pool blocks when full
run_test "Test 6: Buffer Pool Saturation" "trace_buffer.txt" "prevention" "T6 blocks until slot freed"

# ============================================================================
# Summary
# ============================================================================
echo "=============================================================="
echo -e "${GREEN}All Tests Passed!${NC}"
echo "=============================================================="
echo ""
echo "Test Results Summary:"
echo "  ✓ Test 1: No Conflicts"
echo "  ✓ Test 2: Concurrent Readers"
echo "  ✓ Test 3: Deadlock Prevention"
echo "  ✓ Test 4: Deadlock Detection"
echo "  ✓ Test 5: Insufficient Funds"
echo "  ✓ Test 6: Buffer Pool Saturation"
echo ""
echo "To run with ThreadSanitizer: make debug && ./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt"