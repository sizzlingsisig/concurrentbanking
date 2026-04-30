#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>
#include <stdint.h>

#define DEFAULT_TICK_INTERVAL_MS 100

extern volatile int64_t global_tick;
extern pthread_mutex_t tick_lock;
extern pthread_cond_t tick_changed;
extern int tick_interval_ms;
extern volatile int simulation_running;

void init_timer(int interval_ms);
void* timer_thread(void* arg);
void wait_until_tick(int64_t target_tick);
void stop_timer(void);
void cleanup_timer(void);

#endif