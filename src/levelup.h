#pragma once
#include <iostream>
#include <limits>
#include <string>
#include "character.h"

// 레벨업 처리: 스킬 선택 + 스탯 포인트 분배
inline void levelup_player(Character& player){
    int points = player.level;

    // 보유하지 않은 스킬 중 하나 선택
    std::cout << "=== Skill Select ===" << std::endl;
    std::vector<int> available;
    for (int i = 0; i < (int)SKILL_LIST.size(); ++i) {
        bool owned = false;
        for (auto& s : player.skills)
            if (s.type == SKILL_LIST[i].type) { owned = true; break; }
        if (!owned) available.push_back(i);
    }
    if (!available.empty()) {
        for (int i = 0; i < (int)available.size(); ++i) {
            const Skill& sk = SKILL_LIST[available[i]];
            std::string cost_str = sk.rev_cost == -1 ? "all" : std::to_string(sk.rev_cost);
            std::cout << i + 1 << "." << sk.name << " (rev cost: " << cost_str << ")" << std::endl;
        }
        std::cout << available.size() + 1 << ".pass" << std::endl;
        int sc; std::cin >> sc;
        if (sc >= 1 && sc <= (int)available.size())
            player.skills.push_back(SKILL_LIST[available[sc - 1]]);
    }
    std::cout << "=================" << std::endl;

    // 스탯 포인트 분배 — level만큼 포인트 지급
    while (points > 0) {
        int choice = 0, amount = 0;
        std::cout << "point: " << points << std::endl;
        std::cout << "------------------------------------------------" << std::endl;
        std::cout << "1.hp(" << player.max_hp << ")\t|2.str(" << player.atk << ")\t|3.def(" << player.def << ")" << std::endl;
        std::cin >> choice;
        std::cout << "point:";
        std::cin >> amount;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "error" << std::endl;
        }
        else if (amount <= 0 || amount > points) {
            std::cout << "low point" << std::endl;
        }
        else if (choice == 1) { player.max_hp += amount; points -= amount; }
        else if (choice == 2) { player.atk    += amount; points -= amount; }
        else if (choice == 3) { player.def    += amount; points -= amount; }
        else { std::cout << "error" << std::endl; }
        std::cout << "------------------------------------------------" << std::endl;
    }
}