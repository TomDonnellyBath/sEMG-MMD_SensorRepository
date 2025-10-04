#include <atmel_start.h>

#include <peripheral_clk_config.h>

#include "AD5933.h"
#include "MAX30205.h"

#define ALERT_AND_STOP 0 // Toggle for testing intiial I2C connections to sensor units

#define EXT_SLAVE_ADDRESS 0x09 // make sure to change this for each sensor

#define AD5933_FREQUENCY_MAIN 1000 

// Macros to match Arduino function names for simplicity
#define digitalWrite(x, y) gpio_set_pin_level(x, y) 
#define digitalRead(x) gpio_get_pin_level(x)
#define togglePin(x) digitalWrite(x, !digitalRead(x))

// Command enum from the Arduino host
// Commands operate on a register-access style protocol, host issues a command and then reads/writes to the sensors buffer the appropriate number of bytes for the command
enum {
	EMG = 0x0, // Command requesting an EMG read (return 2 bytes)
	IMP_1_REAL, // Command requesting the real component of the impedance of electrode pair 1 (return 2 bytes)
	IMP_1_IMG, // Command requesting the imaginary component of the impedance of electrode pair 1 (return 2 bytes)
	IMP_2_REAL, // Command requesting the real component of the impedance of electrode pair 2 (return 2 bytes)
	IMP_2_IMG, // Command requesting the imaginary component of the impedance of electrode pair 2 (return 2 bytes)
	IMP_ALL, // Command requesting the above 4 impedance readings as a single buffer (return 8 bytes)
	TEMP, // Command requesting the temperature data (return 2 bytes)
	EMG_PERIOD, // *Depracated* Command to set the EMG period in us 
	IMP_PERIOD, // *Depracated* Command to set the impedance period in us 
	TEMP_PERIOD, // *Depracated* Command to set the temperature period in us 
	REQ_READ, // Command requesting the sensor to begin an impedance and temperature read (no return)
	READ_RDY, // Command requesting the state of the impedance and temperature read flag (return 1 byte)
	STOP_IMP_PER, // Command to stop the periodic read of the impedance and temperature (no return)
	START_IMP_PER, // Command to start the periodic read of the impedance and temperature (no return)
	SET_AD_RANGE, // Command to set the AD5933 output value (recieve 1 byte)
	SET_AD_PGA, // Command to set the AD5933 gain (recieve 1 byte)
	SET_REF_SW, //  Command to manually move the REF electrode between the EMG and impedance subsystems (recieve 1 byte)
};

// Data buffers
uint8_t emg[2]; // EMG sample buffer
// Impedance reading buffer. Union maps the struct of four 2 byte arrays to the 8 byte array permitting individual access
union imp {
	struct {
		uint8_t imp_1_real[2];
		uint8_t imp_1_img[2];
		uint8_t imp_2_real[2];
		uint8_t imp_2_img[2];
	};
	uint8_t imp_all[8];
} imp; 
uint8_t temp[2] = {0,0}; // Temp sample buffer

// Periods for delays or periodic sampling
uint16_t emg_period_us = 750;
uint16_t imp_period_ms = 10001;
uint16_t temp_period_ms = 10001;

// Counters for timing various reads or toggles of LEDs
uint16_t led_0_counter = 1;
uint16_t led_1_counter = 1;
uint16_t led_2_counter = 1;
uint32_t emg_counter = 1;
uint16_t imp_counter = 1;
uint16_t temp_counter = 1;

// Flags indicating when impedance and temperature reads are completed or requested
uint8_t read_rdy[1] = {0};
bool req_read = false;

// Flags for to indicate when to perform reads
bool do_emg = false;
bool do_imp = false;
bool do_temp = false;

// Host connection I2C settings, maps the variables to Sercom 0 to access ISR etc
#define I2C_BASE_ADDRESS       EXT_SLAVE_ADDRESS << 1 // adjust 8-bit address to 7-bit format of I2C
#define I2C_SERCOM             SERCOM0
#define I2C_SERCOM_GCLK_ID     SERCOM0_GCLK_ID_CORE
#define I2C_SERCOM_IRQ_HANDLER SERCOM0_Handler
#define I2C_SERCOM_IRQ         SERCOM0_IRQn

#define RX_BUFFER_LEN 2 // Command buffer length
#define TX_BUFFER_MAX_LEN 8 // Transmit buffer length

