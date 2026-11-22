/*
 * bsp_stm32.c - bsp.h for STM32F411CCU7 on a Black Pill board, CMSIS register access (no HAL).
 *
 *   LED     PC13 (on-board, active low)
 *   console USART2 TX on PA2, 115200 8N1 (connect a USB-UART adapter RX to PA2, GND to GND)
 *   sensor  ADC1 channel 1 on PA1 (potentiometer 0..3.3 V; floating pin gives noise)
 *   button  PA0 (on-board KEY, to GND), EXTI0 falling edge, internal pull-up
 *
 * Clock: HSE 25 MHz -> PLL M=25 N=200 P=2 -> 100 MHz. APB1 = 50 MHz (USART2), APB2 = 100 MHz.
 */
#include <stdint.h>
#include <string.h>

#include "stm32f4xx.h"
#include "bsp.h"
#include "bsp_stm32.h"
#include "app_primitives.h"

#define LED_PIN           13U
#define BUTTON_PIN        0U
#define BUTTON_IRQ_PRIO   6U     /* must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5) */
#define UART_BAUD         115200U
#define APB1_HZ           50000000U

static void clock_init_100mhz(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0U) { }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;                              /* voltage scale 1, needed for 100 MHz */

    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_3WS;

    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;
    RCC->PLLCFGR = (25U  << RCC_PLLCFGR_PLLM_Pos)
                 | (200U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U   << RCC_PLLCFGR_PLLP_Pos)       /* P = 2 */
                 | (9U   << RCC_PLLCFGR_PLLQ_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0U) { }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    SystemCoreClock = 100000000U;
}

static void gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;

    /* PC13 output, LED off (active low -> high) */
    GPIOC->MODER = (GPIOC->MODER & ~(3U << (LED_PIN * 2U))) | (1U << (LED_PIN * 2U));
    GPIOC->BSRR  = 1U << LED_PIN;

    /* PA2 = USART2_TX, alternate function 7 */
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (2U * 2U))) | (2U << (2U * 2U));
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFU << (2U * 4U))) | (7U << (2U * 4U));

    /* PA1 = analog (ADC1_IN1) */
    GPIOA->MODER |= (3U << (1U * 2U));
}

static void uart_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    USART2->BRR = (APB1_HZ + UART_BAUD / 2U) / UART_BAUD;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;   /* PCLK2/4 = 25 MHz (max 36) */
    ADC1->SMPR2 = 7U << ADC_SMPR2_SMP1_Pos;                        /* 480 cycles on channel 1 */
    ADC1->SQR1  = 0U;                                              /* 1 conversion in the sequence */
    ADC1->SQR3  = 1U;                                              /* channel 1 */
    ADC1->CR2   = ADC_CR2_ADON;
}

void bsp_stm32_init(void)
{
    clock_init_100mhz();
    gpio_init();
    uart_init();
    adc_init();
}

/* ---- bsp.h ---- */

void bsp_led_toggle(void)
{
    /* BSRR write is atomic, ODR read-modify-write is not (two tasks call this) */
    uint32_t odr = GPIOC->ODR;
    GPIOC->BSRR = ((odr & (1U << LED_PIN)) << 16U) | (~odr & (1U << LED_PIN));
}

void bsp_log_write(const char *text)
{
    /* blocking, called only from the logger task */
    for (size_t i = 0; text[i] != '\0'; i++) {
        while ((USART2->SR & USART_SR_TXE) == 0U) { }
        USART2->DR = (uint8_t)text[i];
    }
    while ((USART2->SR & USART_SR_TC) == 0U) { }
}

int32_t bsp_sensor_read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    uint32_t guard = 100000U;
    while (((ADC1->SR & ADC_SR_EOC) == 0U) && (guard-- > 0U)) { }
    return (int32_t)(ADC1->DR & 0xFFFU);
}

void bsp_button_init(void)
{
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << (BUTTON_PIN * 2U))) | (1U << (BUTTON_PIN * 2U));  /* pull-up */

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;                  /* EXTI0 <- port A */
    EXTI->FTSR |= (1U << BUTTON_PIN);                            /* falling edge */
    EXTI->IMR  |= (1U << BUTTON_PIN);

    NVIC_SetPriority(EXTI0_IRQn, BUTTON_IRQ_PRIO);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void EXTI0_IRQHandler(void)
{
    EXTI->PR = (1U << BUTTON_PIN);        /* clear pending flag */
    app_on_button_isr();
}
