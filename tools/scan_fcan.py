#!/usr/bin/env python3
"""Сплошной поиск по SP-Daten: job'ы, читающие широковещательные кадры CAN.

У некоторых блоков есть job'ы вида STATUS_F_CAN_*, которые возвращают
содержимое чужого шинного сообщения, уже разобранное на именованные поля.
Идентификатор запроса у них равен CAN-ID кадра:

    STATUS_F_CAN_TASTER_AUDIO_TEL   22 01 D6   ->  кадр 0x1D6, шесть кнопок руля

Это описание кадра со стороны изготовителя, а не корреляция по логам, —
поэтому такие job'ы ценны для наполнения DBC.

Критерии отбора. Сервис 0x22 (ReadDataByCommonIdentifier) используется не
только для чтения кадров, но и для блоков кодирования (0x3000-0x3EFF), так
что одного идентификатора мало. Отсюда два уровня:

  ВЫСОКИЙ  — идентификатор попадает в диапазон CAN 0x000-0x7FF И в
             комментарии есть Botschaft / Nachricht / F_CAN
  НИЗКИЙ   — только диапазон; сюда попадает и посторонее, нужен глазами

Использование:
    python3 tools/scan_fcan.py <каталог с .prg> [файл-отчёта.md]
"""

import os
import re
import sys
from collections import defaultdict

BIMMERDATEN = os.environ.get(
    "BIMMERDATEN", "/Users/densh/Work/Develop/BMW/BimmerDaten"
)

TELEGRAM = re.compile(r"move\s+S\d+,\{([^}]*)\}")
BYTE = re.compile(r"\$([0-9A-F]{2})\.B")
BUS_WORD = re.compile(r"botschaft|nachricht|F_?CAN", re.I)

# Блоки кодирования читаются тем же сервисом 0x22 — отсекаем по диапазону.
CODING_LO, CODING_HI = 0x3000, 0x3EFF


def telegrams(job):
    out = []
    for line in job.disassembly:
        m = TELEGRAM.search(line)
        if m:
            b = BYTE.findall(m.group(1))
            if len(b) >= 4:
                out.append(b)
    return out


def note_of(job):
    for c in job.comments:
        m = re.search(r"JOBCOMMENT:(.+)", c)
        if m and "KWP" not in m.group(1):
            return m.group(1).strip()
    return ""


def results_of(job):
    out = []
    for c in job.comments:
        m = re.match(r"RESULT:(\S+)", c)
        if m and not m.group(1).startswith("_"):
            out.append(m.group(1))
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    ecu_dir = sys.argv[1]
    report = sys.argv[2] if len(sys.argv) > 2 else None

    sys.path.insert(0, BIMMERDATEN)
    from decoderPrg import parse_prg

    files = sorted(f for f in os.listdir(ecu_dir) if f.lower().endswith(".prg"))
    strong, weak = [], []
    by_id = defaultdict(set)
    errors = 0

    for i, fn in enumerate(files, 1):
        if i % 50 == 0:
            print(f"  ... {i}/{len(files)}", file=sys.stderr)
        try:
            prg = parse_prg(os.path.join(ecu_dir, fn))
        except Exception:
            errors += 1
            continue

        ecu = fn[:-4]
        for job in prg.jobs:
            note = note_of(job)
            hay = job.name + " " + " ".join(job.comments)
            for t in telegrams(job):
                if t[3] != "22" or len(t) < 6:
                    continue
                ident = int(t[4] + t[5], 16)
                if ident > 0x7FF or CODING_LO <= ident <= CODING_HI:
                    continue
                row = (ident, ecu, job.name, len(results_of(job)), note[:60])
                if BUS_WORD.search(hay):
                    strong.append(row)
                    by_id[ident].add(ecu)
                else:
                    weak.append(row)
                break

    strong.sort()
    weak.sort()

    lines = []
    lines.append(f"Просмотрено файлов: {len(files)}, ошибок разбора: {errors}")
    lines.append(f"Уверенных совпадений: {len(strong)}, "
                 f"уникальных кадров: {len(by_id)}")
    lines.append(f"Слабых (только по диапазону): {len(weak)}")
    lines.append("")
    lines.append("## Кадры, описанные заводом")
    lines.append("")
    lines.append("| CAN ID | Блок | Job | Полей | Комментарий |")
    lines.append("|---|---|---|---|---|")
    for ident, ecu, name, nres, note in strong:
        lines.append(f"| `0x{ident:03X}` | {ecu} | `{name}` | {nres} | {note} |")

    out = "\n".join(lines)
    print(out)
    if report:
        with open(report, "w") as f:
            f.write(out + "\n")
        print(f"\nотчёт записан: {report}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