uint8_t tx_buffer_len = 2;
uint8_t rx_buffer[RX_BUFFER_LEN] = {0};
uint8_t temporary_buffer[TX_BUFFER_MAX_LEN] = {0};
uint8_t * tx_buffer; 
	
uint8_t cmd_chr = 255; // Recieved command

// declare I2C functions
static void i2c_init(void);
void I2C_SERCOM_IRQ_HANDLER(void);
void i2c_process_cmd_chr();

uint8_t ad5933_enabled = false;
uint8_t max30205_enabled = false;

// Interrupt setup to occur every millisecond for timing purposes
void SysTick_Handler() {
	led_0_counter++; // LED0 used to indicate system on
	
	if (ad5933_enabled) {
		imp_counter++;
	} else {
		led_1_counter++; // LED1 used to indicate if the impedance sensor is not connected or enabled
	}
	if (max30205_enabled) {
		temp_counter++;
	} else {
		led_2_counter++; // LED2 used to indicate if the temperature sensor is not connected or enabled
	}
	
	if (led_0_counter % 1001 == 0){
		led_0_counter = 1;
		togglePin(LED0);
	}
	
	if (led_1_counter % 1001 == 0) {
		led_1_counter = 1;
		togglePin(LED1);
	}
	
	if (led_2_counter % 1001 == 0) {
		led_2_counter = 1;
		togglePin(LED2);
	}
	
	if (imp_counter % imp_period_ms == 0) {
		imp_counter = 1;
		do_imp = true;
	}
	
	if (temp_counter % temp_period_ms == 0) {
		temp_counter = 1;
		do_temp = true;
	}
}

