#ifndef SLEEP_H
#define SLEEP_H

#include <stdint.h>

void Sleep_Enter(void);
void Sleep_WakeUp(void);
uint8_t Sleep_IsActive(void);
void Sleep_ResetIdleTimer(void);
void Sleep_IdleTimerHandler(void);  // 10ms 调用，累计1min无操作进入休眠

#endif
