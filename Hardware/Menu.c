#include "Menu.h"
#include "OLED.h"
#include "Apps.h"
#include "Button.h"
#include "DS3231.h"
#include "Buzzer.h"
#include "Sleep.h"
#include "Font.h"

Appearance_Settings appearance = {.bar_style = BAR_STYLE_BAR, .home_style = HOME_STYLE_CLOCK};

// ====== 配置持久化 (内部 Flash，最后一页) ======
#define CFG_FLASH_ADDR  0x0800FC00
#define CFG_PAGE_SIZE   1024
#define CFG_MAGIC       0xA55A

static void Settings_Load(void) {
    uint16_t magic = *(volatile uint16_t*)CFG_FLASH_ADDR;
    if (magic == CFG_MAGIC) {
        uint8_t* p = (uint8_t*)(CFG_FLASH_ADDR + 2);
        appearance.bar_style  = (BarStyle)p[0];
        appearance.home_style = (HomeStyle)p[1];
    }
}

static void Settings_Save(void) {
    __disable_irq();
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {.TypeErase = FLASH_TYPEERASE_PAGES,
                                     .PageAddress = CFG_FLASH_ADDR,
                                     .NbPages = 1};
    uint32_t err;
    if (HAL_FLASHEx_Erase(&erase, &err) == HAL_OK) {
        uint16_t buf[2] = {CFG_MAGIC,
                           (uint16_t)(((uint8_t)appearance.bar_style) | ((uint8_t)appearance.home_style << 8))};
        for (uint8_t i = 0; i < 2; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, CFG_FLASH_ADDR + i * 2, buf[i]);
        }
    }

    HAL_FLASH_Lock();
    __enable_irq();
}

void Menu_Init(void) {
    Settings_Load();
}

static Page_ID current_page = PAGE_HOME;

// ---- 顶部菜单 ----
static uint8_t menu_selection = 0;
static const char *menu_items[] = {"Alarm", "Time Set", "Appearance"};
#define MENU_ITEM_COUNT 3

// ---- 闹钟 ----
static uint8_t  alarm_list_sel   = 0;  // 闹钟列表光标
static uint8_t  alarm_step       = 0;  // 编辑项光标
static uint8_t  alarm_editing    = 0;  // 0=浏览, 1=编辑中
static uint8_t  alarm_is_alarm1  = 1;  // 当前编辑的是 Alarm1 还是 Alarm2
static Alarm_TypeDef alarm_buf;

// ---- 时间设置 ----
static uint8_t time_set_step = 0;
static uint8_t time_editing = 0;
static Time_TypeDef time_buf;
static const char *time_set_labels[] = {"Year", "Month", "Date", "Hour", "Min", "Week"};

// ---- 外观设置 ----
static uint8_t appearance_selection = 0;
static uint8_t appearance_editing = 0;
#define APPR_ITEM_COUNT 2

// ---- 进度条 ----
static uint8_t progress_width = 0;
static uint8_t press_was_active = 0;

// ---- 进度条绘制 ----
static void DrawProgressBar(uint8_t w) {
    if (w == 0) return;
    if (w > 118) w = 118;
    Rectangle_TypeDef border = {.Color = Black, .Location.X = 5, .Location.Y = 50,
                                .Size.Length = 3, .Size.Width = 118};
    SSD1315_DrawRectangle(&border);
    if (w > 0) {
        Rectangle_TypeDef fill = {.Color = White, .Location.X = 6, .Location.Y = 51,
                                   .Size.Length = 1, .Size.Width = w};
        SSD1315_InvertRectangle(&fill);
    }
}

static void DrawProgressBarFullscreen(uint8_t w) {
    if (w == 0) return;
    if (w > 128) w = 128;
    Rectangle_TypeDef fill = {.Color = White, .Location.X = 0, .Location.Y = 0,
                               .Size.Length = 64, .Size.Width = w};
    SSD1315_InvertRectangle(&fill);
}

