#include "../include/timer.h"
#include <unistd.h>
#include <stdio.h>

// Global simulation variables
volatile int64_t global_tick = 0;
pthread_mutex_t tick_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tick_changed = PTHREAD_COND_INITIALIZER;
int tick_interval_ms = 100;
int simulation_running = 1;

/**
 * Initializes the timer infrastructure.
 */
void init_timer(int interval_ms) {
    pthread_mutex_lock(&tick_lock);
    tick_interval_ms = interval_ms;
    global_tick = 0;
    simulation_running = 1;
    pthread_mutex_unlock(&tick_lock);
}

/**
 * The core timer loop. Runs in its own thread to advance global time.
 *
 */
void* timer_thread(void* arg) {
    while (simulation_running) {
        // Sleep for the defined interval
        usleep(tick_interval_ms * 1000);

        pthread_mutex_lock(&tick_lock);
        global_tick++;
        
        // Wake up all transactions waiting for this specific tick
        pthread_cond_broadcast(&tick_changed);
        pthread_mutex_unlock(&tick_lock);
    }
    return NULL;
}

/**
 * Blocks the calling transaction thread until the global clock reaches target_tick.
 *
 */
void wait_until_tick(int64_t target_tick) {
    pthread_mutex_lock(&tick_lock);
    
    // Standard CV pattern: use a while loop to handle spurious wakeups
    while (global_tick < target_tick) {
        pthread_cond_wait(&tick_changed, &tick_lock);
    }
    
    pthread_mutex_unlock(&tick_lock);
}

/**
 * Cleans up timer resources and signals the thread to stop.
 */
void cleanup_timer(void) {
    pthread_mutex_lock(&tick_lock);
    simulation_running = 0;
    pthread_cond_broadcast(&tick_changed);
    pthread_mutex_unlock(&tick_lock);
    
    pthread_mutex_destroy(&tick_lock);
    pthread_cond_destroy(&tick_changed);
}