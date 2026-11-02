#ifndef BUZZER_H
#define BUZZER_H
#include <stdint.h>
#define N_A5 880
#define BUZZER_VOLUME_MAX 10
#define BUZZER_VOLUME_MUTE 0
void Buzzer_Set_Freq(uint16_t f);
void Buzzer_Set_Volume(uint16_t v);
#endif
