# Alarm_ai 项目报告

## 项目概述

基于 STM32F103C8T6 的智能闹钟固件。128×64 OLED 显示屏，DS3231 高精度 RTC 模块，4 键交互，有源蜂鸣器。

**代码规模**：16 个自定义源文件，2339 行 C 代码。Flash 占用 24.2 KB（37.0%），RAM 占用 3.9 KB（19.0%）。

---

## 硬件配置

| 引脚 | 功能 | 配置 |
|------|------|------|
| PB6 / PB7 | I2C1 SCL / SDA | 400kHz 开漏 |
| PA0 / PA2 / PA4 / PA6 | 按键 UP / DOWN / LEFT / RIGHT | 上拉输入，下降沿 EXTI |
| PA1 | 有源蜂鸣器 | 推挽输出（高电平响） |
| PB10 | DS3231 中断输入 | 上拉输入，下降沿 EXTI |

**外设 I2C 地址**：

| 设备 | 写地址 | 功能 |
|------|--------|------|
| SSD1315 OLED | 0x78 | 128×64 单色显示 |
| DS3231 RTC | 0xD0 | 时间 + 电池备份 SRAM |

---

## 软件架构

```
┌─────────────────────────────────────────┐
│  main.c — 主循环，10ms 定时任务分发       │
├─────────────────────────────────────────┤
│  Menu.c — 页面状态机 + 所有 UI 逻辑       │
├──────────┬──────────┬───────────────────┤
│  Apps.c  │  Font.c  │  硬件驱动层         │
│  首页 UI │  字体渲染 │  OLED / DS3231    │
│          │          │  Button / Buzzer  │
│          │          │  Sleep            │
├──────────┴──────────┴───────────────────┤
│  STM32 HAL + CMSIS (CubeMX 生成)         │
└─────────────────────────────────────────┘
```

**数据流**：SysTick 中断 → 10ms 定时任务 → Button/Buzzer/Sleep 状态更新 → Page_Process（状态机）→ Page_Draw（渲染到 OLED 双缓冲 GRAM）→ SSD1315_Update（I2C 推屏）

**控制流**：按键事件（短按/长按）通过 Button 驱动的去抖状态机产生，Page_Process 消费事件驱动页面跳转和数值编辑。

---

## 文件结构

| 路径 | 行数 | 说明 |
|------|------|------|
| `Hardware/Menu.c/h` | 565+45 | 页面状态机、菜单系统、个性化设置及持久化 |
| `Hardware/OLED.c/h` | 432+41 | SSD1315 显示驱动，双缓冲渲染 |
| `Hardware/DS3231.c/h` | 169+52 | RTC 驱动，BCD 编解码，双闹钟完整读写 |
| `Hardware/Font.c/h` | 119+10 | 7 段数码管字体 + 二进制时间渲染 |
| `Hardware/Button.c/h` | 104+26 | 按键去抖 + 长短按检测（真实时间计时） |
| `Hardware/Sleep.c/h` | 47+12 | 30s 无操作进 STOP 休眠，EXTI 唤醒 |
| `Hardware/Buzzer.c/h` | 38+13 | 蜂鸣器控制，定时自动关断 |
| `Hardware/Apps.c/h` | 29+12 | 首页渲染 + 闹钟响铃页 |
| `Core/Src/main.c` | 219 | 主程序入口，外设初始化，10ms 主循环 |
| `Core/Src/stm32f1xx_it.c` | 233 | EXTI 中断处理，DS3231 闹钟回调 |
| `Core/Src/gpio.c` | 95 | GPIO 引脚初始化（含按键、蜂鸣器、中断） |
| `CMakeLists.txt` | 78 | CMake 构建配置 |

---

## 功能清单

### 首页

- 数码管 7 段式时间显示（HH:MM），日期和星期
- 二进制极简模式（16 个 11×11 方框，2 字节时间）
- 标语 "Nothing is impossible"（21 字符，恰好 126px 宽）
- 长按进度条视觉反馈（200ms 后出现，2s 填满进入菜单）

### 进度条

- 底部细条样式（Bar）
- 全屏灌满样式（Fullscreen）
- 松手回落动画
- 样式在 Appearance 中切换，掉电不丢失

### 菜单系统

```
Menu
├── Alarm
│   ├── Alarm 1  → Hour / Min / Sec / Repeat / [Day] / State
│   └── Alarm 2  → Hour / Min /       Repeat / [Day] / State
├── Time Set     → Year / Month / Date / Hour / Min / Week
└── Appearance
    ├── Bar Style:  Bar / FullScr
    └── Home Style: Clock / Binary
```

### 闹钟

- 双闹钟独立配置，存入 DS3231 寄存器
- 三种重复模式：
  - Daily（每天匹配，忽略日期）
  - Date（指定每月某天 1–31）
  - Weekday（指定星期 Mon–Sun）
- 闹钟触发后全屏闪烁 "ALARM!" + 蜂鸣器响
- 任意按键停止

### 休眠

- 首页 30 秒无操作进入 STOP 模式
- OLED 关闭，功耗最低
- 任意按键或闹钟中断唤醒
- 唤醒后自动恢复时钟和显示

### 设置持久化

- 个性化设置存入 STM32 内部 Flash（最后一页 0x0800FC00）
- 魔数 0xA55A 校验，上电自动恢复
- 每次修改实时写入，掉电不丢失

---

## 交互规范

所有设置页面统一两级交互：

| 状态 | LEFT | RIGHT | UP/DOWN |
|------|------|-------|---------|
| 浏览 | 保存 + 返回上级 | 进入编辑 | 移动光标 |
| 编辑 | 保存 + 返回上级 | 退出编辑 | 修改数值 |

唯一的例外：首页长按任一键 2s（配合进度条）进入菜单。

---

## 设计原则

1. **禁止重构已验证的驱动** — OLED.c 和 DS3231.c 除 bug 修复外不修改
2. **所有页面使用统一交互模型** — 两级浏览/编辑、InvertRectangle 高亮
3. **CubeMX USER CODE 区域** — 所有自定义代码放在标记区间内，方便重新生成
4. **模块化字体系统** — Font.c 包含 7 段编码表和绘制逻辑，新增字体只需加编码表 + Draw 函数

---

## 构建与烧录

```bash
# 前置条件：arm-none-eabi-gcc 在 PATH 中，Ninja 可用
cmake --preset Debug
cmake --build --preset Debug

# 产物：build/Debug/test.elf
# 用 ST-Link / OpenOCD / STM32CubeProgrammer 烧录
```

---

## 致谢

本项目由用户策划概念、操作逻辑和审美方向，AI 辅助实现全部 C 代码、硬件驱动和调试。从概念到完整固件的全流程协作。

> *"Nothing is impossible"*
