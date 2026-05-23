#ifndef MENU_H
#define MENU_H

#include <stdint.h>

typedef enum {
    PAGE_HOME = 0,
    PAGE_MENU,
    PAGE_ALARM_LIST,
    PAGE_ALARM1_SET,
    PAGE_ALARM2_SET,
    PAGE_TIME_SET,
    PAGE_APPEARANCE,
    PAGE_ALARM_RINGING
} Page_ID;

typedef enum {
    BAR_STYLE_BAR = 0,
    BAR_STYLE_FULLSCREEN
} BarStyle;

typedef enum {
    TIME_STYLE_DIGITAL = 0,
    TIME_STYLE_TEXT
} TimeStyle;

typedef enum {
    HOME_STYLE_CLOCK = 0,
    HOME_STYLE_BINARY
} HomeStyle;

typedef struct {
    BarStyle bar_style;
    HomeStyle home_style;
} Appearance_Settings;

extern Appearance_Settings appearance;

void Page_Set(Page_ID page);
Page_ID Page_Get(void);
void Page_Process(void);
void Page_Draw(void);
void Menu_Init(void);

#endif
