# AGENTS.md — Alarm_ai (STM32F103 Firmware)

## Project Overview

STM32F103C8Tx ("Blue Pill") embedded firmware for an alarm clock with:
- **DS3231** RTC module (I2C addr: `0xD0`) — timekeeping, alarm, temperature
- **SSD1315** 128×64 OLED display (I2C addr: `0x78`) — 7-segment-style clock, menu UI
- **4 buttons** (UP/DOWN/LEFT/RIGHT) — navigation, long-press detection via polling
- **Active buzzer** on PB0 — alarm sound, auto-shutoff (LOW=on, HIGH=off, needs transistor)
- **I2C1** bus: PB6 (SCL), PB7 (SDA) @ 400kHz
- C11, bare-metal super loop, no RTOS

## Build Commands

```bash
# Configure with GCC toolchain (default)
cmake --preset Debug

# Build
cmake --build --preset Debug

# Release
cmake --preset Release && cmake --build --preset Release
```

**Prerequisites:** `arm-none-eabi-` toolchain on PATH; Ninja generator.

## Code Organization

```
├── Core/                        # STM32CubeMX generated (DO NOT TOUCH outside USER CODE blocks)
│   ├── Inc/                     #   Headers (main.h, gpio.h, i2c.h, ...)
│   └── Src/                     #   Sources (main.c, gpio.c, i2c.c, stm32f1xx_it.c, ...)
├── Hardware/                    # USER-CODE — custom application & peripheral drivers
│   ├── OLED.c / OLED.h          #   SSD1315 display driver (framebuffer + primitives)
│   ├── DS3231.c / DS3231.h      #   RTC driver (time, alarm, temperature, BCD i/o)
│   ├── Apps.c / Apps.h          #   Global state + home screen / alarm ringing UI
│   ├── Font.c / Font.h          #   7-segment & binary time rendering
│   ├── Button.c / Button.h      #   Debounced button driver (short/long press)
│   ├── Buzzer.c / Buzzer.h      #   Buzzer with timed auto-shutoff
│   ├── Menu.c / Menu.h          #   Page state machine + all menu/settings UIs
│   └── Sleep.c / Sleep.h        #   STOP-mode sleep (30s idle timeout)
├── Drivers/                     # STM32 HAL + CMSIS (generated, read-only)
├── cmake/                       # CMake toolchain files + CubeMX sub-build
├── CMakeLists.txt               # Root build (user-editable; add .c paths here)
├── CMakePresets.json            # Ninja + Debug/Release presets
├── STM32F103XX_FLASH.ld         # Linker script (64KB flash, 20KB RAM)
└── startup_stm32f103xb.s        # Reset vector / startup assembly
```

## Architecture & Control Flow

### Main Loop (`Core/Src/main.c`)

Super loop with 10ms tick-based tasks:

```
HAL_Init → SystemClock_Config → MX_GPIO_Init → MX_I2C1_Init
→ DS3231_Init, SSD1315_Init, Button_Init, Buzzer_Init, Menu_Init
→ SSD1315_Update (initial blank)

while(1):
  every 10ms:
    if !sleeping:
      Button_TimerHandler()    # poll & debounce buttons
      Sleep_IdleTimerHandler() # accumulate idle time
    Buzzer_TimerHandler()      # check auto-shutoff regardless of sleep

  if Alarm_Triggered:
    → Buzzer_StartAlarm(30000), Page_Set(PAGE_ALARM_RINGING)

  if !sleeping:
    Page_Process()             # handle button events for current page
    SSD1315_Clear()            # reset framebuffers & OLED addressing
    Page_Draw()                # draw to GRAM, calls SSD1315_Update() at end
```

### System State Machine

```
HOME ──(any long press 2s)──→ MENU ──→ Alarm List ──→ Alarm1/2 Set ──→ HOME
  │                              ├──→ Time Set ──→ HOME
  │                              └──→ Appearance ──→ HOME
  │
  └──(30s idle)──→ SLEEP ──(any EXTI / alarm)──→ HOME (wake, re-init OLED)
  │
  └──(DS3231 alarm INT)──→ RINGING ──(any key)──→ HOME
```

**Crucial detail:** During sleep (`Sleep_IsActive() == 1`), the entire `Page_Process()` / `Page_Draw()` block is skipped — the OLED is off. During RINGING, sleep is NOT active, so `Page_Process()` and `Page_Draw()` both run normally; `Page_Draw()` dispatches to `App_AlarmRinging()` which handles its own flashing UI. Note: `SLEEP` is not a `Page_ID` — it's a boolean flag managed by `Sleep.c`, not part of the page state machine.

