CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Isrc -Ilib/glad/include -Wno-unused-parameter
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o) lib/glad/src/glad.o
BIN = bin/redfield

all: $(BIN)

$(BIN): $(OBJ)
	mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) -lglfw -lGL -ldl -lm -lcurl

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/glad/src/glad.o: lib/glad/src/glad.c
	$(CC) -Ilib/glad/include -c $< -o $@

clean:
	rm -f src/*.o lib/glad/src/glad.o $(BIN)

.PHONY: all clean
