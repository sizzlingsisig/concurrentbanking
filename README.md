# CMSC 125 Lab 3 — Concurrent Banking System

A concurrent multi-threaded banking system built with POSIX threads (pthreads). Implements reader-writer locks for account access, semaphores for a bounded buffer pool, and lock ordering for deadlock prevention. Simulates time progression with a timer thread and outputs transaction logs with performance metrics.

## Group Members

- Christian Joseph Hernia
- Julo Bretana

## Compilation

```bash
make        # Normal build (optimized, -O2, -pthread)
make debug  # Debug build with ThreadSanitizer (-fsanitize=thread)
make clean  # Remove binaries and object files
make test   # Run all 5 test cases
```

## Usage

```bash
./bankdb --accounts=ACCOUNTS_FILE --trace=TRACE_FILE [options]
```

**Required Options:**

| Option | Description |
|--------|-------------|
| `--accounts=FILE` | Initial account balances file |
| `--trace=FILE` | Transaction workload trace file |

**Optional Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `--deadlock=prevention\|detection` | Deadlock handling strategy | prevention |
| `--tick-ms=N` | Milliseconds per simulation tick | 100 |
| `--verbose` | Print detailed operation logs | (disabled) |
| `--help` | Show usage information | — |

**Example:**

```bash
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention --verbose
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --tick-ms=50
```

## Implemented Features

- **Multi-threaded transaction execution** — One pthread per transaction, each running independently
- **Reader-writer locks (pthread_rwlock_t)** — Multiple concurrent reads allowed; writes get exclusive access
- **Banking operations:**
  - `DEPOSIT account_id amount` — Add money to an account
  - `WITHDRAW account_id amount` — Remove money (aborts if insufficient funds)
  - `TRANSFER from_id to_id amount` — Move money between accounts
  - `BALANCE account_id` — Read and print account balance
- **Bounded buffer pool (5 slots)** — Semaphore-based producer-consumer synchronization
- **Deadlock prevention** — Lock ordering (ascending account ID) breaks circular wait
- **Timer thread** — Simulates time progression with pthread condition variables
- **Transaction logging** — Prints ID, status, scheduled time, actual time, wait ticks
- **Performance metrics** — Throughput, average wait time, buffer pool statistics
- **Balance conservation check** — Verifies sum of all balances is preserved
- **ThreadSanitizer validated** — Zero warnings on all test cases

## Test Cases

| File | Operations | What It Tests | Expected Result |
|------|-----------|---------------|-----------------|
| `trace_simple.txt` | DEPOSIT + WITHDRAW | Basic operations, conservation | All committed, conservation holds |
| `trace_readers.txt` | 4x BALANCE | Concurrent reads via rwlock | All committed simultaneously |
| `trace_deadlock.txt` | 2x TRANSFER (opposite) | Lock ordering prevents deadlock | Both committed |
| `trace_abort.txt` | WITHDRAW too large | Insufficient funds handling | Transaction aborted |
| `trace_buffer.txt` | 6x DEPOSIT | Buffer pool saturation | T6 blocks until slot available |

Run all tests:

```bash
make test
```

## Known Limitations

- Only **deadlock prevention** is implemented (lock ordering). Deadlock detection is not supported.
- **In-memory only** — No persistent storage; all data is lost after program exit.
- **Fixed limits** — MAX_ACCOUNTS=100, MAX_TRANSACTIONS=1000, MAX_OPS_PER_TX=256.
- The `trace_buffer.txt` test produces a conservation check **FAILURE** — this is expected behavior, as 6 DEPOSIT operations on 6 different accounts add a net +6000 centavos to the total (demonstrating buffer pool saturation, not conservation).
- The timer thread and transaction threads may finish processing before any ticks are incremented in very fast test runs, resulting in `Throughput: 0.00 tx/tick` and `Wait Ticks: 0` for simple tests.

## Project Structure

```
bankdb/
├── Makefile
├── README.md
├── include/
│   ├── bank.h          # Bank, Account structs; balance operations
│   ├── transaction.h   # Transaction, Operation types
│   ├── timer.h         # Timer thread, global clock
│   ├── lock_mgr.h      # Deadlock strategy, transfer function
│   ├── buffer_pool.h   # Buffer pool with semaphores
│   └── metrics.h       # Statistics collection
├── src/
│   ├── main.c          # CLI parsing, initialization, main loop
│   ├── bank.c          # Account operations (deposit, withdraw, balance)
│   ├── transaction.c   # Transaction execution thread
│   ├── timer.c         # Timer thread implementation
│   ├── lock_mgr.c      # Transfer with lock ordering
│   ├── buffer_pool.c   # Bounded buffer with semaphores
│   └── metrics.c       # Logging and statistics
├── tests/
│   ├── accounts.txt    # Initial account balances
│   ├── trace_simple.txt     # Basic operations test
│   ├── trace_readers.txt    # Concurrent reads test
│   ├── trace_deadlock.txt   # Deadlock prevention test
│   ├── trace_abort.txt      # Insufficient funds test
│   └── trace_buffer.txt     # Buffer pool saturation test
└── docs/
    └── designNotes.md   # Design documentation and checklist
```