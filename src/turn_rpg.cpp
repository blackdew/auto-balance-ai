#include <iostream>
#include <cstdlib>
#include <ctime>
#include <future>
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

int main() {
    srand(time(NULL));
    csv_init();

    vector<EnemyType> enemy_types = load_enemy_types();
    vector<EnemyType> boss_types  = load_boss_types();

    Character   player;
    Character   enemy;
    BattleStats stats;

    // 레벨 1~20, 5층 단위 보스, 20층 클리어 시 종료
    while (enemy.level < 21) {
        bool is_boss = (enemy.level % 5 == 0);

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
            cout << "1.attack\t2.defense\t3.skill" << endl;
            cout << "------------------------------------------------" << endl;

            auto setup = enemy.etype.setup_turn(
                enemy.hp, enemy.max_hp, enemy.rev, battle_turn,
                enemy.atk, enemy.def, player.rev);

            int p_def_eff = setup.skill_fired ? min(player.def, player.atk) : player.def;

            bool turn_done = false;
            while (!turn_done) {
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
            levelup_player(player);
            enemy.level += 1;

            auto stat    = fut.get();
            enemy.atk    = stat.atk;
            enemy.def    = stat.def;
            enemy.max_hp = stat.max_hp;
            enemy.etype  = next_etype;
            cout << endl;
        }
        cout << "-------------------------" << endl;
    }
}
