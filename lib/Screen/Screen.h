#pragma once

#include <stdint.h>

#include <clib/u8g2.h>

#include "Alerts.h"

// Отрисовка экрана. Намеренно использует C-API u8g2, а не Arduino-обёртку:
// тот же самый код собирается и под AVR, и на хосте. Благодаря этому
// симулятор (sim/) показывает настоящую картинку прошивки, а не макет,
// который со временем разъедется с кодом.

// Экраны листаются кнопкой руля. Первый — обзорный, дальше по одному
// параметру крупно.
enum class ScreenId : uint8_t {
  All = 0,   // все параметры разом
  Coolant,   // температура ОЖ с иконкой
  Voltage,   // напряжение бортсети
  COUNT
};

// Обучение кнопки руля показывается поверх всего остального.
enum class LearnPhase : uint8_t {
  None = 0,
  Baseline,  // ничего не трогаем, копим фон
  Hold,      // удерживаем нужную кнопку
  Saved,     // привязка записана
  Failed     // не нашли, работаем без кнопки
};

struct ScreenData {
  ScreenId screen = ScreenId::All;
  LearnPhase learn = LearnPhase::None;
  uint8_t learn_secs = 0;

  bool can_ok = true;
  Alert alert = Alert::None;

  int16_t coolant_c = 0;
  bool coolant_ok = false;

  int16_t atf_c = 0;
  bool atf_ok = false;

  int16_t oil_c = 0;
  bool oil_ok = false;

  uint16_t volts_mv = 0;
  bool volts_ok = false;

  // Температура впуска и топливо на PT-CAN пока не найдены: места в вёрстке
  // держим, чтобы разведка могла их просто заполнить.
  int16_t iat_c = 0;
  bool iat_ok = false;

  uint16_t fuel_total_l_x10 = 0;
  bool fuel_ok = false;

  uint16_t fuel_left_l_x10 = 0;
  uint16_t fuel_right_l_x10 = 0;
  bool tanks_ok = false;
};

// Рисует полный кадр, включая цикл по страницам. В page-режиме (AVR)
// прокрутится восемь раз, в full-buffer (хост) — один.
void screen_draw(u8g2_t* u, const ScreenData& d);
