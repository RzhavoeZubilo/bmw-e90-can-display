#pragma once

#include <stdint.h>

// Логика предупреждений. Отдельно от железа, чтобы пороги можно было
// проверить тестом, а не прогревом двигателя.

enum class Alert : uint8_t {
  None = 0,
  CoolantHot,
  AtfHot,
};

namespace alerts {

static const int16_t COOLANT_HOT_C = 110;

// ZF 6HP19: рабочий диапазон 60..90 °C, тревога выше 130 °C.
// Порог из комментария к сигналу TEMP_GRB в DBC-mega-merge.
static const int16_t ATF_HOT_C = 130;

// Флаги valid говорят, есть ли свежие данные. Без них молчим: пугать
// водителя показанием отвалившегося датчика хуже, чем не сказать ничего.
// Перегрев двигателя приоритетнее перегрева коробки.
Alert evaluate(int16_t coolant_c, bool coolant_valid, int16_t atf_c,
               bool atf_valid);

}  // namespace alerts
