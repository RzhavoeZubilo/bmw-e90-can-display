#pragma once

// Распиновка Arduino Nano V3.0 (ATmega328P).
//
// Занято аппаратно и трогать нельзя:
//   D11 MOSI, D12 MISO, D13 SCK  — SPI к MCP2515
//   A4 SDA, A5 SCL               — I2C к OLED (0x3C)

// --- MCP2515 (CAN) ---
static const uint8_t PIN_CAN_CS = 10;
static const uint8_t PIN_CAN_INT = 2;  // INT0, нужен именно 2 или 3

// --- Адресный светодиод WS2812B ---
// В сборке используется один. Количество почти не влияет на память:
// 3 байта RAM на диод, тогда как сама FastLED стоит ~3 КБ флеша.
static const uint8_t PIN_LEDS = 6;
static const uint8_t NUM_LEDS = 1;

// --- Пассивный зуммер KY-006 ---
// tone() на AVR всегда занимает Timer2 независимо от пина.
static const uint8_t PIN_BUZZER = 9;

// --- Резерв под расширение ---
// Сейчас не используется. Сенсорные кнопки TTP-223: выход активен
// ВЫСОКИМ уровнем, подтяжка не нужна. BME280 садится на ту же шину I2C
// (адрес 0x76 или 0x77), отдельных ног не требует.
static const uint8_t PIN_BTN[4] = {3, 4, 5, 7};
static const uint8_t NUM_BTNS = 4;
