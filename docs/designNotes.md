# Concurrent Banking System - Design Notes

## Project Overview

This document outlines the detailed design for a multi-threaded banking system using POSIX threads (pthreads), synchronization primitives, and concurrency control mechanisms.

---

## Phase 1: Project Setup and Infrastructure

### 1.1 Directory Structure

```
bankdb/
├── Makefile
├── README.md
├── docs/
│   └── designNotes.md (this file)
│   └── design.md (final deliverable)
├── include/
│   ├── bank.h
│   ├── transaction.h
│   ├── timer.h
│   ├── lock_mgr.h
│   ├── buffer_pool.h
│   └── metrics.h
├── src/
│   ├── main.c
│   ├── bank.c
│   ├── transaction.c
│   ├── timer.c
│   ├── lock_mgr.c
│   ├── buffer_pool.c
│   ├── metrics.c
│   └── utils.c
└── tests/
    ├── accounts.txt
    ├── trace_simple.txt
    ├── trace_readers.txt
    ├── trace_deadlock.txt
    ├── trace_abort.txt
    └── trace_buffer.txt
```

### 1.2 Makefile Targets

**Target: `all`**
- Compiles with `-pthread -O2 -Wall -Wextra`
- Produces `bankdb` executable

**Target: `debug`**
- Compiles with `-g -fsanitize=thread -pthread`
- Enables ThreadSanitizer for race condition detection
- Used for validation testing

**Target: `clean`**
- Removes all binaries and object files

**Target: `test`**
- Runs all 5 provided test cases
- Verifies correctness under ThreadSanitizer

---

## Phase 2: Data Structures and Headers

### 2.1 Account and Bank Structures (bank.h)

```c
#define MAX_ACCOUNTS 100

typedef struct {
    int account_id;
    int balance_centavos;
    pthread_rwlock_t lock;
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int num_accounts;
    pthread_mutex_t bank_lock;
} Bank;
```

**Design Decisions:**
- Per-account `pthread_rwlock_t` enables reader-writer locking
- Multiple concurrent readers allowed; exclusive access for writers
- `bank_lock` protects bank metadata (num_accounts)

### 2.2 Transaction Structures (transaction.h)

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
    Operation ops[256];
    int num_ops;
    int start_tick;
    pthread_t thread;
    int actual_start;
    int actual_end;
    int wait_ticks;
    TxStatus status;
} Transaction;
```

**Design Decisions:**
- Maximum 256 operations per transaction (sufficient for test cases)
- `start_tick` - when transaction should start (from trace file)
- `actual_start` - when transaction actually started (measured)
- `wait_ticks` - accumulated lock wait time in ticks

### 2.3 Timer Structures (timer.h)

```c
volatile int global_tick = 0;
pthread_mutex_t tick_lock;
pthread_cond_t tick_changed;
```

**Design Decisions:**
- `volatile` ensures visibility across threads
- Mutex protects access to `global_tick`
- Condition variable notifies waiting threads of tick changes

### 2.4 Buffer Pool Structures (buffer_pool.h)

```c
#define BUFFER_POOL_SIZE 5

typedef struct {
    int account_id;
    Account* data;
    bool in_use;
} BufferSlot;

typedef struct {
    BufferSlot slots[BUFFER_POOL_SIZE];
    sem_t empty_slots;
    sem_t full_slots;
    pthread_mutex_t pool_lock;
} BufferPool;
```

**Design Decisions:**
- Fixed pool size of 5 slots (as per specification)
- Semaphores track empty/full slots (producer-consumer pattern)
- `pool_lock` protects slot metadata

### 2.5 Lock Manager Structures (lock_mgr.h)

```c
typedef enum {
    DEADLOCK_PREVENTION,
    DEADLOCK_DETECTION
} DeadlockStrategy;
```

**Design Decisions:**
- Configurable deadlock strategy via command-line
- For this implementation: **DEADLOCK_PREVENTION** (lock ordering)

---

## Phase 3: Core Implementation

### 3.1 Timer Thread (timer.c)

**Purpose:** Simulate time progression using a separate timer thread.

**Implementation:**

```c
void* timer_thread(void* arg) {
    while (simulation_running) {
        pthread_mutex_lock(&tick_lock);
        usleep(TICK_INTERVAL_MS * 1000);
        global_tick++;
        pthread_cond_broadcast(&tick_changed);
        pthread_mutex_unlock(&tick_lock);
    }
    return NULL;
}

