#ifndef BSP_STM32_H
#define BSP_STM32_H

/* Clock (HSE 25 MHz -> PLL -> 100 MHz), LED, USART2 and ADC1. Call first in main(), before app_start(). */
void bsp_stm32_init(void);

#endif /* BSP_STM32_H */