// ---- 页面切换 ----
void Page_Set(Page_ID page) {
    current_page = page;
    menu_selection = 0;
    progress_width = 0;
    press_was_active = 0;
    Sleep_ResetIdleTimer();
}

Page_ID Page_Get(void) {
    return current_page;
}

// ========== 辅助：进入闹钟编辑页 ==========
static void Alarm_EnterEdit(uint8_t is_alarm1) {
    alarm_is_alarm1 = is_alarm1;
    DS3231_ReadAlarm(&alarm_buf, is_alarm1 ? Alarm1 : Alarm2);
    alarm_step = 0;
    alarm_editing = 0;
}

static uint8_t Alarm_StepCount(void) {
    // Hour + Minute + Repeat + Day(if not daily) + State
    uint8_t n = 3; // Hour, Min, Repeat, State = 4? No: step 0=Hour,1=Min,2=Repeat,3=State,4=Day
    if (alarm_is_alarm1) n++; // + Second
    if (alarm_buf.Repeat != ALARM_DAILY) n++; // + Day
    return n;
}

static uint8_t Alarm_StepLabel(uint8_t step, const char** label) {
    if (alarm_is_alarm1) {
        if (step == 0) { *label = "Hour";   return 1; }
        if (step == 1) { *label = "Min";    return 1; }
        if (step == 2) { *label = "Sec";    return 1; }
        if (step == 3) { *label = "Repeat"; return 1; }
        if (alarm_buf.Repeat != ALARM_DAILY && step == 4) { *label = "Day"; return 1; }
        if (step == Alarm_StepCount() - 1) { *label = "State"; return 1; }
    } else {
        if (step == 0) { *label = "Hour";   return 1; }
        if (step == 1) { *label = "Min";    return 1; }
        if (step == 2) { *label = "Repeat"; return 1; }
        if (alarm_buf.Repeat != ALARM_DAILY && step == 3) { *label = "Day"; return 1; }
        if (step == Alarm_StepCount() - 1) { *label = "State"; return 1; }
    }
    return 0;
}

// ========== 绘制函数 ==========

static void Page_DrawMenu(void) {
    SSD1315_ShowString(40, 0, "Menu", 2, White);
    uint8_t visible_rows = 3;
    uint8_t top = 0;
    if (menu_selection >= visible_rows)
        top = menu_selection - visible_rows + 1;
    for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++) {
        if (i < top || i >= top + visible_rows) continue;
        uint8_t y = 18 + (i - top) * 16;
        if (i == menu_selection) {
            SSD1315_ShowString(8, y, ">", 1, White);
            SSD1315_ShowString(18, y, menu_items[i], 1, White);
        } else {
            SSD1315_ShowChar(8, y, ' ', 1, White);
            SSD1315_ShowString(18, y, menu_items[i], 1, White);
        }
    }
}

static void Page_DrawAlarmList(void) {
    SSD1315_ShowString(36, 0, "Alarm", 2, White);
    const char *names[] = {"Alarm 1", "Alarm 2"};
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t y = 22 + i * 16;
        if (i == alarm_list_sel) {
            SSD1315_ShowString(8, y, ">", 1, White);
            SSD1315_ShowString(24, y, names[i], 1, White);
        } else {
            SSD1315_ShowChar(8, y, ' ', 1, White);
            SSD1315_ShowString(24, y, names[i], 1, White);
        }
    }
}

