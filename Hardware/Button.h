#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

typedef enum {
    BTN_NONE = 0,
    BTN_SHORT_PRESS,
    BTN_LONG_PRESS
} Button_Event;

typedef enum {
    BTN_UP = 0,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_COUNT
} Button_ID;

void Button_Init(void);
void Button_TimerHandler(void);     // 10ms 定时器中断中调用
Button_Event Button_GetEvent(Button_ID btn_id);
uint8_t Button_IsAnyPressed(void);
uint32_t Button_GetMaxPressMs(void);

#endif
