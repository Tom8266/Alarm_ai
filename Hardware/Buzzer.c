#include "Buzzer.h"
#include "gpio.h"

#define BUZZER_PORT GPIOB
#define BUZZER_PIN  GPIO_PIN_0

static uint16_t alarm_counter = 0;
static uint16_t alarm_duration = 0;

void Buzzer_Init(void) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // HIGH = off
}

void Buzzer_On(void) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // LOW = on
}

void Buzzer_Off(void) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // HIGH = off
}

void Buzzer_Toggle(void) {
    HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
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
