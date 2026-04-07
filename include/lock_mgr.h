#ifndef LOCK_MGR_H
#define LOCK_MGR_H

#include <stdbool.h>

typedef enum {
    DEADLOCK_PREVENTION,
    DEADLOCK_DETECTION
} DeadlockStrategy;

extern DeadlockStrategy deadlock_strategy;

int transfer(int from_id, int to_id, int amount_centavos);

#endif