CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -Isrc
TARGET   = asm86
SRC      = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.bin

run-example1: $(TARGET)
	./$(TARGET) examples/example1.asm --listing

run-example2: $(TARGET)
	./$(TARGET) examples/example2.asm --listing

run-example3: $(TARGET)
	./$(TARGET) examples/example3.asm --listing

test: $(TARGET)
	@echo "\n--- Test 1: Basic Arithmetic ---"
	./$(TARGET) examples/example1.asm
	@echo "\n--- Test 2: Loop ---"
	./$(TARGET) examples/example2.asm
	@echo "\n--- Test 3: Subroutine ---"
	./$(TARGET) examples/example3.asm

.PHONY: all clean run-example1 run-example2 run-example3 test