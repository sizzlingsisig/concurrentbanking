#!/bin/bash
# Test 2: Concurrent Readers - Multiple readers should complete concurrently
echo "=== Test 2: Concurrent Readers ==="
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=10
echo ""