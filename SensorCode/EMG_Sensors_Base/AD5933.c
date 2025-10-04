/*
 * AD5933.c
 *
 * Created: 06/06/2023 10:43:30
 *  Author: tommd
 */ 

#include "AD5933.h"
#include <math.h>

const AD_i2c_reg_t _AD_reg[AD5933_NUM_REG] = {
	{AD5933_CTRL_ADDR, AD5933_CTRL_LEN},
	{AD5933_START_FREQ_ADDR, AD5933_START_FREQ_LEN},
	{AD5933_FREQ_INC_ADDR, AD5933_FREQ_INC_LEN},
	{AD5933_N_INC_ADDR, AD5933_N_INC_LEN},
	{AD5933_N_CYC_ADDR, AD5933_N_CYC_LEN},
	{AD5933_STAT_ADDR, AD5933_STAT_LEN},
	{AD5933_TEMP_ADDR, AD5933_TEMP_LEN},
	{AD5933_REAL_ADDR, AD5933_REAL_LEN},
	{AD5933_IMGY_ADDR, AD5933_IMGY_LEN},
};

static struct io_descriptor *i2c_mst_io;

static const unsigned long clock_speed = 16776000;
static const unsigned long ext_clock_speed = 1000000;

uint8_t ret;

bool clock_ext = false;

// Function to check if the AD5933 is present on the bus, and confirms the device is in reset by checking for the default control register value
uint8_t AD_scan(void) {
	//setup buffers
	point_addr_buf[0] = AD5933_PTR;
	block_read_buf[0] = AD5933_BR;
	block_read_buf[1] = 4;
	
	i2c_m_sync_get_io_descriptor(&I2C_MST, &i2c_mst_io);
	i2c_m_sync_set_slaveaddr(&I2C_MST, AD5933_ADDR, I2C_M_SEVEN);
	uint8_t reg_buf[2];
	if (AD_get_bytes(_AD_reg[AD5933_CTRL], reg_buf) == I2C_OK) {
		if((reg_buf[0] << 8 | reg_buf[1])==0xA000) {
			return 0;
		}
	}
	return 1;
}


// Helper functions for getting and setting bytes at register locations on the AD5933
uint8_t AD_get_bytes(AD_i2c_reg_t reg, uint8_t *value) {
	for (uint8_t i = 0; i < reg.size; i++) {
		point_addr_buf[1] = reg.addr + i;
		if ((ret = io_write(i2c_mst_io, point_addr_buf, 2)) < I2C_OK) return ret;
		if ((ret = io_read(i2c_mst_io, &value[i], 1)) < I2C_OK) return ret;
	}
	return I2C_OK;
}

uint8_t AD_set_bytes(AD_i2c_reg_t reg, uint8_t *value) {
	uint8_t addr_d_buf[2];
	for (uint8_t i = 0; i < reg.size; i++) {
		addr_d_buf[0] = reg.addr + i;
		addr_d_buf[1] = value[i];
		if ((ret = io_write(i2c_mst_io, addr_d_buf, 2)) < I2C_OK) return ret;
	}
	return I2C_OK;
}


// Setup functions for the AD5933, access specfic registers and apply required values
uint8_t AD_reset(void) {
	uint8_t val[2];
	if ((ret = AD_get_bytes(_AD_reg[AD5933_CTRL], val)) != I2C_OK) return ret;
	val[1] |= CTRL_RESET;
	return AD_set_bytes(_AD_reg[AD5933_CTRL], val);
}

uint8_t AD_set_control_mode(uint8_t mode) {
	uint8_t val[2];
	if ((ret = AD_get_bytes(_AD_reg[AD5933_CTRL], val)) != I2C_OK) return ret;
	val[0] &= 0x0F; 
	val[0] |= mode;
	return AD_set_bytes(_AD_reg[AD5933_CTRL], val);
}

uint8_t AD_set_clock_internal(void) {
	clock_ext = false;
	uint8_t val[2];
	if ((ret = AD_get_bytes(_AD_reg[AD5933_CTRL], val)) != I2C_OK) return ret;
	val[1] = CTRL_CLOCK_INTERNAL; 
	return AD_set_bytes(_AD_reg[AD5933_CTRL], val);
}

uint8_t AD_set_clock_external(void) {
	clock_ext = true;
	uint8_t val[2];
	if ((ret = AD_get_bytes(_AD_reg[AD5933_CTRL], val)) != I2C_OK) return ret;
	val[1] = CTRL_CLOCK_EXTERNAL;
	return AD_set_bytes(_AD_reg[AD5933_CTRL], val);
}

uint8_t AD_set_settling_cycles(uint32_t time) {
	uint32_t cycles;
	uint8_t settleTime[2], val;
	
	settleTime[1] = time & 0xFF;
	settleTime[0] = (time >> 8) & 0xFF;
	
	cycles = (settleTime[1] | (settleTime[0] & 0x1));
	val = (uint8_t)((settleTime[0] & 0x7) >> 1);
	
	if ((cycles > 0x1FF) || !(val == 0 || val == 1 || val == 3))
	{
		return 255;
	}

	return (AD_set_bytes(_AD_reg[AD5933_N_CYC], settleTime));
}

