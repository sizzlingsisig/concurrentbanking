#!/bin/bash
# Test 6: Buffer Pool Saturation - T6 should block until slot freed
echo "=== Test 6: Buffer Pool Saturation ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --deadlock=prevention --tick-ms=10
echo ""