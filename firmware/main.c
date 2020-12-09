#include <stddef.h>
#include <stdio.h>
#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"

#include "led.h"
#include "log.h"
#include "pd.h"

void gpio_setup() {
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOB);

	// Adapter bootloader signal
	rcu_periph_clock_enable(RCU_GPIOA);
	gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_5);

	// RGB LED
	uint16_t led_pins = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
	gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, led_pins);
	gpio_bit_set(GPIOB, led_pins);

	// FUSB302 I2C interface
	uint16_t i2c_pins = GPIO_PIN_0 | GPIO_PIN_1;
	gpio_af_set(GPIOA, GPIO_AF_4, i2c_pins);
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, i2c_pins);
	gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, i2c_pins);
}

uint8_t get_signal() {
	return gpio_input_bit_get(GPIOA, GPIO_PIN_5) == SET ? 1 : 0;
}

static volatile uint32_t time = 0;
static volatile int no_bootloader = 0;

void sys_tick_handler() {
	time++;

	if (no_bootloader) {
		return;
	}

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
}

void delay(uint32_t duration) {
	uint32_t start = time;
	while (time - start <= duration);
}

void interrupts_setup() {
	__disable_irq();
	SCB->VTOR = 0x08002c00;
	for (int i = 0; i < 52; ++i) {
		nvic_irq_disable(i);
	}
	__enable_irq();
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
	while((RCU_CTL0 & RCU_CTL0_PLLSTB) == 0);

	/* select PLL as system clock */
	RCU_CFG0 &= ~RCU_CFG0_SCS;
	RCU_CFG0 |= RCU_CKSYSSRC_PLL;

	/* wait until PLL is selected as system clock */
	while ((RCU_CFG0 & RCU_SCSS_PLL) == 0);
}

void systick_setup() {
	systick_clksource_set(SYSTICK_CLKSOURCE_HCLK_DIV8);
	SysTick->LOAD = 8999;
	SysTick->CTRL |= (1 << SysTick_CTRL_TICKINT_Pos) | (1 << SysTick_CTRL_ENABLE_Pos);
}

void die() {
	led_set_rgb(0b100);
	while (1);
}

int main() {
	interrupts_setup();
	clock_setup();
	gpio_setup();
	pd_setup();
	systick_setup();

	led_set_rgb(0b001);

	delay(1000);
	log_setup();
	no_bootloader = 1;

	if (!pd_try_attach()) {
		die();
	}

	log_write("Init complete");

	led_set_rgb(0b010);

	struct pd_message message;
	int requested_pdo_idx = -1;
	int msg_id = 1;

	int led = 0b010;
	while (1) {
		if (requested_pdo_idx >= 0) {
			log_printf("pdo=%d,mi=%d", requested_pdo_idx, msg_id);

			uint16_t header = 0b0001000010000010;
			header |= msg_id++ << 9;
			msg_id &= 0b111;

			uint32_t pdo = 0;
			pdo |= (requested_pdo_idx + 1) << 28;
			pdo |= (200 / 10) << 10;
			pdo |= 500 / 10;

			struct pd_message_standard payload;
			payload.data_objects[0] = pdo;

			pd_tx_standard(header, &payload);

			requested_pdo_idx = -1;
		}

		if (pd_poll_rxfifo(&message)) {
			do {
				led_set_rgb(led);
				led ^= 0b010;

				log_printf("hdr=%04x", message.header);

				uint8_t message_type = message.header & 0b1111;
				uint8_t message_id = (message.header >> 9) & 0b111;
				uint8_t number_of_data_objects = (message.header >> 12) & 0b111;
				uint8_t extended = (message.header >> 15) & 0b1;
				uint8_t spec_revision = (message.header >> 6) & 0b11;

				log_printf(
					"mt=%x,mid=%x,dos=%x,ext=%d,sr=%d",
					message_type, message_id, number_of_data_objects, extended, spec_revision
				);

				if (extended == 0) {
					struct pd_message_standard *payload = &message.payload.standard;

					if (number_of_data_objects != 0 && message_type == 0b00001) {
						for (int i = 0; i < number_of_data_objects; ++i) {
							uint32_t pdo = payload->data_objects[i];

							log_printf("pdo=%08lx", pdo);

							if (((pdo >> 30) & 0b11) == 0) {
								requested_pdo_idx = i;
							}
						}
					}
				} else {
					struct pd_message_extended *payload = &message.payload.extended;

					log_printf("Extended header = %04x", payload->extended_header);

					for (int i = 0; i < (payload->extended_header & 0x1FF); ++i) {
						log_printf("%02x", payload->data[i]);
					}
				}
			} while (pd_poll_rxfifo(&message));
		}
	}
}
