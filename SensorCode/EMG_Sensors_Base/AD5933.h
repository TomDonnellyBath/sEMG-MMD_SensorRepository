// Control functions for AD5933, adapted from https://github.com/mjmeli/arduino-ad5933

#ifndef AD5933_H_INCLUDED
#define AD5933_H_INCLUDED

#include <stdint.h>
#include <atmel_start.h>
#include <hal_i2c_m_sync.h>
// return i2c error code for operation

#define AD5933_ADDR 0x0D
#define AD5933_PTR	0xB0
#define AD5933_BR	0xA1

#define CTRL_OUTPUT_RANGE_1		(0b00000000)
#define CTRL_OUTPUT_RANGE_2		(0b00000110)
#define CTRL_OUTPUT_RANGE_3		(0b00000100)
#define CTRL_OUTPUT_RANGE_4		(0b00000010)

#define CTRL_NO_OPERATION       (0b00000000)
#define CTRL_INIT_START_FREQ    (0b00010000)
#define CTRL_START_FREQ_SWEEP   (0b00100000)
#define CTRL_INCREMENT_FREQ     (0b00110000)
#define CTRL_REPEAT_FREQ        (0b01000000)
#define CTRL_TEMP_MEASURE       (0b10010000)
#define CTRL_POWER_DOWN_MODE    (0b10100000)
#define CTRL_STANDBY_MODE       (0b10110000)
#define CTRL_RESET              (0b00010000)
#define CTRL_CLOCK_EXTERNAL     (0b00001000)
#define CTRL_CLOCK_INTERNAL     (0b00000000)
#define CTRL_PGA_GAIN_X1        (0b00000001)
#define CTRL_PGA_GAIN_X5        (0b00000000)

#define POWER_STANDBY   (CTRL_STANDBY_MODE)
#define POWER_DOWN      (CTRL_POWER_DOWN_MODE)
#define POWER_ON        (CTRL_NO_OPERATION)

#define STATUS_DATA_VALID       (0x02)
#define STATUS_SWEEP_DONE       (0x04)
#define STATUS_ERROR            (0xFF)

#define SWEEP_DELAY             (1)

enum AD_reg_t {
	AD5933_CTRL = 0x00,
	AD5933_START_FREQ,
	AD5933_FREQ_INC,
	AD5933_N_INC,
	AD5933_N_CYC,
	AD5933_STAT,
	AD5933_TEMP,
	AD5933_REAL,
	AD5933_IMGY,
	AD5933_NUM_REG
};

#define AD5933_CTRL_ADDR		0x80
#define AD5933_START_FREQ_ADDR	0x82
#define AD5933_FREQ_INC_ADDR	0x85
#define AD5933_N_INC_ADDR		0x88
#define AD5933_N_CYC_ADDR		0x8A
#define AD5933_STAT_ADDR		0x8F
#define AD5933_TEMP_ADDR		0x92
#define AD5933_REAL_ADDR		0x94
#define AD5933_IMGY_ADDR		0x96

#define AD5933_CTRL_LEN			0x02
#define AD5933_START_FREQ_LEN	0x03
#define AD5933_FREQ_INC_LEN		0x03
#define AD5933_N_INC_LEN		0x02
#define AD5933_N_CYC_LEN		0x02
#define AD5933_STAT_LEN			0x01
#define AD5933_TEMP_LEN			0x02
#define AD5933_REAL_LEN			0x02
#define AD5933_IMGY_LEN			0x02

typedef struct AD_i2c_reg_t {
	uint8_t addr;
	uint8_t size;
} AD_i2c_reg_t;

uint8_t point_addr_buf[2];
uint8_t block_read_buf[2];

const AD_i2c_reg_t _AD_reg[AD5933_NUM_REG];

uint8_t AD_scan(void);

uint8_t AD_get_bytes(AD_i2c_reg_t reg, uint8_t *value);

uint8_t AD_set_bytes(AD_i2c_reg_t reg, uint8_t *value);

uint8_t AD_reset(void);

uint8_t AD_set_control_mode(uint8_t mode);

uint8_t AD_set_clock_internal(void);

uint8_t AD_set_clock_external(void);

uint8_t AD_set_settling_cycles(uint32_t time);

uint8_t AD_set_start_freq(uint32_t start);

uint8_t AD_set_increment_freq(uint32_t increment);

uint8_t AD_set_n_increments(uint32_t n);

uint8_t AD_set_PGA_gain(uint8_t gain);

uint8_t AD_set_range(uint8_t range);

uint8_t AD_get_complex_data(uint8_t *real, uint8_t *img);

uint8_t AD_set_power_mode(uint8_t level);



#endif /* AD5933_H_INCLUDED */