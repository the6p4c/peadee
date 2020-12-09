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
	rcu_periph_clock_enable(RCU_I2C1);

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

	delay(5);

	// Turn on all internal enables
	pd_write_reg(PD_REG_POWER, 0xF);

	// Present Rd pull-downs on CC1 and CC2, measure CC1
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_MEAS_CC1 | PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);
	delay(2);
	uint8_t meas_cc1_status0 = pd_read_reg(PD_REG_STATUS0);
	// Disable measurement
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);

	// Present Rd pull-downs on CC1 and CC2, measure CC2
	pd_write_reg(PD_REG_SWITCHES0, PD_SWITCHES0_MEAS_CC2 | PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1);
	delay(2);
	uint8_t meas_cc2_status0 = pd_read_reg(PD_REG_STATUS0);
	// Disable measurement
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
		return 0;
	}

	// Send a hard reset to restart the process now that we know which CC pin to
	// communicate on
	pd_write_reg(PD_REG_CONTROL3, PD_CONTROL3_SEND_HARD_RESET);
	// Perform another full reset of the FUSB302
	pd_write_reg(PD_REG_RESET, PD_RESET_PD_RESET | PD_RESET_SW_RES);

	delay(5);

	// Enable 3 retries and automatic retry for missing GoodCRC
	pd_write_reg(PD_REG_CONTROL3, (3 << PD_CONTROL3_N_RETRIES_POS) | PD_CONTROL3_AUTO_RETRY);

	// One last PD reset for good luck
	pd_write_reg(PD_REG_RESET, PD_RESET_PD_RESET | PD_RESET_SW_RES);

	if (cc1_bc_lvl != 0) {
		// CC1 was highest - use CC1 for PD communication
		pd_write_reg(
			PD_REG_SWITCHES0,
			PD_SWITCHES0_MEAS_CC1 | PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1
		);
		pd_write_reg(
			PD_REG_SWITCHES1,
			(0b10 << PD_SWITCHES1_SPECREV_POS) | PD_SWITCHES1_AUTO_CRC | PD_SWITCHES1_TXCC1
		);
	} else {
		// CC2 was highest - use CC2 for PD communication
		pd_write_reg(
			PD_REG_SWITCHES0,
			PD_SWITCHES0_MEAS_CC2 | PD_SWITCHES0_PDWN2 | PD_SWITCHES0_PDWN1
		);
		pd_write_reg(
			PD_REG_SWITCHES1,
			(0b10 << PD_SWITCHES1_SPECREV_POS) | PD_SWITCHES1_AUTO_CRC | PD_SWITCHES1_TXCC2
		);
	}

	// Turn on all internal enables
	pd_write_reg(PD_REG_POWER, 0xF);

	return 1;
}

int pd_poll_rxfifo(struct pd_message *message) {
	uint8_t status1 = pd_read_reg(PD_REG_STATUS1);
	if ((status1 & PD_STATUS1_RX_EMPTY) == PD_STATUS1_RX_EMPTY) {
		return 0;
	}

	uint8_t sop_token;
	pd_read_fifo(&sop_token, sizeof(sop_token));

	if ((sop_token & PD_RXFIFO_TOK_SOP_MASK) != PD_RXFIFO_TOK_SOP) {
		// TODO: We probably need to flush the RX FIFO here, since we don't
		// know how to deal with the packet that's in there.
		pd_write_reg(PD_REG_CONTROL1, pd_read_reg(PD_REG_CONTROL1) | (1 << 2));
		return 0;
	}

	uint16_t header;
	pd_read_fifo((uint8_t *) &header, sizeof(header));
	message->header = header;

	uint8_t extended = (header >> 15) & 0b1;
	if (extended == 0) {
		struct pd_message_standard *payload = &message->payload.standard;

		uint8_t number_of_data_objects = (header >> 12) & 0b111;
		if (number_of_data_objects != 0) {
			pd_read_fifo(
				(uint8_t *) &payload->data_objects,
				number_of_data_objects * sizeof(payload->data_objects[0])
			);
		}
	} else {
		struct pd_message_extended *payload = &message->payload.extended;

		uint16_t extended_header;
		pd_read_fifo((uint8_t *) &extended_header, sizeof(extended_header));
		payload->extended_header = extended_header;

		uint8_t data_size = extended_header & 0x1FF;
		if (data_size != 0) {
			pd_read_fifo(payload->data, data_size);
		}
	}

	pd_read_fifo((uint8_t *) &message->crc, sizeof(message->crc));

	return 1;
}

void pd_tx_standard(uint16_t header, struct pd_message_standard *payload) {
	uint8_t data[48];
	size_t count = 0;

	uint8_t number_of_data_objects = (header >> 12) & 0b111;
	// Total message length is 16-bit header (2 bytes) + 4 bytes per data object
	uint8_t message_length = 2 + number_of_data_objects * 4;

	// SOP header
	data[count++] = PD_TXFIFO_TOK_SOP1;
	data[count++] = PD_TXFIFO_TOK_SOP1;
	data[count++] = PD_TXFIFO_TOK_SOP1;
	data[count++] = PD_TXFIFO_TOK_SOP2;

	// PACKSYM for message bytes
	data[count++] = PD_TXFIFO_TOK_PACKSYM(message_length);

	// Message header
	data[count++] = header & 0xFF;
	data[count++] = (header >> 4) & 0xFF;

	// Data objects
	for (int i = 0; i < number_of_data_objects; ++i) {
		uint32_t data_object = payload->data_objects[i];
		data[count++] = data_object & 0xFF;
		data[count++] = (data_object >> 8) & 0xFF;
		data[count++] = (data_object >> 16) & 0xFF;
		data[count++] = (data_object >> 24) & 0xFF;
	}

	// Message trailer
	data[count++] = PD_TXFIFO_TOK_JAM_CRC;
	data[count++] = PD_TXFIFO_TOK_EOP;
	data[count++] = PD_TXFIFO_TOK_TXOFF;

	// Write to FIFO and start TX
	pd_write_fifo(data, count);
	pd_write_reg(PD_REG_CONTROL0, PD_CONTROL0_TX_START);
}

void pd_tx_extended(uint16_t header, struct pd_message_extended *payload) {
	uint8_t data[48];
	size_t count = 0;

	uint8_t data_size = payload->extended_header & 0x1FF;
	// Total message length is 16-bit header (2 bytes) + 16-bit extended header
	// (2 bytes) + data size
	uint8_t message_length = 4 + data_size;

	// SOP header
	data[count++] = PD_TXFIFO_TOK_SOP1;
	data[count++] = PD_TXFIFO_TOK_SOP1;
	data[count++] = PD_TXFIFO_TOK_SOP1;
	data[count++] = PD_TXFIFO_TOK_SOP2;

	// PACKSYM for message bytes
	data[count++] = PD_TXFIFO_TOK_PACKSYM(message_length);

	// Message header
	data[count++] = header & 0xFF;
	data[count++] = (header >> 4) & 0xFF;

	// Extended header
	data[count++] = payload->extended_header & 0xFF;
	data[count++] = (payload->extended_header >> 4) & 0xFF;

	// Data
	for (int i = 0; i < data_size; ++i) {
		data[count++] = payload->data[i];
	}

	// Message trailer
	data[count++] = PD_TXFIFO_TOK_JAM_CRC;
	data[count++] = PD_TXFIFO_TOK_EOP;
	data[count++] = PD_TXFIFO_TOK_TXOFF;

	// Write to FIFO and start TX
	pd_write_fifo(data, count);
	pd_write_reg(PD_REG_CONTROL0, PD_CONTROL0_TX_START);
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
