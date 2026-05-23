#include "Button.h"
#include "gpio.h"
#include "stm32f1xx_hal.h"

#define DEBOUNCE_MS        20
#define LONG_PRESS_MS      2000

typedef struct {
    uint8_t  stable_state;
    uint8_t  last_raw;
    uint16_t debounce_counter;
    uint32_t press_start_tick;
    uint8_t  long_press_reported;
    Button_Event pending_event;
} Button_State;

static Button_State btn_state[BTN_COUNT];

static uint8_t Button_ReadRaw(Button_ID id) {
    switch (id) {
        case BTN_UP:    return HAL_GPIO_ReadPin(BUTTON_UP_GPIO_Port, BUTTON_UP_Pin);
        case BTN_DOWN:  return HAL_GPIO_ReadPin(BUTTON_DOWN_GPIO_Port, BUTTON_DOWN_Pin);
        case BTN_LEFT:  return HAL_GPIO_ReadPin(BUTTON_LEFT_GPIO_Port, BUTTON_LEFT_Pin);
        case BTN_RIGHT: return HAL_GPIO_ReadPin(BUTTON_RIGHT_GPIO_Port, BUTTON_RIGHT_Pin);
        default:        return 1;
    }
}

void Button_Init(void) {
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        btn_state[i].stable_state = 1;
        btn_state[i].last_raw = 1;
        btn_state[i].debounce_counter = 0;
        btn_state[i].press_start_tick = 0;
        btn_state[i].long_press_reported = 0;
        btn_state[i].pending_event = BTN_NONE;
    }
}

void Button_TimerHandler(void) {
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        uint8_t raw = Button_ReadRaw((Button_ID)i);
        Button_State *s = &btn_state[i];

        // 去抖
        if (raw == s->last_raw) {
            if (s->debounce_counter < DEBOUNCE_MS) {
                s->debounce_counter += 10;
            }
        } else {
            s->debounce_counter = 0;
        }
        s->last_raw = raw;

        if (s->debounce_counter >= DEBOUNCE_MS) {
            uint8_t prev_stable = s->stable_state;
            s->stable_state = raw;

            // 检测按下
            if (prev_stable == 1 && s->stable_state == 0) {
                s->press_start_tick = now;
                s->long_press_reported = 0;
            }

            // 松手检测：用真实时间判断
            if (prev_stable == 0 && s->stable_state == 1) {
                uint32_t elapsed = now - s->press_start_tick;
                if (elapsed >= LONG_PRESS_MS && !s->long_press_reported) {
                    s->pending_event = BTN_LONG_PRESS;
                } else if (elapsed < LONG_PRESS_MS) {
                    s->pending_event = BTN_SHORT_PRESS;
                }
                s->press_start_tick = 0;
                s->long_press_reported = 0;
            }
        }
    }
}

Button_Event Button_GetEvent(Button_ID btn_id) {
    Button_Event evt = btn_state[btn_id].pending_event;
    btn_state[btn_id].pending_event = BTN_NONE;
    return evt;
}

uint8_t Button_IsAnyPressed(void) {
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (btn_state[i].stable_state == 0) return 1;
    }
    return 0;
}

uint32_t Button_GetMaxPressMs(void) {
    uint32_t max_dur = 0;
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        if (btn_state[i].stable_state == 0 && btn_state[i].press_start_tick > 0) {
            uint32_t dur = now - btn_state[i].press_start_tick;
            if (dur > max_dur) max_dur = dur;
        }
    }
    return max_dur;
}
