// Корпус бортового дисплея BMW E90 — параметрическая модель для печати.
//
// Рендер и экспорт:
//   openscad enclosure.scad                              — открыть в GUI
//   openscad -D 'part="shell"' -o shell.stl enclosure.scad
//   openscad -D 'part="base"'  -o base.stl  enclosure.scad
//
// Печать: обе детали лицом вниз, PETG или ASA, поддержки не нужны.
// Подробности — в docs/enclosure.md, распайка — в docs/wiring.md.
//
// СИСТЕМА КООРДИНАТ
//   X — ширина, 0 слева
//   Y — глубина, 0 спереди (к водителю), box_d сзади (к стеклу)
//   Z — высота, 0 — плоскость стыка корпуса и донной пластины
//
// ВАЖНО: габариты модулей типовые. Промерьте свои платы штангенциркулем и
// поправьте параметры — особенно активную зону дисплея, она смещена от
// центра платы, и вырез под USB, у клонов Nano разъём гуляет от партии.

part = "both";   // "shell" | "base" | "both"

$fn = 64;
eps = 0.01;

/* ================= ОСНОВНЫЕ РАЗМЕРЫ ================= */

box_w   = 84;    // ширина
box_d   = 62;    // глубина
box_h   = 26;    // полная высота вместе с донной пластиной
wall    = 2.0;   // боковые стенки
lid_t   = 3.0;   // крышка
base_t  = 2.0;   // донная пластина

shell_h  = box_h - base_t;      // 24 — высота корпуса без дна
cavity_h = shell_h - lid_t;     // 21 — свободная высота внутри

/* ================= КРЕПЁЖ ================= */

boss_od    = 6.0;    // наружный диаметр бобышки
boss_pilot = 2.6;    // отверстие под самонарез M3
boss_inset = 5.5;    // от края корпуса до центра бобышки
base_clear = 3.2;    // отверстие в донной пластине под M3

boss_pos = [
  [boss_inset,         boss_inset],
  [box_w - boss_inset, boss_inset],
  [boss_inset,         box_d - boss_inset],
  [box_w - boss_inset, box_d - boss_inset]
];

/* ================= ДИСПЛЕЙ ================= */

// Плата OLED утоплена в карман в крышке, окно прорезано под активную зону.
// Над карманом остаётся lid_t - oled_pocket = 1.2 мм.
oled_pcb    = 27.6;   // сторона платы плюс зазор
oled_pocket = 1.8;
oled_cx     = 34;
oled_cy     = 33;

win_w  = 23.0;        // окно чуть больше активной зоны 21.7 x 10.9
win_h  = 12.5;
win_dx = 0;           // смещение окна относительно центра ПЛАТЫ
win_dy = 1.0;         // активная зона смещена — ОБЯЗАТЕЛЬНО ПРОМЕРИТЬ

/* ================= СВЕТОДИОД И ЗУММЕР ================= */

led_d = 5.0;
led_x = 62.5;
led_y = 39.5;

// Решётка ровно над зуммером, который лежит на дне в зоне x 30..49, y 9.5..24.5
buzz_hole_d = 2.0;
buzz_x = 39.5;
buzz_y = 17.0;

/* ================= СЕНСОРНАЯ КНОПКА ================= */

// Лунка под палец СНАРУЖИ. Она же и есть утоньшение: отверстия нет,
// под пальцем остаётся lid_t - dimple_depth = 1.2 мм.
// Геометрия — усечённый конус с фасками 45°, а не сфера: сферическая лунка
// при печати крышкой вниз даёт по кромке нависание около 70° и край плывёт.
dimple_d     = 16.0;
dimple_depth = 1.8;
dimple_x     = 11;
dimple_y     = 31.5;

/* ================= ВЫВОДЫ НАРУЖУ ================= */

// Ввод стоит в промежутке между MCP2515 и Nano — там же внутри стойка
// разгрузки, и жгуту есть куда лечь.
gland_d = 7.0;
gland_x = 53;
gland_z = 12;

usb_w  = 12;          // вырез под USB-C
usb_h  = 7;
usb_x  = 66.5;        // центр по X
usb_z0 = 3.5;         // от плоскости дна

/* ================= РАЗГРУЗКА ЖГУТА ================= */

sr_x = 50;   // стойка со сквозным пазом под стяжку
sr_y = 52;
sr_w = 6;
sr_d = 4;
sr_h = 8;

/* ================= ПОДСТАВКИ ПОД NANO ================= */

// Плата поднята, чтобы разъём USB-C попал в вырез в задней стенке.
nano_pad_h = 3.0;
nano_pad_d = 5.0;
nano_pads = [[59.5, 15], [72.5, 15], [59.5, 56], [72.5, 56]];