static void Page_DrawAlarmSet_Common(void) {
    const char *title = alarm_is_alarm1 ? "Alarm 1" : "Alarm 2";
    SSD1315_ShowString(28, 0, title, 2, White);

    uint8_t visible = 4;
    uint8_t top = 0;
    if (alarm_step >= visible) top = alarm_step - visible + 1;

    for (uint8_t vi = 0; vi < visible; vi++) {
        uint8_t step = top + vi;
        const char *label = NULL;
        if (!Alarm_StepLabel(step, &label)) continue;

        uint8_t y = 12 + vi * 13;
        // 标签 + 值
        SSD1315_ShowString(16, y, label, 1, White);
        SSD1315_ShowString(56, y, ":", 1, White);

        uint8_t n_draw = 0;

        if (step == 0)      { n_draw = alarm_buf.Hour;   }  // Hour
        else if (step == 1) { n_draw = alarm_buf.Minute; }  // Min
        else if (step == 2 && alarm_is_alarm1) { n_draw = alarm_buf.Second; } // Sec
        else if (step == ((alarm_is_alarm1 && alarm_buf.Repeat != ALARM_DAILY) ? 4 : 3) &&
                 alarm_buf.Repeat != ALARM_DAILY) {
            // Day
            if (alarm_buf.Repeat == ALARM_WEEKDAY) {
                const char *dn[] = {"", "Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
                uint8_t d = (alarm_buf.Day >= 1 && alarm_buf.Day <= 7) ? alarm_buf.Day : 1;
                SSD1315_ShowString(64, y, dn[d], 1, White);
            } else {
                SSD1315_ShowNumber(64, y, alarm_buf.Day, 1, White);
            }
            n_draw = 255; // already drawn
        }

        if (n_draw != 255 && label[0] != 'R' && label[0] != 'S') {
            SSD1315_ShowNumber(64, y, n_draw, 1, White);
        } else if (label[0] == 'R') {
            // Repeat
            const char *rn[] = {"Daily", "Date", "Weekday"};
            SSD1315_ShowString(64, y, rn[alarm_buf.Repeat], 1, White);
        } else if (label[0] == 'S') {
            // State
            SSD1315_ShowString(64, y, alarm_buf.State == Enable ? "ON" : "OFF", 1, White);
        }

        // 高亮/光标
        if (alarm_editing && step == alarm_step) {
            Rectangle_TypeDef r = {.Color = White, .Location.X = 0, .Location.Y = (uint8_t)(y - 2),
                                    .Size.Length = 12, .Size.Width = 127};
            SSD1315_InvertRectangle(&r);
        } else {
            SSD1315_ShowString(4, y, step == alarm_step ? ">" : " ", 1, White);
        }
    }
}

static void Page_DrawTimeSet(void) {
    SSD1315_ShowString(20, 0, "Time Set", 2, White);
    uint8_t values[6] = {time_buf.Year, time_buf.Month, time_buf.Date,
                         time_buf.Hour, time_buf.Minute, time_buf.WeekDay};
    const char *day_names[] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t y = 12 + i * 9;
        SSD1315_ShowString(16, y, time_set_labels[i], 1, White);
        SSD1315_ShowString(50, y, ":", 1, White);
        if (i == 5) {
            SSD1315_ShowString(58, y, day_names[values[i]], 1, White);
        } else {
            char buf[4];
            buf[0] = '0' + (values[i] / 10);
            buf[1] = '0' + (values[i] % 10);
            buf[2] = '\0';
            SSD1315_ShowString(58, y, buf, 1, White);
        }
        if (time_editing && time_set_step == i) {
            Rectangle_TypeDef r = {.Color = White, .Location.X = 0, .Location.Y = (uint8_t)(y - 2),
                                    .Size.Length = 9, .Size.Width = 127};
            SSD1315_InvertRectangle(&r);
        } else {
            SSD1315_ShowString(4, y, time_set_step == i ? ">" : " ", 1, White);
        }
    }
}

