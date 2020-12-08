// Implementation based off:
// FUSB302-D datasheet Rev 2 (July 2017)
//   Available from https://www.onsemi.com/pub/Collateral/FUSB302-D.PDF
//   SHA256: 6af1da8af23e7f015f4896df0b37bde57441c102e5361338e0af566f2bec379f
// USB Type-C Cable and Connector Specification Release 2.0 (August 2019)
//   Available from https://www.usb.org/sites/default/files/USB%20Type-C%20Spec%20R2.0_4.zip
//     USB Type-C Spec R2.0 - August 2019.pdf
//   SHA256: a95f23cf9943f4a13ef24b76b32b7cda9fb33888e5072cd4cbebf56ba8a482df
// USB PD Specification Rev 3.0 (07 February 2020)
//   Available from https://www.usb.org/sites/default/files/USB%20Power%20Delivery%2020200212.zip
//     USB PD V3.0 R2.0/USB_PD_R3_0 V2.0 20190829 + ECNs 2020-02-07.pdf
//   SHA256: 2e52ba62bc2a7d723d17cdea15d56c23c039ac9c8a744f004da18a7114b44f85
#include "pd.h"
#include <gd32f1x0.h>
#include <core_cm3.h>
#include "gd32f1x0_libopt.h"
#include "log.h"

#define FUSB302_ADDRESS (0x44)

extern void delay(uint32_t duration);

void pd_setup() {
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_I2C1);

	uint16_t i2c_pins = GPIO_PIN_0 | GPIO_PIN_1;
	gpio_af_set(GPIOA, GPIO_AF_4, i2c_pins);
	gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, i2c_pins);
	gpio_output_options_set(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, i2c_pins);

	i2c_clock_config(I2C1, 100000, I2C_DTCY_2);
	i2c_mode_addr_config(I2C1, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0);
	i2c_ack_config(I2C1, I2C_ACK_ENABLE);
	i2c_ackpos_config(I2C1, I2C_ACKPOS_CURRENT);
	i2c_enable(I2C1);
}

int pd_try_attach() {
	// Perform a complete reset
	pd_write_reg(PD_REG_RESET, PD_RESET_PD_RESET);
	pd_write_reg(PD_REG_RESET, PD_RESET_PD_RESET | PD_RESET_SW_RES);

	delay(10);

	// Turn on all internal enables
	pd_write_reg(PD_REG_POWER, 0xF);

	delay(10);

	// Present Rd pull-downs on CC1 and CC2, measure CC1
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_MEAS_CC1 | PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);

	delay(10);

	uint8_t meas_cc1_status0 = pd_read_reg(PD_REG_STATUS0);
	log_printf("[pd_try_attach] CC1 status: %02x", meas_cc1_status0);
	
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);

	// Present Rd pull-downs on CC1 and CC2, measure CC2
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_MEAS_CC2 | PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);

	delay(10);

	uint8_t meas_cc2_status0 = pd_read_reg(PD_REG_STATUS0);
	log_printf("[pd_try_attach] CC2 status: %02x", meas_cc2_status0);

	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);

	// Stop measurement
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);

	// Determine orientation (from type-C spec):
	//   Once the Sink is powered, the Sink monitors CC1 and CC2 for a voltage
	//   greater than its local ground.  The CC pin that is at a higher voltage
	//   (i.e. pulled up by Rp in the Source) indicates the orientation of the
	//   plug. 
	uint8_t cc1_bc_lvl = meas_cc1_status0 & PD_STATUS0_BC_LVL_MASK;
	uint8_t cc2_bc_lvl = meas_cc2_status0 & PD_STATUS0_BC_LVL_MASK;

	if (cc1_bc_lvl != 0 && cc2_bc_lvl != 0) {
		// Both pins were high, so we weren't able to tell which pin we should
		// use to do PD communication on
		log_write("[pd_try_attach] Fail: both CC pins were pulled up");
		return 0;
	}

	if (cc1_bc_lvl != 0) {
		// Use CC1 for PD communication
		pd_write_reg(
			PD_REG_SWITCHES1,
			pd_read_reg(PD_REG_SWITCHES1) | PD_SWITCHES1_AUTO_CRC | PD_SWITCHES1_TXCC1
		);

		log_write("[pd_try_attach] Success: comms on CC1");

		return 1;
	} else {
		// Use CC2 for PD communication
		pd_write_reg(
			PD_REG_SWITCHES1,
			pd_read_reg(PD_REG_SWITCHES1) | PD_SWITCHES1_AUTO_CRC | PD_SWITCHES1_TXCC2
		);

		log_write("[pd_try_attach] Success: comms on CC2");

		return 2;
	}

	// This shouldn't be reachable, but would be a failure if it was
	return 0;
}

