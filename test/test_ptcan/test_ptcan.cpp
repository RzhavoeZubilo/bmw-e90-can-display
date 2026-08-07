#include <unity.h>

#include "PtCan.h"

// Кадры DME (0x1D0, 0x3B4) проверяются РЕАЛЬНЫМИ байтами из захватов:
// шлюз ретранслирует их на K-CAN, откуда сняты логи DBC-mega-merge.
// Кадр EGS (0x0B5) на K-CAN не выходит, поэтому собран по DBC —
// это проверка формул и разрядности, а не подтверждение с живой машины.

static PtCan p;

void setUp() { p.reset(); }
void tearDown() {}

// --- DME, подтверждено захватами ---

static void test_engine_data_real_capture() {
  const uint8_t d[8] = {0x8B, 0xFF, 0x6A, 0xC8, 0x00, 0x00, 0x0D, 0x9C};
  TEST_ASSERT_TRUE(p.apply(ptcan::ID_ENGINE_DATA, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(91, p.data().coolant_c);
}

static void test_oil_absent_marker_discarded() {
  // 0xFF -> сигнала нет. Наивная формула дала бы 207 °C и ложную тревогу.
  const uint8_t d[8] = {0x8B, 0xFF, 0, 0, 0, 0, 0, 0};
  p.apply(ptcan::ID_ENGINE_DATA, d, 8, 1000);
  TEST_ASSERT_FALSE(PtCan::fresh(p.data().oil_ms, 1000));
}

static void test_oil_present_on_petrol() {
  const uint8_t d[8] = {0x8B, 0x88, 0, 0, 0, 0, 0, 0};  // 136-48 = 88 °C
  p.apply(ptcan::ID_ENGINE_DATA, d, 8, 1000);
  TEST_ASSERT_EQUAL_INT16(88, p.data().oil_c);
  TEST_ASSERT_TRUE(PtCan::fresh(p.data().oil_ms, 1000));
}

static void test_battery_real_capture() {
  const uint8_t d[8] = {0xCB, 0xF3, 0x00, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF};
  TEST_ASSERT_TRUE(p.apply(ptcan::ID_BATTERY, d, 8, 1000));
  TEST_ASSERT_EQUAL_UINT16(14565, p.data().volts_mv);
}

static void test_battery_ignores_upper_nibble() {
  const uint8_t a[2] = {0xCB, 0x03};
  const uint8_t b[2] = {0xCB, 0xF3};
  p.apply(ptcan::ID_BATTERY, a, 2, 1000);
  const uint16_t first = p.data().volts_mv;
  p.apply(ptcan::ID_BATTERY, b, 2, 1000);
  TEST_ASSERT_EQUAL_UINT16(first, p.data().volts_mv);
}

// --- EGS, только по DBC ---

static void test_atf_temp() {
  // TEMP_GRB в байте 7, смещение -40. 0x78 = 120 -> 80 °C,
  // это середина рабочего диапазона ZF 6HP19 (60..90 °C).
  uint8_t d[8] = {0};
  d[7] = 0x78;
  TEST_ASSERT_TRUE(p.apply(ptcan::ID_EGS_TORQUE, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(80, p.data().atf_c);
}

static void test_atf_cold() {
  uint8_t d[8] = {0};
  d[7] = 0x1E;  // 30 - 40 = -10 °C, зимой перед прогревом
  p.apply(ptcan::ID_EGS_TORQUE, d, 8, 1000);
  TEST_ASSERT_EQUAL_INT16(-10, p.data().atf_c);
}

static void test_gearbox_overtemp_status() {
  // ST_OTMP_GRB — биты 6..7 байта 4.
  uint8_t d[8] = {0};
  d[4] = 0x40;  // 01 в старших двух битах
  p.apply(ptcan::ID_EGS_TORQUE, d, 8, 1000);
  TEST_ASSERT_EQUAL_UINT8(1, p.data().gearbox_temp_st);

  d[4] = 0x00;
  p.apply(ptcan::ID_EGS_TORQUE, d, 8, 1000);
  TEST_ASSERT_EQUAL_UINT8(ptcan::GEARBOX_TEMP_OK, p.data().gearbox_temp_st);
}

static void test_atf_requires_full_frame() {
  // Байт 7 обязателен: на коротком кадре читать нечего.
  const uint8_t d[5] = {0, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(p.apply(ptcan::ID_EGS_TORQUE, d, 5, 1000));
}

// --- общее ---

static void test_unknown_id_ignored() {
  const uint8_t d[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  TEST_ASSERT_FALSE(p.apply(0x349, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(0, p.data().coolant_c);
}

static void test_staleness() {
  uint8_t d[8] = {0};
  d[7] = 0x78;
  p.apply(ptcan::ID_EGS_TORQUE, d, 8, 1000);
  TEST_ASSERT_TRUE(PtCan::fresh(p.data().atf_ms, 1500));
  TEST_ASSERT_FALSE(PtCan::fresh(p.data().atf_ms, 2500));
  TEST_ASSERT_FALSE(PtCan::fresh(p.data().volts_ms, 1000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_engine_data_real_capture);
  RUN_TEST(test_oil_absent_marker_discarded);
  RUN_TEST(test_oil_present_on_petrol);
  RUN_TEST(test_battery_real_capture);
  RUN_TEST(test_battery_ignores_upper_nibble);
  RUN_TEST(test_atf_temp);
  RUN_TEST(test_atf_cold);
  RUN_TEST(test_gearbox_overtemp_status);
  RUN_TEST(test_atf_requires_full_frame);
  RUN_TEST(test_unknown_id_ignored);
  RUN_TEST(test_staleness);
  return UNITY_END();
}
