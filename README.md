# Concurrent Banking System (bankdb)

Multi-threaded banking system using POSIX threads and synchronization primitives.

## Authors

- Christian Joseph Hernia
- Julo Bretana

## Compilation

```bash
# Standard build
make all

# Debug build with ThreadSanitizer
make debug

# Clean build artifacts
make clean
```

## Usage

```bash
./bankdb --accounts=FILE --trace=FILE [options]
```

### Required Options

| Option | Description |
|--------|-------------|
| `--accounts=FILE` | Initial account balances file |
| `--trace=FILE` | Transaction workload file |

### Optional Options

| Option | Description | Default |
|--------|-------------|---------|
| `--deadlock=STRATEGY` | Deadlock strategy (prevention/detection) | prevention |
| `--tick-ms=N` | Milliseconds per tick | 100 |
| `--verbose` | Print detailed logs | off |

## Examples

```bash
# Simple test
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt

# Test with deadlock prevention
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention

# Test with deadlock detection
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=detection

# Run all tests
make test
```

## Test Cases

| Test File | Description |
|-----------|-------------|
| `trace_simple.txt` | Basic operations - no conflicts |
| `trace_readers.txt` | Concurrent balance queries |
| `trace_deadlock.txt` | Two transfers in opposite directions |
| `trace_abort.txt` | Insufficient funds scenario |
| `trace_buffer.txt` | Buffer pool saturation |

## Output Format

The program outputs:
- Transaction log with timing details
- Simulation summary (committed/aborted, throughput)
- Buffer pool statistics
- Balance conservation check

## Features Implemented

- [x] Multi-threaded transaction execution
- [x] Reader-writer locks for account access
- [x] Deadlock prevention via lock ordering
- [x] Bounded buffer pool with semaphores
- [x] Timer thread for time simulation
- [x] Condition variables for thread synchronization
- [x] Transaction metrics collection
- [x] Balance conservation verification
- [x] ThreadSanitizer validation

## Known Limitations

- Deadlock detection implementation is experimental

## Screenshots
### ThreadSanitizer producing zero warnings on all test cases
- trace_simple
![trace_simple](screenshots/tsan-trace_simple.png)
- trace_readers
![trace_readers](screenshots/tsan-trace_readers1.png)
![trace_readers](screenshots/tsan-trace_readers2.png)
- trace_deadlock
![trace_deadlock](screenshots/tsan-trace_deadlock1.png)
![trace_deadlock](screenshots/tsan-trace_deadlock2.png)
- trace_abort
![trace_abort](screenshots/tsan-trace_abort.png)
- trace_buffer
![trace_buffer](screenshots/tsan-trace_buffer1.png)
![trace_buffer](screenshots/tsan-trace_buffer2.png)

### Deadlock handling (prevention or detection) working correctly
![deadlock prevention](screenshots/deadlock_prevention1.png)
![deadlock prevention](screenshots/deadlock_prevention2.png)

### Buffer pool blocking when full, then unblocking
![buffer pool](screenshots/buffer_pool1.png)
![buffer pool](screenshots/buffer_pool2.png)

### Balance conservation check passing
- Shown in all the screenshots.
