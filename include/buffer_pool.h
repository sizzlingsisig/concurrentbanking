#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define BUFFER_POOL_SIZE 5

typedef struct {
    int account_id;
    void* data;
    bool in_use;
} BufferSlot;

typedef struct {
    BufferSlot slots[BUFFER_POOL_SIZE];
    sem_t empty_slots;
    sem_t full_slots;
    pthread_mutex_t pool_lock;
} BufferPool;

extern BufferPool buffer_pool;

void init_buffer_pool(void);
void load_account(int account_id);
void unload_account(int account_id);
void cleanup_buffer_pool(void);

#endif