/* ================= ВЕНТИЛЯЦИЯ ================= */

// Прорези в дне под Mini360 (x 7.5..24.5, y 13.5..24.5) — единственный узел,
// который заметно греется.
vent_w  = 12;
vent_h  = 2;
vent_x  = 10;
vent_ys = [15, 19, 23];

/* =====================================================
   ВСПОМОГАТЕЛЬНОЕ
   ===================================================== */

// Каплевидное отверстие: круг с надстройкой под 45° сверху. Обычное круглое
// отверстие в вертикальной стенке нависает в верхней точке и требует
// поддержек; каплевидное печатается само.
module teardrop_2d(r) {
  union() {
    circle(r = r);
    polygon(points = [
      [-r * 0.7071, r * 0.7071],
      [ r * 0.7071, r * 0.7071],
      [ 0,          r * 1.4142]
    ]);
  }
}

module boss(h) {
  difference() {
    cylinder(h = h, d = boss_od);
    translate([0, 0, -eps]) cylinder(h = h + 2 * eps, d = boss_pilot);
  }
}

module strain_relief() {
  difference() {
    translate([sr_x, sr_y, 0]) cube([sr_w, sr_d, sr_h]);
    // сквозной паз под стяжку
    translate([sr_x + sr_w / 2 - 1.5, sr_y - 1, 3])
      cube([3, sr_d + 2, 4]);
  }
}

/* =====================================================
   КОРПУС: крышка со стенками, снизу открыт
   ===================================================== */

module shell() {
  // Бобышки и стойка добавляются ПОСЛЕ выборки полости — иначе полость
  // срезала бы их подчистую.
  union() {
    difference() {
      cube([box_w, box_d, shell_h]);

      // внутренняя полость, открытая вниз
      translate([wall, wall, -eps])
        cube([box_w - 2 * wall, box_d - 2 * wall, cavity_h + eps]);

      // --- карман под плату дисплея, снизу ---
      translate([oled_cx - oled_pcb / 2, oled_cy - oled_pcb / 2, cavity_h - eps])
        cube([oled_pcb, oled_pcb, oled_pocket + eps]);

      // --- окно дисплея, насквозь ---
      translate([oled_cx + win_dx - win_w / 2,
                 oled_cy + win_dy - win_h / 2,
                 cavity_h - eps])
        cube([win_w, win_h, lid_t + 2 * eps]);

      // --- отверстие под светодиод ---
      translate([led_x, led_y, cavity_h - eps])
        cylinder(h = lid_t + 2 * eps, d = led_d);

      // --- решётка зуммера ---
      for (o = [[-5, 0], [0, 0], [5, 0], [-2.5, -4.5], [2.5, -4.5]])
        translate([buzz_x + o[0], buzz_y + o[1], cavity_h - eps])
          cylinder(h = lid_t + 2 * eps, d = buzz_hole_d);

      // --- лунка под палец, снаружи ---
      translate([dimple_x, dimple_y, shell_h - dimple_depth])
        cylinder(h = dimple_depth,
                 d1 = dimple_d - 2 * dimple_depth,   // фаска 45°
                 d2 = dimple_d);
      translate([dimple_x, dimple_y, shell_h - eps])
        cylinder(h = 1, d = dimple_d);

      // --- кабельный ввод в задней стенке ---
      translate([gland_x, box_d + 2, gland_z])
        rotate([90, 0, 0])
          linear_extrude(height = wall + 4)
            teardrop_2d(gland_d / 2);

      // --- вырез под USB-C в задней стенке ---
      translate([usb_x - usb_w / 2, box_d - wall - 1, usb_z0])
        cube([usb_w, wall + 2, usb_h]);
    }

    for (p = boss_pos)
      translate([p[0], p[1], 0]) boss(cavity_h);

    strain_relief();
  }
}

/* =====================================================
   ДОННАЯ ПЛАСТИНА
   ===================================================== */

module base_plate() {
  difference() {
    union() {
      translate([0, 0, -base_t])
        cube([box_w, box_d, base_t]);

      // подставки под Nano поднимают плату к вырезу USB
      for (p = nano_pads)
        translate([p[0], p[1], 0])
          cylinder(h = nano_pad_h, d = nano_pad_d);
    }

    for (p = boss_pos)
      translate([p[0], p[1], -base_t - eps])
        cylinder(h = base_t + 2 * eps, d = base_clear);

    for (y = vent_ys)
      translate([vent_x, y, -base_t - eps])
        cube([vent_w, vent_h, base_t + 2 * eps]);
  }
}

/* =====================================================
   ВЫВОД
   ===================================================== */

if (part == "shell") shell();
else if (part == "base") base_plate();
else {
  shell();
  translate([0, box_d + 12, 0]) base_plate();
}
