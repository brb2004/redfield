#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm.h"
#include "../obj.h"
#include "../memory.h"

void runtimeError(const char* format, ...);

static Value fileOpenNative(int argCount, Value* args) {
    if (argCount < 1 || argCount > 2) {
        runtimeError("fileOpen expects 1-2 arguments (path, mode)");
        return NIL_VAL;
    }
    if (!IS_STRING(args[0])) {
        runtimeError("fileOpen: filename must be a string");
        return NIL_VAL;
    }
    const char* filename = AS_CSTRING(args[0]);
    const char* mode = "r";
    if (argCount == 2) {
        if (!IS_STRING(args[1])) {
            runtimeError("fileOpen: mode must be a string");
            return NIL_VAL;
        }
        mode = AS_CSTRING(args[1]);
    }
    FILE* file = fopen(filename, mode);
    if (file == NULL) return NIL_VAL;
    return OBJ_VAL(newFile(file, mode));
}

static Value fileCloseNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (file->is_Open && file->file != NULL) {
        fclose(file->file);
        file->file = NULL;
        file->is_Open = false;
    }
    return NIL_VAL;
}

static Value fileReadNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileRead: file is not open");
        return NIL_VAL;
    }
    fseek(file->file, 0, SEEK_END);
    long size = ftell(file->file);
    fseek(file->file, 0, SEEK_SET);
    char* buffer = ALLOCATE(char, size + 1);
    size_t bytes_read = fread(buffer, 1, size, file->file);
    buffer[bytes_read] = '\0';
    ObjString* result = copyString(buffer, bytes_read);
    FREE_ARRAY(char, buffer, size + 1);
    return OBJ_VAL(result);
}

static Value fileReadLineNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileReadLine: file is not open");
        return NIL_VAL;
    }
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), file->file) == NULL) return NIL_VAL;
    return OBJ_VAL(copyString(buffer, strlen(buffer)));
}

static Value fileWriteNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0]) || !IS_STRING(args[1])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileWrite: file is not open");
        return NIL_VAL;
    }
    fprintf(file->file, "%s", AS_CSTRING(args[1]));
    return NIL_VAL;
}

static Value fileWriteLineNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0]) || !IS_STRING(args[1])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileWriteLine: file is not open");
        return NIL_VAL;
    }
    fprintf(file->file, "%s\n", AS_CSTRING(args[1]));
    return NIL_VAL;
}

static Value fileIsOpenNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0])) return BOOL_VAL(false);
    ObjFile* file = AS_FILE(args[0]);
    return BOOL_VAL(file->is_Open && file->file != NULL);
}

static Value fileExistsNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_STRING(args[0])) return NIL_VAL;
    FILE* file = fopen(AS_CSTRING(args[0]), "r");
    if (file) { fclose(file); return BOOL_VAL(true); }
    return BOOL_VAL(false);
}

static Value fileFlushNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_FILE(args[0])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (file->is_Open && file->file != NULL) fflush(file->file);
    return NIL_VAL;
}

static Value readCSVNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_STRING(args[0])) {
        runtimeError("readCSV expects a string filename");
        return NIL_VAL;
    }
    const char* filename = AS_CSTRING(args[0]);
    FILE* file = fopen(filename, "r");
    if (!file) {
        runtimeError("Could not open file '%s'", filename);
        return NIL_VAL;
    }
    ObjArray* rows = newArray();
    push(OBJ_VAL(rows));
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        ObjArray* row = newArray();
        push(OBJ_VAL(row));
        char* token = strtok(line, ",\n");
        while (token) {
            size_t len = strlen(token);
            if (len > 0 && (token[len-1] == '\n' || token[len-1] == '\r'))
                token[len-1] = '\0';
            char* endptr;
            double num = strtod(token, &endptr);
            Value cell = (*endptr == '\0') ? NUMBER_VAL(num)
                                           : OBJ_VAL(copyString(token, strlen(token)));
            if (row->count >= row->capacity) {
                int newCap = row->capacity < 8 ? 8 : row->capacity * 2;
                row->items = GROW_ARRAY(Value, row->items, row->capacity, newCap);
                row->capacity = newCap;
            }
            row->items[row->count++] = cell;
            token = strtok(NULL, ",\n");
        }
        if (rows->count >= rows->capacity) {
            int newCap = rows->capacity < 8 ? 8 : rows->capacity * 2;
            rows->items = GROW_ARRAY(Value, rows->items, rows->capacity, newCap);
            rows->capacity = newCap;
        }
        rows->items[rows->count++] = OBJ_VAL(row);
        pop();
    }
    fclose(file);
    pop();
    return OBJ_VAL(rows);
}

void registerFileNatives() {
    defineNative("fileOpen",      fileOpenNative);
    defineNative("fileClose",     fileCloseNative);
    defineNative("fileRead",      fileReadNative);
    defineNative("fileReadLine",  fileReadLineNative);
    defineNative("fileWrite",     fileWriteNative);
    defineNative("fileWriteLine", fileWriteLineNative);
    defineNative("fileIsOpen",    fileIsOpenNative);
    defineNative("fileExists",    fileExistsNative);
    defineNative("fileFlush",     fileFlushNative);
    defineNative("readCSV",       readCSVNative);
}