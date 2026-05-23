#include "OLED.h"
#include "DS3231.h"
#include "Font.h"
#include "Menu.h"
#include <stdint.h>

volatile uint8_t Alarm_Triggered = 0;

void App_Home(void){
    Time_TypeDef Time;
    const char* Days[]={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    DS3231_ReadTime(&Time);
    SSD1315_DrawRectangle(&(Rectangle_TypeDef){.Color = Black, .Location.X = 0, .Location.Y = 0, .Size.Length = 17, .Size.Width = 127});
    SSD1315_ShowString(47,1,"20  /  /  ",2,White);
    SSD1315_ShowNumber(63,1,Time.Year,2,White);
    SSD1315_ShowNumber(87,1,Time.Month,2,White);
    SSD1315_ShowNumber(111,1,Time.Date,2,White);
    SSD1315_ShowString(2, 54, Slogans[current_slogan], 1, White);
    Font_Draw7SegTime(&Time, 18, 19);
    SSD1315_ShowString(2, 1, Days[Time.WeekDay - 1], 2, White);
}

void App_AlarmRinging(void){
    static uint8_t flash = 0;
    flash = !flash;
    SSD1315_Clear();
    SSD1315_ShowString(28, 20, "ALARM!", 2, flash ? White : Black);
    SSD1315_ShowString(20, 45, "Press any key", 1, White);
}
