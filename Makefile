CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread
TARGET = lsm_db
SRC = src/main.cpp src/db.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p data
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
	rm -rf data/*

run: all
	./$(TARGET)