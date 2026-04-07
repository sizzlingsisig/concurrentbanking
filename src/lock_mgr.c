#include "../include/lock_mgr.h"
#include "../include/bank.h"

DeadlockStrategy deadlock_strategy = DEADLOCK_PREVENTION;

int transfer(int from_id, int to_id, int amount_centavos) {
    return 1;
}