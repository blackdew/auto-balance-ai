#pragma once
#include <algorithm>
#include <iostream>
#include <vector>
#include "character.h"

// 한 턴 전투 처리
// p_choice=3 + skill_index >= 0 이면 스킬 사용 시도
// 반환값: true=행동 완료, false=행동 취소(재입력 필요)
inline bool act(int p_atk, int p_def, int& p_hp,
                int e_atk, int e_def, int& e_hp,
                int p_choice, int e_choice,
                bool print_log, int& p_rev, int& e_rev,
                BattleStats& stats,
                Character* player_char = nullptr, // 스킬 사용 시 필요
                int skill_index = -1) {

    // --- 스킬 사용 ---
    if (p_choice == 3 && player_char != nullptr && skill_index >= 0
        && skill_index < (int)player_char->skills.size()) {

        const Skill& sk = player_char->skills[skill_index];
        int cost = (sk.rev_cost == -1) ? p_rev : sk.rev_cost;

        if (p_rev < cost) {
            if (print_log) std::cout << "not enough rev! (need: " << cost << ", cur: " << p_rev << ")" << std::endl;
            return false; // 행동 취소
        }

        switch (sk.type) {
        case SkillType::REV_ATK: {
            // rev 전량 소모, atk+rev로 강화 공격 (방어 관통 없음)
            if (print_log) std::cout << "[RevAtk] atk+" << p_rev << "!" << std::endl;
            int dmg = p_atk + p_rev;
            p_rev = 0;
            dmg > e_def ? (e_hp -= dmg - e_def) : (e_hp -= 1);
            if (print_log) std::cout << "attack!\t" << std::max(1, dmg - e_def) << std::endl;
            // 적 반격
            e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
            if (print_log) {
                e_atk + e_rev > p_def
                    ? std::cout << "attacked!\t" << e_atk + e_rev - p_def << std::endl
                    : std::cout << "attacked!\t" << 1 << std::endl;
            }
            e_rev = 0;
            stats.skl_count += 1;
            break;
        }
        case SkillType::COUNTER:
            // rev 전량 소모, 방어 무시 공격, 반동
            if (print_log) std::cout << "[Counter] rev=" << p_rev << " ignore def!" << std::endl;
            e_hp -= p_atk + p_rev;
            p_hp -= (p_rev - (e_rev/2) - e_atk)/2; // 반격 피해
            if (print_log) std::cout << "attack(ignore)!\t" << p_atk + p_rev << "damaged" << (p_rev - (e_rev/2) - e_atk)/2 << std::endl;
            p_rev = 0;
            stats.skl_count += 1;
            break;

        case SkillType::POWER_DEF: {
            // rev 5 소모, 이번 턴 def 2배로 방어
            p_rev -= cost;
            int boosted_def = p_def * 2;
            if (print_log) std::cout << "[PowerDef] def " << p_def << " -> " << boosted_def << std::endl;
            e_atk + e_rev > boosted_def ? (p_hp -= e_atk + e_rev - boosted_def) : (p_hp -= 1);
            if (print_log) {
                e_atk + e_rev > boosted_def
                    ? std::cout << "attacked!\t" << e_atk + e_rev - boosted_def << std::endl
                    : std::cout << "attacked!\t" << 1 << std::endl;
            }
            e_rev = 0;
            stats.skl_count += 1;
            break;
        }
        case SkillType::DOUBLE_ATK:
            // rev 8 소모, 2회 공격
            p_rev -= cost;
            if (print_log) std::cout << "[DoubleAtk] 2 hits!" << std::endl;
            for (int i = 0; i < 2; ++i) {
                p_atk > e_def ? (e_hp -= p_atk - e_def) : (e_hp -= 1);
                if (print_log) std::cout << "attack!\t" << std::max(1, p_atk - e_def) << std::endl;
            }
            // 적 반격
            e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
            if (print_log) {
                e_atk + e_rev > p_def
                    ? std::cout << "attacked!\t" << e_atk + e_rev - p_def << std::endl
                    : std::cout << "attacked!\t" << 1 << std::endl;
            }
            e_rev = 0;
            stats.skl_count += 1;
            break;

        case SkillType::HEAL: {
            // rev 전량 소모, rev/2 회복, 이후 rev 축적 절반
            int heal = p_rev / 2;
            if (print_log) std::cout << "[Heal] +" << heal << " HP (rev gain halved)" << std::endl;
            p_hp = std::min(p_hp + heal, player_char->max_hp);
            p_rev = 0;
            player_char->rev_halved = true;
            // 적 반격
            e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
            if (print_log) {
                e_atk + e_rev > p_def
                    ? std::cout << "attacked!\t" << e_atk + e_rev - p_def << std::endl
                    : std::cout << "attacked!\t" << 1 << std::endl;
            }
            e_rev = 0;
            stats.skl_count += 1;
            break;
        }
        }
        return true;
    }

    // --- 일반 행동 (공격/방어) ---
    if (p_choice == 1 && e_choice == 1) {
        // 플레이어 공격 vs 적 공격
        stats.atk_count += 1;
        if (print_log) std::cout << "attack!\t";
        p_atk > e_def ? (e_hp -= p_atk - e_def) : (e_hp -= 1);
        if (print_log) {
            p_atk > e_def ? (std::cout << "\t" << (p_atk - e_def)) : (std::cout << "\t" << 1);
            std::cout << std::endl;
        }
        e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
        if (print_log) {
            e_atk + e_rev > p_def ? (std::cout << "attacked!\t" << (e_atk + e_rev - p_def)) : (std::cout << "attacked!\t" << 1);
            std::cout << std::endl;
        }
        e_rev = 0;
    }
    else if (p_choice == 1 && e_choice == 2) {
        // 플레이어 공격 vs 적 방어 — 적이 rev 축적
        stats.atk_count += 1;
        if (print_log) std::cout << "attack!\t";
        p_atk > (e_def + e_def / 2) ? (e_hp -= p_atk - (e_def + e_def / 2)) : (e_hp -= 1);
        int gain = p_atk / 3;
        e_rev += gain;
        if (print_log) {
            p_atk > (e_def + e_def / 2) ? (std::cout << "\t" << p_atk - (e_def + e_def / 2)) : (std::cout << 1);
            std::cout << std::endl;
        }
    }
    else if (p_choice == 2 && e_choice == 1) {
        // 플레이어 방어 vs 적 공격 — 플레이어가 rev 축적
        stats.def_count += 1;
        if (print_log) std::cout << "defence!\t";
        e_atk + e_rev > (p_def + p_def / 2) ? (p_hp -= e_atk + e_rev - (p_def + p_def / 2)) : (p_hp -= 1);
        e_rev = 0;
        int gain = e_atk / 3;
        if (player_char && player_char->rev_halved) gain /= 2; // Heal 사용 후 rev 축적 절반
        p_rev += gain;
        if (print_log) {
            e_atk + e_rev > (p_def + p_def / 2) ? (std::cout << e_atk + e_rev - (p_def + p_def / 2)) : (std::cout << 1);
            std::cout << std::endl;
        }
    }
    else {
        // 양쪽 방어 — 피해 없음
        stats.def_count += 1;
        if (print_log) std::cout << "defence!\t" << 0 << std::endl;
    }
    return true;
}

