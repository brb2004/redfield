CC     = gcc
CXX    = g++
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -Isrc -Ilib/glad/include -Wno-unused-parameter
CXXFLAGS = -std=c++17 -Isrc -Ilib/glad/include -Wno-unused-parameter \
           $(shell llvm-config-18 --cxxflags) -Wno-unused-parameter -fno-rtti

# C sources (exclude editor and jit bridge — compiled separately)
C_SRC   = $(filter-out src/editor.c, $(wildcard src/*.c))
C_OBJ   = $(C_SRC:.c=.o)

# C++ sources (LLVM codegen)
CXX_SRC = $(wildcard src/*.cpp)
CXX_OBJ = $(CXX_SRC:.cpp=.o)

# Natives (C only)
NAT_SRC = $(wildcard src/natives/*.c)
NAT_OBJ = $(NAT_SRC:.c=.o)

OBJ     = $(C_OBJ) $(CXX_OBJ) $(NAT_OBJ)
BIN_DIR = bin

LLVM_LIBS = $(shell llvm-config-18 --ldflags --libs core executionengine mcjit native orcjit)

# Detect OS
ifeq ($(OS),Windows_NT)
    BIN  = $(BIN_DIR)/redfield.exe
    LIBS = -lglfw3 -lopengl32 -lgdi32 -lcurl -lm -mconsole
else
    BIN  = $(BIN_DIR)/redfield
    LIBS = -lglfw -lGL -ldl -lm -lcurl -lstdc++
endif

all: src/redfield_stdlib.h $(BIN)

src/redfield_stdlib.h: lib/stdlib.rf
	python3 -c "import json; content=open('lib/stdlib.rf').read(); print('const char* REDFIELD_STDLIB = ' + json.dumps(content) + ';')" > src/redfield_stdlib.h

$(BIN): $(OBJ)
	mkdir -p $(BIN_DIR)
	gcc -Ilib/glad/include -c lib/glad/src/glad.c -o lib/glad/src/glad.o
	$(CXX) $(OBJ) lib/glad/src/glad.o -o $(BIN) $(LIBS) $(LLVM_LIBS)

# C compilation
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# C++ compilation
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Natives
src/natives/%.o: src/natives/%.c
	$(CC) $(CFLAGS) -Isrc/natives -c $< -o $@

clean:
	rm -f src/*.o src/natives/*.o lib/glad/src/glad.o $(BIN) src/redfield_stdlib.h

.PHONY: all clean