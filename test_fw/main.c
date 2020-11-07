#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/gd32/gpio.h>
#include <libopencm3/gd32/rcc.h>
#include <libopencm3/gd32/flash.h>

volatile uint32_t time = 0;

uint8_t get_signal() {
	return gpio_get(GPIOA, GPIO5) == GPIO5 ? 1 : 0;
}

void sys_tick_handler() {
	static int transitions = 0;
	static uint8_t prev_signal = 0xFF;

	if (prev_signal == 0xFF) {
		prev_signal = get_signal();
	}

	uint8_t curr_signal = get_signal();
	if (curr_signal != prev_signal) {
		transitions++;

		if (transitions == 5) {
			scb_reset_system();
		}
	}

	prev_signal = curr_signal;

	time++;
}

#define DELAY (1000)
void delay(uint32_t duration) {
	uint32_t start = time;
	while (time - start <= duration);
}

void clock_setup() {
	rcc_osc_on(RCC_HSI);
	rcc_wait_for_osc_ready(RCC_HSI);

	rcc_set_sysclk_source(RCC_CFGR_SW_SYSCLKSEL_HSICLK);

	rcc_set_hpre(RCC_CFGR_HPRE_SYSCLK_NODIV);
	rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV8);
	rcc_set_ppre1(RCC_CFGR_PPRE1_HCLK_DIV2);
	rcc_set_ppre2(RCC_CFGR_PPRE2_HCLK_NODIV);

	flash_set_ws(FLASH_ACR_LATENCY_2WS);

	rcc_set_pll_multiplication_factor(RCC_CFGR_PLLMUL_PLL_CLK_MUL16);

	rcc_set_pll_source(RCC_CFGR_PLLSRC_HSI_CLK_DIV2);

	rcc_osc_on(RCC_PLL);
	rcc_wait_for_osc_ready(RCC_PLL);

	rcc_set_sysclk_source(RCC_CFGR_SW_SYSCLKSEL_PLLCLK);

	rcc_ahb_frequency = 64000000;
	rcc_apb1_frequency = 32000000;
	rcc_apb2_frequency = 64000000;
}

int main() {
	cm_disable_interrupts();
	SCB_VTOR = 0x08002c00;
	for (int i = 0; i < 70; ++i) {
		nvic_disable_irq(i);
	}
	cm_enable_interrupts();

	clock_setup();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);

	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO5);
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3 | GPIO4 | GPIO5);

	gpio_set(GPIOB, GPIO3 | GPIO4 | GPIO5);

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
	systick_set_reload(7999);
	systick_interrupt_enable();
	systick_counter_enable();

	delay(DELAY);

	//   bbbr bbrr bbbb bbrr bbbb brbb bbbr bbbb
	// 0b0001 0011 0000 0011 0000 0100 0001 0000
	// 0x   1    3    0    3    0    4    1    0
	uint32_t idcode = *((volatile uint32_t *) 0xE0042000);

	for (int i = 0; i < 32; ++i) {
		int bit = (idcode >> (31 - i)) & 1;

		if (bit) {
			// 1 - red
			gpio_clear(GPIOB, GPIO3);
		} else {
			// 0 - blue
			gpio_clear(GPIOB, GPIO4);
		}

		delay(DELAY);
		gpio_set(GPIOB, GPIO3);
		gpio_set(GPIOB, GPIO4);
		delay(DELAY);
	}

	// green - end of data
	gpio_clear(GPIOB, GPIO5);

	while (1) {

	}
}
