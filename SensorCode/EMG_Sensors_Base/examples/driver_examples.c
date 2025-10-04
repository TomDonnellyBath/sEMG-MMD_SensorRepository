/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_examples.h"
#include "driver_init.h"
#include "utils.h"

/**
 * Example of using ADC_0 to generate waveform.
 */
void ADC_0_example(void)
{
	uint8_t buffer[2];

	adc_sync_enable_channel(&ADC_0, 0);

	while (1) {
		adc_sync_read_channel(&ADC_0, 0, buffer, 2);
	}
}

void I2C_MST_example(void)
{
	struct io_descriptor *I2C_MST_io;

	i2c_m_sync_get_io_descriptor(&I2C_MST, &I2C_MST_io);
	i2c_m_sync_enable(&I2C_MST);
	i2c_m_sync_set_slaveaddr(&I2C_MST, 0x12, I2C_M_SEVEN);
	io_write(I2C_MST_io, (uint8_t *)"Hello World!", 12);
}
