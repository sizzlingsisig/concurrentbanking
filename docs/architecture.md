# Concurrent Banking System - Architecture

## Overview

A multi-threaded banking system demonstrating concurrency control mechanisms including reader-writer locks, deadlock prevention, bounded buffers, and condition variables.

---

## System Components

```
+-------------------+     +-------------------+     +-------------------+
|    main.c         |---->|     Timer         |---->|   Transactions    |
| (CLI, startup)    |     | (time simulation) |     | (concurrent ops)  |
+-------------------+     +-------------------+     +-------------------+
         |                        |                        |
         v                        v                        v
+-------------------+     +-------------------+     +-------------------+
|      Bank         |<---->|   Buffer Pool     |<-----|   Lock Manager    |
| (account storage)|     | (bounded cache)   |     | (transfer logic)  |
+-------------------+     +-------------------+     +-------------------+
```

---

## Core Modules

### 1. Timer (`timer.c`, `timer.h`)

**Purpose:** Simulates time progression for transaction scheduling.

**Singleton Components:**
- `global_tick` - current time in ticks
- `tick_lock` - mutex protecting tick counter
- `tick_changed` - condition variable for tick notifications

**API:**
- `init_timer(interval_ms)` - initialize timer with tick interval
- `timer_thread(void*)` - timer thread function (runs until simulation ends)
- `wait_until_tick(target_tick)` - block until global_tick >= target
- `cleanup_timer()` - shut down timer

**Design Pattern:** Singleton - single timer thread with global state.

---

### 2. Bank (`bank.c`, `bank.h`)

**Purpose:** Manages accounts and provides account operations.

**Data Structures:**
```c
typedef struct {
    int account_id;
    int balance_centavos;      // stored as integers (not floats!)
    pthread_rwlock_t lock;     // per-account reader-writer lock
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int num_accounts;
    pthread_mutex_t bank_lock; // protects bank metadata
} Bank;
```

**API:**
- `init_bank()` - initialize all accounts with locks
- `get_balance(account_id)` - read balance (shared lock)
- `deposit(account_id, amount)` - add to balance (exclusive lock)
- `withdraw(account_id, amount)` - subtract if sufficient funds
- `get_total_balance()` - sum all account balances
- `load_accounts_from_file(filename)` - load initial balances

**Design Pattern:** Singleton - single `Bank bank` global instance.

**Concurrency:** Per-account `pthread_rwlock_t` allows multiple concurrent readers.

---

### 3. Lock Manager (`lock_mgr.c`, `lock_mgr.h`)

**Purpose:** Handles transfers between accounts with deadlock prevention.

**API:**
- `transfer(from_id, to_id, amount)` - move funds between accounts

**Deadlock Prevention Strategy:** Lock Ordering

The transfer function acquires locks in ascending account ID order:
```c
int first = min(from_id, to_id);
int second = max(from_id, to_id);

pthread_rwlock_wrlock(&accounts[first].lock);
pthread_rwlock_wrlock(&accounts[second].lock);
```

This breaks the **circular wait** condition - a necessary condition for deadlock.

**Design Pattern:** Deadlock Prevention (architectural pattern).

---

### 4. Buffer Pool (`buffer_pool.c`, `buffer_pool.h`)

**Purpose:** Bounded cache for account data - demonstrates producer-consumer pattern.

**Data Structures:**
```c
typedef struct {
    int account_id;
    void* data;
    bool in_use;
} BufferSlot;

typedef struct {
    BufferSlot slots[BUFFER_POOL_SIZE];  // 5 slots
    sem_t empty_slots;   // counts empty slots
    sem_t full_slots;   // counts loaded slots
    pthread_mutex_t pool_lock;
} BufferPool;
```

**API:**
- `init_buffer_pool()` - initialize semaphores and mutex
- `load_account(account_id)` - load account into pool (blocks if full)
- `unload_account(account_id)` - remove account from pool
- `cleanup_buffer_pool()` - destroy synchronization primitives

**Design Pattern:** Producer-Consumer (bounded buffer)

**Behavior:**
- `load_account` waits if no empty slots (sem_wait on empty_slots)
- `unload_account` signals when slot is available (sem_post on empty_slots)

---

### 5. Transaction (`transaction.c`, `transaction.h`)

**Purpose:** Parses and executes transactions from trace files.