uint8_t AD_set_start_freq(uint32_t start) {
	uint32_t freqHex;
	if (clock_ext) {
		freqHex = (start/(ext_clock_speed/4.0))*pow(2, 27);
	} else {
		freqHex = (start/(clock_speed/4.0))*pow(2, 27);
	}
	if (freqHex > 0xFFFFFF) return 255;
	
	uint8_t freq_buf[3];
	freq_buf[0] = (freqHex >> 16) & 0xFF;
	freq_buf[1] = (freqHex >> 8) & 0xFF;
	freq_buf[2] = freqHex & 0xFF;
	
	AD_set_bytes(_AD_reg[AD5933_START_FREQ], freq_buf);
	
	return AD_get_bytes(_AD_reg[AD5933_START_FREQ], freq_buf);
}

uint8_t AD_set_increment_freq(uint32_t increment) {
	uint32_t freqHex;
	if (clock_ext) {
		freqHex = (increment/(ext_clock_speed/4.0))*pow(2, 27);
		} else {
		freqHex = (increment/(clock_speed/4.0))*pow(2, 27);
	}
	if (freqHex > 0xFFFFFF) return 255;
	
	uint8_t freq_buf[3];
	freq_buf[0] = (freqHex >> 16) & 0xFF;
	freq_buf[1] = (freqHex >> 8) & 0xFF;
	freq_buf[2] = freqHex & 0xFF;
	
	return AD_set_bytes(_AD_reg[AD5933_FREQ_INC], freq_buf);
}

uint8_t AD_set_n_increments(uint32_t n) {
	if (n > 511) return 255;
	
	uint8_t val[2];
	val[0] = (n >> 8) & 0xFF;
	val[1] = n & 0xFF;
	
	return AD_set_bytes(_AD_reg[AD5933_N_INC], val);
}

uint8_t AD_set_PGA_gain(uint8_t gain) {
	uint8_t val[2];
	if ((ret = AD_get_bytes(_AD_reg[AD5933_CTRL], val)) != I2C_OK) return ret;
	
	val[0] &= 0xFE;
	
	switch (gain) {
		case CTRL_PGA_GAIN_X1:
			val[0] |= CTRL_PGA_GAIN_X1;
			break; 
		case CTRL_PGA_GAIN_X5:
			val[0] |= CTRL_PGA_GAIN_X5;
			break;		
		default:
			return 255;
	}
	
	return AD_set_bytes(_AD_reg[AD5933_CTRL], val);
}

uint8_t AD_set_range(uint8_t range) {
	uint8_t val[2];
	if ((ret = AD_get_bytes(_AD_reg[AD5933_CTRL], val)) != I2C_OK) return ret;
	
	val[0] &= 0xF9;
	
	switch (range) {
		case CTRL_OUTPUT_RANGE_1:
			val[0] |= CTRL_OUTPUT_RANGE_1; // 2V p-p
			break;
		case CTRL_OUTPUT_RANGE_2:
			val[0] |= CTRL_OUTPUT_RANGE_2; // 1v p-p
			break;
		case CTRL_OUTPUT_RANGE_3:
			val[0] |= CTRL_OUTPUT_RANGE_3; // 400mv p-p
			break;
		default:
			val[0] |= CTRL_OUTPUT_RANGE_4; // 200mv p-p
	}
	
	return AD_set_bytes(_AD_reg[AD5933_CTRL], val);
}

uint8_t AD_set_power_mode(uint8_t level) {
	switch(level) {
		case POWER_ON:
			return AD_set_control_mode(CTRL_NO_OPERATION);
		case POWER_STANDBY:
			return AD_set_control_mode(CTRL_STANDBY_MODE);
		case POWER_DOWN:
			return AD_set_control_mode(CTRL_POWER_DOWN_MODE);
		default:
			return 255;
	}
}



// Function to access AD5933 data, follows the flowchart in the AD5933 datasheet
uint8_t AD_get_complex_data(uint8_t *real, uint8_t *img) {
	if ((ret = AD_set_power_mode(POWER_STANDBY)) != I2C_OK) return ret; // Set the power state, return if fails
	
	if ((ret = AD_set_control_mode(CTRL_INIT_START_FREQ)) != I2C_OK) return ret; // Set the initial frequency value, return if fails
	
	if ((ret = AD_set_control_mode(CTRL_START_FREQ_SWEEP)) != I2C_OK) return ret; // Set the sweep parameters, return if fails
	
	// The read begins, poll the device for read completed flag 
	uint8_t status_reg = 0;
	do { 
		if ((ret = AD_get_bytes(_AD_reg[AD5933_STAT], &status_reg)) != I2C_OK) return ret; 
	} while((status_reg & STATUS_DATA_VALID) != STATUS_DATA_VALID);
		
	
	
	// Get the real and imaginary components
	if ((ret = AD_get_bytes(_AD_reg[AD5933_REAL], real)) != I2C_OK) return ret;
	if ((ret = AD_get_bytes(_AD_reg[AD5933_IMGY], img))  != I2C_OK) return ret;
	
	// Call the increment command as this is expected
	if ((ret = AD_set_control_mode(CTRL_INCREMENT_FREQ)) != I2C_OK) return ret;
	
	do {
		if ((ret = AD_get_bytes(_AD_reg[AD5933_STAT], &status_reg)) != I2C_OK) return ret;
	} while ((status_reg & STATUS_SWEEP_DONE) != STATUS_SWEEP_DONE); // sweep will be completed as we setup to have no increments
	
	return AD_set_power_mode(POWER_STANDBY); // reset the power state
}