#pragma once
#include <string>
#include <vector>

enum class SkillType {
    REV_ATK,
    COUNTER,
    POWER_DEF,
    DOUBLE_ATK,
    HEAL,
};

struct Skill {
    std::string name;
    SkillType   type;
    int         rev_cost; // -1 = use all rev
};

inline const std::vector<Skill> SKILL_LIST = {
    { "RevAtk",    SkillType::REV_ATK,     -1 },
    { "Counter",   SkillType::COUNTER,     -1 },
    { "PowerDef",  SkillType::POWER_DEF,    5 },
    { "DoubleAtk", SkillType::DOUBLE_ATK,   8 },
    { "Heal",      SkillType::HEAL,         -1 },
};
