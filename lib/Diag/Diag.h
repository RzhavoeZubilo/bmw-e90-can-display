#pragma once

#include <stdint.h>

// Диагностический опрос блоков по PT-CAN.
//
// Часть параметров в широковещании отсутствует, но блоки отдают их по
// запросу. Телеграммы взяты не из реверса, а из SGBD в SP-Daten — см.
// docs/diag-jobs.md, там же способ их извлечения.
//
// ВНИМАНИЕ: включение опроса означает, что устройство начинает ПЕРЕДАВАТЬ
// в моторную шину. До этого прошивка работала в MCP_LISTENONLY и физически
// не могла отправить ни кадра.

namespace diag {

// Физическая адресация на PT-CAN. Источник — комментарии к сообщениям
// PTCAN_RESP_* в DBC-mega-merge, ссылающиеся на трассировку BMW ISTA.
static const uint32_t DME_REQ = 0x600;
static const uint32_t DME_RESP = 0x608;

// STATUS_AN_LUFTTEMPERATUR у MEV17N46: температура воздуха на впуске.
// Сервис 0x30, двухбайтовый идентификатор.
static const uint8_t INTAKE_PAYLOAD[] = {0x30, 0x0A, 0x01};
static const uint8_t INTAKE_PAYLOAD_LEN = 3;

// Положительный ответ на сервис 0x30.
static const uint8_t POSITIVE_30 = 0x70;

}  // namespace diag

struct DiagData {
  int16_t iat_c = 0;       // температура впуска, целые градусы
  uint32_t iat_ms = 0;
};

class Diag {
 public:
  // Опрос раз в секунду, поэтому окно шире, чем у вещания.
  static const uint32_t STALE_MS = 4000;

  void reset();

  // Собирает однокадровый запрос ISO-TP. out — ровно 8 байт.
  static void build_request(const uint8_t* payload, uint8_t n, uint8_t* out);

  // Разбирает ответ. true, если кадр наш и значение обновилось.
  bool apply(uint32_t id, const uint8_t* d, uint8_t len, uint32_t now_ms);

  const DiagData& data() const { return d_; }

  static bool fresh(uint32_t stamp, uint32_t now_ms) {
    return stamp != 0 && (now_ms - stamp) < STALE_MS;
  }

 private:
  DiagData d_;
};
