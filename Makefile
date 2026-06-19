# Auto-Balance AI — build
# 표준 C++17 컴파일러만 있으면 됩니다. (외부 의존성 0)

# 컴파일러 재정의: make CXX=g++  (기본은 시스템 c++ — Linux=g++, macOS=clang++)
CXXFLAGS ?= -std=c++17 -O2 -Wall
SRC      := src/turn_rpg.cpp
BIN      := src/turn_rpg

.PHONY: all run demo clean

all: $(BIN)

$(BIN): $(SRC) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

# 대화형 플레이
run: $(BIN)
	cd src && ./turn_rpg

# 자동 밸런싱 AI 가시화 데모 (비대화형)
demo: $(BIN)
	cd src && ./turn_rpg --demo

clean:
	rm -f $(BIN)
