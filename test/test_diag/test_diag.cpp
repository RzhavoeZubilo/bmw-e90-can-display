#include <unity.h>

#include "Diag.h"

// Телеграмма запроса взята из SGBD MEV17N46 (SP-Daten 69.0) и проверяема:
// метод извлечения подтверждён на job'е, где идентификатор продублирован в
// комментарии. А вот РАСКЛАДКА ОТВЕТА выведена из порядка результатов job'а
// и на живой машине пока не проверена — тесты фиксируют то, что заложено в
// коде, и сломаются, если раскладку придётся поправить. Так и задумано.

static Diag g;

void setUp() { g.reset(); }
void tearDown() {}

// --- запрос ---

static void test_request_is_iso_tp_single_frame() {
  uint8_t b[8];
  Diag::build_request(diag::INTAKE_PAYLOAD, diag::INTAKE_PAYLOAD_LEN, b);
  TEST_ASSERT_EQUAL_UINT8(0x03, b[0]);  // длина полезной части
  TEST_ASSERT_EQUAL_UINT8(0x30, b[1]);
  TEST_ASSERT_EQUAL_UINT8(0x0A, b[2]);
  TEST_ASSERT_EQUAL_UINT8(0x01, b[3]);
}

// --- ответ ---

// [len][70][0A][01][adc_hi][adc_lo][t_hi][t_lo]
static void resp(uint8_t* d, int16_t raw16) {
  d[0] = 0x07;
  d[1] = 0x70;
  d[2] = 0x0A;
  d[3] = 0x01;
  d[4] = 0x02;  // сырое АЦП, нам не нужно
  d[5] = 0x30;
  d[6] = static_cast<uint8_t>((raw16 >> 8) & 0xFF);
  d[7] = static_cast<uint8_t>(raw16 & 0xFF);
}

static void test_intake_typical() {
  uint8_t d[8];
  resp(d, 34 * 16);  // 34 °C
  TEST_ASSERT_TRUE(g.apply(diag::DME_RESP, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(34, g.data().iat_c);
}

static void test_intake_below_zero() {
  uint8_t d[8];
  resp(d, -15 * 16);
  TEST_ASSERT_TRUE(g.apply(diag::DME_RESP, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(-15, g.data().iat_c);
}

static void test_out_of_range_rejected() {
  // Если раскладка ответа окажется иной, сюда прилетит мусор. Значение,
  // невозможное для датчика впуска, принимать нельзя.
  uint8_t d[8];
  resp(d, 900 * 16);
  TEST_ASSERT_FALSE(g.apply(diag::DME_RESP, d, 8, 1000));
  TEST_ASSERT_FALSE(Diag::fresh(g.data().iat_ms, 1000));
}

static void test_wrong_id_ignored() {
  uint8_t d[8];
  resp(d, 20 * 16);
  TEST_ASSERT_FALSE(g.apply(0x1D0, d, 8, 1000));
}

static void test_negative_response_ignored() {
  // 0x7F — «сервис не поддерживается».
  const uint8_t d[8] = {0x03, 0x7F, 0x30, 0x11, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(g.apply(diag::DME_RESP, d, 8, 1000));
}

static void test_echo_mismatch_ignored() {
  // Ответ на другой идентификатор того же сервиса принимать нельзя.
  uint8_t d[8];
  resp(d, 20 * 16);
  d[2] = 0x1C;  // это ответ про напряжение, а не про впуск
  TEST_ASSERT_FALSE(g.apply(diag::DME_RESP, d, 8, 1000));
}

static void test_multiframe_ignored() {
  uint8_t d[8];
  resp(d, 20 * 16);
  d[0] = 0x10;  // первый кадр многокадрового ответа
  TEST_ASSERT_FALSE(g.apply(diag::DME_RESP, d, 8, 1000));
}

static void test_short_frame_rejected() {
  uint8_t d[8];
  resp(d, 20 * 16);
  TEST_ASSERT_FALSE(g.apply(diag::DME_RESP, d, 4, 1000));
}

static void test_staleness() {
  uint8_t d[8];
  resp(d, 25 * 16);
  g.apply(diag::DME_RESP, d, 8, 1000);
  TEST_ASSERT_TRUE(Diag::fresh(g.data().iat_ms, 4000));
  TEST_ASSERT_FALSE(Diag::fresh(g.data().iat_ms, 6000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_request_is_iso_tp_single_frame);
  RUN_TEST(test_intake_typical);
  RUN_TEST(test_intake_below_zero);
  RUN_TEST(test_out_of_range_rejected);
  RUN_TEST(test_wrong_id_ignored);
  RUN_TEST(test_negative_response_ignored);
  RUN_TEST(test_echo_mismatch_ignored);
  RUN_TEST(test_multiframe_ignored);
  RUN_TEST(test_short_frame_rejected);
  RUN_TEST(test_staleness);
  return UNITY_END();
}
