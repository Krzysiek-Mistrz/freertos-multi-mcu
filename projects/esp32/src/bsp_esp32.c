#include <stdio.h>
#include "driver/gpio.h"
#include "esp_random.h"
#include "bsp.h"


#define LED_GPIO GPIO_NUM_2

static int s_led_state;

// @brief init gpio 4 led type shit...
static void bsp_init_once(void) {
    static int done; // by def static vars eq to 0...

    if (!done) {
        gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
        done = 1;
    }
}

// declared in bsp.h
void bsp_led_toggle(void) {
    bsp_init_once();
    s_led_state = !s_led_state;
    gpio_set_level(LED_GPIO, s_led_state);
}

// declared in bsp.h
void bsp_log_write(const char *txt) {
    fputs(txt, stdout); // txt is \0 terminated string, stdout is UART0
}

#include "app_primitives.h"
#include "esp_attr.h"

/* @brief what 2 do when button is pressed type shi... & 
 * also void IRAM_ATTR because ESP32 has 2 types of memory: 
 * DRAM & IRAM, IRAM is faster but smaller, so we put this 
 * func in IRAM */
static void IRAM_ATTR button_isr(void *arg) {
    (void)arg; // we dont give a fuck about arg...
    app_on_button_isr();
}

// declared in bsp.h
void bsp_button_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << GPIO_NUM_0, // which pin we use...
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0); // 0 is 4 default priority -> 0 means lowest
    gpio_isr_handler_add(GPIO_NUM_0, button_isr, NULL);
}

// declared in bsp.h
int32_t bsp_sensor_read(void) {
    static int32_t ramp; // static vars eq to 0 by def, so ramp = 0
    ramp = (ramp + 40) % 4096;
    int32_t noise = (int32_t)(esp_random() % 200) - 100;
    int32_t v = ramp + noise;
    
    if (v < 0) v = 0;
    if (v > 4095) v = 4095; // 12b range is 0..4095

    return v;
}