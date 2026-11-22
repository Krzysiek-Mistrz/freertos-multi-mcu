/*
 * ESP32 (ESP-IDF via PlatformIO) - entry point.
 * In ESP-IDF the scheduler is already running when app_main() is called
 * (app_main is a normal task), so there is no vTaskStartScheduler() here.
 */
#include "app_pipeline.h"

void app_main(void)
{
    app_start();
}
