/*
 * i.MX 8M Mini EVK, Cortex-M4 core (MCUXpresso SDK).
 * This runs on the M4, the A53 cores run Linux and are not used here.
 * Console: UART4, 115200 8N1.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "app.h"      /* BOARD_InitHardware(): RDC, pins, clocks, console, memory */
#include "app_pipeline.h"

int main(void)
{
    BOARD_InitHardware();

    app_start();
    vTaskStartScheduler();   /* does not return */

    for (;;) { }
}
