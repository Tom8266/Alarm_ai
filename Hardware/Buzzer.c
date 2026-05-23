#include "Buzzer.h"
#include "gpio.h"

static uint16_t alarm_counter = 0;
static uint16_t alarm_duration = 0;

void Buzzer_Init(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);   // HIGH = off
}

void Buzzer_On(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); // LOW = on (needs transistor)
}

void Buzzer_Off(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);   // HIGH = off
}

void Buzzer_Toggle(void) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
}

void Buzzer_StartAlarm(uint16_t duration_ms) {
    alarm_duration = duration_ms;
    alarm_counter = 0;
    Buzzer_On();
}

void Buzzer_TimerHandler(void) {
    if (alarm_duration > 0) {
        alarm_counter += 10;
        if (alarm_counter >= alarm_duration) {
            Buzzer_Off();
            alarm_duration = 0;
            alarm_counter = 0;
        }
    }
}
