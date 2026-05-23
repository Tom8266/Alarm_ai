#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Toggle(void);
void Buzzer_StartAlarm(uint16_t duration_ms);  // 响铃指定时长后自动停
void Buzzer_TimerHandler(void);                 // 10ms 调用

#endif