int main(void) {
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();
	
	SysTick_Config(CONF_CPU_FREQUENCY/1000); // Initialise ms interrupt
	
	// Initialise the counter for the periodic read of IT with half the period on one sensor, this means the periodic reads won't interfere with one another
	if (EXT_SLAVE_ADDRESS == 0x08) {
		imp_counter = 1;
		temp_counter = 1;
	} else {
		imp_counter = 5000;
		temp_counter = 5000;
	}
	
	// Dummy impedance values for testing, wiped on a real read
	imp.imp_1_real[0] = 100;
	imp.imp_1_real[1] = 0; // 100
	imp.imp_1_img[0] = 150;
	imp.imp_1_img[1] = 0;  // 150
	imp.imp_2_real[0] = 200;
	imp.imp_2_real[1] = 0; // 200
	imp.imp_2_img[0] = 250;
	imp.imp_2_img[1] = 0; // 250
	
	i2c_init();
	
	i2c_m_sync_enable(&I2C_MST);

	if (ALERT_AND_STOP) {
		// 1) test presence of AD5933, alert if not present
		if (AD_scan() != I2C_OK) {
			while(1) {
				togglePin(LED2);
				delay_ms(500);
			}
		}
		// 2) configure AD5933, alert by stopping the program and toggling if setup fails
		if ((	AD_reset() ||
				AD_set_clock_external() || 
				AD_set_start_freq(AD5933_FREQUENCY_MAIN) || 
				AD_set_increment_freq(0) || 
				AD_set_n_increments(0) || 
				AD_set_settling_cycles(500) ||
				AD_set_range(CTRL_OUTPUT_RANGE_2) ||
				AD_set_PGA_gain(CTRL_PGA_GAIN_X1)) 
				!= I2C_OK) {
			while(1) {
				togglePin(LED2);
				delay_ms(500);
			}		
		}
	
		AD_set_power_mode(POWER_STANDBY);
		ad5933_enabled = true;
	
		// 3) test presence of MAX30205, alert by stopping the program and toggling if setup fails
		if (MAX_scan() != I2C_OK) {
			while(1) {
				togglePin(LED2);
				delay_ms(500);
			}
		}
		// 4) configure MAX30205
		MAX_begin();
		max30205_enabled = true;
	} else {
		// check for impedance sensor
		if (AD_scan() == I2C_OK) {
			// configure impedance sensor
			if ((	AD_reset() ||
			AD_set_clock_external() ||
			AD_set_start_freq(AD5933_FREQUENCY_MAIN) ||
			AD_set_increment_freq(0) ||
			AD_set_n_increments(0) ||
			AD_set_settling_cycles(500) ||
			AD_set_range(CTRL_OUTPUT_RANGE_2) ||
			AD_set_PGA_gain(CTRL_PGA_GAIN_X1))
			!= I2C_OK) {
				ad5933_enabled = false;
			} else {
				AD_set_power_mode(POWER_STANDBY);
				ad5933_enabled = true;
			}
		} else {
			ad5933_enabled = false;
		}		
		
		// 3) test presence of MAX30205, alert if not present
		if (MAX_scan() == I2C_OK) {
			MAX_begin();
			max30205_enabled = true;
		} else {
			max30205_enabled = false;
		}
		
	}
	
	
	// start ADC
	adc_sync_enable_channel(&ADC_0, 0);
	
	// Running loop
	while (1) {
		if (do_emg) {
			// we assume the switches are always in the emg position
			
			delay_us(750); // Delay to avoid I2C activity
			adc_sync_read_channel(&ADC_0, 0, emg, 2); // Perform a read
			do_emg = false; // clear the flag
		}
		if (do_imp) {
			i2c_m_sync_set_slaveaddr(&I2C_MST, AD5933_ADDR, I2C_M_SEVEN); // Set AD5933 as the I2C target
			// switch to electrode pair 1
			digitalWrite(SW_1, true);
			digitalWrite(SW_2, false);
			digitalWrite(SW_REF, true);
			digitalWrite(SW_IMP, true);
			
			// Perform a read. (This loop may be unnecessary, i.e. AD_get_complex_data is blocking?)
			bool ad_res = false;
			while (!ad_res) {
				if (AD_get_complex_data(imp.imp_1_real, imp.imp_1_img) == I2C_OK) ad_res = true;
			}
			
			// switch to electrode pair 2
			digitalWrite(SW_1, false);
			digitalWrite(SW_2, true);
			digitalWrite(SW_REF, true);
			digitalWrite(SW_IMP, false);
			
			
			// Perform a read. (This loop may be unnecessary, i.e. AD_get_complex_data is blocking?)
			ad_res = false;
			while (!ad_res) {
				if (AD_get_complex_data(imp.imp_2_real, imp.imp_2_img) == I2C_OK) ad_res = true;
			}
			
			// reset switches to emg channel
			digitalWrite(SW_REF, false);
			digitalWrite(SW_1, false);
			digitalWrite(SW_2, false);
			digitalWrite(SW_IMP, false);
			do_imp = false; // Clear the flag
		}
		
		if (do_temp) {
			MAX_get_temperature(temp); // Perform a read
			do_temp = false; // clear the flag
		}
		
		if (req_read) { // Performs the same as the do_imp and do_temp together, triggered externally by command rather than periodically
			req_read = false;
			i2c_m_sync_set_slaveaddr(&I2C_MST, AD5933_ADDR, I2C_M_SEVEN);
			// switch to imp 1
			digitalWrite(SW_1, true);
			digitalWrite(SW_2, true);
			digitalWrite(SW_REF, true);
			digitalWrite(SW_IMP, true);
			
			// run
			bool ad_res = false;
			while (!ad_res) {
				if (AD_get_complex_data(imp.imp_1_real, imp.imp_1_img) == I2C_OK) ad_res = true;
			}
			
			// switch to imp 2
			digitalWrite(SW_1, true);
			digitalWrite(SW_2, true);
			digitalWrite(SW_REF, true);
			digitalWrite(SW_IMP, false);
			
			// run
			ad_res = false;
			while (!ad_res) {
				if (AD_get_complex_data(imp.imp_2_real, imp.imp_2_img) == I2C_OK) ad_res = true;
			}
			
			// reset switches to emg channel
			digitalWrite(SW_REF, false);
			digitalWrite(SW_1, false);
			digitalWrite(SW_2, false);
			digitalWrite(SW_IMP, false);
			//TODO: consider delay to allow settling before first emg read
			// whatever delay is here is hardly going to matter comapred to the 0.8 second it takes to read
			do_imp = false;
			
			MAX_get_temperature(temp);			
			do_temp = false;
			
			read_rdy[0] = 1;
		}
	}
}

