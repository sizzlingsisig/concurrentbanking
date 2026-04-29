#!/bin/bash
# Test 5: Insufficient Funds - Transaction should abort
echo "=== Test 5: Insufficient Funds ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --deadlock=prevention --tick-ms=10
echo ""