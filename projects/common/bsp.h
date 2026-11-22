#ifndef BSP_H
#define BSP_H

#include <stdint.h>


// @brief just swithc led 
void bsp_led_toggle(void);

// @brief sending \0 terminated str 2 cons & called 4m logger task
void bsp_log_write(const char *text);

// @brief reads sensor, returns 0..4095 (12b adc or simulation)
int32_t bsp_sensor_read(void);

// @brief config button interrupt (falling edge) & in isr call app_on_button_isr()
// called once 4m app_primitives_start()
void bsp_button_init(void);

#endif // BSP_H