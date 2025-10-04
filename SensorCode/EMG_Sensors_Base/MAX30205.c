#include "MAX30205.h"

uint8_t MAX_scan(void) {
	i2c_m_sync_set_slaveaddr(&I2C_MST, MAX30205_ADDR, I2C_M_SEVEN);
	uint8_t reg_buf[2];
	uint8_t ret = i2c_m_sync_cmd_read(&I2C_MST, MAX30205_HYST, reg_buf, MAX30205_REG_LEN);
	if (ret==0) {
		if((reg_buf[0] << 8 | reg_buf[1])==0x4B00) {
			return 0;
		}
	}
	return 1;
}

uint8_t MAX_begin(void) {
	uint8_t reg_buf[2] = {0x0, 0x0};
	i2c_m_sync_set_slaveaddr(&I2C_MST, MAX30205_ADDR, I2C_M_SEVEN);
	return i2c_m_sync_cmd_write(&I2C_MST, MAX30205_CONF, reg_buf, MAX30205_REG_LEN);
}

uint8_t MAX_get_temperature(uint8_t *buf) {
	i2c_m_sync_set_slaveaddr(&I2C_MST, MAX30205_ADDR, I2C_M_SEVEN);
	return i2c_m_sync_cmd_read(&I2C_MST, MAX30205_TEMP, buf, MAX30205_REG_LEN);
}