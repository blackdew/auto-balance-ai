# Auto-Balance AI — 자동 난이도 조정 턴제 RPG

[![build](https://github.com/blackdew/auto-balance-ai/actions/workflows/build.yml/badge.svg)](https://github.com/blackdew/auto-balance-ai/actions/workflows/build.yml)

> 학생 캡스톤 제출 프로젝트 — "AI 게임 마스터(GM)"가 플레이어의 실력에 맞춰 **적 스탯을 실시간으로 자동 밸런싱**하는 콘솔 턴제 RPG
> 분석 정리: 2026-06-19

> **In one line (EN):** A console turn-based RPG where an "AI game master" auto-balances each floor's enemy stats to the player's skill — it learns the player's behavior from a battle log via KNN, runs 50 Monte-Carlo simulated fights per candidate, and picks the enemy stats closest to a target difficulty (40% enemy win-rate for bosses, a floor-scaled HP-drain for regular enemies). C++17, header-only, zero external dependencies.

플레이어가 한 층을 클리어할 때마다, **다음 층 적의 공격력/방어력/체력을 시뮬레이션으로 미리 조정**해 항상 적절한 난이도(보스는 적 승률 40%, 일반 몬스터는 층수 비례 HP 소모)를 유지하는 턴제 RPG. 일반 게임이 적 스탯을 고정 테이블로 쓰는 것과 달리, 이 프로젝트는 **플레이어의 실제 플레이 로그(CSV)를 학습한 KNN AI로 플레이어 행동을 모사**해 50회 가상 전투를 돌리고, 목표 난이도에 가장 근접한 적 스탯을 골라낸다. 즉 "auto balance"의 대상은 **게임 난이도(적 스탯)** 다.

---

## 한눈에 보기

| 항목 | 내용 |
|------|------|
| **유형** | 콘솔(CLI) 턴제 RPG + 자동 난이도 조정 시뮬레이터 (C++17) |
| **핵심 가치** | 플레이어 실력에 맞춰 다음 층 적 스탯을 시뮬레이션으로 자동 밸런싱 |
| **핵심 알고리즘** | KNN(k=5) 플레이어 행동 예측 + 몬테카를로 시뮬(50회) 기반 스탯 튜닝 |
| **기술 스택** | C++17 헤더-온리 설계, `std::async` 병렬 시뮬, `<random>` mt19937, 표준 라이브러리만 (외부 의존성 0) |
| **데이터** | `battle_log.csv` — 플레이어 행동 로그(10피처 × 170행). 실시간 누적 + 학습 입력 |
| **빌드** | Visual Studio 2022 (`ai_gm.sln`) 또는 `g++ -std=c++17` 단일 컴파일 |
| **상태** | `(완료)` 제출본 — 20층 클리어까지 동작하는 완성된 게임 루프 |

---

## 실행 방법

외부 라이브러리가 없어 표준 C++17 컴파일러만 있으면 됩니다.

```bash
# 방법 1) Makefile (권장)
make          # src/turn_rpg 생성
make run      # 대화형 플레이
make demo     # 자동 밸런싱 AI 가시화 데모 (비대화형, 입력 없이 자동 진행)

# 방법 2) 단일 컴파일
cd src && g++ -std=c++17 -O2 -Wall turn_rpg.cpp -o turn_rpg && ./turn_rpg

# 방법 3) Visual Studio 2022 — vs_project/ai_gm.sln 열기
#   (단, .vcxproj는 소스를 .. 상위 경로로 참조하던 원본이므로,
#    src/에 모인 현재 구조에서는 make / g++ 빌드가 가장 간단)
```

### 데모 모드 — `make demo`

자동 밸런싱은 평소 백그라운드에서 돌아 **플레이 중엔 보이지 않는다.** `--demo`는 플레이어를 (시뮬과 동일한) KNN 두뇌로 자동 플레이시키면서, 매 층 튜너가 내린 결정을 출력해 이 과정을 드러낸다:

```
  >> AUTO-BALANCE  floor 5  [BOSS]
     enemy : Warlord [BERSERKER]  w(atk/def/hp)=0.6/0.1/0.3
     AI    : 20 stat candidates x 50 simulated battles (KNN-predicted player)
     target: enemy win-rate ~40%
     chosen: atk 6->9  def 6->6  hp 18->20
```

전체 실행 발췌는 [docs/demo-sample.txt](docs/demo-sample.txt) 참조. (튜너가 난수를 쓰므로 수치는 실행마다 달라진다.)

> 실행 시 작업 디렉터리에 `battle_log.csv`가 없으면 헤더만 있는 빈 파일을 생성한다(`csv_init`). 동봉된 `src/battle_log.csv`(170행)를 함께 두면 첫 판부터 학습된 AI로 시작한다. **게임을 진행하면 이 CSV에 행동이 계속 append** 되어 AI가 점점 플레이어를 닮아간다.

조작: 매 턴 `1.공격 / 2.방어 / 3.스킬` 중 선택. 승리하면 레벨업(스킬 선택 + 스탯 포인트 분배), 패배하면 1층으로 리셋. 5/10/15/20층은 보스전이며 20층 클리어 시 `clear!`.

---

## 파일 구조

```
auto-balance-ai/
├── README.md                  ← 이 분석 문서
├── src/                       ← C++ 소스 전체 (헤더-온리 + 진입점)
│   ├── turn_rpg.cpp           ← 진입점 main(): 게임 루프 + 층 진행 + async 튜닝 호출
│   ├── ai.h                   ← ★ 핵심: KNN 행동예측 + auto_tune_enemy 몬테카를로 튜너
│   ├── combat.h               ← 한 턴 전투 해석(act) + 플레이어 5종 스킬 + 시뮬용 스킬
│   ├── enemy_types.h          ← ★ 적 5종 + 보스 4종 정의, 메커니즘(광폭/거북/흡혈/장갑)
│   ├── character.h            ← Character 클래스 + BattleStats(행동 누적 통계)
│   ├── skill.h                ← 플레이어 스킬 5종 enum/테이블
│   ├── levelup.h              ← 레벨업: 스킬 습득 + 스탯 포인트 분배 UI
│   ├── battle_log.h           ← CSV 로그 입출력(LogRow, csv_log/csv_load)
│   ├── sim_rand.h             ← 시뮬 스레드 전용 thread_local mt19937 난수
│   └── battle_log.csv         ← 동봉 학습 데이터 (10피처 × 170행)
└── vs_project/                ← Visual Studio 빌드 참고 파일
    ├── ai_gm.sln
    ├── ai_gm.vcxproj
    ├── ai_gm.vcxproj.filters.txt
    └── claude-build-commands.json  ← 원작자의 빌드 명령 메모(.claude 설정)
```

원본 zip에는 빌드 산출물(`.exe`/`.pdb`/`.obj`/`.ilk`)과 Visual Studio 캐시(`.vs/`, 약 90MB)가 포함돼 있었으나, 소스가 아니므로 제외했다(전체 130MB → 96KB).

---

## 아키텍처 / 동작 원리

### 1. 전체 게임 루프 — `turn_rpg.cpp`
`main()`이 1~20층을 진행한다. 각 층은 `while (enemy.hp>0 && player.hp>0)` 전투 루프이며, 승리 시:
1. 다음 층 적 타입/기준 스탯을 **확정**하고,
2. `std::async(launch::async, auto_tune_enemy, …)`로 **백그라운드 시뮬레이션을 시작**한 뒤,
3. 그 사이 플레이어는 레벨업(스킬·스탯 분배)을 진행하고,
4. `fut.get()`으로 튜닝된 적 스탯을 받아 다음 전투에 적용한다.

→ "플레이어가 레벨업 메뉴를 보는 시간"을 시뮬레이션 연산에 겹쳐 쓰는 **체감 지연 은닉**이 설계 의도다.

### 2. AI 자동 밸런싱의 두뇌 — `ai.h`
이 파일이 프로젝트의 핵심 기여다.

**(a) `predict_action()` — KNN 플레이어 행동 예측**
현재 전투 상태(플레이어/적 HP, atk·def 차이, 양쪽 rev 게이지)를 피처로 만들고, `battle_log.csv`의 과거 행동 로그와 **정규화 유클리드 거리**를 계산해 가장 가까운 `K=5`개 이웃의 행동(공격/방어/스킬) 빈도로 다음 행동을 확률적으로 결정한다. 로그가 비면 누적 통계(`BattleStats`) 비율로 폴백. 스킬이 없는 상태면 스킬 투표를 공격/방어에 비례 배분한다.

**(b) `auto_tune_enemy()` — 몬테카를로 스탯 튜너**
- 적 타입 가중치(`atk_w/def_w/hp_w`)에 따라 레벨만큼의 스탯 포인트를 분배하고 랜덤 편차를 준 **후보 스탯 20개**를 생성(`MAX_ATTEMPTS`).
- 각 후보로 **50회 가상 전투**(`SIM_ROUNDS`)를 돌린다. 가상 전투의 플레이어는 위 KNN으로 행동을 고르므로 *실제 플레이어 성향이 반영된다*.
- 목표:
  - **보스(`WIN_RATE`)**: 적 승률 40%에 가장 근접.
  - **일반(`AVG_DAMAGE`)**: 플레이어 평균 HP 소모량이 `max_hp × min(0.8, 0.3+0.025×층)` 에 근접(저층 30% → 고층 약 77.5%, 상한 80%).
- 목표와의 오차(`score`)가 최소인 후보 스탯을 채택. 즉, **난이도 곡선을 데이터로 역설계**한다.

### 3. 전투 해석 — `combat.h`
한 턴은 (플레이어 선택 × 적 선택)의 가위바위보형 상호작용이다.
- 공격 vs 공격: 양쪽 피해, 최소 1 보장.
- 공격 vs 방어: 방어자는 피해 경감(`def×1.5`) + 공격력의 1/3을 **rev(복수 게이지)** 로 축적.
- 방어 vs 공격: 방어자가 rev 축적. 모두 방어면 피해 0.
- **플레이어 스킬 5종**: RevAtk(rev 전량 강공), Counter(방어 무시), PowerDef(방어 2배), DoubleAtk(2연타), Heal(rev/2 회복, 이후 rev 축적 절반). `sim_use_skill()`은 동일 효과를 시뮬레이션용(출력 없음)으로 재구현.

### 4. 적 메커니즘 / 타입 시스템 — `enemy_types.h`
`EnemyType`이 스탯 분배 가중치 + 메커니즘 + 적 스킬을 묶는다.
- **메커니즘 5종**: NONE / BERSERKER(체력 낮을수록 공격↑·방어↓) / TURTLE(체력 높으면 방어 모드, 공격 절반) / VAMPIRE(가한 피해 1/3 흡혈) / ARMOR(첫 3턴 피해 1 고정).
- **적 스킬 2종**: PowerStrike(rev로 공격 강화) / Guard(방어 2배).
- 일반 5종(Soldier/Berserker/Turtle/Vampire/Knight) + 보스 4종(Warlord/IronGolem/BloodWitch/Dragon, 5·10·15·20층).
- `setup_turn()`이 매 턴 메커니즘·rev·스킬을 종합해 실질 스탯과 행동을 결정하고, `apply_post()`가 전투 후 흡혈·장갑 후처리를 적용한다. `silent` 플래그로 실전/시뮬 출력을 분기.

### 5. 데이터 파이프라인 — `battle_log.h` + `battle_log.csv`
실전 전투의 매 행동이 `csv_log()`로 CSV에 append 된다(10피처: 양쪽 max_hp·hp, atk/def 차이, 양쪽 rev, action, 적 id). 다음 층 튜닝 시 `csv_load()`로 전량을 읽어 KNN 학습셋으로 쓴다. **플레이가 곧 학습 데이터** 인 온라인 루프 구조다(동봉본 분포: 공격 78 / 방어 81 / 스킬 11).

---

## 평가 관점 메모 (멘토링/심사용)

**강점**
- 주제가 선명하고 야심적: 단순 RPG가 아니라 "**플레이어를 학습해 난이도를 자동 조정하는 AI GM**"이라는 한 문장 컨셉을 실제 동작 코드로 끝까지 구현.
- KNN(데이터 기반 행동 모사) + 몬테카를로(목표 난이도 역설계)를 결합한 설계가 영리하다. 보스/일반에 서로 다른 목표함수(승률 vs 데미지)를 둔 것도 게임 디자인 감각.
- `std::async`로 시뮬 연산을 레벨업 입력 시간에 숨긴 점, 시뮬 전용 `thread_local` 난수로 스레드 안전을 챙긴 점은 수준 높은 디테일.
- 외부 의존성 0, 헤더-온리로 모듈을 깔끔히 분리(전투/AI/적/데이터). 한국어 주석이 충실해 가독성이 좋다.

**보완 / 질문거리**
- **밸런싱 품질 검증 부재**: 튜너가 목표 난이도를 실제로 맞추는지 보여주는 수치/로그가 없다. SIM_ROUNDS=50, ATTEMPTS=20이 충분한 표본인지(분산), 후보 생성이 순수 랜덤 탐색이라 더 똑똑한 탐색(이분/경사)으로 개선 여지.
- **콜드 스타트**: CSV가 비면 KNN이 거의 무작위 행동으로 폴백 → 초반 밸런싱 신뢰도 낮음. 동봉 170행도 표본이 작다.
- **실/시뮬 로직 중복**: `act()`(combat.h)와 `sim_use_skill()`, 전투 루프 본체와 시뮬 루프가 메커니즘을 **두 번 구현**해 동기화가 깨지면 "튜닝한 난이도 ≠ 실제 난이도"가 될 위험. 단일 함수로 통합 권장.
- **테스트 부재**: 전투 해석·`signalPhase`류 순수 로직이 단위 테스트하기 좋은 구조인데 테스트가 없다. `predict_action`/`auto_tune_enemy`의 결정성 검증 권장.
- **품질 주의점**: 정수 나눗셈/버림이 곳곳(스탯 분배, 흡혈 1/3, def×1.5)에 있어 의도된 밸런스인지 확인 필요. `Character::etype` 기본 초기화가 `enemy_types.h`의 NONE soldier에 의존(헤더 결합도). 보안 이슈는 없음(콘솔 게임, 네트워크/시크릿 없음).

**측정 결과 (2026-06-19 · `scripts/eval.py`, `--demo` 50회, baseline CSV 고정 · 보스 253전)**

밸런싱이 *목표를 실제로 맞추는지* 정량 측정한 결과, 보스 난이도가 **목표(적 승률 40%) 대비 사실상 무력하다(real 적승률 2%).** 그리고 원인이 층에 따라 둘로 갈린다.

| 보스층 | sim 예측 적승률 | real 실측 적승률 | 목표 | 진단 |
|-------:|----------------:|-----------------:|-----:|------|
| 5  | **40%** | **5%** | 40% | sim은 목표 명중, real만 너무 쉬움 → **sim↔real 로직 괴리** |
| 10 | 1%  | 0% | 40% | sim에서조차 40% 불가 → **탐색/예산 부족** |
| 15 | 0%  | 0% | 40% | 〃 |
| 20 | 0%  | 0% | 40% | 〃 |

- **결함 A (저층, sim↔real 괴리)**: 튜너의 시뮬은 floor 5를 정확히 40%로 맞췄다고 믿지만 실제 적승률은 5% — 8배 쉽다. 시뮬 전투 로직(`sim_use_skill` 등)이 실전(`act()`)과 달라 *"튜닝한 난이도 ≠ 실제 난이도"*. (위 *실/시뮬 로직 중복*의 실증)
- **결함 B (고층, 탐색 한계)**: floor 10+에선 플레이어가 강해져, 레벨 비례 스탯 예산·순수 랜덤 후보 20개로는 적이 40%를 이기는 후보를 *시뮬에서도* 못 찾는다(best ≈ 0%).

> 표본 한계: `SIM_ROUNDS=50`에서 40% 추정의 표준오차는 `√(0.4·0.6/50) ≈ 7%`로, 튜너가 40%와 33%/47%를 구분할 분해능이 애초에 낮다 — 이것 자체가 평가 항목이다.

재현: `make && python3 scripts/eval.py 50` → [docs/eval-report.md](docs/eval-report.md). 원인 규명·수정은 이슈로 추적(#7 진단 → #8 로직통합 / #9 탐색개선).

---

## 출처
- 원본: `(완료)auto balance ai-20260619T015816Z-3-001.zip` (Downloads, 2026-06-19 수신)
- 원작자 작업 환경: Windows + Visual Studio 2022 (`c:\Users\zsx12\source\repos\ai_gm`), `.github/copilot-instructions.md`에 Azure MCP 규칙(미사용) 흔적
- 동봉 학습 데이터: `src/battle_log.csv` (실제 플레이 행동 170행)
