/*
 * FreeRTOSConfig.h - STM32F411CCU7 (Cortex-M4F, 100 MHz), FreeRTOS-Kernel V11, ARM_CM4F port.
 * Only options this project needs are set, everything else keeps the kernel default.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* ---- core ---- */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1   /* CLZ-based, Cortex-M4 has it */
#define configCPU_CLOCK_HZ                      (100000000UL)  /* PLL, set in bsp_stm32.c */
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configUSE_16_BIT_TICKS                  0
#define configMAX_PRIORITIES                    7
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)   /* words */
#define configMAX_TASK_NAME_LEN                 16
#define configIDLE_SHOULD_YIELD                 1

/* ---- memory (heap_4.c) ---- */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   ((size_t)(48 * 1024))   /* RAM is 128 KB */

/* ---- sync primitives used by common/ ---- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configQUEUE_REGISTRY_SIZE               8

/* ---- software timers ---- */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* ---- hooks and debugging ---- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2   /* hook in common/app_pipeline.c */
#define configUSE_MALLOC_FAILED_HOOK            1   /* hook in common/app_pipeline.c */
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0
#define configASSERT(x)                         do { if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;) {} } } while (0)

/* ---- API selection ---- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_eTaskGetState                   1

/* ---- Cortex-M interrupt priorities ----
 * STM32F4 implements 4 priority bits. Priority 15 is the lowest (kernel), 0 is the highest.
 * ISRs that call *FromISR functions must have a priority number >= 5 (numerically),
 * e.g. the button EXTI0 uses 6. ISRs with a lower number (0..4) must never call FreeRTOS API. */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---- map kernel handlers onto the names used in the vector table (startup_stm32f411xx.S) ---- */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
