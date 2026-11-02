#include "main.h"
#include "keyboard.h"
#include "tm1637.h"

volatile uint32_t tickCount;
char lastKey;
uint32_t lastScanTime;

typedef enum {
  OP_ADD = 0,
  OP_SUB,
  OP_MUL,
  OP_DIV
} Operation;

static int operand1 = -1;
static int operand2 = -1;
static int operation = -1;
static uint8_t resultShown = 0;

void osSystickHandler(void) {
  tickCount++;
}

void initGPIO(void) {
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
  GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
  GPIOA->OTYPER &= ~(1U << 5);
  GPIOA->OSPEEDR |= (1U << 10);
}

void initUSART2(void) {
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  GPIOA->MODER = (GPIOA->MODER & ~(0xFU << 4)) | (0xAU << 4);
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFFU << 8)) | (1U << 8) | (1U << 12);

  USART2->BRR = 417; // 48 MHz / 115200
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void initSysTick(void) {
  SysTick->LOAD = 47999; // 1 мс при 48 MHz
  SysTick->VAL = 0;
  SysTick->CTRL = (1U << 2) | (1U << 1) | (1U << 0);
}

int _write(int file, uint8_t *ptr, int len) {
  (void)file;
  for (int i = 0; i < len; i++) {
    while (!(USART2->ISR & USART_ISR_TXE));
    USART2->TDR = ptr[i];
  }
  return len;
}

static char operationChar(int op) {
  static const char chars[] = {'+', '-', '*', '/'};
  return (op >= OP_ADD && op <= OP_DIV) ? chars[op] : '?';
}

static void startNewExpression(int digit) {
  operand1 = digit;
  operand2 = -1;
  operation = -1;
  resultShown = 0;

  tm1637_display_number(operand1);
  printf("First operand: %d\n", operand1);
}

static void calculate(void) {
  int result = 0;

  if (operand1 < 0 || operand2 < 0 || operation < 0) {
    printf("Expression is incomplete\n");
    tm1637_display_error();
    resultShown = 1;
    return;
  }

  switch ((Operation)operation) {
    case OP_ADD:
      result = operand1 + operand2;
      break;

    case OP_SUB:
      result = operand1 - operand2;
      break;

    case OP_MUL:
      result = operand1 * operand2;
      break;

    case OP_DIV:
      if (operand2 == 0) {
        printf("Division by zero\n");
        tm1637_display_error();
        resultShown = 1;
        return;
      }
      // Деление целочисленное
      result = operand1 / operand2;
      break;
  }

  printf("%d %c %d = %d\n",
         operand1, operationChar(operation), operand2, result);
  tm1637_display_number(result);
  resultShown = 1;
}

static void processKey(char key) {
  if (key >= '0' && key <= '9') {
    int digit = key - '0';

    if (resultShown || operand1 < 0) {
      startNewExpression(digit);
      return;
    }

    if (operation < 0) {
      operand1 = digit;
      tm1637_display_number(operand1);
      printf("First operand: %d\n", operand1);
      return;
    }

    operand2 = digit;
    tm1637_display_number(operand2);
    printf("Second operand: %d\n", operand2);
    return;
  }

  if (key == '*') {
    if (resultShown || operand1 < 0) {
      return;
    }
    // Каждое нажатие * переключает операцию:
    operation = (operation + 1) % 4;
    tm1637_display_operation((uint8_t)operation);
    printf("Selected operation: %c\n", operationChar(operation));
    return;
  }

  if (key == '#') {
    calculate();
  }
}

int main(void) {
  initGPIO();
  initUSART2();
  initSysTick();
  initKeyboard();
  tm1637_init();

  printf("STM32 calculator ready\n");
  printf("Operations cycle: +, -, *, /\n");

  GPIOA->ODR |= (1U << 5);
  tm1637_clear();

  while (1) {
    char key = scanKeyboard();
    if (key != '\0') {
      processKey(key);
    }
  }
}
