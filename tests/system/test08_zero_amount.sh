#!/bin/bash
# Test 8: Zero Amount - Deposit/withdraw 0 should be rejected
echo "=== Test 8: Zero Amount ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_zero_amount.txt --deadlock=prevention --tick-ms=10
echo ""