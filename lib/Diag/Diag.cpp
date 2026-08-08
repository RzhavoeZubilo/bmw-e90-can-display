#include "Diag.h"

void Diag::reset() { d_ = DiagData(); }

void Diag::build_request(const uint8_t* payload, uint8_t n, uint8_t* out) {
  if (out == nullptr || payload == nullptr || n > 7) return;
  // Однокадровое сообщение ISO-TP: старший ниббл 0, младший — длина.
  out[0] = n & 0x0F;
  for (uint8_t i = 0; i < n; ++i) out[1 + i] = payload[i];
  // Хвост принято забивать; на приём это не влияет.
  for (uint8_t i = 1 + n; i < 8; ++i) out[i] = 0x55;
}

bool Diag::apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms) {
  if (d == nullptr || id != diag::DME_RESP || len < 3) return false;

  const uint8_t pci = d[0];
  if ((pci & 0xF0) != 0x00) return false;  // многокадровые нам не нужны
  const uint8_t n = pci & 0x0F;
  if (n < 3 || static_cast<uint16_t>(n) + 1 > len) return false;

  if (d[1] != diag::POSITIVE_30) return false;
  // Эхо идентификатора: убеждаемся, что это ответ именно на наш запрос.
  if (d[2] != diag::INTAKE_PAYLOAD[1] || d[3] != diag::INTAKE_PAYLOAD[2])
    return false;

  // РАСКЛАДКА ОТВЕТА ВЫВЕДЕНА, А НЕ ПОДТВЕРЖДЕНА. Из SGBD известно, что job
  // возвращает два двухбайтовых значения в порядке: сначала сырое напряжение
  // АЦП датчика, затем температура. Предполагаем, что в кадре они лежат в том
  // же порядке сразу за эхом идентификатора. Проверяется одним запуском на
  // машине: температура впуска на прогретой машине должна быть правдоподобной.
  if (n < 7) return false;
  const uint8_t* p = d + 4;  // пропускаем эхо 0x0A 0x01

  // Температура: знаковое 16-битное, шаг 1/16 °C.
  // Диапазон из SGBD: -2047.9375 .. 2047.9375 при двух байтах — это ровно
  // деление на 16.
  const int16_t raw =
      static_cast<int16_t>((static_cast<uint16_t>(p[2]) << 8) | p[3]);
  const int16_t c = static_cast<int16_t>(raw / 16);

  // Отсекаем заведомо невозможное: датчик впуска в машине не бывает вне
  // этого диапазона, а мусор от неверной раскладки — легко.
  if (c < -60 || c > 150) return false;

  d_.iat_c = c;
  d_.iat_ms = now_ms;
  return true;
}
