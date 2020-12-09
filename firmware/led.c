#include "led.h"
#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"

void led_set_rgb(uint8_t rgb) {
	gpio_bit_write(GPIOB, GPIO_PIN_3, rgb & 0b100 ? RESET : SET);
	gpio_bit_write(GPIOB, GPIO_PIN_5, rgb & 0b010 ? RESET : SET);
	gpio_bit_write(GPIOB, GPIO_PIN_4, rgb & 0b001 ? RESET : SET);
}
