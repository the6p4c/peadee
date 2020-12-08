#include "led.h"
#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"

extern void delay(uint32_t duration);

void led_setup() {
	rcu_periph_clock_enable(RCU_GPIOB);

	uint16_t led_pins = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
	gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, led_pins);
	gpio_bit_set(GPIOB, led_pins);
}

void led_set_rgb(uint8_t rgb) {
	gpio_bit_write(GPIOB, GPIO_PIN_3, rgb & 0b100 ? RESET : SET);
	gpio_bit_write(GPIOB, GPIO_PIN_5, rgb & 0b010 ? RESET : SET);
	gpio_bit_write(GPIOB, GPIO_PIN_4, rgb & 0b001 ? RESET : SET);
}

void led_flash_value(uint32_t value, int bits) {
	for (int i = 0; i < bits; ++i) {
		int bit = (value >> (bits - 1 - i)) & 1;

		if (bit) {
			// 1 - red
			led_set_rgb(0b100);
		} else {
			// 0 - blue
			led_set_rgb(0b001);
		}

		delay(500);
		led_set_rgb(0b000);
		delay(500);
	}

	// green - end of data
	led_set_rgb(0b010);
}