### Page System (`Hardware/Menu.c`)

`Page_ID` enum defines all screens: `PAGE_HOME`, `PAGE_MENU`, `PAGE_ALARM_LIST`, `PAGE_ALARM1_SET`, `PAGE_ALARM2_SET`, `PAGE_TIME_SET`, `PAGE_APPEARANCE`, `PAGE_ALARM_RINGING`.

Each page has a handler pair: `Page_Draw*()` + `Page_Process*()` dispatched via `switch` in `Page_Draw()`/`Page_Process()`. Button events are consumed (one-shot) via `Button_GetEvent()` which returns and clears the event.

**`Page_Set()` side effects:** Calling `Page_Set()` always resets `menu_selection = 0`, `progress_width = 0`, `press_was_active = 0`, and calls `Sleep_ResetIdleTimer()`. This means switching pages always clears the navigation cursor and resets the idle counter.

**`TimeStyle` enum** (`TIME_STYLE_DIGITAL`, `TIME_STYLE_TEXT`) is defined in `Menu.h` but **currently unused** anywhere in the codebase. Don't add features based on it unless intentionally implementing that setting.

**UI navigation convention:** LEFT = back/save, RIGHT = enter/edit toggle, UP/DOWN = navigate or increment/decrement.

**HOME page progress bar:** Holding any button triggers a progress bar animation. `Button_GetMaxPressMs()` returns the longest current press duration across all buttons. After 200ms of press, the bar appears and grows from 0→128px as press time approaches 2000ms (linear mapping). On release, the bar decays at ~6px per 10ms frame. If the bar reaches full width (128px = 2s hold), `Page_Process()` detects a long-press event and transitions to `PAGE_MENU`. The bar has two visual styles: `BAR_STYLE_BAR` (border + inverted-fill rectangle at y=50) and `BAR_STYLE_FULLSCREEN` (full-screen inverted rectangle).

### Appearance Settings Persistence

Appearance settings (`bar_style`, `home_style`) are saved to the **last flash page at `0x0800FC00`** (1024-byte page). A magic number `0xA55A` validates the data. Config is loaded at boot via `Settings_Load()` and written on change via `Settings_Save()` (erases page, programs two halfwords). The `CFG_MAGIC` sits at offset 0; bar style at offset 2, home style at offset 3.

The **slogan** is not stored — it is computed from the DS3231 time at boot and on sleep wake via a simple hash (`Year×366 + Month×31 + Date + Hour×3600 + Minute×60 + Second`), modulo 10 to pick from 10 preset geek slogans. This requires zero Flash storage and naturally cycles as time advances.

### Pin Assignments

Defined in `Core/Inc/gpio.h` (USER CODE section):

| Pin  | Symbol            | Function         | Config                     |
|------|-------------------|------------------|----------------------------|
| PA0  | `BUTTON_UP`       | Button UP        | Pull-up input, EXTI fall   |
| PB0  | —                 | Buzzer           | Push-pull output (LOW=on)  |
| PA2  | `BUTTON_DOWN`     | Button DOWN      | Pull-up input, EXTI fall   |
| PA4  | `BUTTON_LEFT`     | Button LEFT      | Pull-up input, EXTI fall   |
| PA6  | `BUTTON_RIGHT`    | Button RIGHT     | Pull-up input, EXTI fall   |
| PB6  | —                 | I2C1 SCL         | 400kHz                     |
| PB7  | —                 | I2C1 SDA         | 400kHz                     |
| PB10 | `ALARM_INT`       | DS3231 INT/SQW   | Pull-up input, EXTI fall   |

### Interrupts

`HAL_GPIO_EXTI_Callback` is overridden in `stm32f1xx_it.c` (USER CODE). On `ALARM_INT_Pin`, it sets `Alarm_Triggered = 1` (defined in `Apps.c`). The main loop polls this flag. All button EXTI interrupts are handled via polling in `Button_TimerHandler()`, not in the ISR (buttons use GPIO polling, EXTI is only for DS3231 and sleep wakeup).

### Sleep/Wake (`Hardware/Sleep.c`)

