# AGENTS.md — Alarm_ai (STM32F103 Firmware)

## Project Overview

STM32F103C8Tx ("Blue Pill") embedded firmware for an alarm clock with:
- **DS3231** RTC module (I2C addr: `0xD0`) — timekeeping, alarm, temperature
- **SSD1315** 128x64 OLED display (I2C addr: `0x78`) — custom 7-segment-style digital clock UI
- **I2C1** bus: PB6 (SCL), PB7 (SDA) @ 400kHz
- **PA1** GPIO output
- C11, bare-metal, no RTOS

## Build Commands

```bash
# Configure with GCC toolchain (default)
cmake --preset Debug

# Build
cmake --build --preset Debug

# Release
cmake --preset Release && cmake --build --preset Release

# Alternative toolchain (starm-clang LLVM)
# Edit CMakePresets.json to point toolchainFile at cmake/starm-clang.cmake
```

**Prerequisites:** `arm-none-eabi-` toolchain on PATH; Ninja generator.

## Code Organization

```
├── Core/                      # STM32CubeMX generated (DO NOT TOUCH outside USER CODE blocks)
│   ├── Inc/                   #   Headers (gpio.h, i2c.h, main.h, ...)
│   └── Src/                   #   Sources (main.c, i2c.c, gpio.c, stm32f1xx_it.c, ...)
├── Hardware/                  # USER-CODE — custom application & peripheral drivers
│   ├── OLED.c / OLED.h        #   SSD1315 display driver
│   ├── DS3231.c / DS3231.h    #   RTC driver
│   ├── Apps.c / Apps.h        #   Application logic (home screen UI)
├── Drivers/                   # STM32 HAL + CMSIS (generated, read-only)
├── cmake/                     # CMake toolchain files + CubeMX sub-build
│   ├── gcc-arm-none-eabi.cmake
│   ├── starm-clang.cmake
│   └── stm32cubemx/CMakeLists.txt
├── CMakeLists.txt             # Root build file (user-editable)
├── CMakePresets.json          # Ninja + Debug/Release presets
├── STM32F103XX_FLASH.ld       # Linker script (64KB flash, 20KB RAM)
└── startup_stm32f103xb.s      # Reset vector / startup assembly
```

## Critical Patterns & Gotchas

### STM32CubeMX Generated Code (`Core/`, `Drivers/`)
- Files have `USER CODE BEGIN` / `USER CODE END` markers. **Only add code between these markers.** Regeneration preserves them; everything else gets overwritten.
- To regenerate: open `test.ioc` in STM32CubeMX and regenerate. The build expects `cmake/stm32cubemx/` layout.

### OLED Driver (`Hardware/OLED.c`)
- **Dual framebuffer model:** `Background_GRAM[8][128]` stores a clean copy, `GRAM[8][128]` is the active buffer.
- `SSD1315_Update()` pushes GRAM to the display; `SSD1315_Clear()` resets both buffers.
- **Bug in `SSD1315_Clear()`:** The first `memset` targets `GRAM` but uses `sizeof(Background_GRAM)` — it's line `memset(GRAM, 0, sizeof(Background_GRAM))` when it should be `memset(Background_GRAM, 0, sizeof(Background_GRAM))`. This means `Background_GRAM` is **never cleared** (both memsets operate on GRAM).
- `SSD1315_ShowNumber()` only supports **2-digit numbers** (limitation by design).
- Main loop pattern: `App_Home()` draws to GRAM → `SSD1315_Update()` sends to display → `SSD1315_Clear()` resets for next frame.

### DS3231 RTC Driver (`Hardware/DS3231.c`)
- Uses BCD encoding internally; `BCD_To_Dec()` / `Dec_To_BCD()` helpers convert for/from human-readable values.
- Alarm mask bits are set/cleared with `0x80` on the minute/second register to disable/enable.
- `Alarm_TypeDef` stores only minute, hour, and state — seconds are forced to 0.
- `DS3231_Init()` enables the oscillator (control reg bit 2), disables 32kHz output (status reg bits 0-1 cleared, bits 2-7 masked), and configures INT output.

### Application (`Hardware/Apps.c`)
- `App_Home()` is the main UI function — called in the super loop.
- Draws a 7-segment-style digital clock using rectangle primitives (`DigitalTubeStyle_ShowTime()`).
- Uses hardcoded position constants (`Start_X=18`, `Start_Y=19`, `Middle_Offset=15`, `Number_Offset=2`, `Tube_Length=12`, `Tube_Width=2`).
- Flashing colon dots toggle every second (`Time.Second % 2`).
- Bottom banner: "Nothing is impossible" at row 54.

