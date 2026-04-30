#!/bin/bash
# Test 7: Self-Transfer - Transfer to same account should abort
echo "=== Test 7: Self-Transfer ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_self_transfer.txt --deadlock=prevention --tick-ms=10 --verbose
echo ""