#define _XOPEN_SOURCE 700
#include "../include/bank.h"
#include <stdio.h>
#include <string.h>

/**
 * Global bank instance containing all account data.
 * Initialized by init_bank() and populated by load_accounts_from_file().
 */
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

/**
 * Reads an account file and initializes bank accounts.
 * Skips comment lines (starting with #) and empty lines.
 * Each valid line must contain: "account_id  balance_centavos"
 * Initializes each account's rwlock and sets is_active = 1.
 * Returns the number of accounts loaded, or -1 on file open error.
 */
int load_accounts_from_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return -1;

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        int id, balance;
        if (sscanf(line, "%d %d", &id, &balance) == 2) {
            if (id >= 0 && id < MAX_ACCOUNTS) {
                bank.accounts[id].account_id = id;
                bank.accounts[id].balance_centavos = balance;
                bank.accounts[id].is_active = 1; // Flag to show this ID exists
                pthread_rwlock_init(&bank.accounts[id].lock, NULL);
                count++;
            }
        }
    }
    fclose(file);
    return count;
}