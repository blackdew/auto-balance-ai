#pragma once
#include <vector>
#include "skill.h"
#include "enemy_types.h"

// 전투 관련 누적 통계 — AI fallback 확률 계산에 사용
struct BattleStats {
    int atk_count = 0;
    int def_count = 0;
    int skl_count = 0;
};

// 캐릭터 class — 플레이어와 적 공통
class Character {
public:
    int  level      = 1;
    int  atk        = 0;
    int  def        = 0;
    int  max_hp     = 0;
    int  hp         = 0;
    int  rev        = 0;       // revenge gauge
    bool rev_halved = false;   // set after Heal skill; halves rev gain

    std::vector<Skill> skills; // player only
    EnemyType etype = { 0, "Soldier", 0.33f, 0.34f, 0.33f, EnemyMechanic::NONE 
    };
};
