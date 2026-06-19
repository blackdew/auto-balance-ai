#!/usr/bin/env python3
"""밸런싱 품질 평가 하버스트 (#6).

`./src/turn_rpg --demo`를 N회 실행(매 실행 baseline CSV로 통제)하면서,
보스 층마다 (1) 튜너의 시뮬 예측 적 승률과 (2) 실제 플레이 승패를 함께 모아
목표(적 승률 40%) 대비 sim-예측 vs real-실측을 비교한다.

소스는 건드리지 않는다 — 바이너리 출력만 파싱한다.
사용: python3 scripts/eval.py [N]   (기본 N=30)
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
# 바이너리는 상대경로 "battle_log.csv"를 읽고 쓴다 → 반드시 cwd=src 에서 실행해
# src/battle_log.csv 를 사용·통제하게 한다(아니면 루트에 빈 CSV가 생겨 cold-start로 오염).
BIN = "./turn_rpg"
CSV = os.path.join(SRC, "battle_log.csv")

AB_RE  = re.compile(r">> AUTO-BALANCE\s+floor\s+(\d+)(\s+\[BOSS\])?")
SIM_RE = re.compile(r"sim\s*:\s*enemy won\s+(\d+)/(\d+)")
TARGET_ENEMY_WINRATE = 0.40


def run_once(baseline):
    shutil.copyfile(baseline, CSV)  # 매 실행 동일 baseline에서 시작 (통제)
    out = subprocess.run([BIN, "--demo"], cwd=SRC, stdin=subprocess.DEVNULL,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, timeout=120).stdout
    return out.splitlines()


def parse(lines, boss_real, boss_sim):
    """상태 기계: AUTO-BALANCE floor M이 다시 나오거나 clear!면 직전 층은 승리,
    lose...면 패배. 보스 층만 집계한다."""
    pend_floor = pend_boss = None
    pend_sim = None

    def resolve(won):
        nonlocal pend_floor, pend_boss, pend_sim
        if pend_floor is not None and pend_boss:
            boss_real[pend_floor].append(0 if won else 1)  # 적 승리=1
            if pend_sim is not None:
                boss_sim[pend_floor].append(pend_sim)
        pend_floor = pend_boss = pend_sim = None

    for ln in lines:
        m = AB_RE.search(ln)
        if m:
            resolve(won=True)              # 새 튜닝 진입 → 직전 층은 이긴 것
            pend_floor = int(m.group(1))
            pend_boss = bool(m.group(2))
            continue
        s = SIM_RE.search(ln)
        if s and pend_boss:
            pend_sim = int(s.group(1)) / int(s.group(2))  # 시뮬 예측 적 승률
            continue
        if "lose..." in ln:
            resolve(won=False)
        elif "clear!" in ln:
            resolve(won=True)
    # 미해결 pending(데모 상한 도달 등)은 버린다


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    if not os.path.exists(os.path.join(SRC, "turn_rpg")):
        sys.exit("binary 없음 — 먼저 `make`")

    with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as tf:
        baseline = tf.name
    shutil.copyfile(CSV, baseline)

    boss_real = defaultdict(list)  # floor -> [적승리 0/1, ...] (실측)
    boss_sim  = defaultdict(list)  # floor -> [시뮬예측 적승률, ...]
    try:
        for i in range(n):
            parse(run_once(baseline), boss_real, boss_sim)
    finally:
        shutil.copyfile(baseline, CSV)  # baseline 복원
        os.unlink(baseline)

    floors = sorted(boss_real)
    lines = []
    lines.append(f"# 밸런싱 평가 리포트 — `--demo` {n}회 (baseline CSV 고정)\n")
    lines.append("보스 층: 튜너의 **시뮬 예측 적 승률** vs **실제 플레이 적 승률**, 목표=40%.\n")
    lines.append("| 보스층 | 시도 | sim 예측 적승률 | real 실측 적승률 | 목표 | sim−real 격차 |")
    lines.append("|-------:|-----:|----------------:|-----------------:|-----:|--------------:|")
    tot_sim = tot_real = tot_n = 0.0
    for f in floors:
        real = boss_real[f]
        sim = boss_sim[f]
        rr = sum(real) / len(real) if real else 0.0
        sr = sum(sim) / len(sim) if sim else 0.0
        tot_real += sum(real); tot_n += len(real); tot_sim += sr * len(real)
        lines.append(f"| {f} | {len(real)} | {sr*100:4.0f}% | {rr*100:4.0f}% | "
                     f"40% | {(sr-rr)*100:+4.0f}pp |")
    if tot_n:
        gap = (tot_sim - tot_real) / tot_n
        lines.append(f"\n**전체 보스**: real 적승률 {tot_real/tot_n*100:.0f}% "
                     f"(목표 40%), sim 예측 평균 {tot_sim/tot_n*100:.0f}%, "
                     f"sim−real 격차 {gap*100:+.0f}pp · 표본 {int(tot_n)}전")
        lines.append("\n> sim이 목표(40%)에 가까운데 real이 그보다 낮으면(=플레이어가 더 잘 이김), "
                     "튜너가 *시뮬 속 플레이어*에 맞춰 정한 난이도가 *실제 플레이어*에겐 너무 쉽다는 뜻 "
                     "— 실/시뮬 로직 격차의 증거(#7).")
    report = "\n".join(lines) + "\n"
    print(report)
    out_path = os.path.join(ROOT, "docs", "eval-report.md")
    with open(out_path, "w") as fp:
        fp.write(report)
    print(f"[written] {os.path.relpath(out_path, ROOT)}")


if __name__ == "__main__":
    main()
