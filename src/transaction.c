#define _XOPEN_SOURCE 700
#include "../include/transaction.h"
#include "../include/bank.h"
#include "../include/timer.h"
#include "../include/lock_mgr.h"
#include "../include/buffer_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open trace file");
        return -1;
    }

    char line[256];
    num_transactions = 0;

    while (fgets(line, sizeof(line), file) && num_transactions < MAX_TRANSACTIONS) {
        // 1. Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        Transaction* tx = &transactions[num_transactions];
        char tx_label[16];
        char op_name[16];
        int start_tick;

        // 2. Parse the prefix: Transaction ID, Start Tick, and Operation Name
        // Example: T1 0 DEPOSIT ...
        if (sscanf(line, "%s %d %s", tx_label, &start_tick, op_name) < 3) continue;

        tx->tx_id = num_transactions;
        tx->start_tick = start_tick;
        tx->status = TX_PENDING;
        tx->num_ops = 1; // Simplification: 1 operation per line in this format
        tx->wait_ticks = 0;

        Operation* op = &tx->ops[0];

        // 3. Map string operation names to your Enum types
        if (strcmp(op_name, "DEPOSIT") == 0) {
            op->type = OP_DEPOSIT;
            sscanf(line, "%*s %*d %*s %d %d", &op->account_id, &op->amount_centavos);
        } 
        else if (strcmp(op_name, "WITHDRAW") == 0) {
            op->type = OP_WITHDRAW;
            sscanf(line, "%*s %*d %*s %d %d", &op->account_id, &op->amount_centavos);
        } 
        else if (strcmp(op_name, "TRANSFER") == 0) {
            op->type = OP_TRANSFER;
            sscanf(line, "%*s %*d %*s %d %d %d", &op->account_id, &op->target_account, &op->amount_centavos);
        } 
        else if (strcmp(op_name, "BALANCE") == 0) {
            op->type = OP_BALANCE;
            sscanf(line, "%*s %*d %*s %d", &op->account_id);
        }

        num_transactions++;
    }

    fclose(file);
    return num_transactions;
}