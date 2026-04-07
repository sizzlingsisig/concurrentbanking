#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

void print_transaction_log(void);
void print_metrics(void);
int check_balance_conservation(int64_t initial_total);
void init_metrics(void);
void cleanup_metrics(void);

extern int total_loads;
extern int total_unloads;
extern int peak_usage;
extern int blocked_operations;

#endif