// I2C driver setup. Atmel drivers did not seem to work in peripheral mode from Atmel start
static void i2c_init(void) {
	gpio_set_pin_function(MST_I2C_SDA, PINMUX_PA08C_SERCOM0_PAD0);
	gpio_set_pin_function(MST_I2C_SCL, PINMUX_PA09C_SERCOM0_PAD1);

	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM0_GCLK_ID_CORE, GCLK_PCHCTRL_GEN_GCLK2_Val | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM0_GCLK_ID_SLOW, GCLK_PCHCTRL_GEN_GCLK3_Val | (1 << GCLK_PCHCTRL_CHEN_Pos));
	
	hri_mclk_set_APBCMASK_SERCOM0_bit(MCLK);

	I2C_SERCOM->I2CS.ADDR.reg = I2C_BASE_ADDRESS;
	I2C_SERCOM->I2CS.CTRLB.reg = SERCOM_I2CS_CTRLB_SMEN;

	I2C_SERCOM->I2CS.INTENSET.reg = SERCOM_I2CS_INTENSET_PREC | SERCOM_I2CS_INTENSET_AMATCH |
	SERCOM_I2CS_INTENSET_DRDY;
	
	I2C_SERCOM->I2CS.CTRLA.reg = SERCOM_I2CS_CTRLA_SDAHOLD(0x02) |  SERCOM_I2CS_CTRLA_MODE(0x04); // set SDAHOLD to 300-600ns and set to slave mode
	
	I2C_SERCOM->I2CS.CTRLA.reg |= SERCOM_I2CS_CTRLA_ENABLE;
	
	while((I2C_SERCOM->I2CS.SYNCBUSY.reg & SERCOM_I2CS_SYNCBUSY_ENABLE));

	NVIC_EnableIRQ(I2C_SERCOM_IRQ);
}

uint8_t i2c_idx = 0;

// I2C interrupt handler, called when flags are set by the hardware indicating that the peripheral has been contacted by the controller
void I2C_SERCOM_IRQ_HANDLER(void) {
	
	int flags = I2C_SERCOM->I2CS.INTFLAG.reg;

	if (flags & SERCOM_I2CS_INTFLAG_AMATCH) // Check for address match flag, i.e. is it us 
	{
		I2C_SERCOM->I2CS.CTRLB.bit.ACKACT = 0;
		I2C_SERCOM->I2CS.CTRLB.bit.CMD = 0x3;
		I2C_SERCOM->I2CS.INTFLAG.bit.AMATCH = 1; // clear amatch
	}

	if (flags & SERCOM_I2CS_INTFLAG_DRDY)
	{
		if (I2C_SERCOM->I2CS.STATUS.reg & SERCOM_I2CS_STATUS_DIR) {
			// slave write
			if (i2c_idx == tx_buffer_len-1)	{
				I2C_SERCOM->I2CS.DATA.reg = tx_buffer[i2c_idx++];
				I2C_SERCOM->I2CS.CTRLB.bit.CMD = 0x2;
			} else {
				I2C_SERCOM->I2CS.DATA.reg = tx_buffer[i2c_idx++];
				I2C_SERCOM->I2CS.CTRLB.bit.CMD = 0x3;
			}
			} else {
			// slave read
			if (i2c_idx == RX_BUFFER_LEN-1) {
				I2C_SERCOM->I2CS.CTRLB.bit.ACKACT = 0;
				I2C_SERCOM->I2CS.CTRLB.bit.CMD = 0x2;
			} else {
				rx_buffer[i2c_idx++] = I2C_SERCOM->I2CS.DATA.reg;
				I2C_SERCOM->I2CS.CTRLB.bit.ACKACT = 0;
				I2C_SERCOM->I2CS.CTRLB.bit.CMD = 0x3;
			}
		}

		I2C_SERCOM->I2CS.INTFLAG.bit.DRDY = 1;
	}

	if (flags & SERCOM_I2CS_INTFLAG_PREC)
	{
		I2C_SERCOM->I2CS.INTFLAG.bit.PREC = 1;
		if (!I2C_SERCOM->I2CS.STATUS.bit.DIR) {
			rx_buffer[i2c_idx++] = I2C_SERCOM->I2CS.DATA.reg;
			// if no command, assume we have been sent a register, and setup based on that register
			if (cmd_chr == 255) {
				cmd_chr = rx_buffer[0];
				i2c_process_cmd_chr();
			} else {
				uint16_t v = rx_buffer[1] << 8 | rx_buffer[0];
				if (v > 9999) v = 9999;
			
				// here we already have a command stored, so the next master write must be a value change, so respond accordingly
				switch (cmd_chr) {
					case EMG_PERIOD:
						emg_period_us = v;
						break;
					case IMP_PERIOD:
						imp_period_ms = v;
						break;
					case TEMP_PERIOD:
						temp_period_ms = v;
						break;
					case SET_AD_RANGE:
						if (v==1) {
							AD_set_range(CTRL_OUTPUT_RANGE_1);
						} else if (v==2) {
							AD_set_range(CTRL_OUTPUT_RANGE_2);
						} else if (v==3) {
							AD_set_range(CTRL_OUTPUT_RANGE_3);
						} else if (v==4) {
							AD_set_range(CTRL_OUTPUT_RANGE_4);
						}
						break;
					case SET_AD_PGA:
						if (v==1) {
							AD_set_PGA_gain(CTRL_PGA_GAIN_X1);
						} else if (v==5) {
							AD_set_PGA_gain(CTRL_PGA_GAIN_X5);
						}
						break;
					case SET_REF_SW:
						if (v==0) {
							digitalWrite(SW_REF, false);
						} else {
							digitalWrite(SW_REF, true);
						}
						break;
					default:
						//do nothing, other registers are not writable
						break;
				}
				cmd_chr = 255; //reset command so we expect the new one
			}
		} else {
			//we have finished a transmit here, reset the cmd_chr here instead of after prepping the buffer
			cmd_chr = 255;
		}
		i2c_idx = 0;
	}
	
}

