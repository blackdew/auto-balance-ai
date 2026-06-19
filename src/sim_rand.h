#pragma once
#include <random>

// 시뮬 스레드 전용 난수 엔진 — 스레드마다 독립 인스턴스
inline thread_local std::mt19937 sim_rng{ std::random_device{}() };

// rand() % n 과 동일한 결과, 시뮬 내부 전용
inline int sim_rand(int n) {
    return std::uniform_int_distribution<int>{ 0, n - 1 }(sim_rng);
}
