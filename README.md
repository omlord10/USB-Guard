# USBGuard v4 — Sector Scanner + HDMI Terminal

## Обзор

Arduino Nano (USB Host Shield) сканирует флешку на сигнатуры угроз.
Tang Nano 9K управляет кнопками, LED-индикацией и **выводит лог на HDMI-монитор** (640×480 @ 60 Hz).

Весь текстовый вывод, который раньше шёл только в Serial Monitor Arduino IDE,
теперь дублируется на HDMI-терминал через Tang Nano 9K.

---

## Подключение

### Arduino Nano <-> Tang Nano 9K (через LLC 3.3V/5V)

| Arduino | LLC | Tang Nano 9K |
|---------|-----|--------------|
| D3 (TX SoftSerial) | HV→LV | P34 (uart_rx_pin) |
| D2 (RX SoftSerial) | LV→HV | P33 (uart_tx_pin) |
| GND | GND | GND |

### Внешние кнопки (active HIGH, внешний pull-down 10kΩ → GND)

| Кнопка | Пин Tang | Команда | Описание |
|--------|----------|---------|----------|
| btn1 | P26 | `h` | offset += 100 |
| btn2 | P27 | `k` | offset += 10 000 |
| btn3 | P28 | `m` | offset += 1 000 000 |

### Кнопки на плате Tang Nano 9K

| Кнопка | Пин | Команда | Описание |
|--------|-----|---------|----------|
| S1 | P4 | `s` | Start scan from current offset |
| S2 | P3 | `res` | Reset offset to 0 |

### LED

| LED | Пин | Тип |
|-----|-----|-----|
| led_ob[0..5] | P10,P11,P13,P14,P15,P16 | onboard (active LOW) |
| led_green | P29 | external (active HIGH) |
| led_red | P30 | external (active HIGH) |

### HDMI

Встроенный разъём HDMI на Tang Nano 9K (пины 68-75, LVDS).
Подключить HDMI-кабелем к монитору.

---

## Управление LED через UART

Символы от Arduino → Tang (направление RX):

| Символ | Действие |
|--------|----------|
| `0`-`5` | toggle LED 1-6 (onboard) |
| `g` | toggle green LED |
| `r` | toggle red LED |

### Статусные режимы LED

| Статус | Символ | Зелёный (P29) | Красный (P30) |
|--------|--------|---------------|----------------|
| READY | `R` | медленно мигает | выкл |
| NO_MEDIA | `N` | одиночные вспышки | выкл |
| WORKING | `W` | попеременно | попеременно |
| GOOD | `G` | горит | выкл |
| BAD | `B` | выкл | горит |
| ERROR | `E` | быстро мигает | быстро мигает |

---

## Структура файлов

```
usbguard_v4/
├── fpga/
│   ├── usbguard.gprj          ← открыть в Gowin IDE
│   └── src/
│       ├── top.v               ← главный модуль (UART + LED + HDMI)
│       ├── svo_hdmi.v          ← HDMI pipeline (модифицированный)
│       ├── uart_rx.v           ← UART приёмник 9600 8N1
│       ├── uart_tx.v           ← UART передатчик 9600 8N1
│       ├── hdmi/               ← SVO библиотека (без изменений)
│       │   ├── svo_defines.vh
│       │   ├── svo_enc.v
│       │   ├── svo_tcard.v
│       │   ├── svo_term.v
│       │   ├── svo_tmds.v
│       │   └── svo_utils.v
│       ├── gowin_rpll/         ← PLL: 27→126 MHz
│       │   └── gowin_rpll.v
│       ├── gowin_clkdiv/       ← CLKDIV: 126/5 = 25.2 MHz
│       │   └── gowin_clkdiv.v
│       ├── usbguard.cst        ← пины
│       └── usbguard.sdc        ← тайминги
└── arduino/
    └── arduino_main/
        ├── arduino_main.ino    ← основной скетч
        ├── signature_engine.h
        ├── signature_engine.c
        └── signatures.h
```

---

## Прошивка

### FPGA (Tang Nano 9K)

1. Открыть `fpga/usbguard.gprj` в Gowin IDE
2. Synthesize → Place & Route → Program Device
3. Выбрать «Program to SRAM» для теста или «Program to Flash» для постоянной прошивки

### Arduino Nano

1. Открыть `arduino/arduino_main/arduino_main.ino` в Arduino IDE
2. Установить библиотеку USB Host Shield 2.0 (через Library Manager)
3. Board: Arduino Nano, Processor: ATmega328P
4. Upload

---

## Быстрый тест HDMI

После прошивки FPGA подключи HDMI-кабель к монитору.
На экране появится тестовая карта (цветные полосы) — это `svo_tcard`.
Как только Arduino начнёт отправлять текст по UART (9600 бод),
он будет отображаться белым шрифтом поверх тестовой карты.
