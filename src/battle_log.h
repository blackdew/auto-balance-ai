#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

// CSV 한 행: 행동 직전 상태 + 플레이어 선택
struct LogRow {
    int p_max_hp, p_hp;
    int e_max_hp, e_hp;
    int atk_diff; // p_atk - e_atk
    int def_diff; // p_def - e_def
    int p_rev, e_rev;
    int action;   // 1=공격, 2=방어, 3=스킬
    int e_id;     // 적 타입 id
};

static const std::string CSV_PATH = "battle_log.csv";

// CSV 헤더 한 줄 기록
inline void csv_write_header(std::ofstream& f) {
    f << "p_max_hp,p_hp,e_max_hp,e_hp,atk_diff,def_diff,p_rev,e_rev,action,e_id\n";
}

// 파일이 없으면 헤더만 있는 CSV 생성
inline void csv_init() {
    std::ifstream check(CSV_PATH);
    if (!check.good()) {
        std::ofstream f(CSV_PATH);
        csv_write_header(f);
        std::cout << "[CSV] " << CSV_PATH << " created." << std::endl;
    }
}

// 행동 1건을 CSV에 추가 — 파일 핸들을 세션 동안 유지
inline void csv_log(const LogRow& r) {
    static std::ofstream f(CSV_PATH, std::ios::app);
    f << r.p_max_hp << "," << r.p_hp     << ","
      << r.e_max_hp << "," << r.e_hp     << ","
      << r.atk_diff << "," << r.def_diff << ","
      << r.p_rev    << "," << r.e_rev    << ","
      << r.action   << "," << r.e_id     << "\n";
}

// CSV 전체 로드 — 파일 없으면 빈 벡터 반환
inline std::vector<LogRow> csv_load() {
    std::vector<LogRow> rows;
    std::ifstream f(CSV_PATH);
    if (!f.good()) return rows;
    std::string line;
    std::getline(f, line); // 헤더 스킵
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        LogRow r;
        char comma;
        ss >> r.p_max_hp >> comma >> r.p_hp     >> comma
           >> r.e_max_hp >> comma >> r.e_hp     >> comma
           >> r.atk_diff >> comma >> r.def_diff >> comma
           >> r.p_rev    >> comma >> r.e_rev    >> comma
           >> r.action   >> comma >> r.e_id;
        rows.push_back(r);
    }
    return rows;
}
