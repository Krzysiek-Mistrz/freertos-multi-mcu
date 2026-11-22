#ifndef RTOS_H
#define RTOS_H

// in esp32 we basically have FreeRTOS under "freertos/..." type shit 
#if defined(ESP_PLATFORM)
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
    #include "freertos/event_groups.h"
    #include "freertos/semphr.h"
    #include "freertos/timers.h"
    #define APP_STACK_WORDS(n) ((n) * 4U) // wrds -> B & (n) because expr can be passed like 1+2...
// in stn32 & nxp we basically have FreeRTOS flat
#else
    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"
    #include "event_groups.h"
    #include "semphr.h"
    #include "timers.h"
    #define APP_STACK_WORDS(n) (n)
#endif

#endif // RTOS_H