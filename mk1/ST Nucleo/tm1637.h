#ifndef TM1637_H
#define TM1637_H

#include <stdint.h>
#include "main.h"

#define TM1637_CLK_PIN 6  // PA6
#define TM1637_DIO_PIN 7  // PA7

#define TM1637_CMD1 0x40
#define TM1637_CMD2 0xC0
#define TM1637_CMD3 0x8F

void tm1637_init(void);
void tm1637_start(void);
void tm1637_stop(void);
void tm1637_write_byte(uint8_t byte);
void tm1637_display_digit(uint8_t digit, uint8_t data);
void tm1637_display_number(int number);
void tm1637_display_operation(uint8_t operation);
void tm1637_display_error(void);
void tm1637_clear(void);
void delay_us(uint32_t us);

#endif
