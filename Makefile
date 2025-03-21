CC = g++
CFLAGS = -Wall -Wextra -g

# Source and object files
SRC = main.cpp utils.cpp
OBJ = $(SRC:.cpp=.o)

# Output executable
TARGET = program

# Default rule
all: run clean

# Link object files to create the final executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

# Compile .cpp files into .o files
%.o: %.cpp utils.h
	$(CC) $(CFLAGS) -c $< -o $@

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean up compiled files
clean:
	@rm -f $(OBJ) $(TARGET)