static void Page_DrawAppearance(void) {
    SSD1315_ShowString(12, 0, "Appearance", 2, White);

    SSD1315_ShowString(20, 22, "Bar Style:", 1, White);
    const char *bar_name = (appearance.bar_style == BAR_STYLE_BAR) ? "Bar" : "FullScr";
    SSD1315_ShowString(82, 22, bar_name, 1, White);
    if (appearance_editing && appearance_selection == 0) {
        Rectangle_TypeDef r = {.Color = White, .Location.X = 0, .Location.Y = 20,
                                .Size.Length = 12, .Size.Width = 127};
        SSD1315_InvertRectangle(&r);
    } else {
        SSD1315_ShowString(8, 22, appearance_selection == 0 ? ">" : " ", 1, White);
    }

    SSD1315_ShowString(20, 40, "Home Style:", 1, White);
    const char *home_name = (appearance.home_style == HOME_STYLE_CLOCK) ? "Clock" : "Binary";
    SSD1315_ShowString(88, 40, home_name, 1, White);
    if (appearance_editing && appearance_selection == 1) {
        Rectangle_TypeDef r = {.Color = White, .Location.X = 0, .Location.Y = 38,
                                .Size.Length = 12, .Size.Width = 127};
        SSD1315_InvertRectangle(&r);
    } else {
        SSD1315_ShowString(8, 40, appearance_selection == 1 ? ">" : " ", 1, White);
    }
}

// ========== 页面处理 ==========

static void Page_ProcessMenu(void) {
    if (Button_GetEvent(BTN_LEFT) == BTN_SHORT_PRESS) {
        Page_Set(PAGE_HOME);
        return;
    }
    if (Button_GetEvent(BTN_RIGHT) == BTN_SHORT_PRESS) {
        if (menu_selection == 0) {
            alarm_list_sel = 0;
            Page_Set(PAGE_ALARM_LIST);
        } else if (menu_selection == 1) {
            DS3231_ReadTime(&time_buf);
            time_set_step = 0;
            time_editing = 0;
            Page_Set(PAGE_TIME_SET);
        } else if (menu_selection == 2) {
            appearance_selection = 0;
            appearance_editing = 0;
            Page_Set(PAGE_APPEARANCE);
        }
        return;
    }
    if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
        if (menu_selection > 0) menu_selection--;
    }
    if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
        if (menu_selection < MENU_ITEM_COUNT - 1) menu_selection++;
    }
}

static void Page_ProcessAlarmList(void) {
    if (Button_GetEvent(BTN_LEFT) == BTN_SHORT_PRESS) {
        Page_Set(PAGE_MENU);
        return;
    }
    if (Button_GetEvent(BTN_RIGHT) == BTN_SHORT_PRESS) {
        Alarm_EnterEdit(alarm_list_sel == 0);
        Page_Set(alarm_is_alarm1 ? PAGE_ALARM1_SET : PAGE_ALARM2_SET);
        return;
    }
    if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
        if (alarm_list_sel > 0) alarm_list_sel--;
    }
    if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
        if (alarm_list_sel < 1) alarm_list_sel++;
    }
}

