/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef ATMEL_START_PINS_H_INCLUDED
#define ATMEL_START_PINS_H_INCLUDED

#include <hal_gpio.h>

// SAML21 has 9 pin functions

#define GPIO_PIN_FUNCTION_A 0
#define GPIO_PIN_FUNCTION_B 1
#define GPIO_PIN_FUNCTION_C 2
#define GPIO_PIN_FUNCTION_D 3
#define GPIO_PIN_FUNCTION_E 4
#define GPIO_PIN_FUNCTION_F 5
#define GPIO_PIN_FUNCTION_G 6
#define GPIO_PIN_FUNCTION_H 7
#define GPIO_PIN_FUNCTION_I 8

#define EMG_ADC GPIO(GPIO_PORTA, 7)
#define MST_I2C_SDA GPIO(GPIO_PORTA, 8)
#define MST_I2C_SCL GPIO(GPIO_PORTA, 9)
#define GCLK_4_Out GPIO(GPIO_PORTA, 10)
#define LED0 GPIO(GPIO_PORTA, 11)
#define LED2 GPIO(GPIO_PORTA, 14)
#define LED1 GPIO(GPIO_PORTA, 15)
#define I2C_SDA GPIO(GPIO_PORTA, 16)
#define I2C_SCL GPIO(GPIO_PORTA, 17)
#define SW_1 GPIO(GPIO_PORTA, 22)
#define SW_2 GPIO(GPIO_PORTA, 23)
#define SW_REF GPIO(GPIO_PORTA, 24)
#define SW_IMP GPIO(GPIO_PORTA, 25)

#endif // ATMEL_START_PINS_H_INCLUDED
