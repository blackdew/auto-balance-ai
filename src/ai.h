#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "sim_rand.h"
#include "battle_log.h"
#include "character.h"
#include "combat.h"

// KNN으로 현재 상태에 가장 가까운 행들을 찾아 action 빈도로 확률 결정
// 반환: 1=공격, 2=방어, 3=스킬
inline int predict_action(const std::vector<LogRow>& log,
                          int p_max_hp, int p_hp,
                          int e_max_hp, int e_hp,
                          int atk_diff, int def_diff,
                          int p_rev,    int e_rev,
                          const BattleStats& fallback_stats,
                          bool has_skills) {
    const int K = 5;

    // CSV 데이터 없으면 실제 행동 비율 기반 fallback
    if (log.empty()) {
        int skl   = has_skills ? fallback_stats.skl_count : 0;
        int total = fallback_stats.atk_count + fallback_stats.def_count + skl;
        if (total == 0) return sim_rand(has_skills ? 3 : 2) + 1;
        int roll = sim_rand(total) + 1;
        if (roll <= fallback_stats.atk_count)                            return 1;
        if (roll <= fallback_stats.atk_count + fallback_stats.def_count) return 2;
        return 3;
    }

    auto safe = [](int v) { return v == 0 ? 1 : v; };

    // 거리 계산 (각 피처를 최대값 기준 정규화)
    std::vector<std::pair<double, int>> dist_action;
    for (auto& r : log) {
        double d = 0;
        d += pow((double)(p_hp     - r.p_hp)     / safe(p_max_hp), 2);
        d += pow((double)(e_hp     - r.e_hp)     / safe(e_max_hp), 2);
        d += pow((double)(atk_diff - r.atk_diff) / 10.0,           2);
        d += pow((double)(def_diff - r.def_diff) / 10.0,           2);
        d += pow((double)(p_rev    - r.p_rev)    / 10.0,           2);
        d += pow((double)(e_rev    - r.e_rev)    / 10.0,           2);
        dist_action.push_back({ sqrt(d), r.action });
    }

    std::sort(dist_action.begin(), dist_action.end());
    int k = std::min(K, (int)dist_action.size());
    int atk_cnt = 0, def_cnt = 0, skl_cnt = 0;
    for (int i = 0; i < k; ++i) {
        if      (dist_action[i].second == 1) atk_cnt++;
        else if (dist_action[i].second == 2) def_cnt++;
        else                                 skl_cnt++;
    }
    // 스킬 보유 없으면 스킬 투표를 공격/방어에 비례 배분
    if (!has_skills) {
        int base = atk_cnt + def_cnt;
        if (base > 0) {
            int atk_share = skl_cnt * atk_cnt / base;
            def_cnt += skl_cnt - atk_share;
            atk_cnt += atk_share;
        } else {
            atk_cnt += skl_cnt / 2;
            def_cnt += skl_cnt - skl_cnt / 2;
        }
        skl_cnt = 0;
    }
    int total = atk_cnt + def_cnt + skl_cnt;
    if (total == 0) return sim_rand(2) + 1;
    int roll = sim_rand(total) + 1;
    if (roll <= atk_cnt)           return 1;
    if (roll <= atk_cnt + def_cnt) return 2;
    return 3;
}

struct EnemyStat { int atk, def, max_hp; };

enum class TuneMode {
    WIN_RATE,   // 보스: 적 승률 40% 목표
    AVG_DAMAGE, // 일반: 층수에 비례한 HP 소모량 목표
};