void wait_until_tick(int target_tick) {
    pthread_mutex_lock(&tick_lock);
    while (global_tick < target_tick) {
        pthread_cond_wait(&tick_changed, &tick_lock);
    }
    pthread_mutex_unlock(&tick_lock);
}
```

**Design Rationale:**
- Timer thread runs continuously until simulation ends
- `pthread_cond_broadcast` wakes all waiting threads when tick advances
- `wait_until_tick` blocks until global_tick >= target_tick

**Why a timer thread?**
1. Enables true concurrent transaction scheduling (transactions can start at same tick)
2. Simulates time progression independent of transaction execution
3. Allows measurement of lock wait times in "ticks" rather than wall-clock time
4. Tests concurrency control mechanisms under realistic conditions

### 3.2 Account Operations (bank.c)

**Reader-Writer Lock Usage:**

```c
int get_balance(int account_id) {
    Account* acc = &bank.accounts[account_id];
    pthread_rwlock_rdlock(&acc->lock);
    int balance = acc->balance_centavos;
    pthread_rwlock_unlock(&acc->lock);
    return balance;
}

void deposit(int account_id, int amount_centavos) {
    Account* acc = &bank.accounts[account_id];
    pthread_rwlock_wrlock(&acc->lock);
    acc->balance_centavos += amount_centavos;
    pthread_rwlock_unlock(&acc->lock);
}

bool withdraw(int account_id, int amount_centavos) {
    Account* acc = &bank.accounts[account_id];
    pthread_rwlock_wrlock(&acc->lock);
    if (acc->balance_centavos < amount_centavos) {
        pthread_rwlock_unlock(&acc->lock);
        return false;
    }
    acc->balance_centavos -= amount_centavos;
    pthread_rwlock_unlock(&acc->lock);
    return true;
}
```

**Design Rationale:**
- `get_balance`: Read lock allows concurrent reads
- `deposit`: Write lock for exclusive access
- `withdraw`: Write lock with insufficient funds check; returns false on failure

### 3.3 Transfer with Deadlock Prevention (lock_mgr.c)

**Lock Ordering Strategy:**

```c
bool transfer(int from_id, int to_id, int amount_centavos) {
    // Self-transfer: no-op, return success immediately
    // Required to prevent self-deadlock when from_id == to_id
    // POSIX rwlock is not recursive - acquiring same lock twice blocks forever
    if (from_id == to_id) {
        return true;
    }

    int first = (from_id < to_id) ? from_id : to_id;
    int second = (from_id < to_id) ? to_id : from_id;
    
    Account* acc_first = &bank.accounts[first];
    Account* acc_second = &bank.accounts[second];
    
    pthread_rwlock_wrlock(&acc_first->lock);
    pthread_rwlock_wrlock(&acc_second->lock);
    
    Account* from_acc = &bank.accounts[from_id];
    if (from_acc->balance_centavos < amount_centavos) {
        pthread_rwlock_unlock(&acc_second->lock);
        pthread_rwlock_unlock(&acc_first->lock);
        return false;
    }
    
    bank.accounts[from_id].balance_centavos -= amount_centavos;
    bank.accounts[to_id].balance_centavos += amount_centavos;
    
    pthread_rwlock_unlock(&acc_second->lock);
    pthread_rwlock_unlock(&acc_first->lock);
    return true;
}
```

**Deadlock Prevention Proof:**

This implementation breaks the **circular wait** condition (one of the four Coffman conditions):

1. **Mutual Exclusion**: Locks are exclusive (satisfied)
2. **Hold and Wait**: Transactions hold at least one lock while waiting for another (could occur)
3. **No Preemption**: Locks cannot be forcibly taken (satisfied)
4. **Circular Wait**: BROKEN - By always acquiring locks in ascending order, a transaction can never wait for a lock with a lower ID than one it already holds, preventing circular wait

**Example:**
- Transaction A transfers from account 10 to 20: acquires lock 10, then 20
- Transaction B transfers from account 20 to 10: tries to acquire 20 first, but A already holds it, so B waits for 20 (which is > 10, the lock A holds)
- No circular wait possible because both transactions acquire locks in the same order

### 3.4 Buffer Pool (buffer_pool.c)

**Implementation:**

```c
void init_buffer_pool(BufferPool* pool) {
    sem_init(&pool->empty_slots, 0, BUFFER_POOL_SIZE);
    sem_init(&pool->full_slots, 0, 0);
    pthread_mutex_init(&pool->pool_lock, NULL);
}

void load_account(BufferPool* pool, int account_id) {
    sem_wait(&pool->empty_slots);
    pthread_mutex_lock(&pool->pool_lock);
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!pool->slots[i].in_use) {
            pool->slots[i].account_id = account_id;
            pool->slots[i].data = &bank.accounts[account_id];
            pool->slots[i].in_use = true;
            break;
        }
    }
    pthread_mutex_unlock(&pool->pool_lock);
    sem_post(&pool->full_slots);
}

