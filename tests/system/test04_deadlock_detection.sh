#!/bin/bash
# Test 4: Deadlock Detection - One transaction aborts to break cycle
echo "=== Test 4: Deadlock Detection ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=detection --tick-ms=10 --verbose
echo ""