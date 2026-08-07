// Корпус бортового дисплея BMW E90 / CAN — параметрическая модель для печати.
// Только печатаемые детали: корпус (крышка со стенками) и донная пластина.
//
// Экспорт STL для OrcaSlicer:
//   openscad -D 'part="shell"' -o shell.stl enclosure.scad
//   openscad -D 'part="base"'  -o base.stl  enclosure.scad
//
// Печать: обе детали лицом вниз (крышка — наружной поверхностью на стол),
// PETG или ASA, поддержки не нужны. Гравировка, лунка, воронка окна и
// каплевидный ввод рассчитаны именно под эту ориентацию.
//
// СИСТЕМА КООРДИНАТ
//   X — ширина, 0 слева
//   Y — глубина, 0 спереди (к водителю), box_d сзади (разъёмы)
//   Z — высота, 0 — плоскость стыка корпуса и донной пластины
//
// ВАЖНО: габариты модулей типовые. Промерьте свои платы штангенциркулем.
// Особенно: активная зона OLED смещена от центра платы, и вырез под USB —
// у клонов Nano разъём гуляет от партии к партии.

part = "both";   // "shell" | "base" | "both"

$fn = 72;
eps = 0.01;

/* ================= ОСНОВНЫЕ РАЗМЕРЫ ================= */

box_w  = 72;
box_d  = 54;
box_h  = 22;     // полная высота вместе с донной пластиной
wall   = 2.0;
lid_t  = 3.0;
base_t = 2.0;
corner = 5.0;    // радиус вертикальных углов

shell_h  = box_h - base_t;      // 20
cavity_h = shell_h - lid_t;     // 17

/* ================= КРЕПЁЖ ================= */

boss_od    = 6.0;
boss_pilot = 2.6;    // под самонарез M3
boss_inset = 5.5;
base_clear = 3.2;

boss_pos = [
  [boss_inset,         boss_inset],
  [box_w - boss_inset, boss_inset],
  [boss_inset,         box_d - boss_inset],
  [box_w - boss_inset, box_d - boss_inset]
];

/* ================= ПЛАТЫ (справочно, для проверки коллизий) =========
   Nano      18 x 45, x  9..27, y  4..49, USB к задней стенке
   MCP2515   28 x 40, x 27..55, y  6..46
   Mini360   11 x 17, x 57..68, y 30..47
   KY-006    15 x 18.5, x 55..70, y 10..28.5
   OLED      27.5 x 27.5, к крышке, центр (30, 28)
   TTP223    15 x 11, к крышке, центр (14, 11)
   WS2812B   10 x 10, к крышке, центр (45, 8)
   ==================================================================== */

nano_holes = [[10.4, 4.9], [25.6, 4.9], [10.4, 48.1], [25.6, 48.1]];
can_holes  = [[29.75, 8.75], [52.25, 8.75], [29.75, 43.25], [52.25, 43.25]];
pcb_pad_h  = 3.0;    // высота стоек под платы
pcb_pad_d  = 5.0;

// клеевые площадки под модули без отверстий
glue_pads = [[62.5, 33], [62.5, 44],      // Mini360
             [62.5, 13], [62.5, 25.5]];   // KY-006

/* ================= ДИСПЛЕЙ ================= */

oled_cx = 30;
oled_cy = 28;
oled_pcb    = 27.6;   // сторона платы плюс зазор
oled_pocket = 1.4;    // карман в крышке под толщину платы

// Окно-воронка: снаружи шире, внутри по активной зоне. Скос ~40° убирает
// толщину крышки из поля зрения и печатается крышкой вниз без поддержек.
win_w  = 23.0;        // внутри, активная зона 21.7 x 10.9 плюс зазор
win_h  = 12.5;
win_flare = 2.5;      // расширение на сторону к наружной поверхности
win_dy = 1.0;         // активная зона смещена — ПРОМЕРИТЬ

// прижимные лапки, держат плату OLED в кармане
clamp_d = 3.4;
clamp_h = 2.4;

/* ================= СЕНСОРНАЯ КНОПКА ================= */

// Лунка под палец СНАРУЖИ, она же утоньшение для TTP-223: отверстия нет,
// под пальцем остаётся lid_t - dimple_depth = 1.2 мм. Геометрия —
// усечённый конус с фаской 45°, а не сфера: сферическая лунка при печати
// крышкой вниз даёт по кромке нависание около 70° и край плывёт.
dimple_d     = 15.0;
dimple_depth = 1.8;
dimple_x     = 14;
dimple_y     = 11;

touch_text   = "TOUCH";
touch_size   = 3.0;
engrave_d    = 0.4;   // глубина гравировки

/* ================= НАДПИСИ НА КРЫШКЕ ================= */

label_1 = "BMW E90";
label_2 = "CAN BUS";
label_x = 32;
label_1_y = 13;
label_2_y = 6.5;
label_1_size = 5.0;
label_2_size = 3.6;

/* ================= СВЕТОДИОД И ЗУММЕР ================= */

led_d = 5.0;
led_x = 45;
led_y = 8;

// Решётка: три концентрические дуги отверстий вместо россыпи точек.
buzz_x = 62.5;
buzz_y = 19;
buzz_hole_d = 1.6;
buzz_rings  = [[4.0, 6], [7.2, 10], [10.4, 14]];
buzz_span   = 140;    // угловой раствор веера, градусов

/* ================= ВЫВОДЫ НАРУЖУ ================= */

gland_d = 7.0;        // кабельный ввод, задняя стенка
gland_x = 45;
gland_z = 9;

usb_w  = 12;          // вырез под USB-C, задняя стенка
usb_h  = 7;
usb_x  = 18;
usb_z0 = 3.5;

/* ================= РАЗГРУЗКА ЖГУТА ================= */

