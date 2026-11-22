#ifndef APP_PRIMITIVES_H
#define APP_PRIMITIVES_H

// @brief creates tasks & obj demonstrating synchronization primitives
void app_primitives_start(void);

// @brief called 4m button isr & gives bin semaphore
void app_on_button_isr(void);

#endif // APP_PRIMITIVES_H