void pd_write_reg(uint8_t reg, uint8_t value) {
	// See FUSB302 datasheet, Figure 13 "I2C Write Example"

	// Wait for bus idle
	while (i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, FUSB302_ADDRESS, I2C_TRANSMITTER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send register address
	i2c_data_transmit(I2C1, reg);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send register value
	i2c_data_transmit(I2C1, value);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send stop condition
	i2c_stop_on_bus(I2C1);
	while (I2C_CTL0(I2C1) & 0x0200);
}

void pd_write_fifo(uint8_t *data, size_t count) {
	// See FUSB302 datasheet, Figure 13 "I2C Write Example"

	// Wait for bus idle
	while (i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, FUSB302_ADDRESS, I2C_TRANSMITTER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	i2c_data_transmit(I2C1, PD_REG_FIFOS);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send FIFO data
	for (size_t i = 0; i < count; ++i) {
		i2c_data_transmit(I2C1, data[i]);
		while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));
	}

	// Send stop condition
	i2c_stop_on_bus(I2C1);
	while (I2C_CTL0(I2C1) & 0x0200);
}

uint8_t pd_read_reg(uint8_t reg) {
	// See FUSB302 datasheet, Figure 14 "I2C Read Example"

	// Wait for bus idle
	while (i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, FUSB302_ADDRESS, I2C_TRANSMITTER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send register address
	i2c_data_transmit(I2C1, reg);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, FUSB302_ADDRESS, I2C_RECEIVER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	// Send NACK immediately, since we're only reading one byte
	i2c_ack_config(I2C1, I2C_ACK_DISABLE);

	// Read register value
	while (!i2c_flag_get(I2C1, I2C_FLAG_RBNE));
	uint8_t value = i2c_data_receive(I2C1);

	// Re-enable sending ACKs
	i2c_ack_config(I2C1, I2C_ACK_ENABLE);

	// Send stop condition
	i2c_stop_on_bus(I2C1);
	while (I2C_CTL0(I2C1) & 0x0200);

	return value;
}

void pd_read_fifo(uint8_t *data, size_t count) {
	// See FUSB302 datasheet, Figure 14 "I2C Read Example"

	// Wait for bus idle
	while (i2c_flag_get(I2C1, I2C_FLAG_I2CBSY));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, FUSB302_ADDRESS, I2C_TRANSMITTER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	i2c_data_transmit(I2C1, PD_REG_FIFOS);
	while (!i2c_flag_get(I2C1, I2C_FLAG_TBE));

	// Send start condition
	i2c_start_on_bus(I2C1);
	while (!i2c_flag_get(I2C1, I2C_FLAG_SBSEND));

	// Send slave address
	i2c_master_addressing(I2C1, FUSB302_ADDRESS, I2C_RECEIVER);
	while (!i2c_flag_get(I2C1, I2C_FLAG_ADDSEND));
	i2c_flag_clear(I2C1, I2C_STAT0_ADDSEND);

	for (size_t i = 0; i < count; ++i) {
		// Is this the last byte?
		if (i == count - 1) {
			// Send NACK to stop read
			i2c_ack_config(I2C1, I2C_ACK_DISABLE);
		}

		// Read register value
		while (!i2c_flag_get(I2C1, I2C_FLAG_RBNE));
		data[i] = i2c_data_receive(I2C1);
	}

	// Re-enable sending ACKs
	i2c_ack_config(I2C1, I2C_ACK_ENABLE);

	// Send stop condition
	i2c_stop_on_bus(I2C1);
	while (I2C_CTL0(I2C1) & 0x0200);
}
