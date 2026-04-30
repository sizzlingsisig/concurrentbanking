#!/bin/bash
# Test 3: Deadlock Prevention - Both transfers succeed with lock ordering
echo "=== Test 3: Deadlock Prevention ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention --tick-ms=10 --verbose
echo ""