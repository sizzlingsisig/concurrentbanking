#include "../include/metrics.h"
#include "../include/bank.h"
#include "../include/timer.h"
#include "../include/buffer_pool.h"
#include <stdio.h>
#include <stdint.h>

#include "../include/main.h"

static pthread_mutex_t metrics_lock = PTHREAD_MUTEX_INITIALIZER;

static int64_t net_external_flow = 0;

int committed_count = 0;
int aborted_count = 0;
long total_wait_ticks = 0;
int total_completed = 0;

int total_loads = 0;
int total_unloads = 0;
int peak_usage = 0;
int blocked_operations = 0;

void on_transaction_completed(Transaction* tx) {
    pthread_mutex_lock(&metrics_lock);
    total_completed++;
    if (tx->status == TX_COMMITTED) {
        committed_count++;
    } else {
        aborted_count++;
    }
    total_wait_ticks += tx->wait_ticks;
    pthread_mutex_unlock(&metrics_lock);
}

/**
 * Initializes metrics-related counters.
 */
void record_external_flow(int64_t delta) {
    pthread_mutex_lock(&metrics_lock);
    net_external_flow += delta;
    pthread_mutex_unlock(&metrics_lock);
}

void init_metrics(void) {
    net_external_flow = 0;
    committed_count = 0;
    aborted_count = 0;
    total_wait_ticks = 0;
    total_completed = 0;
    total_loads = 0;
    total_unloads = 0;
    peak_usage = 0;
    blocked_operations = 0;
}

void register_metrics_listener(TxCallback callback) {
    register_tx_completed(callback);
}

/**
 * Iterates through all completed transactions and prints their
 * lifecycle details to the console.
 */
void print_transaction_log(void) {
    if (verbose) {
        printf("\n=== Transaction Performance Metrics ===\n");
        printf("TxID | StartTick | ActualStart | End | WaitTicks | Status\n");
        printf("-----|-----------|-------------|-----|-----------|----------\n");
        for (int i = 0; i < num_transactions; i++) {
            Transaction* tx = &transactions[i];
            const char* status_str = (tx->status == TX_COMMITTED) ? "COMMITTED" : "ABORTED";
            printf("T%-3d | %9d | %11d | %3d | %9d | %s\n",
                   tx->tx_id, tx->start_tick,
                   tx->actual_start, tx->actual_end, tx->wait_ticks, status_str);
        }
    } else {
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
}

/**
 * Prints a high-level summary of the simulation performance.
 */
void print_metrics(void) {
    int total = committed_count + aborted_count;
    double avg_wait = (total > 0) ? (double)total_wait_ticks / total : 0;
    double throughput = (global_tick > 0) ? (double)committed_count / global_tick : 0;

    if (verbose) {
        printf("\nAverage wait time: %.1f ticks\n", avg_wait);
        printf("Throughput: %d transactions / %ld ticks = %.2f tx/tick\n",
               committed_count, (long)global_tick, throughput);
    } else {
        printf("\n--- Simulation Summary ---\n");
        printf("Total Transactions: %d\n", total_completed);
        printf("Committed:          %d\n", committed_count);
        printf("Aborted:            %d\n", aborted_count);
        printf("Throughput:         %.2f tx/tick\n", throughput);
        printf("Average Wait Time:  %.2f ticks\n", avg_wait);
    }

    if (verbose) {
        printf("\n=== Buffer Pool Report ===\n");
        printf("Pool size: %d slots\n", BUFFER_POOL_SIZE);
        printf("Total loads: %d\n", total_loads);
        printf("Total unloads: %d\n", total_unloads);
        printf("Peak usage: %d slots\n", peak_usage);
        printf("Blocked operations (pool full): %d\n", blocked_operations);
    } else {
        printf("\n--- Buffer Pool Stats ---\n");
        printf("Total Loads:        %d\n", total_loads);
        printf("Peak Usage:         %d/%d slots\n", peak_usage, BUFFER_POOL_SIZE);
        printf("Blocked Ops:        %d\n", blocked_operations);
    }
}

/**
 * Verification step: Sums all account balances and compares 
 * against the starting total.
 */
int check_balance_conservation(int64_t initial_total) {
    int64_t final_total = (int64_t)get_total_balance();
    int64_t expected_total = initial_total + net_external_flow;
    int64_t abs_net = net_external_flow < 0 ? -net_external_flow : net_external_flow;
    
    printf("\n--- Balance Conservation Check ---\n");
    printf("Initial Total: PHP %ld.%02ld\n", initial_total / 100, initial_total % 100);
    printf("Net External:  PHP %+ld.%02ld\n", net_external_flow / 100, abs_net % 100);
    printf("Final Total:   PHP %ld.%02ld\n", final_total / 100, final_total % 100);
    
    if (final_total == expected_total) {
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

void track_buffer_metrics(int current_usage, bool blocked) {
    pthread_mutex_lock(&metrics_lock);
    if (blocked) {
        blocked_operations++;
    }
    if (current_usage > peak_usage) {
        peak_usage = current_usage;
    }
    pthread_mutex_unlock(&metrics_lock);
}
