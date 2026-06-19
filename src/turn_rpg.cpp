#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <future>
#include <string>
#include "enemy_types.h"
#include "battle_log.h"
#include "character.h"
#include "combat.h"
#include "ai.h"
#include "levelup.h"

using namespace std;

// 보스 기초 스탯 — 일반 적보다 높은 시작점에서 시뮬 조정
constexpr int BOSS_BASE_ATK    = 6;
constexpr int BOSS_BASE_DEF    = 6;
constexpr int BOSS_BASE_MAX_HP = 18;

// ───────── 데모 모드 헬퍼 ─────────────────────────────────────────────────────
// 목적: 백그라운드에서만 도는 자동 밸런싱 AI를 비대화형으로 "보여준다".
// 원칙(additive): ai.h/combat.h의 밸런싱·전투 로직은 호출만 하고 수정하지 않는다.
//                여기 추가된 것은 자동 주행(입력 대체)과 결정 출력뿐이다.

static const char* mechanic_name(EnemyMechanic m) {
    switch (m) {
        case EnemyMechanic::BERSERKER: return "BERSERKER";
        case EnemyMechanic::TURTLE:    return "TURTLE";
        case EnemyMechanic::VAMPIRE:   return "VAMPIRE";
        case EnemyMechanic::ARMOR:     return "ARMOR";
        default:                       return "NONE";
    }
}

// 데모 플레이어 행동: 시뮬레이션과 "동일한" KNN 두뇌(predict_action)를 재사용한다.
static int demo_pick_action(const vector<LogRow>& log,
                            const Character& player, const Character& enemy,
                            int battle_turn, const BattleStats& stats) {
    if (battle_turn > 60) return 1; // 양쪽 방어로 인한 교착 방지
    return predict_action(log,
                          player.max_hp, player.hp,
                          enemy.max_hp,  enemy.hp,
                          player.atk - enemy.atk,
                          player.def - enemy.def,
                          player.rev,    enemy.rev,
                          stats, !player.skills.empty());
}

// 데모 스킬 선택: rev로 실제 사용 가능한 첫 스킬 (없으면 -1 → 공격으로 폴백)
static int demo_choose_skill(const Character& player) {
    for (int i = 0; i < (int)player.skills.size(); ++i) {
        const Skill& sk = player.skills[i];
        int cost = (sk.rev_cost == -1) ? player.rev : sk.rev_cost;
        if (player.rev > 0 && player.rev >= cost) return i;
    }
    return -1;
}

// 데모 레벨업: 입력 없이 미보유 스킬 1개 습득 + 스탯 라운드로빈 분배
static void demo_levelup(Character& player) {
    for (int i = 0; i < (int)SKILL_LIST.size(); ++i) {
        bool owned = false;
        for (auto& s : player.skills)
            if (s.type == SKILL_LIST[i].type) { owned = true; break; }
        if (!owned) { player.skills.push_back(SKILL_LIST[i]); break; }
    }
    int points = player.level;
    for (int i = 0; points > 0; ++i, --points) {
        if      (i % 3 == 0) player.max_hp += 1;
        else if (i % 3 == 1) player.atk    += 1;
        else                 player.def    += 1;
    }
}

