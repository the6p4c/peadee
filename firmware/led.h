#ifndef LED_H
#define LED_H
#include <stdint.h>

void led_setup();
void led_set_rgb(uint8_t rgb);
void led_flash_value(uint32_t value, int bits);

#endif