static void Page_ProcessAlarmSet(void) {
    if (Button_GetEvent(BTN_LEFT) == BTN_SHORT_PRESS) {
        DS3231_WriteAlarm(&alarm_buf, alarm_is_alarm1 ? Alarm1 : Alarm2);
        Page_Set(PAGE_ALARM_LIST);
        return;
    }
    if (Button_GetEvent(BTN_RIGHT) == BTN_SHORT_PRESS) {
        alarm_editing = !alarm_editing;
        return;
    }
    uint8_t total = Alarm_StepCount();
    if (alarm_editing) {
        if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
            switch (alarm_step) {
                case 0: if (alarm_buf.Hour < 23)   alarm_buf.Hour++; else alarm_buf.Hour = 0; break;
                case 1: if (alarm_buf.Minute < 59) alarm_buf.Minute++; else alarm_buf.Minute = 0; break;
                case 2: if (alarm_is_alarm1) {
                            if (alarm_buf.Second < 59) alarm_buf.Second++; else alarm_buf.Second = 0;
                        } break;
                case 3: // Repeat (position depends on whether Sec exists)
                {
                    uint8_t r_step = alarm_is_alarm1 ? 3 : 2;
                    if (alarm_step == r_step) {
                        if (alarm_buf.Repeat < ALARM_WEEKDAY) alarm_buf.Repeat++;
                        else alarm_buf.Repeat = ALARM_DAILY;
                    }
                    break;
                }
                case 4:
                case 5: // State or Day
                {
                    uint8_t state_step = total - 1;
                    if (alarm_step == state_step) {
                        alarm_buf.State = (alarm_buf.State == Enable) ? Disable : Enable;
                    } else {
                        // Day
                        uint8_t dmax = (alarm_buf.Repeat == ALARM_WEEKDAY) ? 7 : 31;
                        uint8_t dmin = 1;
                        if (alarm_buf.Day < dmax) alarm_buf.Day++; else alarm_buf.Day = dmin;
                    }
                    break;
                }
            }
        }
        if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
            switch (alarm_step) {
                case 0: if (alarm_buf.Hour > 0)   alarm_buf.Hour--; else alarm_buf.Hour = 23; break;
                case 1: if (alarm_buf.Minute > 0) alarm_buf.Minute--; else alarm_buf.Minute = 59; break;
                case 2: if (alarm_is_alarm1) {
                            if (alarm_buf.Second > 0) alarm_buf.Second--; else alarm_buf.Second = 59;
                        } break;
                case 3: {
                    uint8_t r_step = alarm_is_alarm1 ? 3 : 2;
                    if (alarm_step == r_step) {
                        if (alarm_buf.Repeat > ALARM_DAILY) alarm_buf.Repeat--;
                        else alarm_buf.Repeat = ALARM_WEEKDAY;
                    }
                    break;
                }
                case 4:
                case 5: {
                    uint8_t state_step = total - 1;
                    if (alarm_step == state_step) {
                        alarm_buf.State = (alarm_buf.State == Enable) ? Disable : Enable;
                    } else {
                        uint8_t dmax = (alarm_buf.Repeat == ALARM_WEEKDAY) ? 7 : 31;
                        if (alarm_buf.Day > 1) alarm_buf.Day--; else alarm_buf.Day = dmax;
                    }
                    break;
                }
            }
        }
    } else {
        if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
            if (alarm_step > 0) alarm_step--;
        }
        if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
            if (alarm_step < total - 1) alarm_step++;
        }
    }
}

static void Page_ProcessTimeSet(void) {
    if (Button_GetEvent(BTN_LEFT) == BTN_SHORT_PRESS) {
        DS3231_WriteTime(&time_buf);
        Page_Set(PAGE_MENU);
        return;
    }
    if (Button_GetEvent(BTN_RIGHT) == BTN_SHORT_PRESS) {
        time_editing = !time_editing;
        return;
    }
    uint8_t *val = NULL;
    uint8_t max = 59;
    uint8_t min = 0;
    switch (time_set_step) {
        case 0: val = &time_buf.Year;    max = 99; min = 0;  break;
        case 1: val = &time_buf.Month;   max = 12; min = 1;  break;
        case 2: val = &time_buf.Date;    max = 31; min = 1;  break;
        case 3: val = &time_buf.Hour;    max = 23; min = 0;  break;
        case 4: val = &time_buf.Minute;  max = 59; min = 0;  break;
        case 5: val = &time_buf.WeekDay; max = 7;  min = 1;  break;
    }
    if (time_editing) {
        if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
            if (*val < max) (*val)++; else *val = min;
        }
        if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
            if (*val > min) (*val)--; else *val = max;
        }
    } else {
        if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
            if (time_set_step > 0) time_set_step--;
        }
        if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
            if (time_set_step < 5) time_set_step++;
        }
    }
}

