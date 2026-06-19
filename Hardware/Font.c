#include "Font.h"
#include "OLED.h"
#include <stdint.h>

// 7段数码管编码: 每段一位，从高位到低位: [A,B,C,D,E,F,G] = [6,5,4,3,2,1,0]
static const uint8_t Font_7Seg[10] = {
    0x7E, // 0: A B C D E F _
    0x30, // 1: _ B C _ _ _ _
    0x6D, // 2: A B _ D E _ G
    0x79, // 3: A B C D _ _ G
    0x33, // 4: _ B C _ _ F G
    0x5B, // 5: A _ C D _ F G
    0x5F, // 6: A _ C D E F G
    0x70, // 7: A B C _ _ _ _
    0x7F, // 8: A B C D E F G
    0x7B  // 9: A B C D _ F G
};

// 默认尺寸
#define SEG_LEN   12
#define SEG_WID    2
#define MID_GAP   15   // 两组小时分钟之间的额外间距
#define NUM_GAP    2   // 数字之间的间距

static void DrawSegment(uint8_t x, uint8_t y, uint8_t horizontal, uint8_t len, uint8_t wid) {
    Rectangle_TypeDef r;
    r.Color = White;
    if (horizontal) {
        r.Location.X = x;
        r.Location.Y = y;
        r.Size.Width  = len;
        r.Size.Length = wid;
    } else {
        r.Location.X = x;
        r.Location.Y = y;
        r.Size.Width  = wid;
        r.Size.Length = len;
    }
    SSD1315_DrawRectangle(&r);
}

static void DrawDigit(uint8_t x, uint8_t y, uint8_t digit) {
    uint8_t seg = Font_7Seg[digit];

    // A: 上横
    if (seg & 0x40) DrawSegment(x + SEG_WID, y,               1, SEG_LEN, SEG_WID);
    // B: 右上竖
    if (seg & 0x20) DrawSegment(x + SEG_WID + SEG_LEN, y + SEG_WID,  0, SEG_LEN, SEG_WID);
    // C: 右下竖
    if (seg & 0x10) DrawSegment(x + SEG_WID + SEG_LEN, y + SEG_WID*2 + SEG_LEN, 0, SEG_LEN, SEG_WID);
    // D: 下横
    if (seg & 0x08) DrawSegment(x + SEG_WID, y + SEG_WID*2 + SEG_LEN*2, 1, SEG_LEN, SEG_WID);
    // E: 左下竖
    if (seg & 0x04) DrawSegment(x, y + SEG_WID*2 + SEG_LEN,  0, SEG_LEN, SEG_WID);
    // F: 左上竖
    if (seg & 0x02) DrawSegment(x, y + SEG_WID,               0, SEG_LEN, SEG_WID);
    // G: 中横
    if (seg & 0x01) DrawSegment(x + SEG_WID, y + SEG_WID + SEG_LEN, 1, SEG_LEN, SEG_WID);
}

static uint8_t DigitWidth(void) {
    return SEG_WID * 3 + SEG_LEN + NUM_GAP;
}

void Font_Draw7SegTime(Time_TypeDef *time, uint8_t x, uint8_t y) {
    uint8_t digits[4] = {
        (uint8_t)(time->Hour   / 10), (uint8_t)(time->Hour   % 10),
        (uint8_t)(time->Minute / 10), (uint8_t)(time->Minute % 10)
    };
    uint8_t dw = DigitWidth();

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t px = x + dw * i + (i >= 2 ? MID_GAP : 0);
        DrawDigit(px, y, digits[i]);
    }

    // 冒号闪烁
    if (time->Second & 1) {
        uint8_t cx = x + dw * 2 + MID_GAP / 2;
        Rectangle_TypeDef dot = {.Color = White, .Location.X = cx, .Location.Y = (uint8_t)(y + SEG_WID + SEG_LEN / 2),
                                  .Size.Length = SEG_WID, .Size.Width = SEG_WID};
        SSD1315_DrawRectangle(&dot);
        dot.Location.Y += SEG_LEN;
        SSD1315_DrawRectangle(&dot);
    }
}

// ====== 二进制时间 ======

#define BIN_SQ_SIZE 11
#define BIN_SQ_GAP   4
#define BIN_COUNT    8

static void DrawBitSquare(uint8_t x, uint8_t y, uint8_t bit) {
    Rectangle_TypeDef r;
    r.Location.X = x;
    r.Location.Y = y;
    r.Size.Length = BIN_SQ_SIZE;
    r.Size.Width  = BIN_SQ_SIZE;
    r.Color = bit ? White : Black;
    SSD1315_DrawRectangle(&r);
}

void Font_DrawBinaryTime(Time_TypeDef *time) {
    uint8_t total_w = BIN_COUNT * BIN_SQ_SIZE + (BIN_COUNT - 1) * BIN_SQ_GAP;
    uint8_t start_x = (128 - total_w) / 2;
    uint8_t h = time->Hour;
    uint8_t m = time->Minute;

    for (uint8_t i = 0; i < BIN_COUNT; i++) {
        uint8_t px = start_x + i * (BIN_SQ_SIZE + BIN_SQ_GAP);
        DrawBitSquare(px, 12, (h >> (BIN_COUNT - 1 - i)) & 1);
    }

    for (uint8_t i = 0; i < BIN_COUNT; i++) {
        uint8_t px = start_x + i * (BIN_SQ_SIZE + BIN_SQ_GAP);
        DrawBitSquare(px, 38, (m >> (BIN_COUNT - 1 - i)) & 1);
    }
}
