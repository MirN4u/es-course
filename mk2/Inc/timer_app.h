#ifndef TIMER_APP_H_
#define TIMER_APP_H_

/* Инициализация приложения (экран, зуммер). Вызывается один раз из main. */
void TimerApp_Init(void);

/* Один шаг приложения: опрос клавиатуры, таймер, экран, звук. Вызывать в главном цикле. */
void TimerApp_Step(void);

#endif /* TIMER_APP_H_ */
