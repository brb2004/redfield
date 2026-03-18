CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -Isrc -Ilib/glad/include -Wno-unused-parameter -Ilib/cJSON
SRC = $(filter-out src/editor.c, $(wildcard src/*.c) $(wildcard src/natives/*.c)) lib/cJSON/cJSON.c

OBJ = $(SRC:.c=.o)
BIN_DIR = bin

# Detect OS
ifeq ($(OS),Windows_NT)
    BIN = $(BIN_DIR)/redfield.exe
    LIBS = -lglfw3 -lopengl32 -lgdi32 -lcurl -lm -mconsole
else
    BIN = $(BIN_DIR)/redfield
    LIBS = -lglfw -lGL -ldl -lm -lcurl
endif

all: src/redfield_stdlib.h $(BIN)

src/redfield_stdlib.h: lib/stdlib.rf
	python3 -c "import json; content=open('lib/stdlib.rf').read(); print('const char* REDFIELD_STDLIB = ' + json.dumps(content) + ';')" > src/redfield_stdlib.h

$(BIN): $(OBJ)
	mkdir -p $(BIN_DIR)
	gcc -Ilib/glad/include -c lib/glad/src/glad.c -o lib/glad/src/glad.o
	$(CC) $(OBJ) lib/glad/src/glad.o -o $(BIN) $(LIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o lib/glad/src/glad.o $(BIN) src/redfield_stdlib.h

install: all
	cp $(BIN) /usr/local/bin/redfield

.PHONY: all clean install