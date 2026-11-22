#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "rtos.h"
#include "bsp.h"
#include "app_pipeline.h"
#include "app_primitives.h"
#include "fsm.h" // embedkit: pure C, no hw & no rtos deps


#define SAMPLE_PERIOD_MS 100U // period for taskdelayuntil
#define WINDOW_SIZE      8U // moving average over 8 samples
#define ALARM_THRESHOLD  3000 // average above this -> alarm
#define LOG_MSG_MAX      96U // max lenh of log message
#define BIT_ALARM        (1U << 0) // shifting 1 by 0 bits to create a bitmask for the alarm event

// @brief struct 4 storing sample w/ timestamp & value
typedef struct {
    TickType_t ts; // timestamp of sample
    int32_t value; 
} sample_t;

typedef struct {
    char text[LOG_MSG_MAX]; // text 4 log message
} log_msg_t;

static QueueHandle_t s_sample_q; // queue handle 4 samples
static QueueHandle_t s_log_q; // queue handle 4 log messages
static EventGroupHandle_t s_events; // event group handle 4 events it acts as alarm = bitmask 4 events to be set or cleared

// @brief logging func called 4m any task
void app_log(const char *fmt, ...) {
    log_msg_t m; // create log message struct
    va_list ap; // variable arg list

    va_start(ap, fmt); // initialize variable arg list
    vsnprintf(m.text, sizeof(m.text), fmt, ap); // format log message
    va_end(ap); // clean up vars
    // logging should never impede real-time task
    (void)xQueueSend(s_log_q, &m, 0);
}

// @brief logger task
static void logger_task(void *arg) {
    (void)arg;
    log_msg_t m; // create log message struct

    for (;;) {
        if (xQueueReceive(s_log_q, &m, portMAX_DELAY) == pdPASS) {
            bsp_log_write(m.text);
        }
    }
}

// @brief producer task 4 sensor readings
static void sensor_task(void *arg) {
    (void)arg; // ignore unused arg casting 2 void 4 ignoring -Wunused-parameter
    TickType_t last_wake = xTaskGetTickCount(); // get current tick count

    for (;;) {
        sample_t s = { 
            .ts = xTaskGetTickCount(),
            .value = bsp_sensor_read()
        }; // create sample struct with timestamp and sensor value

        if (xQueueSend(s_sample_q, &s, 0) != pdPASS) {
            app_log("warning -> sample queue full, drop\r\n");
        } // wait until next sample period

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS)); // maintain const, indep of loop execution time
    }
}

// alarm state machine (embedkit fsm)
// NORMAL --HIGH--> ALARM  (action: set event bit 4 alarm_task)
// ALARM  --LOW --> NORMAL (action: log)
// hysteresis stops the alarm from flapping near the threshold & the bit
// is set once per entering the alarm instead of every single sample
#define ALARM_HYSTERESIS 300

enum { ST_NORMAL = 0, ST_ALARM };
enum { EV_HIGH = 0, EV_LOW };

static void on_alarm_enter(void *ctx) {
    (void)ctx;
    xEventGroupSetBits(s_events, BIT_ALARM);
}

static void on_alarm_leave(void *ctx) {
    (void)ctx;
    app_log("alarm cleared\r\n");
}

static const fsm_transition_t k_alarm_table[] = {
    { ST_NORMAL, EV_HIGH, ST_ALARM,  on_alarm_enter },
    { ST_ALARM,  EV_LOW,  ST_NORMAL, on_alarm_leave },
};

