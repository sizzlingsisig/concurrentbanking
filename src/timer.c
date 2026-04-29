#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "../include/timer.h"


/**
 * Global simulation clock and synchronization primitives.
 *
 * global_tick:   Current simulation time, incremented every tick_interval_ms.
 * tick_lock:     Mutex protecting global_tick and the condition variable.
 * tick_changed:  Condition variable; timer broadcasts on each tick increment
 *                 to wake all transaction threads waiting on wait_until_tick().
 * tick_interval_ms: Duration of one simulation tick in milliseconds.
 * simulation_running: Flag set to 0 by stop_timer() to signal thread exit.
 */
volatile int64_t global_tick = 0;
pthread_mutex_t tick_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tick_changed = PTHREAD_COND_INITIALIZER;
int tick_interval_ms = 100;
volatile int simulation_running = 1;

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
 * Timer thread worker loop. Runs until stop_timer() sets simulation_running = 0.
 *
 * Each iteration: sleeps for tick_interval_ms, then increments global_tick
 * and broadcasts tick_changed to wake all transaction threads waiting on
 * wait_until_tick(). The mutex is unlocked during the sleep so that other
 * threads can safely read global_tick while the timer is sleeping.
 */
void* timer_thread(void* arg) {
    (void)arg; // unused parameter
    
    pthread_mutex_lock(&tick_lock);
    while (simulation_running) {
        // Unlock while sleeping so other threads can access global_tick
        pthread_mutex_unlock(&tick_lock);
        
        // Sleep for the defined interval
        usleep(tick_interval_ms * 1000);
        
        pthread_mutex_lock(&tick_lock);
        
        if (simulation_running) {
            global_tick++;
            
            // Wake up all transactions waiting for this specific tick
            pthread_cond_broadcast(&tick_changed);
        }
    }
    pthread_mutex_unlock(&tick_lock);
    
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
 * Signals the timer thread to stop (call before pthread_join).
 */
void stop_timer(void) {
    pthread_mutex_lock(&tick_lock);
    simulation_running = 0;
    pthread_cond_broadcast(&tick_changed);
    pthread_mutex_unlock(&tick_lock);
}

/**
 * Cleans up timer resources (call after pthread_join).
 */
void cleanup_timer(void) {
    pthread_mutex_destroy(&tick_lock);
    pthread_cond_destroy(&tick_changed);
}