#include "../include/buffer_pool.h"
#include "../include/bank.h"
#include "../include/metrics.h"
#include <stdio.h>

BufferPool buffer_pool;

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
    // Wait for an empty slot
    if (sem_wait(&buffer_pool.empty_slots) != 0) {
        // Check if we blocked
        blocked_operations++;
    }
    
    pthread_mutex_lock(&buffer_pool.pool_lock);
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!buffer_pool.slots[i].in_use) {
            buffer_pool.slots[i].account_id = account_id;
            buffer_pool.slots[i].data = &bank.accounts[account_id];
            buffer_pool.slots[i].in_use = true;
            
            // Track metrics
            total_loads++;
            int current_usage = 0;
            for (int j = 0; j < BUFFER_POOL_SIZE; j++) {
                if (buffer_pool.slots[j].in_use) current_usage++;
            }
            if (current_usage > peak_usage) {
                peak_usage = current_usage;
            }
            break;
        }
    }
    pthread_mutex_unlock(&buffer_pool.pool_lock);
    
    // Increment full slots count
    sem_post(&buffer_pool.full_slots);
}

/**
 * Removes an account from the pool, freeing up a slot.
 */
void unload_account(int account_id) {
    // Wait for a full slot to be available for unloading
    sem_wait(&buffer_pool.full_slots);
    
    pthread_mutex_lock(&buffer_pool.pool_lock);
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (buffer_pool.slots[i].in_use && buffer_pool.slots[i].account_id == account_id) {
            buffer_pool.slots[i].in_use = false;
            buffer_pool.slots[i].account_id = -1;
            buffer_pool.slots[i].data = NULL;
            
            // Track metrics
            total_unloads++;
            break;
        }
    }
    pthread_mutex_unlock(&buffer_pool.pool_lock);
    
    // Signal that an empty slot is now available
    sem_post(&buffer_pool.empty_slots);
}

/**
 * Cleans up buffer pool resources.
 */
void cleanup_buffer_pool(void) {
    sem_destroy(&buffer_pool.empty_slots);
    sem_destroy(&buffer_pool.full_slots);
    pthread_mutex_destroy(&buffer_pool.pool_lock);
}