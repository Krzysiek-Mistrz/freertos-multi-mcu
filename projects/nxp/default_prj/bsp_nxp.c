/*
 * bsp_nxp.c - bsp.h for i.MX 8M Mini EVK (Cortex-M4, MCUXpresso SDK).
 *
 * Pins (EVK base board, expansion header J1003). The EVK has no user LED or
 * button for the M4, so wire them externally:
 *   LED    GPIO5_IO11 (ECSPI2_MOSI), header J1003 pin 19 (from the SDK gpio led_output example)
 *   button GPIO5_IO10 (ECSPI2_SCLK), to GND, internal pull-up enabled.
 *          Check the J1003 pin number in the EVK schematic before wiring.
 * These pins are used by Linux (ECSPI2): do not run this next to a Linux BSP that uses them.
 */
#include <stdint.h>

#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "fsl_gpio.h"
#include "fsl_debug_console.h"

#include "bsp.h"
#include "app_primitives.h"

#define BSP_LED_GPIO          GPIO5
#define BSP_LED_PIN           11U
#define BSP_BUTTON_GPIO       GPIO5
#define BSP_BUTTON_PIN        10U
#define BSP_BUTTON_IRQ        GPIO5_Combined_0_15_IRQn
/* must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (2) numerically to call *FromISR */
#define BSP_BUTTON_IRQ_PRIO   3U

static void bsp_pins_init(void)
{
    /* LED: push-pull output */
    IOMUXC_SetPinMux(IOMUXC_ECSPI2_MOSI_GPIO5_IO11, 0U);

    /* button: input with pull-up (PE = pull enable, PUE = pull-up) */
    IOMUXC_SetPinMux(IOMUXC_ECSPI2_SCLK_GPIO5_IO10, 0U);
    IOMUXC_SetPinConfig(IOMUXC_ECSPI2_SCLK_GPIO5_IO10,
                        IOMUXC_SW_PAD_CTL_PAD_PE(1U) | IOMUXC_SW_PAD_CTL_PAD_PUE(1U));
}

static int s_inited;

static void bsp_init_once(void)
{
    if (s_inited) { return; }
    s_inited = 1;

    bsp_pins_init();

    gpio_pin_config_t led = { kGPIO_DigitalOutput, 0U, kGPIO_NoIntmode };
    GPIO_PinInit(BSP_LED_GPIO, BSP_LED_PIN, &led);
}

void bsp_led_toggle(void)
{
    bsp_init_once();
    GPIO_PortToggle(BSP_LED_GPIO, 1U << BSP_LED_PIN);
}

void bsp_log_write(const char *text)
{
    /* PRINTF goes to UART4, called only from the logger task */
    PRINTF("%s", text);
}

int32_t bsp_sensor_read(void)
{
    /* simulation (triangle wave), replace with a real ADC/I2C read */
    static int32_t v;
    static int32_t dir = 60;

    v += dir;
    if (v >= 4095) { v = 4095; dir = -60; }
    if (v <= 0)    { v = 0;    dir =  60; }
    return v;
}

void bsp_button_init(void)
{
    bsp_init_once();

    gpio_pin_config_t btn = { kGPIO_DigitalInput, 0U, kGPIO_NoIntmode };
    GPIO_PinInit(BSP_BUTTON_GPIO, BSP_BUTTON_PIN, &btn);

    GPIO_SetPinInterruptConfig(BSP_BUTTON_GPIO, BSP_BUTTON_PIN, kGPIO_IntFallingEdge);
    GPIO_PortClearInterruptFlags(BSP_BUTTON_GPIO, 1U << BSP_BUTTON_PIN);
    GPIO_PortEnableInterrupts(BSP_BUTTON_GPIO, 1U << BSP_BUTTON_PIN);

    NVIC_SetPriority(BSP_BUTTON_IRQ, BSP_BUTTON_IRQ_PRIO);
    EnableIRQ(BSP_BUTTON_IRQ);
}

/* GPIO5 pins 0..15 share one interrupt line */
void GPIO5_Combined_0_15_IRQHandler(void)
{
    GPIO_PortClearInterruptFlags(BSP_BUTTON_GPIO, 1U << BSP_BUTTON_PIN);
    app_on_button_isr();
    SDK_ISR_EXIT_BARRIER;
}
