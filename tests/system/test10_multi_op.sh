#!/bin/bash
# Test 10: Multi-Op - Multiple operations on same account serialize correctly
echo "=== Test 10: Multi-Op Single TX ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_multi_op.txt --deadlock=prevention --tick-ms=10
echo ""