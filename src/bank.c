#include "../include/bank.h"
#include <string.h>

Bank bank;

/**
 * Initializes the bank structure, including all account balances 
 * and their respective reader-writer locks.
 */
void init_bank(void) {
    memset(&bank, 0, sizeof(Bank));
    pthread_mutex_init(&bank.bank_lock, NULL);
    
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        bank.accounts[i].account_id = i;
        bank.accounts[i].balance_centavos = 0;
        // Initialize the per-account rwlock
        pthread_rwlock_init(&bank.accounts[i].lock, NULL);
    }
}

/**
 * Returns the balance of an account using a read lock to 
 * allow for concurrent readers.
 */
int get_balance(int account_id) {
    Account* acc = &bank.accounts[account_id];
    pthread_rwlock_rdlock(&acc->lock);
    int balance = acc->balance_centavos;
    pthread_rwlock_unlock(&acc->lock);
    return balance;
}

/**
 * Deposits centavos into an account using a write lock 
 * for exclusive access.
 */
void deposit(int account_id, int amount_centavos) {
    Account* acc = &bank.accounts[account_id];
    pthread_rwlock_wrlock(&acc->lock);
    acc->balance_centavos += amount_centavos;
    pthread_rwlock_unlock(&acc->lock);
}

/**
 * Withdraws centavos from an account if funds are sufficient.
 * Returns 1 (true) on success, 0 (false) on failure.
 */
int withdraw(int account_id, int amount_centavos) {
    Account* acc = &bank.accounts[account_id];
    pthread_rwlock_wrlock(&acc->lock);
    
    if (acc->balance_centavos < amount_centavos) {
        // Must unlock before returning on failure
        pthread_rwlock_unlock(&acc->lock);
        return 0; 
    }
    
    acc->balance_centavos -= amount_centavos;
    pthread_rwlock_unlock(&acc->lock);
    return 1;
}

/**
 * Calculates the sum of all account balances.
 * Used for the balance conservation check.
 */
int get_total_balance(void) {
    int total = 0;
    // For a consistent snapshot, we lock the bank metadata or 
    // lock all accounts sequentially.
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        total += get_balance(i);
    }
    return total;
}