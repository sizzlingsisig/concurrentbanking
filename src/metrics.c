#include "../include/metrics.h"
#include "../include/bank.h"
#include "../include/transaction.h"
#include "../include/timer.h"
#include <stdio.h>

// Global stats for Buffer Pool performance
int total_loads = 0;
int total_unloads = 0;
int peak_usage = 0;
int blocked_operations = 0;

/**
 * Initializes metrics-related counters.
 */
void init_metrics(void) {
    total_loads = 0;
    total_unloads = 0;
    peak_usage = 0;
    blocked_operations = 0;
}

/**
 * Iterates through all completed transactions and prints their 
 * lifecycle details to the console.
 */
void print_transaction_log(void) {
    printf("\n--- Transaction Log ---\n");
    printf("ID\tStatus\t\tScheduled\tActual\tEnd\tWait Ticks\n");
    for (int i = 0; i < num_transactions; i++) {
        Transaction* tx = &transactions[i];
        const char* status_str = (tx->status == TX_COMMITTED) ? "COMMITTED" : "ABORTED";
        printf("%d\t%s\t%d\t\t%d\t%d\t%d\n", 
               tx->tx_id, status_str, tx->start_tick, 
               tx->actual_start, tx->actual_end, tx->wait_ticks);
    }
}

/**
 * Prints a high-level summary of the simulation performance.
 */
void print_metrics(void) {
    int committed = 0;
    int aborted = 0;
    long total_wait = 0;

    for (int i = 0; i < num_transactions; i++) {
        if (transactions[i].status == TX_COMMITTED) committed++;
        else aborted++;
        total_wait += transactions[i].wait_ticks;
    }

    double avg_wait = (num_transactions > 0) ? (double)total_wait / num_transactions : 0;
    double throughput = (global_tick > 0) ? (double)committed / global_tick : 0;

    printf("\n--- Simulation Summary ---\n");
    printf("Total Transactions: %d\n", num_transactions);
    printf("Committed:          %d\n", committed);
    printf("Aborted:            %d\n", aborted);
    printf("Throughput:         %.2f tx/tick\n", throughput);
    printf("Average Wait Time:  %.2f ticks\n", avg_wait);
    
    printf("\n--- Buffer Pool Stats ---\n");
    printf("Total Loads:        %d\n", total_loads);
    printf("Peak Usage:         %d/5 slots\n", peak_usage);
    printf("Blocked Ops:        %d\n", blocked_operations);
}

/**
 * Verification step: Sums all account balances and compares 
 * against the starting total.
 */
int check_balance_conservation(int64_t initial_total) {
    int64_t final_total = (int64_t)get_total_balance();
    
    printf("\n--- Balance Conservation Check ---\n");
    printf("Initial Total: PHP %ld.%02ld\n", initial_total / 100, initial_total % 100);
    printf("Final Total:   PHP %ld.%02ld\n", final_total / 100, final_total % 100);
    
    if (initial_total == final_total) {
        printf("Result:        SUCCESS (Balances match)\n");
        return 1;
    } else {
        printf("Result:        FAILURE (Money was created or lost!)\n");
        return 0;
    }
}

void cleanup_metrics(void) {
    // Placeholder for any dynamic memory cleanup if added later
}