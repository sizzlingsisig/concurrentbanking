#include "../include/lock_mgr.h"
#include "../include/bank.h"
#include "../include/transaction.h"
#include "../include/timer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "../include/main.h"

DeadlockHandler dl_handler;

void dl_init(void) {
    pthread_mutex_init(&dl_handler.lock, NULL);
    pthread_mutex_init(&dl_handler.account_locks_lock, NULL);
    dl_handler.current = STRATEGY_PREVENTION;
    dl_handler.active_tx_count = 0;
    
    for (int i = 0; i < MAX_TXN_TRACKED; i++) {
        dl_handler.wait_graph[i].active = false;
        dl_handler.wait_graph[i].waiting_tx = -1;
        dl_handler.wait_graph[i].waiting_on_acc = -1;
        dl_handler.wait_graph[i].held_by_tx = -1;
    }
    
    for (int i = 0; i < MAX_ACCOUNTS_TRACKED; i++) {
        dl_handler.account_locks[i].account_id = i;
        dl_handler.account_locks[i].held_by_tx = -1;
    }
}

void dl_set_strategy(StrategyType type) {
    pthread_mutex_lock(&dl_handler.lock);
    dl_handler.current = type;
    pthread_mutex_unlock(&dl_handler.lock);
}

void dl_cleanup(void) {
    pthread_mutex_destroy(&dl_handler.lock);
    pthread_mutex_destroy(&dl_handler.account_locks_lock);
}

static int get_account_holder(int account_id) {
    if (account_id < 0 || account_id >= MAX_ACCOUNTS_TRACKED) {
        return -1;
    }
    pthread_mutex_lock(&dl_handler.account_locks_lock);
    int holder = dl_handler.account_locks[account_id].held_by_tx;
    pthread_mutex_unlock(&dl_handler.account_locks_lock);
    return holder;
}

static void set_account_holder(int account_id, int tx_id) {
    if (account_id < 0 || account_id >= MAX_ACCOUNTS_TRACKED) {
        return;
    }
    pthread_mutex_lock(&dl_handler.account_locks_lock);
    dl_handler.account_locks[account_id].held_by_tx = tx_id;
    pthread_mutex_unlock(&dl_handler.account_locks_lock);
}

static void clear_account_holder(int account_id) {
    if (account_id < 0 || account_id >= MAX_ACCOUNTS_TRACKED) {
        return;
    }
    pthread_mutex_lock(&dl_handler.account_locks_lock);
    dl_handler.account_locks[account_id].held_by_tx = -1;
    pthread_mutex_unlock(&dl_handler.account_locks_lock);
}

static void add_to_wait_graph(int tx_id, int account_id, int holder_tx) {
    for (int i = 0; i < MAX_TXN_TRACKED; i++) {
        if (!dl_handler.wait_graph[i].active) {
            dl_handler.wait_graph[i].active = true;
            dl_handler.wait_graph[i].waiting_tx = tx_id;
            dl_handler.wait_graph[i].waiting_on_acc = account_id;
            dl_handler.wait_graph[i].held_by_tx = holder_tx;
            if (i >= dl_handler.active_tx_count) {
                dl_handler.active_tx_count = i + 1;
            }
            break;
        }
    }
}

static void remove_from_wait_graph(int tx_id) {
    for (int i = 0; i < dl_handler.active_tx_count; i++) {
        if (dl_handler.wait_graph[i].active && 
            dl_handler.wait_graph[i].waiting_tx == tx_id) {
            dl_handler.wait_graph[i].active = false;
            dl_handler.wait_graph[i].waiting_tx = -1;
            dl_handler.wait_graph[i].waiting_on_acc = -1;
            dl_handler.wait_graph[i].held_by_tx = -1;
            break;
        }
    }
}

static void abort_transaction_in_cycle(int tx_to_abort) {
    int tx_id = dl_handler.wait_graph[tx_to_abort].waiting_tx;
    
    for (int i = 0; i < num_transactions; i++) {
        if (transactions[i].tx_id == tx_id) {
            transactions[i].status = TX_ABORTED;
            break;
        }
    }
    
    dl_handler.wait_graph[tx_to_abort].active = false;
}

static bool has_cycle_dfs(int tx_idx, bool* visited, bool* rec_stack, int* cycle_start) {
    visited[tx_idx] = true;
    rec_stack[tx_idx] = true;
    
    for (int i = 0; i < dl_handler.active_tx_count; i++) {
        if (!dl_handler.wait_graph[i].active) continue;
        if (dl_handler.wait_graph[i].waiting_tx != tx_idx) continue;
        
        int next = dl_handler.wait_graph[i].held_by_tx;
        if (next < 0) continue;
        
        int next_idx = -1;
        for (int j = 0; j < dl_handler.active_tx_count; j++) {
            if (dl_handler.wait_graph[j].active && dl_handler.wait_graph[j].waiting_tx == next) {
                next_idx = j;
                break;
            }
        }
        
        if (next_idx >= 0) {
            if (!visited[next_idx]) {
                if (has_cycle_dfs(next_idx, visited, rec_stack, cycle_start)) {
                    return true;
                }
            } else if (rec_stack[next_idx]) {
                *cycle_start = next_idx;
                return true;
            }
        }
    }
    
    rec_stack[tx_idx] = false;
    return false;
}

static int check_cycle_and_abort(void) {
    bool visited[MAX_TXN_TRACKED] = {false};
    bool rec_stack[MAX_TXN_TRACKED] = {false};
    int cycle_start = -1;
    
    for (int i = 0; i < dl_handler.active_tx_count; i++) {
        if (!visited[i] && dl_handler.wait_graph[i].active) {
            for (int j = 0; j < MAX_TXN_TRACKED; j++) visited[j] = false;
            for (int j = 0; j < MAX_TXN_TRACKED; j++) rec_stack[j] = false;
            
            if (has_cycle_dfs(i, visited, rec_stack, &cycle_start)) {
                abort_transaction_in_cycle(cycle_start);
                return 1;
            }
        }
    }
    return 0;
}

