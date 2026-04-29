#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <pthread.h>

#define MAX_OPS_PER_TX 256

extern __thread int current_tx_id;
#define MAX_TRANSACTIONS 1000
#define LINE_BUFFER_SIZE 256
#define OPERATION_DELAY_NS 100000000  // 100ms in nanoseconds

typedef enum {
    OP_DEPOSIT,
    OP_WITHDRAW,
    OP_TRANSFER,
    OP_BALANCE
} OpType;

typedef struct {
    OpType type;
    int account_id;
    int amount_centavos;
    int target_account;
} Operation;

typedef enum {
    TX_RUNNING,
    TX_COMMITTED,
    TX_ABORTED,
    TX_PENDING
} TxStatus;

typedef struct {
    int tx_id;
    Operation ops[MAX_OPS_PER_TX];
    int num_ops;
    int start_tick;
    pthread_t thread;
    int actual_start;
    int actual_end;
    int wait_ticks;
    TxStatus status;
} Transaction;


typedef void (*TxCallback)(Transaction*);

void register_tx_completed(TxCallback callback);

#define MAX_TRANSACTIONS 1000
extern Transaction transactions[MAX_TRANSACTIONS];
extern int num_transactions;

void* execute_transaction(void* arg);
int load_transactions_from_file(const char* filename);
void wait_for_all_transactions(void);

#endif