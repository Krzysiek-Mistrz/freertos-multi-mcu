#include <stdint.h>
#include "rtos.h"
#include "bsp.h"
#include "app_pipeline.h"
#include "app_primitives.h"


#define POOL_SLOTS  3U
#define NUM_WORKERS 5U
#define MAX_TRACKED 10U

/*
semaphore is a non-negative integer variable with two atomic ops -> Wait / Take
Decrements the counter by 1. If the counter is 0, the task becomes blocked until a "Give" op is performed.
Signal & Give Increments the counter by 1. If there are tasks blocked on this 
semaphore, it wakes one of them (the one with the highest priority).
*/
static SemaphoreHandle_t s_mutex;
static SemaphoreHandle_t s_pool;
static SemaphoreHandle_t s_button_sem;
static TimerHandle_t s_timer;
static TaskHandle_t s_stats_task;
static volatile uint32_t s_shared_counter;
static TaskHandle_t s_tracked[MAX_TRACKED];
static uint32_t s_tracked_n;

// @brief track task handle for later stats -> we just store it in an array
static void track(TaskHandle_t h) {
    if (h != NULL && s_tracked_n < MAX_TRACKED) {
        s_tracked[s_tracked_n++] = h;
    }
}

// @brief mutex task that increments a shared counter with a mutex lock
static void mutex_task(void *arg) {
    // casting 2 uintptr first so that the val has 
    // min 32b and then after castig 2 uint32 
    const uint32_t id = (uint32_t)(uintptr_t)arg; 

    for (;;) {
        // semaphoretake waits up 50ms 4 mutex 2 become free, either task gets lock or 50ms pass
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            uint32_t tmp = s_shared_counter;
            vTaskDelay(1);
            s_shared_counter = tmp + 1U;
            // release the lock so the other task can take it
            xSemaphoreGive(s_mutex);
        }

        // delay for 40ms + id*10ms so that each task has a different delay
        vTaskDelay(pdMS_TO_TICKS(40U + id * 10U)); 
    }
}

// @brief worker task takes a slot 4m pool and "uses" it for 800ms, if no slot is available it times out after 500ms
static void worker_task(void *arg) {
    const uint32_t id = (uint32_t)(uintptr_t)arg;

    for (;;) {
        if (xSemaphoreTake(s_pool, pdMS_TO_TICKS(500)) == pdTRUE) {
            // uxSemaphoreGetCount returns the current count of the semaphore, which is the number of available slots
            app_log("worker %lu -> occupied slot (free: %lu)\r\n",
            (unsigned long)id, (unsigned long)uxSemaphoreGetCount(s_pool));
            vTaskDelay(pdMS_TO_TICKS(800));
            xSemaphoreGive(s_pool);
        } else {
            app_log("worker %lu -> timeout, no free slot\r\n", (unsigned long)id);
        }
        // without this delay the same worker immediately retakes the slot & others starve
        vTaskDelay(pdMS_TO_TICKS(2000U + id * 300U));
    }
}

// @brief on button task waits for a button press and immediately button task can execute
void app_on_button_isr(void) {
    BaseType_t woken = pdFALSE;
    // give the semaphore from ISR so that the button task can take it and process the button press & woken gets true
    xSemaphoreGiveFromISR(s_button_sem, &woken);
    // portYIELD_FROM_ISR will yield to button task if woken is true
    portYIELD_FROM_ISR(woken);
}

// @brief button task waits for a button press and toggles the LED, it also debounces the button press by waiting 50ms
static void button_task(void *arg) {
    (void)arg;
    uint32_t presses = 0;

    for (;;) {
        xSemaphoreTake(s_button_sem, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(50));
        // clear any extra button presses that may have occurred during debounce period
        while (xSemaphoreTake(s_button_sem, 0) == pdTRUE) {}
        app_log("button -> press #%lu\r\n", (unsigned long)++presses);
        bsp_led_toggle();
    }
}

// @brief timer callback that notifies the stats task to execute
static void timer_cb(TimerHandle_t t) {
    (void)t;
    // notify the stats task for it to execute
    xTaskNotifyGive(s_stats_task);
}

static void stats_task(void *arg) {
    (void)arg;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        app_log("stats -> shared counter: %lu, free heap: %lu\r\n",
                (unsigned long)s_shared_counter, (unsigned long)xPortGetFreeHeapSize());
        
        for (uint32_t i = 0; i < s_tracked_n; i++) {
            // print the minimum free stack space 4 each tracked task
            // pcTaskGetName returns name of the task & uxTaskGetStackHighWaterMark 
            // returns the minimum amount of stack space that has been left for the task since it started
            app_log("%s stack min free = %lu\r\n", pcTaskGetName(s_tracked[i]),
                    (unsigned long)uxTaskGetStackHighWaterMark(s_tracked[i]));
        }
    }
}

void app_primitives_start(void) {
    s_mutex = xSemaphoreCreateMutex(); // creates mutex bin which one task have at one time
    s_pool = xSemaphoreCreateCounting(POOL_SLOTS, POOL_SLOTS); // creates counting semaphore with 3 slots, initial count is 3, slot is basically a mutex but multiple slots means that many task can have for ex 3 mutexes and 4-th wont execute...
    s_button_sem = xSemaphoreCreateBinary(); // creates bin semaphore which is initially empty, so button task will block until button is pressed
    s_timer = xTimerCreate("stats", pdMS_TO_TICKS(5000), pdTRUE, NULL, timer_cb); // tworzy soft timer 
    configASSERT(s_mutex && s_pool && s_button_sem && s_timer);
    TaskHandle_t h;
    BaseType_t ok = pdPASS;
    // mutex_task func 4 task; "mutexA" name 4 task; APP_STACK_WORDS(192) -> stack: 192 words (eg. 192×4 = 768 B); 
    // (void *)(uintptr_t)0 -> id taska (0); 1 — priority; &h — handle
    ok &= xTaskCreate(mutex_task, "mutexA", APP_STACK_WORDS(192), (void *)(uintptr_t)0, 1, &h); track(h);
    ok &= xTaskCreate(mutex_task, "mutexB", APP_STACK_WORDS(192), (void *)(uintptr_t)1, 2, &h); track(h);
    static const char *names[NUM_WORKERS] = {"worker0", "worker1", "worker2", "worker3", "worker4"};

    for (uint32_t i = 0; i < NUM_WORKERS; i++) {
        ok &= xTaskCreate(worker_task, names[i], APP_STACK_WORDS(512), (void *)(uintptr_t)i, 1, &h);
        track(h);
    }

    ok &= xTaskCreate(button_task, "button", APP_STACK_WORDS(512), NULL, 1, &h); track(h);
    ok &= xTaskCreate(stats_task, "stats", APP_STACK_WORDS(512), NULL, 1, &s_stats_task); 
    track(s_stats_task);
    configASSERT(ok == pdPASS);
    xTimerStart(s_timer, 0); // starts the timer now after each 5 seconds timer_cb will be called and stats_task will be notified to exec
    bsp_button_init();
}