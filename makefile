# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -pthread

# Target Executable
TARGET = server

# Source Files
SRC = server.c

# Object Files
OBJ = $(SRC:.c=.o)

# Build Rules
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(OBJ) $(TARGET)

# Run the server
run: $(TARGET)
	./$(TARGET)