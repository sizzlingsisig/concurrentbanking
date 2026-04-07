#include "../include/timer.h"

volatile int64_t global_tick = 0;
pthread_mutex_t tick_lock;
pthread_cond_t tick_changed;
int tick_interval_ms = 100;
int simulation_running = 0;

void init_timer(int interval_ms) {
}

void* timer_thread(void* arg) {
    return NULL;
}

void wait_until_tick(int64_t target_tick) {
}

void cleanup_timer(void) {
}