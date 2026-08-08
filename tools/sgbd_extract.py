#!/usr/bin/env python3
"""Достаёт из SGBD (.prg от SP-Daten) список job'ов и телеграммы запросов.

Зачем. Идентификаторы диагностических данных BMW публично не документированы,
и мы собирались нащупывать их, слушая трафик Tool32. Оказалось, что этого не
нужно: телеграмма лежит прямо в байткоде job'а, её видно в дизассемблере как

    move  S1,{$82.B,$FF.B,$F1.B,$21.B,$0A.B}
                 |    |    |    |    +-- идентификатор данных
                 |    |    |    +------- сервис KWP2000
                 |    |    +------------ адрес тестера
                 |    +----------------- адрес блока, подставляется EDIABAS
                 +---------------------- формат: 0x80 | длина полезной части

Метод проверен на job'е, где идентификатор продублирован в комментарии:
KOMB87 STATUS_TANKINHALT комментирован как "$21 ReadDataByLocalIdentifier,
$0A Tankinhalt" — извлечённая телеграмма 82 FF F1 21 0A совпала.

Использование:
    python3 tools/sgbd_extract.py <файл.prg> [шаблон-имени]

Примеры:
    python3 tools/sgbd_extract.py ".../E89/ecu/GS19D.prg" TEMPERATUR
    python3 tools/sgbd_extract.py ".../E89/ecu/KOMB87.prg" TANK

Требуется декодер BimmerDaten: путь задаётся BIMMERDATEN или лежит рядом.
"""

import os
import re
import sys

BIMMERDATEN = os.environ.get(
    "BIMMERDATEN", "/Users/densh/Work/Develop/BMW/BimmerDaten"
)

TELEGRAM = re.compile(r"move\s+S\d+,\{([^}]*)\}")
BYTE = re.compile(r"\$([0-9A-F]{2})\.B")

# Сервисы KWP2000, которые реально встречаются в этих SGBD.
SERVICES = {
    "21": "ReadDataByLocalIdentifier",
    "22": "ReadDataByCommonIdentifier",
    "2E": "WriteDataByCommonIdentifier",
    "30": "InputOutputControlByLocalIdentifier",
    "31": "StartRoutineByLocalIdentifier",
    "1A": "ReadEcuIdentification",
}


def telegrams(job):
    """Все телеграммы job'а. Короткие отбрасываем: это не запросы."""
    out = []
    for line in job.disassembly:
        m = TELEGRAM.search(line)
        if not m:
            continue
        b = BYTE.findall(m.group(1))
        if len(b) >= 4 and b not in out:
            out.append(b)
    return out


def describe(tel):
    """Пояснение к телеграмме: сервис и идентификатор."""
    if len(tel) < 4:
        return ""
    svc = tel[3]
    name = SERVICES.get(svc, "?")
    ident = " ".join(tel[4:]) if len(tel) > 4 else "-"
    return f"{name} ident={ident}"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    sys.path.insert(0, BIMMERDATEN)
    try:
        from decoderPrg import parse_prg
    except ImportError:
        print(f"не найден decoderPrg.py в {BIMMERDATEN}", file=sys.stderr)
        print("задайте путь через переменную BIMMERDATEN", file=sys.stderr)
        return 1

    path = sys.argv[1]
    pattern = sys.argv[2] if len(sys.argv) > 2 else ""

    prg = parse_prg(path)
    print(f"=== {os.path.basename(path)} — {len(prg.jobs)} job'ов ===")
    if prg.info.author:
        print(f"автор: {prg.info.author}")
    print()

    shown = 0
    for job in prg.jobs:
        if pattern and not re.search(pattern, job.name, re.I):
            continue
        tels = telegrams(job)
        if not tels:
            continue
        shown += 1

        # Первая строка комментария обычно и есть человеческое название.
        note = ""
        for c in job.comments:
            m = re.search(r"JOBCOMMENT:(.+)", c)
            if m and "KWP2000" not in m.group(1):
                note = m.group(1).strip()
                break

        print(f"{job.name}")
        if note:
            print(f"    {note}")
        for t in tels[:3]:
            print(f"    {' '.join(t):<28} {describe(t)}")

        # Результаты с единицами измерения — по ним видно, что вернётся.
        for c in job.comments:
            m = re.search(r"RESULT:(STAT_\S+_WERT)", c)
            if m:
                print(f"    -> {m.group(1)}")
        print()

    if not shown:
        print("ничего не найдено — проверьте шаблон имени")
    return 0


if __name__ == "__main__":
    sys.exit(main())