// 시뮬레이션으로 적 스탯을 조정 — 보스는 승률, 일반은 데미지 기준
inline EnemyStat auto_tune_enemy(int e_atk, int e_def, int e_max_hp,
                                  int e_level,
                                  int p_atk, int p_def, int p_max_hp,
                                  const BattleStats& stats,
                                  const std::vector<Skill>& player_skills,
                                  const EnemyType& etype,
                                  TuneMode mode = TuneMode::WIN_RATE) {
    const int SIM_ROUNDS   = 50;
    const int MAX_ATTEMPTS = 20;
    // WIN_RATE: 적 목표 승률 40%
    const int TARGET_WINS  = (int)(SIM_ROUNDS * 0.4);
    // AVG_DAMAGE: 층수에 따라 목표 증가 (1층 30% ~ 19층 77.5%, 상한 80%)
    const int TARGET_DMG   = (int)(p_max_hp * std::min(0.8, 0.3 + 0.025 * e_level));

    std::vector<LogRow> log = csv_load();
    bool has_skills = !player_skills.empty();

    int best_atk    = e_atk;
    int best_def    = e_def;
    int best_max_hp = e_max_hp;
    int best_score  = -1;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        if (best_score == 0) break;

        int try_atk    = e_atk;
        int try_def    = e_def;
        int try_max_hp = e_max_hp;
        int remaining  = e_level;

        // 타입 가중치에 따라 스탯 포인트 분배 + 랜덤 편차
        int atk_pts  = (int)(remaining * etype.atk_w);
        int def_pts  = (int)(remaining * etype.def_w);
        int hp_pts   = remaining - atk_pts - def_pts;
        int variance = std::max(1, remaining / 5);
        atk_pts = std::max(0, atk_pts + sim_rand(variance * 2 + 1) - variance);
        def_pts = std::max(0, std::min(remaining - atk_pts, def_pts + sim_rand(variance * 2 + 1) - variance));
        hp_pts  = remaining - atk_pts - def_pts;

        try_atk    += atk_pts;
        try_def    += def_pts;
        try_max_hp += hp_pts;

        int wins = 0, total_dmg = 0;
        for (int sim = 0; sim < SIM_ROUNDS; ++sim) {
            int  sim_p_rev      = 0, sim_e_rev = 0;
            int  sim_p_hp       = p_max_hp;
            int  sim_e_hp       = try_max_hp;
            bool sim_rev_halved = false;
            int  sim_battle_turn = 0;
            BattleStats sim_stats = stats;

            while (sim_e_hp > 0 && sim_p_hp > 0) {
                int p_rev_before = sim_p_rev;
                int p_hp_before  = sim_p_hp;
                int e_hp_before  = sim_e_hp;

                // 메커니즘 적용: 실질 스탯·행동 선택 결정
                auto setup = etype.setup_turn(
                    sim_e_hp, try_max_hp, sim_e_rev, sim_battle_turn,
                    try_atk, try_def, sim_p_rev, /*silent=*/true);

                int p_def_eff = setup.skill_fired
                    ? std::min(p_def, p_atk) : p_def;

                int p_ac = predict_action(log,
                                          p_max_hp,   sim_p_hp,
                                          try_max_hp, sim_e_hp,
                                          p_atk - try_atk,
                                          p_def - try_def,
                                          sim_p_rev,  sim_e_rev,
                                          sim_stats,  has_skills);

                if (p_ac == 3) {
                    if (!sim_use_skill(player_skills,
                                       p_atk, p_def_eff, sim_p_hp, p_max_hp,
                                       setup.e_atk_eff, setup.e_def_eff, sim_e_hp,
                                       sim_p_rev, sim_e_rev, sim_rev_halved)) {
                        p_ac = 1;
                        (void)act(p_atk, p_def_eff, sim_p_hp,
                                  setup.e_atk_eff, setup.e_def_eff, sim_e_hp,
                                  p_ac, setup.e_choice, false,
                                  sim_p_rev, sim_e_rev, sim_stats);
                    }
                } else {
                    if (p_ac == 2 && setup.e_choice == 1 && sim_rev_halved) {
                        int before = sim_p_rev;
                        (void)act(p_atk, p_def_eff, sim_p_hp,
                                  setup.e_atk_eff, setup.e_def_eff, sim_e_hp,
                                  p_ac, setup.e_choice, false,
                                  sim_p_rev, sim_e_rev, sim_stats);
                        int gained = sim_p_rev - before;
                        sim_p_rev = before + gained / 3;
                    } else {
                        (void)act(p_atk, p_def_eff, sim_p_hp,
                                  setup.e_atk_eff, setup.e_def_eff, sim_e_hp,
                                  p_ac, setup.e_choice, false,
                                  sim_p_rev, sim_e_rev, sim_stats);
                    }
                }

                etype.apply_post(sim_e_hp, e_hp_before, try_max_hp,
                                 sim_p_hp, p_hp_before,
                                 sim_p_rev, p_rev_before,
                                 setup.e_choice, sim_battle_turn,
                                 sim_e_rev, /*silent=*/true);
                sim_battle_turn++;
            }
            if (sim_p_hp <= 0 && sim_e_hp > 0) wins++;
            total_dmg += p_max_hp - std::max(0, sim_p_hp);
        }

        int score;
        if (mode == TuneMode::WIN_RATE) {
            score = std::abs(wins - TARGET_WINS);
        } else {
            int avg_dmg = total_dmg / SIM_ROUNDS;
            score = std::abs(avg_dmg - TARGET_DMG);
        }

        if (best_score < 0 || score < best_score) {
            best_score  = score;
            best_atk    = try_atk;
            best_def    = try_def;
            best_max_hp = try_max_hp;
        }
    }

    return { best_atk, best_def, best_max_hp };
}
