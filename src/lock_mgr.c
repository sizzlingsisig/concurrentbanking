#include "../include/lock_mgr.h"
#include "../include/bank.h"
#include <stdbool.h>

/**
 * Global deadlock strategy selection.
 * DEADLOCK_PREVENTION: lock ordering (ascending account ID) — default, implemented.
 * DEADLOCK_DETECTION:  wait-for graph cycle detection — not implemented.
 */
DeadlockStrategy deadlock_strategy = DEADLOCK_PREVENTION;

/**
 * Executes a transfer between two accounts using deadlock-prevention.
 *
 * Steps:
 *   1. Guard against self-transfer (avoids double-lock on same account).
 *   2. Acquire write locks in ascending account ID order — this breaks the
 *      circular-wait Coffman condition and prevents all deadlock scenarios.
 *   3. Check sufficient funds; if OK, perform the debit/credit.
 *   4. Release locks in reverse order of acquisition.
 *
 * Returns 1 on success, 0 on failure (insufficient funds).
 */
int transfer(int from_id, int to_id, int amount_centavos) {
    // 1. Handle self-transfer to prevent self-deadlock
    // POSIX rwlocks are non-recursive; locking twice in one thread hangs forever
    if (from_id == to_id) {
        return 1;
    }

    // 2. Deadlock Prevention: Deterministic Lock Ordering
    // Always acquire the lock for the smaller account ID first
    int first = (from_id < to_id) ? from_id : to_id;
    int second = (from_id < to_id) ? to_id : from_id;
    
    Account* acc_first = &bank.accounts[first];
    Account* acc_second = &bank.accounts[second];
    
    // Acquire both write locks in order
    pthread_rwlock_wrlock(&acc_first->lock);
    pthread_rwlock_wrlock(&acc_second->lock);
    
    // 3. Perform the transaction
    Account* from_acc = &bank.accounts[from_id];
    Account* to_acc = &bank.accounts[to_id];
    
    int success = 0;
    if (from_acc->balance_centavos >= amount_centavos) {
        from_acc->balance_centavos -= amount_centavos;
        to_acc->balance_centavos += amount_centavos;
        success = 1;
    }
    
    // 4. Release locks in reverse order
    pthread_rwlock_unlock(&acc_second->lock);
    pthread_rwlock_unlock(&acc_first->lock);
    
    return success;
}