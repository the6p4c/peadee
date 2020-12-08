#include "log.h"
#include <stdint.h>
#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"

static uint32_t log_address = 0x0800f400;

void log_setup() {
	fmc_unlock();
	fmc_page_erase(log_address);
}

void log_try_program(uint32_t address, uint16_t word) {
	if (address >= 0x0800f400 && address < 0x0800f800) {
		fmc_halfword_program(address, word);
	}
}

void log_write(char *s) {
	int i = 0;
	char prev = '\0';
	do {
		if ((i & 1) == 0) {
			prev = *s;
		} else {
			uint16_t word = (((uint16_t) *s) << 8) | prev;
			log_try_program(log_address, word);
			log_address += 2;
		}

		i++;
	} while (*s++ != '\0');

	if ((i & 1) == 1) {
		uint16_t word = prev;
		log_try_program(log_address, word);
		log_address += 2;
	}
}
