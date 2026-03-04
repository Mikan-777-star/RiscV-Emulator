# Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2

# 最終的に作りたい実行ファイル名
TARGET = rv32i_emu

# コンパイルするソースファイル
SRCS = main.cpp CPU.cpp

# オブジェクトファイル（自動生成）
OBJS = $(SRCS:.cpp=.o)

# デフォルトのターゲット
all: $(TARGET)

# リンク処理
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 各.cppから.oを作るコンパイル処理
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# コンパイル結果の掃除
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean