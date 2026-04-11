#include "../include/transaction.h"
#include "../include/bank.h"
#include "../include/timer.h"
#include "../include/lock_mgr.h"
#include "../include/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>

Transaction transactions[MAX_TRANSACTIONS];
int num_transactions = 0;

/**
 * The worker function for each transaction thread.
 * Coordinates timing, execution, and metric collection.
 */
void* execute_transaction(void* arg) {
    Transaction* tx = (Transaction*)arg;
    
    // 1. Wait until the simulation reaches the scheduled start time
    wait_until_tick(tx->start_tick);
    tx->actual_start = (int)global_tick;
    
    // 2. Iterate through operations
    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];
        int tick_before = (int)global_tick;
        
        // Logical constraint: Load account into buffer pool before use
        load_account(op->account_id);
        if (op->type == OP_TRANSFER) {
            load_account(op->target_account);
        }

        switch (op->type) {
            case OP_DEPOSIT:
                deposit(op->account_id, op->amount_centavos);
                break;
                
            case OP_WITHDRAW:
                if (!withdraw(op->account_id, op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    goto cleanup;
                }
                break;
                
            case OP_TRANSFER:
                if (!transfer(op->account_id, op->target_account, op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    goto cleanup;
                }
                break;
                
            case OP_BALANCE: {
                int balance = get_balance(op->account_id);
                printf("T%d: Account %d balance = PHP %d.%02d\n",
                       tx->tx_id, op->account_id, balance / 100, balance % 100);
                break;
            }
        }
        
        // Track time spent waiting for locks
        tx->wait_ticks += ((int)global_tick - tick_before);
    }
    
    tx->status = TX_COMMITTED;

cleanup:
    // Ensure all buffer slots are released regardless of success or abort
    for (int i = 0; i < tx->num_ops; i++) {
        unload_account(tx->ops[i].account_id);
        if (tx->ops[i].type == OP_TRANSFER) {
            unload_account(tx->ops[i].target_account);
        }
    }
    
    tx->actual_end = (int)global_tick;
    return NULL;
}

/**
 * Blocks the main thread until all transaction threads have finished.
 */
void wait_for_all_transactions(void) {
    for (int i = 0; i < num_transactions; i++) {
        if (transactions[i].thread != 0) {
            pthread_join(transactions[i].thread, NULL);
        }
    }
}

/**
 * Stub for loading transactions. Implementation depends on your 
 * specific parser logic for the trace file format.
 */
int load_transactions_from_file(const char* filename) {
    // Logic to parse file and populate transactions[] goes here
    return 0;
}