sr_x = 42;
sr_y = 46;
sr_w = 6;
sr_d = 4;
sr_h = 9;

/* ================= ВЕНТИЛЯЦИЯ ================= */

// Сквозная тяга через узел Mini360: приток через дно, выброс через правую
// стенку. Щели только в дне не работают — воздуху некуда уходить.
vent_slot_w = 2.2;    // прорези в правой стенке
vent_slot_h = 8;
vent_slot_z = 6;
vent_wall_ys = [32, 36, 40, 44];

vent_floor_w  = 10;   // щели в дне
vent_floor_h  = 2;
vent_floor_x  = 57.5;
vent_floor_ys = [34, 38, 42];

/* =====================================================
   ВСПОМОГАТЕЛЬНОЕ
   ===================================================== */

// Скруглённая по вертикальным углам плита.
module rounded_slab(w, d, h, r) {
  hull() for (x = [r, w - r], y = [r, d - r])
    translate([x, y, 0]) cylinder(h = h, r = r);
}

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
    translate([sr_x + sr_w / 2 - 1.5, sr_y - 1, 3.5])
      cube([3, sr_d + 2, 4]);
  }
}

// Гравировка на наружной поверхности крышки: текст утоплен на engrave_d.
// Печатается крышкой вниз идеально; накладной текст лёг бы в стол.
module lid_engrave(txt, size, x, y, z_top) {
  translate([x, y, z_top - engrave_d])
    linear_extrude(height = engrave_d + eps)
      text(txt, size = size, halign = "center", valign = "center",
           font = "DejaVu Sans:style=Bold", $fn = 32);
}

/* =====================================================
   КОРПУС: крышка со стенками, снизу открыт
   ===================================================== */

module shell() {
  union() {
    difference() {
      rounded_slab(box_w, box_d, shell_h, corner);

      // --- внутренняя полость, открытая вниз ---
      translate([wall, wall, -eps])
        rounded_slab(box_w - 2 * wall, box_d - 2 * wall,
                     cavity_h + eps, max(corner - wall, 0.5));

      // --- карман под плату дисплея ---
      translate([oled_cx - oled_pcb / 2, oled_cy - oled_pcb / 2, cavity_h - eps])
        cube([oled_pcb, oled_pcb, oled_pocket + eps]);

      // --- окно дисплея: воронка, наружу расширяется ---
      translate([oled_cx, oled_cy + win_dy, cavity_h - eps])
        linear_extrude(height = lid_t + 2 * eps, scale = [
          (win_w + 2 * win_flare) / win_w,
          (win_h + 2 * win_flare) / win_h
        ])
          square([win_w, win_h], center = true);

      // --- отверстие под WS2812B ---
      translate([led_x, led_y, cavity_h - eps])
        cylinder(h = lid_t + 2 * eps, d = led_d);

      // --- решётка зуммера: концентрические дуги ---
      for (ring = buzz_rings)
        for (i = [0 : ring[1] - 1]) {
          a = -buzz_span / 2 + buzz_span * i / (ring[1] - 1);
          translate([buzz_x + sin(a) * ring[0],
                     buzz_y - cos(a) * ring[0],
                     cavity_h - eps])
            cylinder(h = lid_t + 2 * eps, d = buzz_hole_d);
        }

      // --- лунка под палец, снаружи ---
      translate([dimple_x, dimple_y, shell_h - dimple_depth])
        cylinder(h = dimple_depth + eps,
                 d1 = dimple_d - 2 * dimple_depth,   // фаска 45°
                 d2 = dimple_d);

      // --- гравировка ---
      lid_engrave(touch_text, touch_size, dimple_x, dimple_y,
                  shell_h - dimple_depth);
      lid_engrave(label_1, label_1_size, label_x, label_1_y, shell_h);
      lid_engrave(label_2, label_2_size, label_x, label_2_y, shell_h);

      // --- кабельный ввод в задней стенке ---
      translate([gland_x, box_d + 2, gland_z])
        rotate([90, 0, 0])
          linear_extrude(height = wall + 4)
            teardrop_2d(gland_d / 2);

      // --- вырез под USB-C в задней стенке ---
      translate([usb_x - usb_w / 2, box_d - wall - 1, usb_z0])
        cube([usb_w, wall + 2, usb_h]);

      // --- вытяжные прорези в правой стенке над Mini360 ---
      for (y = vent_wall_ys)
        translate([box_w - wall - 1, y - vent_slot_w / 2, vent_slot_z])
          cube([wall + 2, vent_slot_w, vent_slot_h]);
    }

    // бобышки под винты
    for (p = boss_pos)
      translate([p[0], p[1], 0]) boss(cavity_h);

    // лапки, прижимающие плату OLED к карману
    for (dx = [-1, 1], dy = [-1, 1])
      translate([oled_cx + dx * (oled_pcb / 2 - clamp_d / 2 - 0.4),
                 oled_cy + dy * (oled_pcb / 2 - clamp_d / 2 - 0.4),
                 cavity_h - clamp_h])
        cylinder(h = clamp_h, d = clamp_d);

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
        rounded_slab(box_w, box_d, base_t, corner);

      for (p = concat(nano_holes, can_holes))
        translate([p[0], p[1], 0]) cylinder(h = pcb_pad_h, d = pcb_pad_d);

      for (p = glue_pads)
        translate([p[0], p[1], 0]) cylinder(h = 1.2, d = 6);
    }

    for (p = boss_pos)
      translate([p[0], p[1], -base_t - eps])
        cylinder(h = base_t + 2 * eps, d = base_clear);

    for (y = vent_floor_ys)
      translate([vent_floor_x, y - vent_floor_h / 2, -base_t - eps])
        cube([vent_floor_w, vent_floor_h, base_t + 2 * eps]);
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
