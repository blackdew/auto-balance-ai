#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include "sim_rand.h"
using namespace std;

enum class EnemyMechanic {
    NONE,       // 특수 없음
    BERSERKER,  // hp 절반 이하 시 공격 확률 대폭 상승, 방어력 하락
    TURTLE,     // hp 절반 이상이면 방어 확률 대폭 상승, 이하면 공격력 하락
    VAMPIRE,    // 적이 플레이어에게 준 실제 피해의 절반만큼 자신 회복
    ARMOR,      // 처음 3턴 동안 받는 피해 1 고정
};

enum class EnemySkillType {
    POWER_STRIKE, // e_rev 전량 소모: 이번 턴 공격력에 e_rev 추가
    GUARD,        // e_rev 소모: 이번 턴 방어력 2배
};

struct EnemySkill {
    string        name;
    EnemySkillType type;
    int           rev_cost;   // -1이면 전량 소모
    int           use_chance; // 발동 확률 (0~100)
};

struct EnemyType {
    int           id;
    string        name;
    float         atk_w; // 스탯 포인트 분배 가중치 (atk_w + def_w + hp_w = 1.0)
    float         def_w;
    float         hp_w;
    EnemyMechanic mechanic;
    vector<EnemySkill> skills;

    // 메커니즘·rev·battle_turn을 종합해 행동 가중치 결정
    void action_weights(int hp, int max_hp, int e_rev, int battle_turn,
                        int& atk_w, int& def_w) const {
        atk_w = 50; def_w = 50;
        switch (mechanic) {
        case EnemyMechanic::NONE:
            // rev 충분 시 공격 선호
            if (e_rev >= 5) { atk_w = 65; def_w = 35; }
            break;
        case EnemyMechanic::BERSERKER: {
            // 잃은 HP 비율에 선형 비례 (stat_mods와 동일 기준)
            int lost_pct = (max_hp - hp) * 100 / (max_hp > 0 ? max_hp : 1);
            atk_w = 50 + 40 * lost_pct / 100; // 50(풀피) ~ 90(빈사)
            def_w = 100 - atk_w;
            // rev 충분 시 추가 공격 의욕
            if (e_rev >= 5) atk_w = min(95, atk_w + 10);
            def_w = 100 - atk_w;
            break;
        }
        case EnemyMechanic::TURTLE:
            if (hp > max_hp / 2) { atk_w = 10; def_w = 90; } // 껍질 유지
            else                 { atk_w = 70; def_w = 30; } // 껍질 해제 후 반격
            // rev 충분하면 공격 가중치 보정
            if (e_rev >= 5) { atk_w = min(90, atk_w + 15); def_w = 100 - atk_w; }
            break;
        case EnemyMechanic::VAMPIRE:
            // 기본: 공격 선호 (흡혈 발동 조건)
            atk_w = 65; def_w = 35;
            if (e_rev < 3)            { atk_w = 40; def_w = 60; } // rev 부족 → 방어로 축적
            if (hp <= max_hp / 3)     { atk_w = min(90, atk_w + 20); def_w = 100 - atk_w; } // 빈사 → 흡혈 필요
            break;
        case EnemyMechanic::ARMOR:
            if (armor_active(battle_turn)) {
                atk_w = 20; def_w = 80; // armor 유지 중: 방어로 rev 축적
            } else if (e_rev >= 5) {
                atk_w = 80; def_w = 20; // armor 해제 + rev 충분 → 강공
            } else {
                atk_w = 55; def_w = 45; // armor 해제 + rev 부족
            }
            break;
        }
    }

    // ARMOR 메커니즘 활성 여부 — 피격 3회 미만일 때 true
    bool armor_active(int armor_hits) const {
        return mechanic == EnemyMechanic::ARMOR && armor_hits < 3;
    }

