#include "tm1637.h"

static const uint8_t digit_codes[] = {
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5 
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F  // 9
};
#define SEG_PLUS  0x32
#define SEG_MINUS 0x40
#define SEG_A     0x77
#define SEG_S     0x6D
#define SEG_P     0x73
#define SEG_d     0x5E
#define SEG_E     0x79
#define SEG_r     0x50

void delay_us(uint32_t us) {
  volatile uint32_t cycles = us * 24;
  while (cycles-- > 0) {
    __asm__("nop");
  }
}

void tm1637_start(void) {
  GPIOA->BSRR = (1U << TM1637_DIO_PIN) | (1U << TM1637_CLK_PIN);
  delay_us(2);
  GPIOA->BRR = (1U << TM1637_DIO_PIN);
  delay_us(2);
  GPIOA->BRR = (1U << TM1637_CLK_PIN);
  delay_us(2);
}

void tm1637_stop(void) {
  GPIOA->BRR = (1U << TM1637_CLK_PIN);
  delay_us(2);
  GPIOA->BRR = (1U << TM1637_DIO_PIN);
  delay_us(2);
  GPIOA->BSRR = (1U << TM1637_CLK_PIN);
  delay_us(2);
  GPIOA->BSRR = (1U << TM1637_DIO_PIN);
  delay_us(2);
}

void tm1637_write_byte(uint8_t byte) {
  for (uint8_t i = 0; i < 8; i++) {
    GPIOA->BRR = (1U << TM1637_CLK_PIN);
    delay_us(2);

    if (byte & 0x01) {
      GPIOA->BSRR = (1U << TM1637_DIO_PIN);
    } else {
      GPIOA->BRR = (1U << TM1637_DIO_PIN);
    }

    delay_us(2);
    GPIOA->BSRR = (1U << TM1637_CLK_PIN);
    delay_us(2);
    byte >>= 1;
  }

  GPIOA->BRR = (1U << TM1637_CLK_PIN);
  GPIOA->BSRR = (1U << TM1637_DIO_PIN);
  delay_us(2);
  GPIOA->BSRR = (1U << TM1637_CLK_PIN);
  delay_us(2);
  GPIOA->BRR = (1U << TM1637_CLK_PIN);
  delay_us(2);
}

static void tm1637_display_raw(const uint8_t data[4]) {
  tm1637_start();
  tm1637_write_byte(TM1637_CMD1);
  tm1637_stop();

  tm1637_start();
  tm1637_write_byte(TM1637_CMD2);
  for (uint8_t i = 0; i < 4; i++) {
    tm1637_write_byte(data[i]);
  }
  tm1637_stop();

  tm1637_start();
  tm1637_write_byte(TM1637_CMD3);
  tm1637_stop();
}

void tm1637_init(void) {
  GPIOA->MODER = (GPIOA->MODER & ~(0xFU << (TM1637_CLK_PIN * 2))) |
                 (0x5U << (TM1637_CLK_PIN * 2));
  GPIOA->OTYPER |= (1U << TM1637_CLK_PIN) | (1U << TM1637_DIO_PIN);
  GPIOA->OSPEEDR |= (0x3U << (TM1637_CLK_PIN * 2)) |
                    (0x3U << (TM1637_DIO_PIN * 2));

  GPIOA->BSRR = (1U << TM1637_CLK_PIN) | (1U << TM1637_DIO_PIN);

  tm1637_start();
  tm1637_write_byte(TM1637_CMD1);
  tm1637_stop();

  tm1637_start();
  tm1637_write_byte(TM1637_CMD3);
  tm1637_stop();

  tm1637_clear();
}

void tm1637_display_digit(uint8_t digit, uint8_t data) {
  if (digit > 3) return;

  tm1637_start();
  tm1637_write_byte(TM1637_CMD2 | digit);
  tm1637_write_byte(data);
  tm1637_stop();

  tm1637_start();
  tm1637_write_byte(TM1637_CMD3);
  tm1637_stop();
}

void tm1637_clear(void) {
  const uint8_t blank[4] = {0, 0, 0, 0};
  tm1637_display_raw(blank);
}

void tm1637_display_number(int number) {
  uint8_t out[4] = {0, 0, 0, 0};
  uint8_t pos = 3;
  uint8_t negative = 0;

  if (number < 0) {
    negative = 1;
    number = -number;
  }

  if (number == 0) {
    out[pos] = digit_codes[0];
  } else {
    while (number > 0 && pos < 4) {
      out[pos] = digit_codes[number % 10];
      number /= 10;
      if (pos == 0) break;
      pos--;
    }
  }

  if (negative) {
    if (out[pos] != 0 && pos > 0) {
      pos--;
    }
    out[pos] = SEG_MINUS;
  }

  tm1637_display_raw(out);
}

void tm1637_display_operation(uint8_t operation) {
  uint8_t out[4] = {0, 0, 0, 0};

  switch (operation) {
    case 0: out[3] = SEG_PLUS; break;
    case 1: out[3] = SEG_MINUS; break;
    case 2: out[3] = SEG_r; break;
    case 3: out[3] = SEG_r; break;
    default: return;
  }

  tm1637_display_raw(out);
}

void tm1637_display_error(void) {
  const uint8_t out[4] = {SEG_E, SEG_r, SEG_r, 0};
  tm1637_display_raw(out);
}