void unload_account(BufferPool* pool, int account_id) {
    sem_wait(&pool->full_slots);
    pthread_mutex_lock(&pool->pool_lock);
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->slots[i].in_use && pool->slots[i].account_id == account_id) {
            pool->slots[i].in_use = false;
            pool->slots[i].account_id = -1;
            break;
        }
    }
    pthread_mutex_unlock(&pool->pool_lock);
    sem_post(&pool->empty_slots);
}
```

**Integration Strategy:**

Load on first access, unload on commit:
- When a transaction first accesses an account, load it into the buffer pool
- Keep the account in the pool throughout the transaction
- Unload when the transaction commits or aborts

**Design Rationale:**
- Avoids repeated load/unload for transactions accessing same account multiple times
- If pool is full, transaction blocks (demonstrates bounded buffer problem)
- Alternative strategies (load per operation, LRU) add complexity without clear benefit for test cases

### 3.5 Transaction Execution (transaction.c)

**Implementation:**

```c
void* execute_transaction(void* arg) {
    Transaction* tx = (Transaction*)arg;
    
    wait_until_tick(tx->start_tick);
    tx->actual_start = global_tick;
    
    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];
        int tick_before = global_tick;
        
        switch (op->type) {
            case OP_DEPOSIT:
                deposit(op->account_id, op->amount_centavos);
                break;
            case OP_WITHDRAW:
                if (!withdraw(op->account_id, op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    return NULL;
                }
                break;
            case OP_TRANSFER:
                if (!transfer(op->account_id, op->target_account, op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    return NULL;
                }
                break;
            case OP_BALANCE:
                int balance = get_balance(op->account_id);
                printf("T%d: Account %d balance = PHP %d.%02d\n",
                       tx->tx_id, op->account_id, balance / 100, balance % 100);
                break;
        }
        tx->wait_ticks += (global_tick - tick_before);
    }
    
    tx->actual_end = global_tick;
    tx->status = TX_COMMITTED;
    return NULL;
}
```

**Design Rationale:**
- Each transaction runs in its own pthread
- Waits until its scheduled start_tick before beginning
- Tracks timing: actual_start, actual_end, wait_ticks
- Aborts on insufficient funds (withdraw/transfer failure)

### 3.6 Metrics (metrics.c)

**Metrics Collected:**

1. **Transaction Metrics:**
   - Start tick, actual start tick, end tick
   - Wait ticks (lock contention time)
   - Status (COMMITTED/ABORTED)

2. **Buffer Pool Statistics:**
   - Total loads, total unloads
   - Peak usage (max slots used simultaneously)
   - Blocked operations count

3. **Summary Metrics:**
   - Total transactions, committed, aborted
   - Average wait time
   - Throughput (transactions per tick)
   - Balance conservation check

---

## Phase 4: Input Handling

### 4.1 Command-Line Interface (main.c)

**Required Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `--accounts=FILE` | Initial account balances | (required) |
| `--trace=FILE` | Transaction workload | (required) |
| `--deadlock=prevention\|detection` | Deadlock strategy | prevention |
| `--tick-ms=N` | Milliseconds per tick | 100 |
| `--verbose` | Print detailed logs | false |

### 4.2 Trace File Format

```
# Comment line
T1  0  DEPOSIT   10  5000
T2  1  WITHDRAW  10  2000
T3  2  TRANSFER  10  20  3000
T5  5  BALANCE   10
```

**Operations:**
- `DEPOSIT account_id amount` - Add amount centavos
- `WITHDRAW account_id amount` - Remove amount centavos (abort if insufficient)
- `TRANSFER from_id to_id amount` - Move amount between accounts
- `BALANCE account_id` - Read and print balance

### 4.3 Account File Format

```
# AccountID  InitialBalanceCentavos
0   10000
1   25000
10  50000
20  30000
```

---

## Phase 5: Testing Strategy

### 5.1 Test Cases

| Test File | Purpose | Expected Behavior |
|-----------|---------|-------------------|
| trace_simple.txt | Basic operations | All succeed, final balance correct |
| trace_readers.txt | Concurrent reads | All read simultaneously (rwlock) |
| trace_deadlock.txt | Transfer deadlock | Both succeed with lock ordering |
| trace_abort.txt | Insufficient funds | Transaction aborts |
| trace_buffer.txt | Buffer saturation | T6 blocks until slot available |

### 5.2 ThreadSanitizer Validation

**Command:**
```bash
make debug
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_*.txt --deadlock=prevention
```

**Requirements:**
- Zero ThreadSanitizer warnings
- No data races detected
- Proper lock acquisition/release

### 5.3 Balance Conservation

**Verification:**
```
Initial total: PHP X.XX
Final total:   PHP X.XX
Conservation check: PASSED/FAILED
```

**Implementation:**
- Calculate sum of all account balances before any transactions
- Calculate sum after all transactions complete
- Must be equal (money is neither created nor destroyed)

---

## Phase 6: Concurrency Concepts Demonstrated

### 6.1 Mutual Exclusion

- Account balances protected by reader-writer locks
- Only one thread can modify an account at a time
- Reader-writer locks allow concurrent reads

### 6.2 Reader-Writer Problem

- Multiple transactions can read account balance simultaneously
- Write operations get exclusive access
- pthread_rwlock_t implementation

### 6.3 Bounded Buffer (Producer-Consumer)

- Buffer pool with fixed size (5 slots)
- Semaphores coordinate producers (load) and consumers (unload)
- Demonstrates blocking when pool is full

### 6.4 Deadlock Prevention

- Lock ordering breaks circular wait condition
- All transfers acquire locks in ascending account ID order
- No deadlock possible regardless of transaction interleaving

---

## Phase 7: Expected Output Formats

### 7.1 Transaction Log

```
=== Banking System Execution Log ===
Timer thread started (tick interval: 100ms)

