CXX = g++
CXXFLAGS = -std=c++23 -g -Iinclude -Wall -Wextra -pthread 

SRC = $(wildcard cppsrc/*.cpp)
OBJ = $(SRC:.cpp=.o)
TARGET = Engine

.PHONY: all clean

# Default target to build
all: $(TARGET)

# Rule for creating the target
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) -pthread -Wl,--subsystem,console

# Rule for compiling individual .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up the object files and target executable
clean:
	rm -f $(OBJ) $(TARGET)
