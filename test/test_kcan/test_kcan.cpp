#include <unity.h>

#include "KCan.h"

// Тесты гоняют РЕАЛЬНЫЕ кадры из захватов, а не выдуманные значения.
// Байты взяты из tools/known_values.json репозитория DBC-mega-merge —
// это самые частые значения из логов живой машины и HIL-стенда.

static KCan k;

void setUp() { k.reset(); }
void tearDown() {}

// --- 0x1D0 EngineData ---

static void test_engine_data_from_real_capture() {
  // HIL, 85306 кадров: 8B FF 6A C8 00 00 0D 9C
  const uint8_t d[8] = {0x8B, 0xFF, 0x6A, 0xC8, 0x00, 0x00, 0x0D, 0x9C};
  TEST_ASSERT_TRUE(k.apply(kcan::ID_ENGINE_DATA, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(91, k.data().coolant_c);  // 0x8B=139, 139-48
}

static void test_oil_temp_absent_marker_not_stored() {
  // На N47 байт 1 равен 0xFF — сигнала нет. Наивная формула дала бы
  // 207 °C и ложную тревогу, поэтому значение не принимается вовсе.
  const uint8_t d[8] = {0x8B, 0xFF, 0, 0, 0, 0, 0, 0};
  k.apply(kcan::ID_ENGINE_DATA, d, 8, 1000);
  TEST_ASSERT_FALSE(KCan::fresh(k.data().oil_ms, 1000));
}

static void test_oil_temp_stored_when_present() {
  // Бензиновые моторы отдают реальное значение: 0x88=136, 136-48 = 88 °C
  const uint8_t d[8] = {0x8B, 0x88, 0, 0, 0, 0, 0, 0};
  k.apply(kcan::ID_ENGINE_DATA, d, 8, 1000);
  TEST_ASSERT_EQUAL_INT16(88, k.data().oil_c);
  TEST_ASSERT_TRUE(KCan::fresh(k.data().oil_ms, 1000));
}

static void test_coolant_cold_start() {
  // Из лога прогрева: 0x3E = 62, 62-48 = 14 °C
  const uint8_t d[8] = {0x3E, 0xFF, 0, 0, 0, 0, 0, 0};
  k.apply(kcan::ID_ENGINE_DATA, d, 8, 1000);
  TEST_ASSERT_EQUAL_INT16(14, k.data().coolant_c);
}

// --- 0x349 Raw_data_level_tank ---

static void test_fuel_both_tanks_from_real_capture() {
  // HIL, 35098 кадров: FE 01 7E 16 00
  // левый  0x01FE = 510  -> 510 * 0.006249 = 3.19 л
  // правый 0x167E = 5758 -> 5758 * 0.006249 = 35.98 л
  const uint8_t d[5] = {0xFE, 0x01, 0x7E, 0x16, 0x00};
  TEST_ASSERT_TRUE(k.apply(kcan::ID_FUEL_LEVEL, d, 5, 1000));
  TEST_ASSERT_EQUAL_UINT16(31, k.data().fuel_left_l_x10);
  TEST_ASSERT_EQUAL_UINT16(359, k.data().fuel_right_l_x10);
  TEST_ASSERT_EQUAL_UINT16(390, k.fuel_total_l_x10());
}

static void test_fuel_empty() {
  const uint8_t d[4] = {0x00, 0x00, 0x00, 0x00};
  k.apply(kcan::ID_FUEL_LEVEL, d, 4, 1000);
  TEST_ASSERT_EQUAL_UINT16(0, k.fuel_total_l_x10());
}

static void test_fuel_near_full_stays_in_range() {
  // Потолок сигнала по DBC — 62.5 л на датчик.
  const uint8_t d[4] = {0x10, 0x27, 0x10, 0x27};  // 10000 в обоих
  k.apply(kcan::ID_FUEL_LEVEL, d, 4, 1000);
  TEST_ASSERT_EQUAL_UINT16(624, k.data().fuel_left_l_x10);
  TEST_ASSERT_EQUAL_UINT16(1248, k.fuel_total_l_x10());
}

static void test_fuel_short_frame_rejected() {
  const uint8_t d[3] = {0xFE, 0x01, 0x7E};
  TEST_ASSERT_FALSE(k.apply(kcan::ID_FUEL_LEVEL, d, 3, 1000));
}

// --- 0x3B4 PowerBatteryVoltage ---

static void test_battery_from_real_capture() {
  // HIL, 17718 кадров: CB F3 00 FC FF FF FF FF
  // 12 бит: (0xF3 & 0x0F) << 8 | 0xCB = 0x3CB = 971 -> 971 * 15 = 14565 мВ
  const uint8_t d[8] = {0xCB, 0xF3, 0x00, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF};
  TEST_ASSERT_TRUE(k.apply(kcan::ID_BATTERY, d, 8, 1000));
  TEST_ASSERT_EQUAL_UINT16(14565, k.data().volts_mv);
}

static void test_battery_ignores_upper_nibble() {
  // Старший ниббл байта 1 в сигнал не входит и не должен на него влиять.
  const uint8_t a[2] = {0xCB, 0x03};
  const uint8_t b[2] = {0xCB, 0xF3};
  k.apply(kcan::ID_BATTERY, a, 2, 1000);
  const uint16_t first = k.data().volts_mv;
  k.apply(kcan::ID_BATTERY, b, 2, 1000);
  TEST_ASSERT_EQUAL_UINT16(first, k.data().volts_mv);
}

// --- 0x2CA Outside_temperature ---

static void test_outside_temp_from_real_capture() {
  // HIL, 6889 кадров: 6B FF -> 107 * 0.5 - 40 = 13.5 °C
  const uint8_t d[2] = {0x6B, 0xFF};
  TEST_ASSERT_TRUE(k.apply(kcan::ID_OUTSIDE_TEMP, d, 2, 1000));
  TEST_ASSERT_EQUAL_INT16(135, k.data().outside_c_x10);
}

static void test_outside_temp_below_zero() {
  const uint8_t d[2] = {0x28, 0xFF};  // 40 * 0.5 - 40 = -20 °C
  k.apply(kcan::ID_OUTSIDE_TEMP, d, 2, 1000);
  TEST_ASSERT_EQUAL_INT16(-200, k.data().outside_c_x10);
}

// --- общее ---

static void test_unknown_id_ignored() {
  const uint8_t d[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  TEST_ASSERT_FALSE(k.apply(0x1A0, d, 8, 1000));
  TEST_ASSERT_EQUAL_INT16(0, k.data().coolant_c);
}

static void test_staleness() {
  const uint8_t d[8] = {0x8B, 0x88, 0, 0, 0, 0, 0, 0};
  k.apply(kcan::ID_ENGINE_DATA, d, 8, 1000);
  TEST_ASSERT_TRUE(KCan::fresh(k.data().coolant_ms, 2500));
  TEST_ASSERT_FALSE(KCan::fresh(k.data().coolant_ms, 3500));
  TEST_ASSERT_FALSE(KCan::fresh(k.data().fuel_ms, 1000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_engine_data_from_real_capture);
  RUN_TEST(test_oil_temp_absent_marker_not_stored);
  RUN_TEST(test_oil_temp_stored_when_present);
  RUN_TEST(test_coolant_cold_start);
  RUN_TEST(test_fuel_both_tanks_from_real_capture);
  RUN_TEST(test_fuel_empty);
  RUN_TEST(test_fuel_near_full_stays_in_range);
  RUN_TEST(test_fuel_short_frame_rejected);
  RUN_TEST(test_battery_from_real_capture);
  RUN_TEST(test_battery_ignores_upper_nibble);
  RUN_TEST(test_outside_temp_from_real_capture);
  RUN_TEST(test_outside_temp_below_zero);
  RUN_TEST(test_unknown_id_ignored);
  RUN_TEST(test_staleness);
  return UNITY_END();
}