// assume we are using a repeated start method for i2c, the master will send us a char with a register address (the enum of our registers), we store this, then on request kick out the expected result
// The pointer tx_buffer is simply changed to the pointer of each data buffer or storage register and the buffer legnth updated appropriately, such that the I2C_SERCOM_IRQ_HANDLER returns the correct data following the read transaction  
void i2c_process_cmd_chr() {
	switch (cmd_chr) {
		case EMG:
			tx_buffer = (uint8_t *)emg;	
			tx_buffer_len = 2;
			do_emg = true;
			break;
		case IMP_1_REAL:
			tx_buffer = (uint8_t *)imp.imp_1_real;
			tx_buffer_len = 2;
			break;
		case IMP_1_IMG:
			tx_buffer = (uint8_t *)imp.imp_1_img;
			tx_buffer_len = 2;
			break;
		case IMP_2_REAL:
			tx_buffer = (uint8_t *)imp.imp_2_real;
			tx_buffer_len = 2;
			break;
		case IMP_2_IMG:
			tx_buffer = (uint8_t *)imp.imp_2_img;
			tx_buffer_len = 2;
			break;
		case IMP_ALL:
			tx_buffer = (uint8_t *)imp.imp_all;
			tx_buffer_len = 8;
			read_rdy[0] = 0;
			break;
		case TEMP:
			tx_buffer = (uint8_t *)temp;
			tx_buffer_len = 2;
			break;
		case EMG_PERIOD:
			utoa(emg_period_us, (char*)temporary_buffer, 10);
			tx_buffer = (uint8_t *)temporary_buffer;
			tx_buffer_len = 4;
			break;
		case IMP_PERIOD:
			utoa(imp_period_ms, (char*)temporary_buffer, 10);
			tx_buffer = (uint8_t *)temporary_buffer;
			tx_buffer_len = 4;
			break;
		case TEMP_PERIOD:
			utoa(temp_period_ms, (char*)temporary_buffer, 10);
			tx_buffer = (uint8_t *)temporary_buffer;
			tx_buffer_len = 4;
			break;
		case REQ_READ:
			read_rdy[0] = 0;
			req_read = true;
			break;
		case READ_RDY:
			tx_buffer = (uint8_t *)read_rdy;
			tx_buffer_len = 1;
			break;
		case STOP_IMP_PER:
			max30205_enabled = false;
			ad5933_enabled = false;
			imp_counter = 1;
			temp_counter = 1;		
			break;
		case START_IMP_PER:
			max30205_enabled = true;
			ad5933_enabled = true;
			if (EXT_SLAVE_ADDRESS == 0x08) {
				imp_counter = 1;
				temp_counter = 1;
			} else {
				imp_counter = 5000;
				temp_counter = 5000;
			}
			break;
		default:
			// nothing here
			break;
	}
}
