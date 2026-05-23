#include "stm32f1xx_hal.h"
#include "i2c.h"
#include <stdint.h>
#include "DS3231.h"

#define DS3231_Address                             0XD0
#define Control_MemAddress                     0x0E
#define Status_MemAddress                     0x0F   
#define Second_MemAddress                     0x00
#define Alarm1_Second_MemAddress        0x07
#define Alarm2_Minute_MemAddress         0x0B
#define Temperature_High_MemAddress    0x11        

void BCD_To_Dec(uint8_t* BCD){
    *BCD=(*BCD>>4)*10+(*BCD&0x0F);
}

void Dec_To_BCD(uint8_t* Dec){
    *Dec=(*Dec/10<<4)|(*Dec%10);
}

void DS3231_WriteData(uint8_t MemAddress,uint8_t* pData,uint8_t Size){
    HAL_I2C_Mem_Write(&hi2c1, DS3231_Address , MemAddress, I2C_MEMADD_SIZE_8BIT, pData,Size, 100);
}

void DS3231_ReadData(uint8_t MemAddress,uint8_t* Buf,uint8_t Size){
    HAL_I2C_Mem_Read(&hi2c1, DS3231_Address, MemAddress, I2C_MEMADD_SIZE_8BIT, Buf, Size, 100);
}

void DS3231_Init(void){
    uint8_t Buf[2];
    HAL_Delay(10);
    DS3231_ReadData(Control_MemAddress, Buf, 2);
    Buf[0] |= 0x05;                                                // INTCN + A1IE, enable alarm interrupt
    Buf[1] &= 0x03;                                                // Keep A1F and A2F alarm flags, clear 32kHz output
    DS3231_WriteData(Control_MemAddress, Buf,2);
}

void DS3231_ReadTime(Time_TypeDef* Time){
    uint8_t Buf[7];
    DS3231_ReadData(Second_MemAddress, Buf, 7);              //second,minute,hour,day,date,month,year
    for (uint8_t i=0; i<7; i++) {BCD_To_Dec(&Buf[i]);}
    Time->Second=Buf[0];
    Time->Minute=Buf[1];
    Time->Hour=Buf[2];
    Time->WeekDay=Buf[3];
    Time->Date=Buf[4];
    Time->Month=Buf[5];
    Time->Year=Buf[6];
}

void DS3231_WriteTime(Time_TypeDef* Time){
    uint8_t Buf[7];
    Buf[0]=Time->Second;
    Buf[1]=Time->Minute;
    Buf[2]=Time->Hour;
    Buf[3]=Time->WeekDay;
    Buf[4]=Time->Date;
    Buf[5]=Time->Month;
    Buf[6]=Time->Year;
    for (uint8_t i=0; i<7; i++) {Dec_To_BCD(&Buf[i]);}
    DS3231_WriteData(Second_MemAddress,Buf,7);
}

