#!/bin/bash
# Test 12: Performance - Compare throughput with different tick intervals
# Also tests balance consistency across runs

echo "=== Test 12: Performance & Balance Consistency ==="
echo ""

# Test 1: Fast tick interval (10ms)
echo "--- Running with tick-ms=10 ---"
RESULT1=$(./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=10 2>&1 | grep "Throughput:")
echo "$RESULT1"

# Test 2: Slow tick interval (100ms)
echo ""
echo "--- Running with tick-ms=100 ---"
RESULT2=$(./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=100 2>&1 | grep "Throughput:")
echo "$RESULT2"

echo ""
echo "=== Note ==="
echo "Reader-writer locks allow concurrent reads"
echo "Performance should be similar regardless of tick interval"
echo "Key check: Balance conservation in all runs"
echo ""

# Run balance check
echo "--- Balance Conservation Check ---"
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=10 2>&1 | grep -A2 "Balance Conservation"
echo ""