static int validate_and_precheck(int from_id, int to_id, int amount_centavos) {
    if (!validate_account_id(from_id) || !validate_account_id(to_id)) {
        return 0;
    }
    if (amount_centavos <= 0) {
        return 0;
    }
    return 1;
}

int transfer_prevention(int from_id, int to_id, int amount_centavos);
int transfer_detection(int from_id, int to_id, int amount_centavos);

int dl_transfer(int from_id, int to_id, int amount_centavos) {
    if (!validate_and_precheck(from_id, to_id, amount_centavos)) {
        return 0;
    }
    
    if (from_id == to_id) {
        return 0;
    }
    
    pthread_mutex_lock(&dl_handler.lock);
    StrategyType strategy = dl_handler.current;
    pthread_mutex_unlock(&dl_handler.lock);
    
    if (strategy == STRATEGY_PREVENTION) {
        return transfer_prevention(from_id, to_id, amount_centavos);
    } else {
        return transfer_detection(from_id, to_id, amount_centavos);
    }
}

int transfer_prevention(int from_id, int to_id, int amount_centavos) {
    int first = (from_id < to_id) ? from_id : to_id;
    int second = (from_id < to_id) ? to_id : from_id;

    int tx_id = current_tx_id;

    Account* acc_first = &bank.accounts[first];
    Account* acc_second = &bank.accounts[second];

    pthread_rwlock_wrlock(&acc_first->lock);

    if (verbose && from_id != to_id) {
        printf("  T%d acquired lock on account %d\n", tx_id, first);
    }

    pthread_rwlock_wrlock(&acc_second->lock);

    if (verbose && from_id != to_id) {
        printf("  T%d acquired lock on account %d\n", tx_id, second);
        if (first != from_id) {
            printf("  [DEADLOCK PREVENTED] Lock ordering: T%d waiting for account %d\n", tx_id, second);
        }
    }
    
    Account* from_acc = &bank.accounts[from_id];
    Account* to_acc = &bank.accounts[to_id];
    
    int success = 0;
    if (from_acc->balance_centavos >= amount_centavos) {
        from_acc->balance_centavos -= amount_centavos;
        to_acc->balance_centavos += amount_centavos;
        success = 1;
    }
    
    pthread_rwlock_unlock(&acc_second->lock);
    pthread_rwlock_unlock(&acc_first->lock);
    
    return success;
}

int transfer_detection(int from_id, int to_id, int amount_centavos) {
    int tx_id = current_tx_id;
    
    if (tx_id < 0) {
        return 0;
    }
    
    if (verbose) {
        printf("  [DETECTION] T%d attempting transfer from %d to %d\n", tx_id, from_id, to_id);
    }
    
    Account* from_acc = &bank.accounts[from_id];
    Account* to_acc = &bank.accounts[to_id];
    
    int acquired_from = 0;
    int acquired_to = 0;
    
    int holder_from = get_account_holder(from_id);
    if (holder_from >= 0) {
        add_to_wait_graph(tx_id, from_id, holder_from);
        if (verbose) {
            int tick;
            pthread_mutex_lock(&tick_lock);
            tick = (int)global_tick;
            pthread_mutex_unlock(&tick_lock);
            printf("Tick %d:\n", tick);
            printf("  T%d waiting for lock on account %d (held by T%d)\n", tx_id, from_id, holder_from);
        }
        if (check_cycle_and_abort()) {
            if (verbose) {
                printf("[DEADLOCK DETECTED] T%d aborted to break cycle\n", tx_id);
            }
            remove_from_wait_graph(tx_id);
            return 0;
        }
    }
    pthread_rwlock_wrlock(&from_acc->lock);
    acquired_from = 1;
    set_account_holder(from_id, tx_id);
    if (holder_from >= 0) {
        remove_from_wait_graph(tx_id);
    }
    
    int holder_to = get_account_holder(to_id);
    if (holder_to >= 0) {
        add_to_wait_graph(tx_id, to_id, holder_to);
        if (verbose) {
            int tick;
            pthread_mutex_lock(&tick_lock);
            tick = (int)global_tick;
            pthread_mutex_unlock(&tick_lock);
            printf("Tick %d:\n", tick);
            printf("  T%d waiting for lock on account %d (held by T%d)\n", tx_id, to_id, holder_to);
        }
        if (check_cycle_and_abort()) {
            if (verbose) {
                printf("[DEADLOCK DETECTED] T%d aborted to break cycle\n", tx_id);
            }
            clear_account_holder(from_id);
            pthread_rwlock_unlock(&from_acc->lock);
            remove_from_wait_graph(tx_id);
            return 0;
        }
    }
    pthread_rwlock_wrlock(&to_acc->lock);
    acquired_to = 1;
    set_account_holder(to_id, tx_id);
    if (holder_to >= 0) {
        remove_from_wait_graph(tx_id);
    }
    
    int success = 0;
    if (from_acc->balance_centavos >= amount_centavos) {
        from_acc->balance_centavos -= amount_centavos;
        to_acc->balance_centavos += amount_centavos;
        success = 1;
    }
    
    if (acquired_to) {
        clear_account_holder(to_id);
        pthread_rwlock_unlock(&to_acc->lock);
    }
    if (acquired_from) {
        clear_account_holder(from_id);
        pthread_rwlock_unlock(&from_acc->lock);
    }
    
    remove_from_wait_graph(tx_id);
    
    return success;
}