**Data Structures:**
```c
typedef enum {
    OP_DEPOSIT,
    OP_WITHDRAW,
    OP_TRANSFER,
    OP_BALANCE
} OpType;

typedef struct {
    OpType type;
    int account_id;
    int amount_centavos;
    int target_account;
} Operation;

typedef enum {
    TX_RUNNING,
    TX_COMMITTED,
    TX_ABORTED
} TxStatus;

typedef struct {
    int tx_id;
    Operation ops[MAX_OPS_PER_TX];
    int num_ops;
    int start_tick;         // when transaction should start
    pthread_t thread;       // thread handle
    int actual_start;      // actual start tick
    int actual_end;        // end tick
    int wait_ticks;        // accumulated wait time
    TxStatus status;
} Transaction;
```

**API:**
- `load_transactions_from_file(filename)` - parse trace file
- `execute_transaction(void*)` - thread function for transaction execution
- `wait_for_all_transactions()` - join all transaction threads

**Trace File Format:**
```
T1  0  DEPOSIT   10  5000
T1  1  WITHDRAW  10  2000
T2  0  TRANSFER  10  20  3000
T3  0  BALANCE   10
```
- Column 1: Transaction ID
- Column 2: Start tick
- Column 3: Operation type
- Column 4: Account ID (or source for TRANSFER)
- Column 5: Amount (or target for TRANSFER)
- Column 6: Amount (for TRANSFER only)

---

### 6. Metrics (`metrics.c`, `metrics.h`)

**Purpose:** Track and report performance statistics.

**Tracked Metrics:**
- Transaction timing (start, end, wait ticks)
- Buffer pool statistics (loads, unloads, peak usage)
- Balance conservation verification

---

## Synchronization Primitives

| Primitive | Usage | Module |
|-----------|-------|--------|
| `pthread_mutex_t` | Mutual exclusion | Timer, Bank, Buffer Pool |
| `pthread_rwlock_t` | Reader-writer lock | Bank (per-account) |
| `pthread_cond_t` | Condition variable | Timer |
| `sem_t` | Counting semaphore | Buffer Pool |

---

## Design Patterns

### 1. Singleton
- **Timer**: Single timer thread with global state
- **Bank**: Single `Bank bank` global instance

In C, singleton is implemented as a global variable with module-level encapsulation.

### 2. Producer-Consumer
- **Buffer Pool**: Uses semaphores to coordinate producers (load) and consumers (unload)
- Bounded to 5 slots - blocks when full

### 3. Deadlock Prevention
- **Lock Ordering**: Always acquire locks in ascending account ID order
- Breaks circular wait condition

### 4. Reader-Writer Lock
- **Per-account locks**: Multiple readers allowed, exclusive access for writers
- `pthread_rwlock_t` implementation

---

## Concurrency Concepts Demonstrated

1. **Mutual Exclusion**: Account balances protected by locks
2. **Reader-Writer Problem**: Concurrent reads, exclusive writes
3. **Bounded Buffer**: Fixed-size buffer pool with blocking
4. **Deadlock Prevention**: Lock ordering strategy
5. **Condition Variables**: Timer tick notifications

---

## Build and Run

```bash
make          # build release version
make debug    # build with ThreadSanitizer
make test     # run all test cases
```

### Command Line Options
```
--accounts=FILE    Account balances file (required)
--trace=FILE      Transaction trace file (required)
--tick-ms=N       Milliseconds per tick (default: 100)
--verbose         Print detailed operation logs
--deadlock=prevention|detection  Deadlock strategy
```

### Example Usage
```bash
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --tick-ms=50
```

---

## Test Cases

| File | Purpose | Expected Behavior |
|------|---------|-------------------|
| `trace_simple.txt` | Basic operations | All succeed |
| `trace_readers.txt` | Concurrent reads | All read simultaneously |
| `trace_deadlock.txt` | Transfer deadlock | Both succeed with lock ordering |
| `trace_abort.txt` | Insufficient funds | Transaction aborts |
| `trace_buffer.txt` | Buffer saturation | T6 blocks until slot available |

---

## Important Implementation Notes

1. **Never use floating-point for money**: All amounts stored as integer centavos
2. **Always release locks on error paths**: Don't forget unlock on early returns
3. **Lock ordering prevents deadlock**: Acquire in consistent order (ascending ID)
4. **Use volatile for shared counters**: Ensures visibility across threads
5. **Semaphore initialization**: `sem_init(&sem, 0, count)` - second param is 0 for threads
