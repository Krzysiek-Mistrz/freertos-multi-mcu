/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Krzychu_Mistrz
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include <stdint.h>
#include "app_pipeline.h"
#include "bsp_stm32.h"
#include "FreeRTOS.h"
#include "task.h"

int main(void)
{
    // clock (100 MHz), gpio, usart2, adc1. no HAL, plain CMSIS registers
    bsp_stm32_init();

    app_start();
    vTaskStartScheduler(); // starts the freertos scheduler, does not return

    for (;;) { }
}
