#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "../include/bank.h"
#include "../include/timer.h"
#include "../include/lock_mgr.h"
#include "../include/transaction.h"
#include "../include/metrics.h"
#include "../include/buffer_pool.h"

// Global flags for CLI options
char* accounts_file = NULL;
char* trace_file = NULL;
int tick_ms = 100;
int verbose = 0;

void print_usage(char* prog_name) {
    printf("Usage: %s --accounts=FILE --trace=FILE [options]\n", prog_name);
    printf("Options:\n");
    printf("  --accounts=FILE           Initial account balances (required)\n");
    printf("  --trace=FILE              Transaction workload (required)\n");
    printf("  --deadlock=strategy       'prevention' or 'detection' (default: prevention)\n");
    printf("  --tick-ms=N               Milliseconds per tick (default: 100)\n");
    printf("  --verbose                 Print detailed logs\n");
}

int main(int argc, char* argv[]) {
    int opt;
    int option_index = 0;

    static struct option long_options[] = {
        {"accounts", required_argument, 0, 'a'},
        {"trace",    required_argument, 0, 't'},
        {"deadlock", required_argument, 0, 'd'},
        {"tick-ms",  required_argument, 0, 'm'},
        {"verbose",  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "a:t:d:m:v", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                accounts_file = optarg;
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'd':
                if (strcmp(optarg, "detection") == 0) {
                    deadlock_strategy = DEADLOCK_DETECTION;
                } else {
                    deadlock_strategy = DEADLOCK_PREVENTION;
                }
                break;
            case 'm':
                tick_ms = atoi(optarg);
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Validation: Ensure required files are provided
    if (!accounts_file || !trace_file) {
        fprintf(stderr, "Error: Missing required arguments.\n");
        print_usage(argv[0]);
        return 1;
    }

    // --- Start Simulation ---
    init_bank();
    init_buffer_pool();
    init_metrics();
    register_metrics_listener(on_transaction_completed);
    init_timer(tick_ms);

    if (load_accounts_from_file(accounts_file) < 0) return 1;
    int64_t initial_total = get_total_balance();
    
    if (load_transactions_from_file(trace_file) < 0) return 1;

    // Spawn Timer Thread
    pthread_t timer_tid;
    pthread_create(&timer_tid, NULL, timer_thread, NULL);

    // Spawn Transaction Threads
    for (int i = 0; i < num_transactions; i++) {
        pthread_create(&transactions[i].thread, NULL, execute_transaction, &transactions[i]);
    }

    wait_for_all_transactions();

    // Cleanup and Report
    // Signal timer to stop, then join, then cleanup
    stop_timer();
    pthread_join(timer_tid, NULL);
    cleanup_timer();

    print_transaction_log();
    print_metrics();
    check_balance_conservation(initial_total);

    return 0;
}