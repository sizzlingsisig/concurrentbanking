#ifndef LOCK_MGR_H
#define LOCK_MGR_H

#include <stdbool.h>
#include <pthread.h>

#define MAX_TXN_TRACKED 1000

typedef enum {
    STRATEGY_PREVENTION,
    STRATEGY_DETECTION
} StrategyType;

typedef struct {
    int waiting_tx;      // Transaction waiting
    int waiting_on_acc; // Account ID being waited on
    int held_by_tx;     // Transaction holding the lock (-1 if none)
    bool active;
} WaitGraphEntry;

typedef struct {
    StrategyType current;
    pthread_mutex_t lock;
    
    // Detection-specific state
    WaitGraphEntry wait_graph[MAX_TXN_TRACKED];
    int active_tx_count;
} DeadlockHandler;

extern DeadlockHandler dl_handler;

void dl_init(void);
void dl_set_strategy(StrategyType type);
int dl_transfer(int from_id, int to_id, int amount_centavos);
void dl_cleanup(void);

// Backward compatibility
#define deadlock_strategy dl_handler.current
#define transfer dl_transfer

#endif