    // VAMPIRE: actual_dmg/3 만큼 rev를 소모해 회복. rev 부족 시 가능한 만큼만.
    int vampire_heal(int actual_dmg, int& e_rev) const {
        if (mechanic != EnemyMechanic::VAMPIRE) return 0;
        int heal = min(actual_dmg / 3, e_rev);
        e_rev -= heal;
        return heal;
    }

    // BERSERKER: 잃은 HP 비율만큼 atk 상승, 상승량의 2/3(버림)만큼 def 하락(최소 0).
    // TURTLE: HP 절반 초과 시 atk 절반.
    void stat_mods(int hp, int max_hp, int& e_atk_eff, int& e_def_eff) const {
        switch (mechanic) {
        case EnemyMechanic::BERSERKER: {
            int atk_bonus   = e_atk_eff * (max_hp - hp) / max_hp;
            int def_penalty = atk_bonus * 2 / 3;
            e_atk_eff += atk_bonus;
            e_def_eff  = max(0, e_def_eff - def_penalty);
            break;
        }
        case EnemyMechanic::TURTLE:
            if (hp > max_hp / 2) {
                e_atk_eff = max(1, e_atk_eff / 2);
                e_def_eff *= 2;
            }
            break;
        default: break;
        }
    }

    // rev 조건과 발동 확률을 만족하는 스킬 인덱스 반환, 없으면 -1
    int pick_skill(int e_rev) const {
        vector<int> usable;
        for (int i = 0; i < (int)skills.size(); ++i) {
            int cost = (skills[i].rev_cost == -1) ? e_rev : skills[i].rev_cost;
            if (cost > 0 && e_rev >= cost && sim_rand(100) < skills[i].use_chance)
                usable.push_back(i);
        }
        if (usable.empty()) return -1;
        return usable[sim_rand((int)usable.size())];
    }

    // ── 턴 준비: 유효 스탯·행동 선택 반환 ────────────────────────────────────
    // 호출 측은 반환값만 act()에 넘기면 됨; e_rev는 스킬 소모로 내부에서 갱신됨
    struct TurnSetup {
        int  e_atk_eff;   // 이번 턴 실질 공격력
        int  e_def_eff;   // 이번 턴 실질 방어력
        int  e_choice;    // 1=공격, 2=방어
        bool skill_fired; // 적 스킬 발동 여부 (플레이어 방어력 캡 적용 판단용)
        bool armor_on;    // ARMOR 메커니즘 활성 여부
    };

    // silent=true 시 cout 출력 억제 — 시뮬에서 사용
    TurnSetup setup_turn(int hp, int max_hp, int& e_rev, int battle_turn,
                         int e_atk, int e_def, int p_rev,
                         bool silent = false) const {
        TurnSetup s;
        s.e_atk_eff  = e_atk;
        s.e_def_eff  = e_def;
        s.skill_fired = false;
        s.armor_on   = armor_active(battle_turn);

        // 메커니즘별 행동 가중치 → e_choice
        int aw, dw;
        action_weights(hp, max_hp, e_rev, battle_turn, aw, dw);
        s.e_choice = (sim_rand(aw + dw) < aw) ? 1 : 2;

        // 메커니즘별 스탯 보정 (BERSERKER/TURTLE)
        stat_mods(hp, max_hp, s.e_atk_eff, s.e_def_eff);

        // 적 스킬 발동 — e_rev 소모 후 e_atk_eff/e_def_eff/e_choice 갱신
        int esk_idx = pick_skill(e_rev);
        if (esk_idx >= 0) {
            const EnemySkill& esk = skills[esk_idx];
            int cost = (esk.rev_cost == -1) ? e_rev : esk.rev_cost;
            switch (esk.type) {
            case EnemySkillType::POWER_STRIKE:
                s.e_atk_eff += e_rev;
                s.e_choice   = 1;
                e_rev       -= cost;
                if (!silent) cout << "[" << esk.name << "] e_atk+" << cost << "!" << endl;
                break;
            case EnemySkillType::GUARD:
                s.e_def_eff *= 2;
                s.e_choice   = 2;
                e_rev       -= cost;
                if (!silent) cout << "[" << esk.name << "] e_def*2!" << endl;
                break;
            }
            s.skill_fired = true;
        }

        // ARMOR: 처음 3턴 실질 방어력을 사실상 무적으로 설정
        if (s.armor_on)
            s.e_def_eff = s.e_atk_eff + p_rev;

        return s;
    }

