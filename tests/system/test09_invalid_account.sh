#!/bin/bash
# Test 9: Invalid Account - Account ID outside valid range
echo "=== Test 9: Invalid Account ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_invalid_account.txt --deadlock=prevention --tick-ms=10
echo ""