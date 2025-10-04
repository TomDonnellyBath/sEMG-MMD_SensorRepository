/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_init.h"
#include <peripheral_clk_config.h>
#include <utils.h>
#include <hal_init.h>

#include <hpl_adc_base.h>

struct adc_sync_descriptor ADC_0;

struct i2c_m_sync_desc I2C_MST;

void ADC_0_PORT_init(void)
{

	// Disable digital pin circuitry
	gpio_set_pin_direction(EMG_ADC, GPIO_DIRECTION_OFF);

	gpio_set_pin_function(EMG_ADC, PINMUX_PA07B_ADC_AIN7);
}

void ADC_0_CLOCK_init(void)
{
	hri_mclk_set_APBDMASK_ADC_bit(MCLK);
	hri_gclk_write_PCHCTRL_reg(GCLK, ADC_GCLK_ID, CONF_GCLK_ADC_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
}

void ADC_0_init(void)
{
	ADC_0_CLOCK_init();
	ADC_0_PORT_init();
	adc_sync_init(&ADC_0, ADC, (void *)NULL);
}

void I2C_MST_PORT_init(void)
{

	gpio_set_pin_pull_mode(I2C_SDA,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(I2C_SDA, PINMUX_PA16C_SERCOM1_PAD0);

	gpio_set_pin_pull_mode(I2C_SCL,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(I2C_SCL, PINMUX_PA17C_SERCOM1_PAD1);
}

void I2C_MST_CLOCK_init(void)
{
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM1_GCLK_ID_CORE, CONF_GCLK_SERCOM1_CORE_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM1_GCLK_ID_SLOW, CONF_GCLK_SERCOM1_SLOW_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));

	hri_mclk_set_APBCMASK_SERCOM1_bit(MCLK);
}

void I2C_MST_init(void)
{
	I2C_MST_CLOCK_init();
	i2c_m_sync_init(&I2C_MST, SERCOM1);
	I2C_MST_PORT_init();
}

void system_init(void)
{
	init_mcu();

	// GPIO on PA08

	gpio_set_pin_pull_mode(MST_I2C_SDA,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(MST_I2C_SDA, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA09

	gpio_set_pin_pull_mode(MST_I2C_SCL,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(MST_I2C_SCL, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA10

	gpio_set_pin_direction(GCLK_4_Out,
	                       // <y> Pin direction
	                       // <id> pad_direction
	                       // <GPIO_DIRECTION_OFF"> Off
	                       // <GPIO_DIRECTION_IN"> In
	                       // <GPIO_DIRECTION_OUT"> Out
	                       GPIO_DIRECTION_OUT);

	gpio_set_pin_level(GCLK_4_Out,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	gpio_set_pin_pull_mode(GCLK_4_Out,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(GCLK_4_Out,
	                      // <y> Pin function
	                      // <id> pad_function
	                      // <i> Auto : use driver pinmux if signal is imported by driver, else turn off function
	                      // <GPIO_PIN_FUNCTION_OFF"> Auto
	                      // <GPIO_PIN_FUNCTION_OFF"> Off
	                      // <GPIO_PIN_FUNCTION_A"> A
	                      // <GPIO_PIN_FUNCTION_B"> B
	                      // <GPIO_PIN_FUNCTION_C"> C
	                      // <GPIO_PIN_FUNCTION_D"> D
	                      // <GPIO_PIN_FUNCTION_E"> E
	                      // <GPIO_PIN_FUNCTION_F"> F
	                      // <GPIO_PIN_FUNCTION_G"> G
	                      // <GPIO_PIN_FUNCTION_H"> H
	                      // <GPIO_PIN_FUNCTION_I"> I
	                      GPIO_PIN_FUNCTION_H);

	// GPIO on PA11

	gpio_set_pin_level(LED0,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   true);

	// Set pin direction to output
	gpio_set_pin_direction(LED0, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(LED0, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA14

	gpio_set_pin_level(LED2,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   true);

	// Set pin direction to output
	gpio_set_pin_direction(LED2, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(LED2, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA15

	gpio_set_pin_level(LED1,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   true);

	// Set pin direction to output
	gpio_set_pin_direction(LED1, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(LED1, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA22

	gpio_set_pin_level(SW_1,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(SW_1, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(SW_1, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA23

	gpio_set_pin_level(SW_2,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(SW_2, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(SW_2, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA24

	gpio_set_pin_level(SW_REF,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(SW_REF, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(SW_REF, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PA25

	gpio_set_pin_level(SW_IMP,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(SW_IMP, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(SW_IMP, GPIO_PIN_FUNCTION_OFF);

	ADC_0_init();

	I2C_MST_init();
}
