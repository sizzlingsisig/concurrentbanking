#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include "transaction.h"

void register_metrics_listener(TxCallback callback);
void on_transaction_completed(Transaction* tx);
void print_transaction_log(void);
void print_metrics(void);
int check_balance_conservation(int64_t initial_total);
void init_metrics(void);
void cleanup_metrics(void);
void track_buffer_metrics(int current_usage, bool blocked);

extern int total_loads;
extern int total_unloads;
extern int peak_usage;
extern int blocked_operations;

extern int committed_count;
extern int aborted_count;
extern long total_wait_ticks;
extern int total_completed;

#endif