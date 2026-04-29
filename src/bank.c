#define _XOPEN_SOURCE 700
#include "../include/bank.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
        bank.accounts[i].is_active = 1;
        pthread_rwlock_init(&bank.accounts[i].lock, NULL);
    }
}

/**
 * Validates an account ID is within valid range.
 * Returns 1 if valid, 0 if invalid.
 */
int validate_account_id(int account_id) {
    return (account_id >= 0 && account_id < MAX_ACCOUNTS && bank.accounts[account_id].is_active);
}

/**
 * Returns the balance of an account using a read lock to 
 * allow for concurrent readers.
 */
int get_balance(int account_id) {
    if (!validate_account_id(account_id)) {
        return 0;
    }
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
    if (!validate_account_id(account_id)) {
        return;
    }
    if (amount_centavos <= 0) {
        return;
    }
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
    if (!validate_account_id(account_id)) {
        return 0;
    }
    if (amount_centavos <= 0) {
        return 0;
    }
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
int64_t get_total_balance(void) {
    int64_t total = 0;
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        if (bank.accounts[i].is_active) { 
            pthread_rwlock_rdlock(&bank.accounts[i].lock);
            total += bank.accounts[i].balance_centavos;
            pthread_rwlock_unlock(&bank.accounts[i].lock);
        }
    }
    return total;
}

int load_accounts_from_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return -1;

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        int id, balance;
        // Use IF, not WHILE, to avoid an infinite loop on the same string
        if (sscanf(line, "%d %d", &id, &balance) == 2) {
            if (id >= 0 && id < MAX_ACCOUNTS) {
                bank.accounts[id].balance_centavos = balance;
                bank.accounts[id].is_active = 1;
                pthread_rwlock_init(&bank.accounts[id].lock, NULL);
                count++;
            }
        }
    }
    fclose(file);
    return count; 
}