// 시뮬에서 랜덤으로 스킬 1개 선택·실행 — 사용 가능한 스킬 없으면 false 반환
inline bool sim_use_skill(const std::vector<Skill>& skills,
                          int p_atk, int p_def, int& p_hp, int p_max_hp,
                          int e_atk, int e_def, int& e_hp,
                          int& p_rev, int& e_rev, bool& rev_halved) {
    // rev가 충분한 스킬 목록 수집
    std::vector<int> usable;
    for (int i = 0; i < (int)skills.size(); ++i) {
        int cost = (skills[i].rev_cost == -1) ? p_rev : skills[i].rev_cost;
        if (p_rev >= cost) usable.push_back(i);
    }
    if (usable.empty()) return false;

    const Skill& sk = skills[usable[rand() % usable.size()]];
    int cost = (sk.rev_cost == -1) ? p_rev : sk.rev_cost;

    switch (sk.type) {
    case SkillType::REV_ATK: {
        int dmg = p_atk + p_rev;
        p_rev = 0;
        dmg > e_def ? (e_hp -= dmg - e_def) : (e_hp -= 1);
        e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
        e_rev = 0;
        break;
    }
    case SkillType::COUNTER:
        e_hp  -= p_atk + p_rev;
        p_rev  = 0;
        break;
    case SkillType::POWER_DEF: {
        p_rev -= cost;
        int boosted = p_def * 2;
        e_atk + e_rev > boosted ? (p_hp -= e_atk + e_rev - boosted) : (p_hp -= 1);
        e_rev = 0;
        break;
    }
    case SkillType::DOUBLE_ATK:
        p_rev -= cost;
        for (int i = 0; i < 2; ++i)
            p_atk > e_def ? (e_hp -= p_atk - e_def) : (e_hp -= 1);
        e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
        e_rev = 0;
        break;
    case SkillType::HEAL: {
        int heal = p_rev / 2;
        p_hp = std::min(p_hp + heal, p_max_hp);
        p_rev = 0;
        rev_halved = true;
        e_atk + e_rev > p_def ? (p_hp -= e_atk + e_rev - p_def) : (p_hp -= 1);
        e_rev = 0;
        break;
    }
    }
    return true;
}
