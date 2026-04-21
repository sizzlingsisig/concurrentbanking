#ifndef BANK_H
#define BANK_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include <pthread.h>

#define MAX_ACCOUNTS 100

typedef struct {
    int account_id;
    int balance_centavos;
    int is_active;
    pthread_rwlock_t lock;
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int num_accounts;
    pthread_mutex_t bank_lock;
} Bank;

extern Bank bank;

void init_bank(void);
int get_balance(int account_id);
void deposit(int account_id, int amount_centavos);
int withdraw(int account_id, int amount_centavos);
int get_total_balance(void);
int load_accounts_from_file(const char* filename);

#endif