### Naming Conventions
| Prefix | Used for |
|--------|----------|
| `SSD1315_` | OLED display driver functions |
| `DS3231_` | RTC driver functions |
| `App_` | Application-level functions |
| `MX_` | CubeMX-generated peripheral init functions |
| `HAL_` | STM32 HAL library functions |
| Types: `_TypeDef` suffix, enums: PascalCase members |

### I2C Details
- Both DS3231 (`0xD0` write / `0xD1` read) and SSD1315 (`0x78` write / `0x79` read) share I2C1.
- SSD1315 uses command address `0x00` and data address `0x40` for memory-mapped I2C writes.
- DS3231 uses memory addressing mode (register address byte + data).

### Threading / Concurrency
- **No RTOS** — bare-metal super loop in `main.c`.
- Interrupts handled by STM32 HAL (systick for `HAL_Delay`, I2C IRQs if used).
- `Error_Handler()` disables IRQs and halts.

### Linker
- 64KB flash, 20KB RAM (STM32F103C8Tx).
- `--gc-sections` enabled; unused functions are stripped.
- Calls `--print-memory-usage` during link.

## ⚠️ 驱动文件不可修改

**不要修改以下文件的已有代码（除非要增加新功能）：**
- `Hardware/OLED.c` / `OLED.h`
- `Hardware/DS3231.c` / `DS3231.h`

这些驱动虽然有些代码看起来不太好理解（比如奇怪的命名、硬编码的魔法数字、不够优雅的实现），但已经过实际硬件测试验证，能稳定工作。**不要因为"看着不舒服"就去重构它们。**

如果想改动，必须满足以下条件之一：
1. 明确的新功能需求（比如增加对 OLED 新指令的支持、DS3231 闹钟的新模式等）
2. 证实了实际运行时触发的 bug（而不是代码审查觉得"可能有问题"）

---

## New Hardware Drivers
- Place source files in `Hardware/` directory.
- Add `.c` files to `target_sources()` in root `CMakeLists.txt` (line ~47).
- Add include paths to `target_include_directories()` in `CMakeLists.txt` (line ~55).

---

## 新增功能结构 (2026-05)

### 引脚分配
| 引脚 | 功能 | 配置 |
|------|------|------|
| PA0 | 按键 UP | 上拉输入，下降沿 EXTI |
| PA2 | 按键 DOWN | 上拉输入，下降沿 EXTI |
| PA4 | 按键 LEFT | 上拉输入，下降沿 EXTI |
| PA6 | 按键 RIGHT | 上拉输入，下降沿 EXTI |
| PA1 | 有源蜂鸣器 | 推挽输出（拉高响） |
| PB10 | DS3231 中断输入 | 上拉输入，下降沿 EXTI |
| PB6/PB7 | I2C1 | 400kHz (已有) |

### 系统状态机
```
HOME ──(LEFT长按2s)──→ MENU ──── 闹钟设置 ──→ HOME
  │                       └──── 时间设置 ──→ HOME
  │
  └──(1min无操作)──→ SLEEP ──(任意按键/闹钟)──→ HOME
                       │
                       └──(DS3231闹钟)──→ RINGING ──(任意按键)──→ HOME
```

### 新增文件 (`Hardware/`)
- **`Button.c/h`** — 按键驱动，10ms 轮询去抖，支持短按/长按(2s松手)检测
- **`Buzzer.c/h`** — 蜂鸣器驱动，支持定时自动关断（如闹钟响30s）
- **`Menu.c/h`** — 页面状态机 + 目录菜单 + 闹钟/时间设置界面
- **`Sleep.c/h`** — 休眠管理：1min 无操作进入 STOP 模式（OLED 关），EXTI 唤醒

### 中断
- `HAL_GPIO_EXTI_Callback` 在 `stm32f1xx_it.c` 中覆盖，DS3231 中断时设置 `Alarm_Triggered` 标志
- 所有 EXTI 优先级设为 15（最低，避免影响 HAL 滴答定时器）

### 注意事项
- `SystemClock_Config()` 定义在 `main.c` 中静态函数，`Sleep.c` 通过 `extern` 声明调用
- 休眠唤醒后需要重新配置系统时钟（HAL 要求）
- 主循环 10ms 定时任务由 `HAL_GetTick()` 差值控制