- Idle timeout: **30 seconds** (not 1 minute). Accumulated via `Sleep_IdleTimerHandler()` called every 10ms.
- Enter: `SSD1315_WriteCommand(0xAE)` to turn off OLED, then `HAL_PWR_EnterSTOPMode()` with WFI.
- Wake: EXTI from any button or DS3231 alarm triggers wake. After wake: `HAL_ResumeTick()`, re-call `SystemClock_Config()` (required after STOP), re-init OLED with `SSD1315_WriteCommand(0xAF)` + `SSD1315_Init()`.
- `SystemClock_Config()` is a static function in `main.c` — `Sleep.c` accesses it via `extern void SystemClock_Config(void)`.

## Critical Patterns & Gotchas

### STM32CubeMX Generated Code (`Core/`, `Drivers/`)
- Files have `USER CODE BEGIN` / `USER CODE END` markers. **Only add code between these markers.** Regeneration preserves them; everything else gets overwritten.
- Build system: the root `CMakeLists.txt` calls `add_subdirectory(cmake/stm32cubemx)` which builds the CubeMX-generated `stm32cubemx` static library.

### OLED Driver (`Hardware/OLED.c`)

- **Dual framebuffer model:** `Background_GRAM[8][128]` maintains a clean state, `GRAM[8][128]` is the active buffer used for drawing and display.
- `SSD1315_Clear()` zeroes both buffers and repositions the OLED column pointer. It does **not** update the display — call `SSD1315_Update()` after drawing.
- `SSD1315_Update()` sends the full 8-page GRAM to the display in one pass.
- **`SSD1315_ShowNumber()` only supports 2-digit numbers** (0-99). The implementation hardcodes `str[3]` with two decimal digits.
- Drawing primitives (`SSD1315_DrawRectangle`, etc.) operate on **both** `GRAM` and `Background_GRAM` simultaneously — every pixel write mirrors to both buffers.
- `SSD1315_DrawRectangle` with `Color=White` fills a solid rectangle. With `Color=Black` it draws an unfilled **outline** (border only) — this is non-obvious.
- The display uses a 6×8 pixel font (size 1) and 8×16 pixel font (size 2). Both are stored as large lookup tables `ANSIFont68` and `ANSIFont816` directly in the .c file.
- `SSD1315_Init()` does a full init sequence: close display, set clock, multiplex, COM config, charge pump, pre-charge, VCOMH, contrast, scroll disable, addressing mode, then finally open display and clear.

### SSD1315_InvertRectangle — Critical Behavior

`SSD1315_InvertRectangle()` does **NOT** simply XOR pixels. The actual sequence is:
1. **Restore** `GRAM` from `Background_GRAM` (resetting any previous inversion)
2. **Then XOR** the specified rectangle region on `GRAM`

This means:
- **Only ONE inverted region can exist at a time** — calling it again restores the previous one
- It is used for cursor/selection highlighting in menus — each frame redraws content, then applies one highlight
- The effect is self-resetting: if you stop calling it, the normal content reappears on the next `SSD1315_Clear()` + redraw cycle
- `SSD1315_CleanDrawRectangle()` makes an inversion permanent by copying `GRAM` back to `Background_GRAM` after inverting

### DS3231 RTC Driver (`Hardware/DS3231.c`)

- Uses BCD encoding internally; `BCD_To_Dec()` / `Dec_To_BCD()` convert at the driver boundary.
- `DS3231_Init()` enables oscillator, alarm interrupt (INTCN + A1IE), and disables 32kHz output.
- `Alarm_TypeDef` fields: Second (Alarm1 only), Minute, Hour, Day, Repeat (Daily/Date/Weekday), State (Enable/Disable).
- Alarm disable: all match bits (A1M1-A1M4 or A2M2-A2M4) set to 1 = Disable; State flag derived from this at read time.
- `DS3231_ClearAlarmFlag()` clears both A1F and A2F bits in the status register.

### Button Driver (`Hardware/Button.c`)

- **Polled, no ISR.** `Button_TimerHandler()` called every 10ms.
- Debounce: counter increments in 10ms steps up to 20ms (`DEBOUNCE_MS = 20`). State is considered stable after 20ms of consistent raw reading.
- Short press: released before `LONG_PRESS_MS` (2000ms).
- Long press: detected at release time if pressed ≥ 2000ms. Note: long press fires on **release**, not after holding 2s.
- `Button_GetEvent()` returns the pending event and clears it (one-shot consumption pattern).
- `Button_GetMaxPressMs()` returns the longest current press duration across all buttons — used for the HOME page progress bar animation.

### Buzzer Driver (`Hardware/Buzzer.c`)