void DS3231_ReadAlarm(Alarm_TypeDef* Alarm,Alarm_Select Selected_Alarm){
    uint8_t Buf[4];
    if (Selected_Alarm==Alarm1) {
        DS3231_ReadData(Alarm1_Second_MemAddress, Buf, 4);
        // 判断状态：所有掩码位全为1 = Disable
        uint8_t all_masked = (Buf[0] & Buf[1] & Buf[2] & Buf[3]) & 0x80;
        Alarm->State = all_masked ? Disable : Enable;
        for (uint8_t i=0; i<4; i++) {BCD_To_Dec(&Buf[i]);}
        Alarm->Second  = Buf[0] & 0x7F;
        Alarm->Minute  = Buf[1] & 0x7F;
        Alarm->Hour    = Buf[2] & 0x7F;
        // Buf[3]: bit7=A1M4, bit6=DY/DT, bits5-0=day/date
        if (Buf[3] & 0x80) {
            Alarm->Repeat = ALARM_DAILY;
            Alarm->Day = 1;
        } else if (Buf[3] & 0x40) {
            Alarm->Repeat = ALARM_WEEKDAY;
            Alarm->Day = Buf[3] & 0x3F;
        } else {
            Alarm->Repeat = ALARM_DATE;
            Alarm->Day = Buf[3] & 0x3F;
        }
    }
    if (Selected_Alarm==Alarm2) {
        DS3231_ReadData(Alarm2_Minute_MemAddress, Buf, 3);
        uint8_t all_masked = (Buf[0] & Buf[1] & Buf[2]) & 0x80;
        Alarm->State = all_masked ? Disable : Enable;
        for (uint8_t i=0; i<3; i++) {BCD_To_Dec(&Buf[i]);}
        Alarm->Second  = 0;
        Alarm->Minute  = Buf[0] & 0x7F;
        Alarm->Hour    = Buf[1] & 0x7F;
        if (Buf[2] & 0x80) {
            Alarm->Repeat = ALARM_DAILY;
            Alarm->Day = 1;
        } else if (Buf[2] & 0x40) {
            Alarm->Repeat = ALARM_WEEKDAY;
            Alarm->Day = Buf[2] & 0x3F;
        } else {
            Alarm->Repeat = ALARM_DATE;
            Alarm->Day = Buf[2] & 0x3F;
        }
    }
}

void DS3231_WriteAlarm(Alarm_TypeDef* Alarm,Alarm_Select Selected_Alarm){
    if (Selected_Alarm==Alarm1) {
        uint8_t Buf[4];
        if (Alarm->State==Disable) {
            // 全掩码 = 禁用
            Buf[0]=0x80; Buf[1]=0x80; Buf[2]=0x80; Buf[3]=0x80;
        } else {
            Buf[0]=Alarm->Second & 0x7F;            // A1M1=0, match seconds
            Buf[1]=Alarm->Minute & 0x7F;            // A1M2=0, match minutes
            Buf[2]=Alarm->Hour   & 0x7F;            // A1M3=0, match hours
            Buf[3]=Alarm->Day    & 0x3F;            // A1M4 and DY/DT set below
            for (uint8_t i=0; i<4; i++) {Dec_To_BCD(&Buf[i]);}
            switch (Alarm->Repeat) {
                case ALARM_DAILY:   Buf[3] |= 0x80;           break; // A1M4=1
                case ALARM_DATE:    /* DY/DT=0 by default */  break;
                case ALARM_WEEKDAY: Buf[3] |= 0x40;           break; // DY/DT=1
            }
        }
        DS3231_WriteData(Alarm1_Second_MemAddress, Buf, 4);
    }
    if (Selected_Alarm==Alarm2) {
        uint8_t Buf[3];
        if (Alarm->State==Disable) {
            Buf[0]=0x80; Buf[1]=0x80; Buf[2]=0x80;
        } else {
            Buf[0]=Alarm->Minute & 0x7F;            // A2M2=0, match minutes
            Buf[1]=Alarm->Hour   & 0x7F;            // A2M3=0, match hours
            Buf[2]=Alarm->Day    & 0x3F;
            for (uint8_t i=0; i<3; i++) {Dec_To_BCD(&Buf[i]);}
            switch (Alarm->Repeat) {
                case ALARM_DAILY:   Buf[2] |= 0x80;           break;
                case ALARM_DATE:    /* DY/DT=0 by default */  break;
                case ALARM_WEEKDAY: Buf[2] |= 0x40;           break;
            }
        }
        DS3231_WriteData(Alarm2_Minute_MemAddress, Buf, 3);
    }
}

void DS3231_ReadTemperature(float* Temperature){
    uint8_t Buf[2];
    DS3231_ReadData(Temperature_High_MemAddress, Buf, 2);
    *Temperature=(int8_t)Buf[0]+(Buf[1]>>6)*0.25f;
}

void DS3231_ClearAlarmFlag(void){
    uint8_t Buf[1];
    HAL_Delay(10);
    DS3231_ReadData(Status_MemAddress, Buf, 1);
    Buf[0]&=~0x03;                                                //clear the alarm flag
    DS3231_WriteData(Status_MemAddress, Buf,1);
}