// @brief processing task for saving queue data into moving 
// win 4 average and alarm detection
static void process_task(void *arg) {
    (void)arg;
    fsm_t alarm_fsm;
    fsm_init(&alarm_fsm, k_alarm_table, sizeof(k_alarm_table) / sizeof(k_alarm_table[0]), ST_NORMAL, NULL);
    int32_t window[WINDOW_SIZE] = {0};
    uint32_t idx = 0, filled = 0;
    int32_t sum = 0;
    sample_t s;
    uint32_t n = 0;

    for (;;) {
        xQueueReceive(s_sample_q, &s, portMAX_DELAY); // wait 4 sample 4m queue
        sum -= window[idx]; // subtract oldest sample from sum
        window[idx] = s.value; // store new sample in window
        sum += s.value; // add new sample to sum
        idx = (idx + 1U) % WINDOW_SIZE; // increment index, wrap around if necessary

        // increment filled count until window is full
        if (filled < WINDOW_SIZE) {
            filled++;
        }

        int32_t avg = sum / (int32_t)filled;

        if ((++n % 10U) == 0U) {
            app_log("t=%lu, raw=%ld, avg=%ld\r\n", (unsigned long)s.ts, (long)s.value, (long)avg);
        }
        // check if win is full & avg exceeds threshoold
        if (filled == WINDOW_SIZE) {
            if (avg > ALARM_THRESHOLD) {
                (void)fsm_dispatch(&alarm_fsm, EV_HIGH); // false = no transition 4 this event, ignore
            } else if (avg < ALARM_THRESHOLD - ALARM_HYSTERESIS) {
                (void)fsm_dispatch(&alarm_fsm, EV_LOW);
            }
        }
    }
}

// @brief task 4 alarm 
static void alarm_task(void *arg) {
    (void)arg;

    for (;;) {
        // waiting tuill alarm bit is set in event group, pdTRUE clears the bit 
        // after it is set, pdFALSE means we don't want 2 wait 4 all bits 2 
        // be set, portMAX_DELAY means we wait indefinitely
        xEventGroupWaitBits(s_events, BIT_ALARM, pdTRUE, pdFALSE, portMAX_DELAY);
        app_log("alarm -> average above limit\r\n");

        for (int i = 0; i < 6; i++) {
            bsp_led_toggle(); // toggle LED 6 times
            vTaskDelay(pdMS_TO_TICKS(100)); // delay 100ms between toggles & pdMS_TO_TICKS converts ms to ticks
        }
    }
}

// @brief app init func
void app_start(void) {
    // creating queues for sending & receiving samples of diff types
    s_sample_q = xQueueCreate(16U, sizeof(sample_t));
    s_log_q = xQueueCreate(16U, sizeof(log_msg_t));
    s_events = xEventGroupCreate(); // create ev group 4 alarm event
    configASSERT(s_sample_q && s_log_q && s_events); // if everything was creatied succesfully...
    BaseType_t ok = pdPASS; // pdPASS is a macro that evaluates 2 1, used to check if task creation was successful
    // create task tgype shit... APP_STACK_WORDS is a macro that calculates the stack size in words, based on the number of bytes specified
    // every task that calls app_log() (vsnprintf) needs >= 512 words, on esp32 it is 2 KB
    ok &= xTaskCreate(logger_task, "logger", APP_STACK_WORDS(512), NULL, 1, NULL);
    ok &= xTaskCreate(sensor_task, "sensor", APP_STACK_WORDS(512), NULL, 3, NULL);
    ok &= xTaskCreate(process_task, "process", APP_STACK_WORDS(512), NULL, 2, NULL);
    ok &= xTaskCreate(alarm_task, "alarm", APP_STACK_WORDS(512), NULL, 2, NULL);
    configASSERT(ok == pdPASS);

    app_log("app started -> pipeline up\r\n");
    app_primitives_start();
}

// hooks required by FreeRTOSConfig.h on stm32 & nxp (esp-idf provides its own)
#if !defined(ESP_PLATFORM)
#if defined(configCHECK_FOR_STACK_OVERFLOW) && (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName; // look at pcTaskName in the debugger
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
#endif

#if defined(configUSE_MALLOC_FAILED_HOOK) && (configUSE_MALLOC_FAILED_HOOK == 1)
void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) {} // configTOTAL_HEAP_SIZE too small
}
#endif
#endif