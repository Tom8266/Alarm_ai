#ifndef OLED_H
#define OLED_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef enum{
    Black,
    White
}Color;

typedef struct{
    uint8_t X;
    uint8_t Y;
} Location_Typedef;                   

typedef struct{
    uint8_t Length;
    uint8_t Width;
}Size_Typedef;

typedef struct{
    Location_Typedef Location;
    Size_Typedef Size;
    Color Color;
}Rectangle_TypeDef;

void SSD1315_WriteCommand(uint8_t Command);
void SSD1315_Clear(void);
void SSD1315_Init(void);
void SSD1315_Update(void);
void SSD1315_DrawPixel(uint8_t X,uint8_t Y,Color Pixel_Color);
void SSD1315_WriteData(uint8_t* pData);
void SSD1315_ReversePixel(uint8_t Buf[8][128], uint8_t X,uint8_t Y);
void SSD1315_DrawRectangle(Rectangle_TypeDef* Rectangle);
void SSD1315_InvertRectangle(Rectangle_TypeDef* Rectangle);
void SSD1315_CleanDrawRectangle(Rectangle_TypeDef* Rectangle);
void SSD1315_ShowChar(uint8_t X,uint8_t Y,uint8_t Char,uint8_t Size,Color Color);
void SSD1315_ShowString(uint8_t X,uint8_t Y,const char* String,uint8_t Size,Color Color);
void SSD1315_ShowNumber(uint8_t X,uint8_t Y,uint8_t Number,uint8_t Size,uint8_t Color);

#endif