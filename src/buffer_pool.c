#include "../include/buffer_pool.h"
#include "../include/bank.h"
#include "../include/metrics.h"
#include <stdio.h>

BufferPool buffer_pool;

extern int total_unloads;

/**
 * Initializes the buffer pool with 5 empty slots.
 * Uses semaphores to coordinate access.
 */
void init_buffer_pool(void) {
    // 0 indicates the semaphore is shared between threads
    sem_init(&buffer_pool.empty_slots, 0, BUFFER_POOL_SIZE);
    sem_init(&buffer_pool.full_slots, 0, 0);
    pthread_mutex_init(&buffer_pool.pool_lock, NULL);

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        buffer_pool.slots[i].in_use = false;
        buffer_pool.slots[i].account_id = -1;
        buffer_pool.slots[i].data = NULL;
    }
}

/**
 * Loads an account into an available slot.
 * Blocks if the pool is full.
 */
void load_account(int account_id) {
    if (!validate_account_id(account_id)) {
        return;
    }

    pthread_mutex_lock(&buffer_pool.pool_lock);

    // Check if loaded
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool.slots[i].in_use &&
            buffer_pool.slots[i].account_id == account_id) {
            buffer_pool.slots[i].pin_count++;

            // Already loaded → just reuse
            pthread_mutex_unlock(&buffer_pool.pool_lock);
            return;
        }
    }

    pthread_mutex_unlock(&buffer_pool.pool_lock);

    // THEN do normal loading
    sem_wait(&buffer_pool.empty_slots);

    pthread_mutex_lock(&buffer_pool.pool_lock);

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool.slots[i].in_use &&
            buffer_pool.slots[i].account_id == account_id) {

            buffer_pool.slots[i].pin_count++;
            pthread_mutex_unlock(&buffer_pool.pool_lock);

            sem_post(&buffer_pool.empty_slots); // give slot back
            return;
        }
    }

    total_loads++;

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!buffer_pool.slots[i].in_use) {
            buffer_pool.slots[i].account_id = account_id;
            buffer_pool.slots[i].data = &bank.accounts[account_id];
            buffer_pool.slots[i].in_use = true;
            buffer_pool.slots[i].pin_count = 1;
            break;
        }
    }

    pthread_mutex_unlock(&buffer_pool.pool_lock);
    sem_post(&buffer_pool.full_slots);
    
}

/**
 * Removes an account from the pool, freeing up a slot.
 */
void unload_account(int account_id) {
    if (!validate_account_id(account_id)) {
        return;
    }
    
    pthread_mutex_lock(&buffer_pool.pool_lock);

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool.slots[i].in_use &&
            buffer_pool.slots[i].account_id == account_id) {

            if (buffer_pool.slots[i].pin_count > 0) {
                buffer_pool.slots[i].pin_count--;
            }

            if (buffer_pool.slots[i].pin_count == 0) {
                buffer_pool.slots[i].in_use = false;
                buffer_pool.slots[i].account_id = -1;
                buffer_pool.slots[i].data = NULL;

                total_unloads++;

                pthread_mutex_unlock(&buffer_pool.pool_lock);
                sem_post(&buffer_pool.empty_slots);
                return;
            }

            // Still in use by others
            pthread_mutex_unlock(&buffer_pool.pool_lock);
            return;
        }
    }

    // Not found (shouldn't happen)
    pthread_mutex_unlock(&buffer_pool.pool_lock);
}

/**
 * Cleans up buffer pool resources.
 */
void cleanup_buffer_pool(void) {
    sem_destroy(&buffer_pool.empty_slots);
    sem_destroy(&buffer_pool.full_slots);
    pthread_mutex_destroy(&buffer_pool.pool_lock);
}