int main(int argc, char** argv) {
    bool demo = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--demo") == 0) demo = true;

    const int DEMO_MAX_BATTLES = 40; // 데모 안전 상한 (clear! 도달 시 더 일찍 종료)
    int demo_battles = 0;

    srand(time(NULL));
    csv_init();

    vector<EnemyType> enemy_types = load_enemy_types();
    vector<EnemyType> boss_types  = load_boss_types();

    Character   player;
    Character   enemy;
    BattleStats stats;

    // 데모용 KNN 학습셋 스냅샷 (대화형 플레이에는 영향 없음)
    vector<LogRow> demo_log;
    if (demo) demo_log = csv_load();

    // 레벨 1~20, 5층 단위 보스, 20층 클리어 시 종료
    while (enemy.level < 21 && (!demo || demo_battles < DEMO_MAX_BATTLES)) {
        bool is_boss = (enemy.level % 5 == 0);
        if (demo) demo_battles++;

        if (player.level == 1) {
            player.atk    = 8;
            player.def    = 4;
            player.max_hp = 15;
            player.hp     = 15;
            player.skills.push_back(SKILL_LIST[0]);

            if (is_boss) {
                enemy.etype  = boss_types[enemy.level / 5 - 1];
                enemy.atk    = BOSS_BASE_ATK;
                enemy.def    = BOSS_BASE_DEF;
                enemy.max_hp = BOSS_BASE_MAX_HP;
            } else {
                enemy.etype  = enemy_types[rand() % enemy_types.size()];
                enemy.atk    =  4;
                enemy.def    =  3;
                enemy.max_hp = 12;
            }
        }
        // level 2+: enemy.atk/def/max_hp/etype 는 이전 승리 시점의 async 결과로 설정됨

        enemy.hp  = enemy.max_hp;
        player.hp = player.max_hp;
        player.rev_halved = false;

        if (is_boss)
            cout << "===== BOSS: " << enemy.etype.name << " =====" << endl;

        int battle_turn = 0;

        // ── 전투 루프 ──────────────────────────────────────────────────────────
        while (enemy.hp > 0 && player.hp > 0) {
            cout << "player:" << player.level << "\t"
                 << "enemy:" << enemy.level << " [" << enemy.etype.name << "]" << endl;
            cout << "HP:" << player.hp << "/" << player.max_hp
                 << "\t\tHP:" << enemy.hp << "/" << enemy.max_hp << endl;
            cout << "rev(player):" << player.rev;
            if (player.rev_halved) cout << " (gain halved)";
            cout << "\t" << "rev(enemy):" << enemy.rev << endl;
            cout << "------------------------------------------------" << endl;
            if (!demo) cout << "1.attack\t2.defense\t3.skill" << endl;
            cout << "------------------------------------------------" << endl;

            auto setup = enemy.etype.setup_turn(
                enemy.hp, enemy.max_hp, enemy.rev, battle_turn,
                enemy.atk, enemy.def, player.rev);

            int p_def_eff = setup.skill_fired ? min(player.def, player.atk) : player.def;

            bool turn_done = false;
            while (!turn_done) {
                // ── 데모: KNN 두뇌로 행동을 자동 선택 (입력 대기 없음) ──
                if (demo) {
                    int a_choice    = demo_pick_action(demo_log, player, enemy, battle_turn, stats);
                    int skill_index = -1;
                    if (a_choice == 3) {
                        skill_index = demo_choose_skill(player);
                        if (skill_index < 0) a_choice = 1; // 사용 가능한 스킬 없으면 공격
                    }
                    const char* an = (a_choice == 1 ? "attack" : a_choice == 2 ? "defense" : "skill");
                    cout << "act(demo): " << an << endl;

                    int log_p_rev   = player.rev, log_e_rev = enemy.rev;
                    int log_p_hp    = player.hp,  log_e_hp  = enemy.hp;
                    int p_hp_before = player.hp,  e_hp_before = enemy.hp;

                    bool done = act(player.atk, p_def_eff, player.hp,
                                    setup.e_atk_eff, setup.e_def_eff, enemy.hp,
                                    a_choice, setup.e_choice, true, player.rev, enemy.rev,
                                    stats, &player, skill_index);
                    if (!done) { // 스킬 취소(예외) → 공격 폴백
                        a_choice = 1;
                        act(player.atk, p_def_eff, player.hp,
                            setup.e_atk_eff, setup.e_def_eff, enemy.hp,
                            1, setup.e_choice, true, player.rev, enemy.rev,
                            stats, &player);
                    }
                    enemy.etype.apply_post(
                        enemy.hp, e_hp_before, enemy.max_hp,
                        player.hp, p_hp_before,
                        player.rev, log_p_rev,
                        setup.e_choice, battle_turn, enemy.rev);
                    csv_log({ player.max_hp, log_p_hp,
                              enemy.max_hp,  log_e_hp,
                              player.atk - enemy.atk,
                              player.def - enemy.def,
                              log_p_rev, log_e_rev,
                              a_choice, enemy.etype.id });
                    turn_done = true;
                    continue;
                }

                cout << "act:";
                int a_choice; cin >> a_choice;
                cout << endl;

                if (a_choice == 1 || a_choice == 2) {
                    int log_p_rev   = player.rev, log_e_rev = enemy.rev;
                    int log_p_hp    = player.hp,  log_e_hp  = enemy.hp;
                    int p_hp_before = player.hp;
                    int e_hp_before = enemy.hp;

                    act(player.atk, p_def_eff, player.hp,
                        setup.e_atk_eff, setup.e_def_eff, enemy.hp,
                        a_choice, setup.e_choice, true, player.rev, enemy.rev,
                        stats, &player);

                    enemy.etype.apply_post(
                        enemy.hp, e_hp_before, enemy.max_hp,
                        player.hp, p_hp_before,
                        player.rev, log_p_rev,
                        setup.e_choice, battle_turn, enemy.rev);

                    csv_log({ player.max_hp, log_p_hp,
                              enemy.max_hp,  log_e_hp,
                              player.atk - enemy.atk,
                              player.def - enemy.def,
                              log_p_rev, log_e_rev,
                              a_choice, enemy.etype.id });
                    turn_done = true;
                }
                else if (a_choice == 3) {
                    if (player.skills.empty()) {
                        cout << "no skill" << endl;
                    }
                    else {
                        cout << "--- skill list ---" << endl;
                        for (int i = 0; i < (int)player.skills.size(); ++i) {
                            const Skill& sk = player.skills[i];
                            string cost_str = sk.rev_cost == -1
                                ? "all(" + to_string(player.rev) + ")"
                                : to_string(sk.rev_cost);
                            cout << i + 1 << "." << sk.name
                                 << " [rev " << cost_str << "]" << endl;
                        }
                        cout << "0.cancel" << endl;
                        int sc; cin >> sc;
                        if (sc >= 1 && sc <= (int)player.skills.size()) {
                            int log_p_rev   = player.rev, log_e_rev = enemy.rev;
                            int log_p_hp    = player.hp,  log_e_hp  = enemy.hp;
                            int p_hp_before = player.hp;
                            int e_hp_before = enemy.hp;

                            turn_done = act(player.atk, p_def_eff, player.hp,
                                            setup.e_atk_eff, setup.e_def_eff, enemy.hp,
                                            3, setup.e_choice, true, player.rev, enemy.rev,
                                            stats, &player, sc - 1);
                            if (turn_done) {
                                enemy.etype.apply_post(
                                    enemy.hp, e_hp_before, enemy.max_hp,
                                    player.hp, p_hp_before,
                                    player.rev, log_p_rev,
                                    setup.e_choice, battle_turn, enemy.rev);

                                csv_log({ player.max_hp, log_p_hp,
                                          enemy.max_hp,  log_e_hp,
                                          player.atk - enemy.atk,
                                          player.def - enemy.def,
                                          log_p_rev, log_e_rev,
                                          3, enemy.etype.id });
                            }
                        }
                    }
                }
                else {
                    cout << "error" << endl;
                }
            }

            battle_turn++;

            if (enemy.hp <= 0 && player.hp <= 0) {
                cout << "-------------------------" << endl;
                cout << "draw" << endl;
                enemy.hp  = enemy.max_hp;
                player.hp = player.max_hp;
            }
            cout << endl;
            cout << "-------------------------" << endl;
        }
        // ── 전투 루프 끝 ────────────────────────────────────────────────────────

        if (enemy.level == 20 && enemy.hp <= 0 && player.hp > 0) {
            cout << "clear!" << endl;
            break;
        }

        player.rev = 0;
        enemy.rev  = 0;

        if (player.hp <= 0 && enemy.hp > 0) {
            cout << "lose..." << endl;
            enemy.level  = 1;
            player.level = 1;
            player.skills.clear();
            stats = BattleStats{};
        }
        else if (enemy.hp <= 0 && player.hp > 0) {
            cout << "Win!" << endl << endl;

            // 다음 층 정보 결정 — async 시작 전에 확정
            int next_level    = enemy.level + 1;
            bool next_is_boss = (next_level % 5 == 0);
            TuneMode mode     = next_is_boss ? TuneMode::WIN_RATE : TuneMode::AVG_DAMAGE;

            EnemyType next_etype = next_is_boss
                ? boss_types[next_level / 5 - 1]
                : enemy_types[rand() % enemy_types.size()];

            int base_atk    = next_is_boss ? BOSS_BASE_ATK    : enemy.atk;
            int base_def    = next_is_boss ? BOSS_BASE_DEF    : enemy.def;
            int base_max_hp = next_is_boss ? BOSS_BASE_MAX_HP : enemy.max_hp;

            // 시뮬을 백그라운드에서 시작, 레벨업 입력 동안 병렬 진행
            auto fut = std::async(std::launch::async,
                                  auto_tune_enemy,
                                  base_atk, base_def, base_max_hp,
                                  next_level,
                                  player.atk, player.def, player.max_hp,
                                  stats, player.skills, next_etype, mode);

            player.level += 1;
            if (demo) demo_levelup(player); else levelup_player(player);
            enemy.level += 1;

            auto stat    = fut.get();
            enemy.atk    = stat.atk;
            enemy.def    = stat.def;
            enemy.max_hp = stat.max_hp;
            enemy.etype  = next_etype;

            // 데모: 백그라운드 튜너가 방금 내린 결정을 가시화
            if (demo) {
                double ratio = std::min(0.8, 0.3 + 0.025 * next_level);
                int    tdmg  = (int)(player.max_hp * ratio);
                cout << "\n  >> AUTO-BALANCE  floor " << next_level
                     << (next_is_boss ? "  [BOSS]" : "") << "\n";
                cout << "     enemy : " << next_etype.name
                     << " [" << mechanic_name(next_etype.mechanic) << "]"
                     << "  w(atk/def/hp)=" << next_etype.atk_w << "/"
                     << next_etype.def_w << "/" << next_etype.hp_w << "\n";
                cout << "     AI    : 20 stat candidates x 50 simulated battles (KNN-predicted player)\n";
                if (next_is_boss)
                    cout << "     target: enemy win-rate ~40%\n";
                else
                    cout << "     target: player HP loss ~" << tdmg << "/" << player.max_hp
                         << " (" << (int)(ratio * 100) << "%)\n";
                cout << "     chosen: atk " << base_atk << "->" << stat.atk
                     << "  def "  << base_def << "->" << stat.def
                     << "  hp "   << base_max_hp << "->" << stat.max_hp << "\n";
            }
            cout << endl;
        }
        cout << "-------------------------" << endl;
    }

    if (demo)
        cout << "\n[demo] finished — reached floor " << enemy.level
             << " over " << demo_battles << " battle(s)." << endl;
}
