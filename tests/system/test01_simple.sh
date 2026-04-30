#!/bin/bash
# Test 1: No Conflicts - Sequential operations should all succeed
echo "=== Test 1: No Conflicts ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --tick-ms=10
echo ""