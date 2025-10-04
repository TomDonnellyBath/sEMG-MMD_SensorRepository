#ifndef MAX30205_H_INCLUDED
#define MAX30205_H_INCLUDED

#include <stdint.h>
#include <atmel_start.h>

#define MAX30205_ADDR 0x48
#define MAX30205_TEMP 0x00
#define MAX30205_CONF 0x01	
#define MAX30205_HYST 0x02
#define MAX30205_TOS  0x03
#define MAX30205_REG_LEN 0x02

uint8_t MAX_scan(void);

uint8_t MAX_begin(void);

uint8_t MAX_get_temperature(uint8_t *buf); 

#endif /* MAX30205_H_INCLUDED */