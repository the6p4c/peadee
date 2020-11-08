#include <gd32f1x0.h>
#include <core_cm3.h>
#include <gd32f1x0_gpio.h>
#include <gd32f1x0_i2c.h>
#include <gd32f1x0_rcu.h>

volatile uint32_t time = 0;

uint8_t get_signal() {
	return gpio_input_bit_get(GPIOA, GPIO_PIN_5) == SET ? 1 : 0;
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
			SCB->AIRCR = (0x05FA << SCB_AIRCR_VECTKEY_Pos) | (1 << SCB_AIRCR_SYSRESETREQ_Pos);
			while (1);
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
	/* AHB = SYSCLK */
	RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
	/* APB2 = AHB */
	RCU_CFG0 |= RCU_APB2_CKAHB_DIV1;
	/* APB1 = AHB */
	RCU_CFG0 |= RCU_APB1_CKAHB_DIV1;
	/* PLL = (IRC8M/2) * 18 = 72 MHz */
	RCU_CFG0 &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PLLMF);
	RCU_CFG0 |= (RCU_PLLSRC_IRC8M_DIV2 | RCU_PLL_MUL18);

	/* enable PLL */
	RCU_CTL0 |= RCU_CTL0_PLLEN;

	/* wait until PLL is stable */
	while(0 == (RCU_CTL0 & RCU_CTL0_PLLSTB));

	/* select PLL as system clock */
	RCU_CFG0 &= ~RCU_CFG0_SCS;
	RCU_CFG0 |= RCU_CKSYSSRC_PLL;

	/* wait until PLL is selected as system clock */
	while(0 == (RCU_CFG0 & RCU_SCSS_PLL));
}

int main() {
	__disable_irq();
	SCB->VTOR = 0x08002c00;
	for (int i = 0; i < 70; ++i) {
		nvic_irq_disable(i);
	}
	__enable_irq();

	clock_setup();

	i2c_enable(I2C1);

	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOB);

	gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_5);
	gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);

	gpio_bit_set(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);

	systick_clksource_set(SYSTICK_CLKSOURCE_HCLK_DIV8);
	SysTick->LOAD = 8999;
	SysTick->CTRL |= (1 << SysTick_CTRL_TICKINT_Pos) | (1 << SysTick_CTRL_ENABLE_Pos);

	delay(DELAY);

	//   bbbr bbrr bbbb bbrr bbbb brbb bbbr bbbb
	// 0b0001 0011 0000 0011 0000 0100 0001 0000
	// 0x   1    3    0    3    0    4    1    0
	uint32_t idcode = *((volatile uint32_t *) 0xE0042000);

	for (int i = 0; i < 32; ++i) {
		int bit = (idcode >> (31 - i)) & 1;

		if (bit) {
			// 1 - red
			gpio_bit_reset(GPIOB, GPIO_PIN_3);
		} else {
			// 0 - blue
			gpio_bit_reset(GPIOB, GPIO_PIN_4);
		}

		delay(DELAY);
		gpio_bit_set(GPIOB, GPIO_PIN_3);
		gpio_bit_set(GPIOB, GPIO_PIN_4);
		delay(DELAY);
	}

	// green - end of data
	gpio_bit_reset(GPIOB, GPIO_PIN_5);

	while (1) {

	}
}
