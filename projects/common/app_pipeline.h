#ifndef APP_PIPELINE_H
#define APP_PIPELINE_H

// @brief creates queues & event group & all tasks. 
// Call once from main()/app_main(). On STM32/NXP then vTaskStartScheduler().
void app_start(void);

// @brief thread safe printf -> copies msg to logger queue & Non-blocking
void app_log(const char *fmt, ...);

#endif // APP_PIPELINE_H