    // act() 호출 직후 실행; e_hp/p_hp/p_rev/e_rev를 현재 상태로 전달
    void apply_post(int& e_hp, int e_hp_before, int e_max_hp,
                    int& p_hp, int p_hp_before,
                    int& p_rev, int p_rev_before,
                    int e_choice, int battle_turn,
                    int& e_rev,
                    bool silent = false) const {
        // ARMOR: 적이 받은 피해 최대 1, 플레이어 rev 획득 최대 1
        if (armor_active(battle_turn)) {
            int dealt = e_hp_before - e_hp;
            if (dealt > 1) e_hp = e_hp_before - 1;
            int gained = p_rev - p_rev_before;
            if (gained > 1) p_rev = p_rev_before + 1;
        }
        // VAMPIRE: 플레이어에게 준 실제 피해의 1/3 회복
        if (e_choice == 1) {
            int actual_dmg = p_hp_before - p_hp;
            int heal = vampire_heal(actual_dmg, e_rev);
            if (heal > 0) {
                e_hp = min(e_hp + heal, e_max_hp);
                if (!silent) cout << "[Vampire] healed " << heal << endl;
            }
        }
    }
};

// 적 목록 반환 — turn_rpg와 별개로 이 파일에서 관리
inline vector<EnemyType> load_enemy_types() {
    return {
        { 0, "Soldier",   0.33f, 0.34f, 0.33f, EnemyMechanic::NONE,
          {} },
        { 1, "Berserker", 0.60f, 0.10f, 0.30f, EnemyMechanic::BERSERKER,
          { { "PowerStrike", EnemySkillType::POWER_STRIKE, -1, 60 } } },
        { 2, "Turtle",    0.10f, 0.50f, 0.40f, EnemyMechanic::TURTLE,
          { { "Guard", EnemySkillType::GUARD, 5, 70 } } },
        { 3, "Vampire",   0.40f, 0.20f, 0.40f, EnemyMechanic::VAMPIRE,
          {} },
        { 4, "Knight",    0.20f, 0.30f, 0.50f, EnemyMechanic::ARMOR,
          { { "Guard", EnemySkillType::GUARD, 5, 50 } } },
    };
}

// 5층 단위 보스 타입 — 인덱스 0~3이 층 5/10/15/20에 대응
inline vector<EnemyType> load_boss_types() {
    return {
        // 5층: Warlord — BERSERKER + PowerStrike, 공격 특화
        { 10, "Warlord",    0.60f, 0.15f, 0.25f, EnemyMechanic::BERSERKER,
          { { "PowerStrike", EnemySkillType::POWER_STRIKE, -1, 70 } } },
        // 10층: Iron Golem — ARMOR + Guard, 방어 특화
        { 11, "IronGolem",  0.15f, 0.50f, 0.35f, EnemyMechanic::ARMOR,
          { { "Guard", EnemySkillType::GUARD, 5, 80 } } },
        // 15층: Blood Witch — VAMPIRE + PowerStrike, 흡혈 특화
        { 12, "BloodWitch", 0.45f, 0.15f, 0.40f, EnemyMechanic::VAMPIRE,
          { { "PowerStrike", EnemySkillType::POWER_STRIKE, -1, 65 } } },
        // 20층: Dragon — BERSERKER + PowerStrike/Guard 복합, 최종 보스
        { 13, "Dragon",     0.50f, 0.20f, 0.30f, EnemyMechanic::BERSERKER,
          { { "PowerStrike", EnemySkillType::POWER_STRIKE, -1, 60 },
            { "Guard",       EnemySkillType::GUARD,         5, 50 } } },
    };
}