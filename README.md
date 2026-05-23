# Alarm_ai

STM32F103C8T6 智能闹钟固件

[![STM32](https://img.shields.io/badge/MCU-STM32F103C8Tx-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html)

## 功能

- 🕐 **双闹钟** — 秒级精度，Daily/Date/Weekday 三种重复模式
- 📟 **128×64 OLED 显示** — 7 段数码管 + 二进制极简模式
- 🎨 **个性化** — 进度条样式、首页风格可切换，掉电不丢失
- 😴 **30s 自动休眠** — STOP 模式，任意键/闹钟唤醒
- 🔔 **蜂鸣器响铃** — 30s 自动关断，任意键停止

## 硬件

- STM32F103C8Tx (Blue Pill)
- SSD1315 OLED 128×64 (I2C 0x78)
- DS3231 RTC (I2C 0xD0)
- 4 按键 + 有源蜂鸣器

## 构建

```bash
cmake --preset Debug
cmake --build --preset Debug
# 产物: build/Debug/test.elf
```

前置条件：`arm-none-eabi-gcc` + Ninja

## 项目结构

```
Hardware/    用户硬件驱动和应用代码
Core/        STM32CubeMX 生成代码
Drivers/     STM32 HAL + CMSIS
cmake/       工具链配置
```

详见 [AGENTS.md](AGENTS.md) 和 [PROJECT_REPORT.md](PROJECT_REPORT.md)
