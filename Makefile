# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -g

# Target executable
TARGET = shaditor

# Source files
SRCS = ./src/main.c ./src/appendbuffer.c ./src/terminalutil.c
OBJS = $(SRCS:.cpp=.o)

# Default rule to build the target
all: $(TARGET)

# Rule to build the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Rule to compile source files into object files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build with -DDEBUG
debug: CFLAGS += -DDEBUG
debug: $(TARGET)

# Rule to clean build files
clean:
	rm -f $(OBJS) $(TARGET)


