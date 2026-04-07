#include "../include/transaction.h"

Transaction transactions[MAX_TRANSACTIONS];
int num_transactions = 0;

void* execute_transaction(void* arg) {
    return NULL;
}

int load_transactions_from_file(const char* filename) {
    return 0;
}

void wait_for_all_transactions(void) {
}