- Active buzzer on PB0: `HAL_GPIO_WritePin` LOW=on, HIGH=off (needs external transistor for current).
- `Buzzer_StartAlarm(duration_ms)` starts buzzing and arms an auto-shutoff timer.
- `Buzzer_TimerHandler()` (10ms) decrements the counter; when expired, turns off buzzer.
- **Timer handler runs even during sleep** — so alarm auto-shutoff works during RINGING state.
- `Buzzer_On()`, `Buzzer_Off()`, `Buzzer_Toggle()` are low-level helpers available but **not used by the main app** — the app only uses `Buzzer_StartAlarm()`. Similarly, `Button_IsAnyPressed()` exists but isn't called in the current main loop.

### Menu & UI (`Hardware/Menu.c`)

- **Each page's Process function runs every 10ms loop iteration.** Button events are consumed; if no button was pressed, `Button_GetEvent()` returns `BTN_NONE` and the page handler takes no action.
- **Editing mode convention:** RIGHT toggles `editing` flag. In browsing mode (`editing == 0`), UP/DOWN move cursor between fields. In editing mode (`editing == 1`), UP/DOWN modify the selected value.
- **Alarm editing:** The step count and label ordering is dynamic — Alarm1 has an extra "Sec" field; the "Day" field only appears if `Repeat != ALARM_DAILY`; "State" is always last. `Alarm_StepCount()` and `Alarm_StepLabel()` manage this complexity.
- `Settings_Save()` disables IRQs during flash operations (erase + program).
- **Appearance saves on every toggle** — `Settings_Save()` is called immediately when a style is changed in the Appearance page.
- **Alarm saves on LEFT (back)** — `DS3231_WriteAlarm()` is called when leaving the alarm set page, not on each edit.

### Font Rendering (`Hardware/Font.c`)

- `Font_Draw7SegTime()`: Renders a 7-segment-style digital clock at coordinates (`x`, `y`). Each digit is composed of 7 rectangle segments (A-G). Colon dots flash based on `time->Second & 1`.
- `Font_DrawBinaryTime()`: Renders hour and minute as two rows of 8-bit binary squares (11×11 px each, centered).
- Segment size constants: `SEG_LEN=12`, `SEG_WID=2`, `MID_GAP=15` (extra gap between HH and MM), `NUM_GAP=2`.

### Naming Conventions

| Prefix         | Used for                                     |
|----------------|----------------------------------------------|
| `SSD1315_`     | OLED display driver functions                |
| `DS3231_`      | RTC driver functions                         |
| `App_`         | Application-level UI functions (Home, Alarm) |
| `Button_`      | Button driver                                |
| `Buzzer_`      | Buzzer driver                                |
| `Font_`        | Font/rendering helpers                       |
| `Page_`        | Menu page state machine                      |
| `Sleep_`       | Sleep mode management                        |
| `MX_`          | CubeMX-generated peripheral init functions   |
| `HAL_`         | STM32 HAL library                            |

Types use `_TypeDef` suffix. Enums use PascalCase members.

### I2C Details
- Both DS3231 (`0xD0` write / `0xD1` read) and SSD1315 (`0x78` write / `0x79` read) share I2C1.
- SSD1315 uses memory-mapped I2C writes with command address `0x00` and data address `0x40`.
- DS3231 uses standard register-address + data I2C memory transfers.
- All I2C transactions have a 100ms timeout.

### Linker
- 64KB flash, 20KB RAM (STM32F103C8Tx).
- `--gc-sections` enabled; unused functions are stripped.
- Flash page size: 1KB (1024 bytes), last page `0x0800FC00` used for config storage.

## ⚠️ 驱动文件不可修改

**不要修改以下文件的已有代码（除非要增加新功能）：**
- `Hardware/OLED.c` / `OLED.h`
- `Hardware/DS3231.c` / `DS3231.h`

这些驱动已经过实际硬件测试验证，能稳定工作。**不要因为"看着不舒服"就去重构它们。**

可以改动的情况：
1. 有新功能需求（如新 OLED 指令支持、DS3231 新闹钟模式）
2. 实际运行时触发的 bug（而非代码审查猜测的潜在问题）

## Adding New Hardware Drivers

1. Place `.c` + `.h` files in `Hardware/`
2. Add `.c` files to `target_sources()` in root `CMakeLists.txt` (~line 47)
3. Add `Hardware` to `target_include_directories()` if not already present (~line 60)
