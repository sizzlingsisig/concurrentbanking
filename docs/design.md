# Concurrent Banking System - Design Documentation

## 1. Deadlock Strategy Choice

### Chosen Strategy: Deadlock Prevention (Lock Ordering)

We chose **deadlock prevention** over detection for the following reasons:

1. **Simplicity** - Lock ordering is straightforward to implement correctly
2. **No runtime overhead** - No cycle detection needed during execution
3. **Guaranteed safety** - Prevention guarantees no deadlock occurs; detection only finds it after it happens
4. **Standard practice** - Most production banking systems use prevention

### Proof: Lock Ordering Eliminates Circular Wait

Our implementation in `lock_mgr.c` acquires locks in ascending account ID order:

```c
int first = (from_id < to_id) ? from_id : to_id;
int second = (from_id < to_id) ? to_id : from_id;

pthread_rwlock_wrlock(&accounts[first].lock);
pthread_rwlock_wrlock(&accounts[second].lock);
```

This breaks **Coffman Condition #4: Circular Wait**:

- A transaction holding lock on account N can only wait for locks on accounts M where M > N
- Therefore, it can never wait for a lock it already holds
- No circular wait chain is possible

### Coffman Conditions Analysis

| Condition | Status | How We Address It |
|-----------|--------|-------------------|
| 1. Mutual Exclusion | ✅ Holds | Locks are exclusive |
| 2. Hold and Wait | ⚠️ May occur | Transactions hold locks while waiting |
| 3. No Preemption | ✅ Holds | Locks cannot be forcibly taken |
| 4. Circular Wait | ✅ **BROKEN** | Lock ordering prevents circular wait |

### Tradeoffs: Prevention vs Detection

| Factor | Prevention (Lock Ordering) | Detection (Wait-For Graph) |
|--------|---------------------------|---------------------------|
| Implementation complexity | Low | Medium |
| Runtime overhead | None | Per-lock acquire |
| Transaction abort | Never (prevented) | Yes (when cycle detected) |
| Can handle dynamic scenarios | Limited | Flexible |
| Response to deadlock | Prevention (guaranteed) | Detection + recovery |

#### Prevention Tradeoffs

**Pros:**
- Simple implementation
- No runtime overhead
- Guarantees no deadlock occurs

**Cons:**
- Restricts lock acquisition order
- Cannot adapt to dynamic scenarios
- May reduce concurrency unnecessarily

#### Detection Tradeoffs

**Pros:**
- More flexible lock usage
- Allows deadlock recovery via abort
- Better for dynamic workloads

**Cons:**
- Requires runtime cycle detection
- Must maintain wait-for graph
- Aborting transactions has costs

#### Our Choice

We chose **prevention** because:
1. Simpler to implement correctly in C
2. No runtime overhead during normal operation
3. Banking transactions have predictable patterns (transfers between accounts)
4. Prevention is standard practice in production banking systems

---

## 2. Buffer Pool Integration

### Design Decision: Load on First Access, Unload on Commit/Abort

**When we load accounts:**
- Load account into buffer pool when transaction first accesses it
- Keep the account in the pool for the duration of the transaction
- Unload when the transaction commits or aborts

**What happens when the pool is full:**
- Transaction blocks on `sem_wait(&empty_slots)`
- This demonstrates the bounded buffer (producer-consumer) problem
- Unblocks when another transaction unloads an account

### Implementation

The buffer pool uses:
- `sem_t empty_slots` - Counts available slots (starts at 5)
- `pthread_mutex_t pool_lock` - Protects slot metadata
- `int pin_count` - Per-slot reference count to handle multi-operation transactions

```c
void load_account(int account_id) {
    // Check if already in pool (reuse if found)
    pthread_mutex_lock(&pool_lock);
    if (found) {
        slot->pin_count++;
        pthread_mutex_unlock(&pool_lock);
        return;
    }
    pthread_mutex_unlock(&pool_lock);

    // Otherwise, acquire new slot
    sem_wait(&buffer_pool.empty_slots);  // Block if full
    pthread_mutex_lock(&pool_lock);
    // Find empty slot, load data, and set pin_count = 1
    pthread_mutex_unlock(&pool_lock);
}
```

### Design Justification

1. **Performance** - Avoids repeated loads for same account during multi-operation transaction using `pin_count`.
2. **Simplicity** - Straightforward strategy vs. LRU or other eviction policies.
3. **Demonstrates problem** - Properly shows blocking when pool is saturated.
4. **Reference Counting** - The `pin_count` ensures that a slot is only truly "freed" when all operations using it have finished, preventing premature eviction.

### Why Not Other Strategies?

#### Load/Unload Per Operation
- **Disadvantage**: Would require loading/unloading for every single operation within a transaction
- **Impact**: Adds unnecessary semaphore overhead when a transaction accesses the same account multiple times
- **Our choice**: Avoid this overhead by keeping loaded accounts for transaction duration

#### Load All Accounts at Start, Unload All at End
- **Disadvantage**: Need to know all accounts a transaction will access before execution starts
- **Problem**: Our transaction file format doesn't pre-declare all accounts - they're discovered per operation
- **Additional issue**: Would hold buffer slots unnecessarily long if transaction accesses only a subset

#### LRU Eviction Policy
- **Disadvantage**: More complex to implement correctly
- **Overhead**: Requires tracking access order and timestamps for every slot
- **Unnecessary**: For our workload (short transactions, 5-slot pool), the added complexity doesn't yield significant benefit
- **Our approach**: Pin-count based (same slot can be reused by multiple ops) is simpler and sufficient for our use case

### Summary Table

