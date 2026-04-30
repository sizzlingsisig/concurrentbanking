#!/bin/bash
# Test 11: ThreadSanitizer - Must produce ZERO warnings
# Run this separately - it compiles with -fsanitize=thread

echo "=== Test 11: ThreadSanitizer ==="
echo ""

# Clean and compile with ThreadSanitizer
echo "Compiling with ThreadSanitizer..."
make clean > /dev/null 2>&1
make debug

echo ""
echo "Running with ThreadSanitizer enabled..."
echo ""

# Run the test
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=10 2>&1

# Check output for ThreadSanitizer warnings
echo ""
echo "=== ThreadSanitizer Check ==="
RESULT=$(./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention --tick-ms=10 2>&1 | grep -c "ThreadSanitizer warnings: 0")

if [ "$RESULT" = "1" ]; then
    echo "✓ PASSED: ThreadSanitizer produced ZERO warnings"
else
    echo "✗ FAILED: ThreadSanitizer detected issues"
    exit 1
fi
echo ""