#include "keyboard.h"

void initKeyboard(void) {
  GPIOB->MODER = (GPIOB->MODER & ~(3U << (7 * 2))) | (1U << (7 * 2));
  GPIOB->MODER = (GPIOB->MODER & ~(3U << (6 * 2))) | (1U << (6 * 2));
  GPIOB->MODER = (GPIOB->MODER & ~(3U << (3 * 2))) | (1U << (3 * 2));
  GPIOA->MODER = (GPIOA->MODER & ~(3U << (10 * 2))) | (1U << (10 * 2));

  GPIOB->OTYPER |= (1U << 7) | (1U << 6) | (1U << 3);
  GPIOA->OTYPER |= (1U << 10);

  GPIOB->BSRR = (1U << 7) | (1U << 6) | (1U << 3);
  GPIOA->BSRR = (1U << 10);

  GPIOB->MODER &= ~((3U << (10 * 2)) | (3U << (4 * 2)) | (3U << (5 * 2)));
  GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (10 * 2))) | (1U << (10 * 2));
  GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (4 * 2))) | (1U << (4 * 2));
  GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (5 * 2))) | (1U << (5 * 2));

  lastKey = '\0';
  lastScanTime = 0;
}

char readKey(void) {
  const uint8_t rows[] = {7, 6, 10, 3};
  const uint8_t cols[] = {10, 4, 5};
  const char keymap[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
  };

  for (uint8_t i = 0; i < 4; i++) {
    if (rows[i] == 10) {
      GPIOA->BSRR = (1U << (rows[i] + 16));
    } else {
      GPIOB->BSRR = (1U << (rows[i] + 16));
    }

    for (volatile int d = 0; d < 100; d++);

    for (uint8_t j = 0; j < 3; j++) {
      if ((GPIOB->IDR & (1U << cols[j])) == 0) {
        if (rows[i] == 10) {
          GPIOA->BSRR = (1U << rows[i]);
        } else {
          GPIOB->BSRR = (1U << rows[i]);
        }
        return keymap[i][j];
      }
    }

    if (rows[i] == 10) {
      GPIOA->BSRR = (1U << rows[i]);
    } else {
      GPIOB->BSRR = (1U << rows[i]);
    }
  }

  return '\0';
}

char scanKeyboard(void) {
  if ((tickCount - lastScanTime) < 50) {
    return '\0';
  }

  lastScanTime = tickCount;
  char currentKey = readKey();

  if (currentKey != '\0' && currentKey != lastKey) {
    lastKey = currentKey;
    printf("Pressed: %c\n", currentKey);
    return currentKey;
  }

  if (currentKey == '\0') {
    lastKey = '\0';
  }

  return '\0';
}