| Strategy | Complexity | Overhead | Chosen? |
|----------|------------|----------|---------|
| Load on first access, unload on commit | Low | Minimal | ✅ |
| Load/unload per operation | Low | High | ❌ |
| Load all at start, unload all at end | Medium | Requires pre-declaration | ❌ |
| LRU eviction | High | Per-slot tracking | ❌ |

---

## 3. Reader-Writer Lock Performance

### Comparison: pthread_mutex_t vs pthread_rwlock_t

| Lock Type | Behavior | Best For |
|----------|----------|----------|
| `pthread_mutex_t` | Exclusive access | Write-heavy workloads |
| `pthread_rwlock_t` | Multiple readers OR one writer | Read-heavy workloads |

### Performance Analysis

**trace_readers.txt** (4 concurrent BALANCE operations):

| Lock Type | Expected Behavior |
|-----------|-------------------|
| mutex | Serialized - each read waits for others |
| rwlock | Concurrent - all 4 read simultaneously |

With `pthread_rwlock_t`:
- Multiple readers can access the same account simultaneously
- Writers still get exclusive access (no readers can hold lock)
- Significantly faster on read-heavy workloads

**Benchmark Results:**

```
# With pthread_mutex_t (simulated - would serialize reads)
Total Transactions: 4
Time: ~4 ticks (serialized)

# With pthread_rwlock_t (current implementation)  
Total Transactions: 4
Throughput: 4.00 tx/tick (concurrent)
```

### Why RWLock Helps

- Banking operations are typically read-heavy (balance inquiries)
- Writes (deposits, withdrawals) are less frequent
- RWLock allows concurrent reads while maintaining write isolation

---

## 4. Timer Thread Design

### Why is a Separate Timer Thread Necessary?

1. **Enables true concurrent transaction scheduling**
   - Transactions can start at the same tick
   - Tests concurrency control under realistic conditions

2. **Consistent timing measurement**
   - Lock wait times measured in "ticks" (simulation time)
   - Reproducible across multiple runs (not dependent on CPU load)

3. **Simulates real-world time-based systems**
   - Banking systems schedule transactions by time
   - Allows testing of scheduled vs. actual start times

### Implementation

The timer uses a **Condition Variable** (`pthread_cond_t`) to efficiently wake up transactions:

```c
void* timer_thread(void* arg) {
    while (simulation_running) {
        usleep(TICK_INTERVAL_MS * 1000);
        
        pthread_mutex_lock(&tick_lock);
        global_tick++;
        pthread_cond_broadcast(&tick_changed);  // Wake all waiting threads
        pthread_mutex_unlock(&tick_lock);
    }
    return NULL;
}
```

Transactions wait for their scheduled start using the **Condition Wait** pattern:

```c
void wait_until_tick(int target_tick) {
    pthread_mutex_lock(&tick_lock);
    while (global_tick < target_tick) {
        pthread_cond_wait(&tick_changed, &tick_lock);
    }
    pthread_mutex_unlock(&tick_lock);
}
```

### Why Condition Variables?

- **Efficiency**: Prevents "busy waiting" where threads consume CPU while waiting for a tick.
- **Precision**: Threads wake up immediately when the tick increments.
- **Scalability**: `pthread_cond_broadcast` allows multiple transactions starting at the same time to wake up simultaneously.

### What Breaks Without the Timer Thread?

1. **No concurrent scheduling** - All operations would process sequentially
2. **No meaningful wait_ticks** - No way to measure lock contention time
3. **Cannot test concurrency** - Would lose ability to test:
   - Reader-writer lock behavior under concurrent reads
   - Deadlock prevention with simultaneous transfers
   - Buffer pool blocking under saturation

4. **No timing metrics** - Would only measure wall-clock time, not simulation time

---

## Summary

| Design Decision | Choice | Rationale |
|-----------------|--------|-----------|
| Deadlock handling | Prevention (lock ordering) | Simpler, no runtime overhead |
| Buffer pool | Load on access, unload on commit | Performance + demonstrates bounded buffer |
| Account locks | Reader-writer locks | Better performance on read-heavy ops |
| Time simulation | Separate timer thread | Enables true concurrency testing |

---

## System Architecture

```
         +-----------+     +-----------+
         |   Timer  |     |  Transaction |
         |  Thread |     |   Thread(s)  |
         +-----------+     +-----------+
              |                 |
              v                 v
         +-----------+     +-----------+
         | Global    |     |   Bank    |
         | Clock     |     | (accounts)|
         +-----------+     +-----------+
              ^                 |
              |                 v
         +-----------+     +-----------+
         |   Wait   |<----|  Account  |
         | Condition|     |  Locks    |
         +-----------+     +-----------+
                               ^
                               |
                          +---------+
                          | Buffer  |
                          |  Pool   |
                          +---------+
```

### Components

| Component | File | Purpose |
|-----------|------|---------|
| Timer | `timer.c` | Advances global clock |
| Bank | `bank.c` | Account storage & operations |
| Lock Manager | `lock_mgr.c` | Deadlock prevention |
| Buffer Pool | `buffer_pool.c` | Bounded cache |
| Transaction | `transaction.c` | Per-transaction execution |
| Metrics | `metrics.c` | Performance tracking |
| Main | `main.c` | CLI & coordination |

---

## Testing Results

### Balance Conservation

All tests verify money conservation:
- Initial total = Final total
- No money created or destroyed

### ThreadSanitizer

```bash
make debug
make test
```

**Result**: Zero warnings - no data races detected

### Test Cases

| Test | Status | Notes |
|------|--------|-------|
| trace_simple | ✅ Pass | 1 committed |
| trace_readers | ✅ Pass | 4 committed |
| trace_deadlock | ✅ Pass | Prevention works |
| trace_abort | ✅ Pass | 1 aborted |
| trace_buffer | ✅ Pass | 6 committed |