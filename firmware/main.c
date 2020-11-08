#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"

#define PD_ADDR 0x44

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

void delay(uint32_t duration) {
	uint32_t start = time;
	while (time - start <= duration);
}

void setup_interrupts() {
	__disable_irq();
	SCB->VTOR = 0x08002c00;
	for (int i = 0; i < 52; ++i) {
		nvic_irq_disable(i);
	}
	__enable_irq();
}

void setup_clock() {
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
	while((RCU_CTL0 & RCU_CTL0_PLLSTB) == 0);

	/* select PLL as system clock */
	RCU_CFG0 &= ~RCU_CFG0_SCS;
	RCU_CFG0 |= RCU_CKSYSSRC_PLL;

	/* wait until PLL is selected as system clock */
	while ((RCU_CFG0 & RCU_SCSS_PLL) == 0);
}

void setup_gpio() {
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOB);

	// Adapter signal
	gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_5);

	// I2C to FUSB302
	uint16_t i2c_pins = GPIO_PIN_0 | GPIO_PIN_1;
	gpio_af_set(GPIOA, GPIO_AF_4, i2c_pins);
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, i2c_pins);
	gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, i2c_pins);

	// RGB LED (off by default)
	uint16_t led_pins = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
	gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, led_pins);
	gpio_bit_set(GPIOB, led_pins);
}

void setup_pd() {
	rcu_periph_clock_enable(RCU_I2C1);

	i2c_clock_config(I2C1, 100000, I2C_DTCY_2);
	i2c_mode_addr_config(I2C1, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0);
	i2c_ack_config(I2C1, I2C_ACK_ENABLE);
	i2c_enable(I2C1);
}

void pd_write_reg(uint8_t reg, uint8_t value) {
	// Wait for bus idle
	while (i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, PD_ADDR, I2C_TRANSMITTER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send register address (FUSB302 datasheet, Figure 13)
	i2c_data_transmit(I2C1, reg);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send register value (FUSB302 datasheet, Figure 13)
	i2c_data_transmit(I2C1, value);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send stop condition
	i2c_stop_on_bus(I2C1);
	while (I2C_CTL0(I2C1) & 0x0200);
}

uint8_t pd_read_reg(uint8_t reg) {
	// Wait for bus idle
	while (i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, PD_ADDR, I2C_TRANSMITTER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send register address (FUSB302 datasheet, Figure 14)
	i2c_data_transmit(I2C1, reg);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, PD_ADDR, I2C_RECEIVER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	// Send NACK, since we're only reading one byte (FUSB302 datasheet, Figure
	// 14)
	i2c_ack_config(I2C1, I2C_ACK_DISABLE);

	while (!i2c_flag_get(I2C1, I2C_FLAG_RBNE));
	uint8_t value = i2c_data_receive(I2C1);

	// Re-enable sending ACKs
	i2c_ack_config(I2C1, I2C_ACK_ENABLE);

	// Send stop condition
	i2c_stop_on_bus(I2C1);
	while (I2C_CTL0(I2C1) & 0x0200);

	return value;
}

void setup_systick() {
	systick_clksource_set(SYSTICK_CLKSOURCE_HCLK_DIV8);
	SysTick->LOAD = 8999;
	SysTick->CTRL |= (1 << SysTick_CTRL_TICKINT_Pos) | (1 << SysTick_CTRL_ENABLE_Pos);
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

int main() {
	setup_interrupts();
	setup_clock();
	setup_gpio();
	setup_pd();
	setup_systick();

	delay(1000);

	// Reset PD logic and FUSB302
	pd_write_reg(0x0C, 0x03);

	uint8_t device_id = pd_read_reg(0x01);
	led_flash_value(device_id, 8);

	while (1);
}
