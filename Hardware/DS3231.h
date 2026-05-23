#ifndef DS3231_H
#define DS3231_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef enum{
    Disable,
    Enable
}Alarm_State;

typedef enum{
    Alarm1,
    Alarm2
}Alarm_Select;

typedef enum{
    ALARM_DAILY,
    ALARM_DATE,
    ALARM_WEEKDAY
}Alarm_Repeat;

typedef  struct{
    uint8_t Second;
    uint8_t Minute;
    uint8_t Hour;
    uint8_t WeekDay;
    uint8_t Month;
    uint8_t Date;
    uint8_t Year;
}Time_TypeDef;

typedef  struct{
    uint8_t Second;          // 0-59, Alarm1 only (ignored for Alarm2)
    uint8_t Minute;          // 0-59
    uint8_t Hour;            // 0-23
    uint8_t Day;             // 1-31 (Date mode) or 1-7 (Weekday mode)
    Alarm_Repeat Repeat;     // Daily / Date / Weekday
    Alarm_State State;       // Enable / Disable
}Alarm_TypeDef;

void DS3231_Init(void);
void DS3231_ReadTime(Time_TypeDef* Time);
void DS3231_WriteTime(Time_TypeDef* Time);
void DS3231_ReadAlarm(Alarm_TypeDef* Alarm, Alarm_Select Selected_Alarm);
void DS3231_WriteAlarm(Alarm_TypeDef* Alarm, Alarm_Select Selected_Alarm);
void DS3231_ReadTemperature(float* Temperature);
void DS3231_ClearAlarmFlag(void);
void DS3231_WriteRAM(uint8_t addr, uint8_t* pData, uint8_t size);
void DS3231_ReadRAM(uint8_t addr, uint8_t* pData, uint8_t size);

#endif