Tick 0:
  T1 started: DEPOSIT account 10 amount PHP 50.00

Tick 1:
  T1 completed: DEPOSIT successful
  T2 started: WITHDRAW account 10 amount PHP 20.00

...
```

### 7.2 Performance Metrics

```
=== Transaction Performance Metrics ===
TxID | StartTick | ActualStart | End | WaitTicks | Status
-----|-----------|-------------|-----|-----------|----------
T1   |     0     |      0      |  1  |     0     | COMMITTED
...

Average wait time: 0.4 ticks
Throughput: 5 transactions / 6 ticks = 0.83 tx/tick
```

### 7.3 Buffer Pool Report

```
=== Buffer Pool Report ===
Pool size: 5 slots
Total loads: 8
Total unloads: 8
Peak usage: 4 slots
Blocked operations (pool full): 0
```

---

## Implementation Checklist

### Phase 1: Setup
- [ ] Create directory structure
- [ ] Create Makefile with all targets

### Phase 2: Headers
- [ ] bank.h - Account and Bank structs
- [ ] transaction.h - Transaction and Operation types
- [ ] timer.h - Timer thread declarations
- [ ] lock_mgr.h - Lock ordering functions
- [ ] buffer_pool.h - Buffer pool declarations
- [ ] metrics.h - Metrics functions

### Phase 3: Core Implementation
- [ ] timer.c - Timer thread, wait_until_tick
- [ ] bank.c - get_balance, deposit, withdraw
- [ ] lock_mgr.c - transfer with lock ordering
- [ ] buffer_pool.c - load/unload with semaphores
- [ ] transaction.c - execute_transaction thread
- [ ] metrics.c - Statistics collection

### Phase 4: Main Program
- [ ] main.c - CLI parsing, file loading, initialization

### Phase 5: Test Files
- [ ] accounts.txt - Initial balances
- [ ] trace_simple.txt - Basic operations
- [ ] trace_readers.txt - Concurrent reads
- [ ] trace_deadlock.txt - Transfer scenario
- [ ] trace_abort.txt - Insufficient funds
- [ ] trace_buffer.txt - Buffer saturation

### Phase 6: Validation
- [ ] ThreadSanitizer - Zero warnings
- [ ] Balance conservation - Pass
- [ ] Deadlock prevention - Verified

### Phase 7: Documentation
- [ ] README.md - Compilation and usage
- [ ] design.md - Required discussion topics

---

## Appendix: Key Synchronization Primitives Used

| Primitive | Purpose | Usage |
|-----------|---------|-------|
| pthread_rwlock_t | Reader-writer lock | Per-account lock for balance operations |
| pthread_mutex_t | Mutual exclusion | Bank metadata, tick counter, buffer pool |
| pthread_cond_t | Condition variable | Timer signaling |
| sem_t | Counting semaphore | Buffer pool empty/full slots |

---

## Appendix: Common Pitfalls to Avoid

1. **Forgotten unlock**: Always release locks, even on error paths
2. **Lock after free**: Release lock AFTER done reading/writing data
3. **Deadlock without ordering**: Acquire locks in consistent order
4. **Race on global_tick**: Read global_tick while holding tick_lock
5. **Semaphore init**: sem_init(&sem, 0, count) - second param 0 for threads
6. **Money conservation**: Sum of balances must remain constant