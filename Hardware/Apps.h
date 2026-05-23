#ifndef APPS_H
#define APPS_H

#include "DS3231.h"
#include <stdint.h>

extern volatile uint8_t Alarm_Triggered;

void App_Home(void);
void App_AlarmRinging(void);

#endif //APPS_H