static void Page_ProcessAppearance(void) {
    if (Button_GetEvent(BTN_LEFT) == BTN_SHORT_PRESS) {
        Page_Set(PAGE_MENU);
        return;
    }
    if (Button_GetEvent(BTN_RIGHT) == BTN_SHORT_PRESS) {
        appearance_editing = !appearance_editing;
        return;
    }
    if (appearance_editing) {
        if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS ||
            Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
            if (appearance_selection == 0) {
                appearance.bar_style = (appearance.bar_style == BAR_STYLE_BAR)
                                     ? BAR_STYLE_FULLSCREEN : BAR_STYLE_BAR;
            } else {
                appearance.home_style = (appearance.home_style == HOME_STYLE_CLOCK)
                                      ? HOME_STYLE_BINARY : HOME_STYLE_CLOCK;
            }
            Settings_Save();
        }
    } else {
        if (Button_GetEvent(BTN_UP) == BTN_SHORT_PRESS) {
            if (appearance_selection > 0) appearance_selection--;
        }
        if (Button_GetEvent(BTN_DOWN) == BTN_SHORT_PRESS) {
            if (appearance_selection < APPR_ITEM_COUNT - 1) appearance_selection++;
        }
    }
}

static void Page_ProcessRinging(void) {
    if (Button_GetEvent(BTN_UP) != BTN_NONE ||
        Button_GetEvent(BTN_DOWN) != BTN_NONE ||
        Button_GetEvent(BTN_LEFT) != BTN_NONE ||
        Button_GetEvent(BTN_RIGHT) != BTN_NONE) {
        Buzzer_Off();
        DS3231_ClearAlarmFlag();
        Page_Set(PAGE_HOME);
    }
}

void Page_Process(void) {
    switch (current_page) {
        case PAGE_HOME:
            if (Button_GetEvent(BTN_UP)   == BTN_LONG_PRESS ||
                Button_GetEvent(BTN_DOWN) == BTN_LONG_PRESS ||
                Button_GetEvent(BTN_LEFT) == BTN_LONG_PRESS ||
                Button_GetEvent(BTN_RIGHT)== BTN_LONG_PRESS) {
                Page_Set(PAGE_MENU);
            }
            break;
        case PAGE_MENU:          Page_ProcessMenu();        break;
        case PAGE_ALARM_LIST:    Page_ProcessAlarmList();   break;
        case PAGE_ALARM1_SET:
        case PAGE_ALARM2_SET:    Page_ProcessAlarmSet();    break;
        case PAGE_TIME_SET:      Page_ProcessTimeSet();     break;
        case PAGE_APPEARANCE:    Page_ProcessAppearance();  break;
        case PAGE_ALARM_RINGING: Page_ProcessRinging();     break;
    }
}

void Page_Draw(void) {
    switch (current_page) {
        case PAGE_HOME:
            if (appearance.home_style == HOME_STYLE_BINARY) {
                Time_TypeDef Time;
                DS3231_ReadTime(&Time);
                Font_DrawBinaryTime(&Time);
            } else {
                App_Home();
            }
            // 进度条
            {
                uint32_t press_ms = Button_GetMaxPressMs();
                if (press_ms > 200) {
                    progress_width = (press_ms >= 2000) ? 128 : (press_ms * 128 / 2000);
                    if (progress_width > 128) progress_width = 128;
                    press_was_active = 1;
                } else if (press_was_active) {
                    press_was_active = 0;
                } else if (progress_width > 0) {
                    if (progress_width > 6) progress_width -= 6;
                    else progress_width = 0;
                }
                if (appearance.bar_style == BAR_STYLE_FULLSCREEN)
                    DrawProgressBarFullscreen(progress_width);
                else
                    DrawProgressBar(progress_width);
            }
            break;
        case PAGE_MENU:          Page_DrawMenu();            break;
        case PAGE_ALARM_LIST:    Page_DrawAlarmList();       break;
        case PAGE_ALARM1_SET:
        case PAGE_ALARM2_SET:    Page_DrawAlarmSet_Common(); break;
        case PAGE_TIME_SET:      Page_DrawTimeSet();         break;
        case PAGE_APPEARANCE:    Page_DrawAppearance();      break;
        case PAGE_ALARM_RINGING: App_AlarmRinging();         break;
    }
    SSD1315_Update();
}
