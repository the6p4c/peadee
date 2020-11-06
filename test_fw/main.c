#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

volatile uint8_t ctr = 0;
// 0bRGB
volatile uint8_t led = 0;

void sys_tick_handler() {
	if ((ctr & 1) && (led & 4)) {
		gpio_clear(GPIOB, GPIO3);
	} else {
		gpio_set(GPIOB, GPIO3);
	}

	if ((ctr & 3) && (led & 2)) {
		gpio_clear(GPIOB, GPIO5);
	} else {
		gpio_set(GPIOB, GPIO5);
	}

	if (led & 1) {
		gpio_clear(GPIOB, GPIO4);
	} else {
		gpio_set(GPIOB, GPIO4);
	}

	ctr++;
}

int main() {
	cm_disable_interrupts();
	SCB_VTOR = 0x08002c00;
	for (int i = 0; i < 70; ++i) {
		nvic_disable_irq(i);
	}
	cm_enable_interrupts();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);

	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO5);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO4);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(4999);
	systick_interrupt_enable();
	systick_counter_enable();

	static uint8_t LUT[] = {
		//0b100, // red
		//0b010, // green
		//0b001, // blue
		//0b110, // yellow
		//0b011, // cyan
		//0b101, // magenta
		//0b111  // white
		0b001, // blue
		0b101, // magenta
		0b111, // white
		0b101, // magenta
		0b001, // blue
		0b000  // off
	};

	int lut_idx = 0;
	while (1) {
		led = LUT[lut_idx];
		lut_idx = (lut_idx + 1) % (sizeof(LUT) / sizeof(*LUT));

		for (int i = 0; i < 2500000; ++i) {
			__asm__ volatile("nop");
		}

		if (gpio_get(GPIOA, GPIO5) != GPIO5) {
			scb_reset_system();
		}
	}
}
