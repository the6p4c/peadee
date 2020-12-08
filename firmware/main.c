#include <stddef.h>
#include <stdio.h>
#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"

#include "led.h"
#include "log.h"
#include "pd.h"

#define PD_ADDR 0x44

volatile uint32_t time = 0;

uint8_t get_signal() {
	return gpio_input_bit_get(GPIOA, GPIO_PIN_5) == SET ? 1 : 0;
}

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
	// Adapter bootloader signal
	rcu_periph_clock_enable(RCU_GPIOA);
	gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_5);

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
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_GPIOB);
	led_setup();
	pd_setup();
	systick_setup();

	delay(1000);
	log_setup();
	no_bootloader = 1;

	pd_write_reg(0xc, 2);
	pd_write_reg(0xc, 3);
	delay(5);
	pd_write_reg(0xb, 0xf);
	pd_write_reg(2, 7);
	delay(2);
	int status1 = pd_read_reg(0x40);
	pd_write_reg(2, 3);
	pd_write_reg(2, 0xb);
	delay(2);
	int status2 = pd_read_reg(0x40);
	pd_write_reg(2, 3);

	int success = (status1 & 3) | (status2 & 3);
	if (success == 0) {
		log_write("failed to find pin to comm on");
		die();
	}

	pd_write_reg(9, 0x40);

	pd_write_reg(0xc, 3);
	delay(5);
	pd_write_reg(9, 7);
	pd_write_reg(0xe, 0xfc);
	pd_write_reg(0xf, 0xff);
	pd_write_reg(10, 0xef);
	pd_write_reg(6, 0);
	pd_write_reg(0xc, 2);

	status1 &= 3;
	status2 &= 3;

	if (status1 == 0) {
		log_write("txcc2");
		pd_write_reg(2, 0xb);
		pd_write_reg(3, 0x42);
	} else {
		log_write("txcc1");
		pd_write_reg(2, 7);
		pd_write_reg(3, 0x41);
	}

	pd_write_reg(0xb, 0xf);
	pd_read_reg(0x3e);
	pd_read_reg(0x3f);
	pd_read_reg(0x42);

	log_write("Init complete");

	delay(100);

	//uint8_t get_source_cap[] = {
	//	0x12, 0x12, 0x12, 0x13, // SOP
	//	0x82, // PACKSYM (2)
	//	0b00000000,
	//	0b01000111,
	//	0xFF, // JAM_CRC
	//	0x14, // EOP
	//	0xA1 // TXON
	//};

	//pd_write_reg(0xb, 0xf);
	//pd_write_reg(0x6, 0x40);
	//pd_write_fifo(get_source_cap, sizeof(get_source_cap));
	//pd_write_reg(6, 5);
	//pd_write_reg(0xb, 7);

	led_set_rgb(0b010);

	int led = 0b010;
	while (1) {
		uint8_t status1 = pd_read_reg(PD_REG_STATUS1);
		log_printf("[main] STATUS1 = %02x", status1);

		if ((status1 & PD_STATUS1_RX_EMPTY) != PD_STATUS1_RX_EMPTY) {
			log_write("[main] RX FIFO not empty");

			pd_read_reg(0x3e);
			pd_read_reg(0x3f);
			pd_read_reg(0x40);
			pd_read_reg(0x41);
			pd_read_reg(0x42);

			uint8_t packet_type;
			pd_read_fifo(&packet_type, sizeof(packet_type));

			if ((packet_type & 0b11100000) == 0b11100000) {
				led_set_rgb(led);
				led ^= 0b010;

				log_write("[main] Got SOP");

				uint8_t message_header_bytes[2];
				pd_read_fifo(message_header_bytes, sizeof(message_header_bytes));

				uint16_t message_header = (((uint16_t) message_header_bytes[1]) << 8) | message_header_bytes[0];

				log_printf("[main] Message header = %04x", message_header);

				uint8_t message_type = message_header & 0b1111;
				uint8_t message_id = (message_header >> 9) & 0b111;
				uint8_t number_of_data_objects = (message_header >> 12) & 0b111;
				uint8_t extended = (message_header >> 15) & 0b1;

				log_printf(
					"[main] Message Type = %x, Message ID = %x, Number of Data Objects = %x, Extended = %d",
					message_type, message_id, number_of_data_objects, extended
				);

				if (extended == 0) {
					log_write("[main] Standard");

					if (number_of_data_objects != 0 && message_type == 0b00001) {
						log_write("[main] Source_Capabilities");

						for (int i = 0; i < number_of_data_objects; ++i) {
							uint8_t pdo_bytes[4];
							pd_read_fifo(pdo_bytes, sizeof(pdo_bytes));

							uint32_t pdo = 
								(((uint32_t) pdo_bytes[3]) << 24) |
								(((uint32_t) pdo_bytes[2]) << 16) |
								(((uint32_t) pdo_bytes[1]) << 8) |
								pdo_bytes[0];
							
							log_printf("[main] PDO = %08lx", pdo);
						}
					}
				} else {
					log_write("[main] Extended!");
					uint8_t extended_header_bytes[4];
					pd_read_fifo(extended_header_bytes, sizeof(extended_header_bytes));

					uint32_t extended_header = 
						(((uint32_t) extended_header_bytes[3]) << 24) |
						(((uint32_t) extended_header_bytes[2]) << 16) |
						(((uint32_t) extended_header_bytes[1]) << 8) |
						extended_header_bytes[0];

					log_printf("[main] Extended header = %08lx", extended_header);

					uint8_t data_size = extended_header & 0b11111111;

					log_printf("[main] Data Size = %x", data_size);
				}

				// Flush RX FIFO
				pd_write_reg(PD_REG_CONTROL1, pd_read_reg(PD_REG_CONTROL1) | (1 << 2));
			}
		}

		delay(100);
	}

	while (1);
}
