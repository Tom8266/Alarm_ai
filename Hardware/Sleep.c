#include "Sleep.h"
#include "OLED.h"
#include "stm32f1xx_hal.h"

#define IDLE_TIMEOUT_MS  30000   // 30s 无操作休眠

// SystemClock_Config is defined in Core/Src/main.c
extern void SystemClock_Config(void);

static uint32_t idle_counter = 0;
static uint8_t sleep_active = 0;

void Sleep_ResetIdleTimer(void) {
    idle_counter = 0;
}

void Sleep_IdleTimerHandler(void) {
    if (sleep_active) return;
    idle_counter += 10;
    if (idle_counter >= IDLE_TIMEOUT_MS) {
        Sleep_Enter();
    }
}

void Sleep_Enter(void) {
    sleep_active = 1;
    SSD1315_WriteCommand(0xAE);  // 关闭 OLED 显示

    // 进入 STOP 模式，外部中断唤醒
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
    // 醒来后继续执行
    HAL_ResumeTick();
    SystemClock_Config();  // STOP 模式后需要重新配置时钟
    Sleep_WakeUp();
}

void Sleep_WakeUp(void) {
    sleep_active = 0;
    idle_counter = 0;
    SSD1315_WriteCommand(0xAF);  // 开启 OLED 显示
    SSD1315_Init();
}

uint8_t Sleep_IsActive(void) {
    return sleep_active;
}
