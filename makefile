CC = gcc
CXXFLAGS = -Wall -Wextra -std=c++17 -Ilib/glad/include -Ilib/cJSON -Isrc
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -Isrc -Ilib/glad/include -Ilib/cJSON -Wno-unused-parameter

BIN_DIR = bin

ifeq ($(OS),Windows_NT)
	BIN       = $(BIN_DIR)/redfield.exe
	LIBS      = -lglfw3 -lopengl32 -lgdi32 -lcurl -lm -lwinmm -lws2_32 -lcrypt32 -lwldap32
	MKDIR     = mkdir -p
	STDLIB_GEN = python -c "import json; content=open('lib/stdlib.rf').read(); print('const char* REDFIELD_STDLIB = ' + json.dumps(content) + ';')"
else
	BIN       = $(BIN_DIR)/redfield
	LIBS      = -lglfw -lGL -ldl -lm -lcurl
	MKDIR     = mkdir -p
	STDLIB_GEN = python3 -c "import json; content=open('lib/stdlib.rf').read(); print('const char* REDFIELD_STDLIB = ' + json.dumps(content) + ';')"
endif

C_SRC   = $(filter-out src/editor.c, $(wildcard src/*.c) $(wildcard src/natives/*.c))
CXX_SRC = $(wildcard src/*.cpp)

C_OBJ   = $(C_SRC:.c=.o)
CXX_OBJ = $(CXX_SRC:.cpp=.o)
OBJ     = $(C_OBJ) $(CXX_OBJ) lib/cJSON/cJSON.o lib/glad/src/glad.o

all: src/redfield_stdlib.h $(BIN)

src/redfield_stdlib.h: lib/stdlib.rf
	$(STDLIB_GEN) > src/redfield_stdlib.h || true

lib/cJSON/cJSON.o: lib/cJSON/cJSON.c
	$(CC) -std=c99 -D__USE_MINGW_ANSI_STDIO=1 -Ilib/cJSON -c $< -o $@

lib/glad/src/glad.o: lib/glad/src/glad.c
	$(CC) -Ilib/glad/include -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.cpp
	g++ $(CXXFLAGS) -c $< -o $@

src/natives/%.o: src/natives/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJ)
	$(MKDIR) $(BIN_DIR)
	g++ $(OBJ) -o $(BIN) $(LIBS)

clean:
	rm -f src/*.o src/natives/*.o lib/glad/src/glad.o lib/cJSON/cJSON.o $(BIN) src/redfield_stdlib.h

install: all
	cp $(BIN) /usr/local/bin/redfield

.PHONY: all clean install