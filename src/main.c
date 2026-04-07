#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/bank.h"
#include "../include/transaction.h"
#include "../include/timer.h"
#include "../include/lock_mgr.h"
#include "../include/buffer_pool.h"
#include "../include/metrics.h"

void print_usage(const char* prog) {
    printf("Usage: %s --accounts=FILE --trace=FILE --deadlock=prevention|detection [options]\n", prog);
    printf("Options:\n");
    printf("  --tick-ms=N       Milliseconds per tick (default: 100)\n");
    printf("  --verbose        Print detailed operation logs\n");
}

int main(int argc, char* argv[]) {
    printf("Concurrent Banking System\n");
    printf("Phase 1: Placeholder main - all modules linked\n");
    return 0;
}