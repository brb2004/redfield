#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include "vm.h"
#include "natives.h"
#include "obj.h"
#include "memory.h"

void runtimeError(const char* format, ...);

extern GLFWwindow* window;
extern void initOpenGL();
#define ENSURE_WINDOW() do { if (window == NULL) initOpenGL(); } while(0)

static Value windowShouldCloseNative(int argCount, Value* args) {
    (void)argCount; (void)args;
    ENSURE_WINDOW();
    return BOOL_VAL(glfwWindowShouldClose(window));
}
static Value pollEventsNative(int argCount, Value* args) {
    (void)argCount; (void)args;
    ENSURE_WINDOW();
    glfwPollEvents();
    return NIL_VAL;
}
static Value swapBuffersNative(int argCount, Value* args) {
    (void)argCount; (void)args;
    ENSURE_WINDOW();
    glfwSwapBuffers(window);
    return NIL_VAL;
}
static Value clearColorNative(int argCount, Value* args) {
    (void)argCount;
    ENSURE_WINDOW();
    float r = (float)AS_NUMBER(args[0]);
    float g = (float)AS_NUMBER(args[1]);
    float b = (float)AS_NUMBER(args[2]);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return NIL_VAL;
}

static Value sinNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(sin(AS_NUMBER(args[0]))); }
static Value cosNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(cos(AS_NUMBER(args[0]))); }
static Value tanNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(tan(AS_NUMBER(args[0]))); }
static Value sqrtNative(int argCount, Value* args)  { (void)argCount; return NUMBER_VAL(sqrt(AS_NUMBER(args[0]))); }
static Value powNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1]))); }
static Value absNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(fabs(AS_NUMBER(args[0]))); }
static Value floorNative(int argCount, Value* args) { (void)argCount; return NUMBER_VAL(floor(AS_NUMBER(args[0]))); }
static Value ceilNative(int argCount, Value* args)  { (void)argCount; return NUMBER_VAL(ceil(AS_NUMBER(args[0]))); }
static Value atan2Native(int argCount, Value* args) { (void)argCount; return NUMBER_VAL(atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1]))); }
static Value logNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(log(AS_NUMBER(args[0]))); }
static Value acosNative(int argCount, Value* args)  { (void)argCount; return NUMBER_VAL(acos(AS_NUMBER(args[0]))); }
static Value asinNative(int argCount, Value* args)  { (void)argCount; return NUMBER_VAL(asin(AS_NUMBER(args[0]))); }
static Value atanNative(int argCount, Value* args)  { (void)argCount; return NUMBER_VAL(atan(AS_NUMBER(args[0]))); }
static Value expNative(int argCount, Value* args)   { (void)argCount; return NUMBER_VAL(exp(AS_NUMBER(args[0]))); }
static Value log2Native(int argCount, Value* args)  { (void)argCount; return NUMBER_VAL(log2(AS_NUMBER(args[0]))); }
static Value log10Native(int argCount, Value* args) { (void)argCount; return NUMBER_VAL(log10(AS_NUMBER(args[0]))); }
static Value roundNative(int argCount, Value* args) { (void)argCount; return NUMBER_VAL(round(AS_NUMBER(args[0]))); }
static Value hypotNative(int argCount, Value* args) { (void)argCount; return NUMBER_VAL(hypot(AS_NUMBER(args[0]), AS_NUMBER(args[1]))); }

static Value toStringNative(int argCount, Value* args) {
    (void)argCount;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(args[0]));
    return OBJ_VAL(copyString(buffer, strlen(buffer)));
}

static Value randNative(int argCount, Value* args) {
    (void)argCount; (void)args;
    return NUMBER_VAL((double)rand() / ((double)RAND_MAX + 1.0));
}

static Value randRangeNative(int argCount, Value* args) {
    (void)argCount;
    double lo = AS_NUMBER(args[0]);
    double hi = AS_NUMBER(args[1]);
    double r   = (double)rand() / ((double)RAND_MAX + 1.0);
    return NUMBER_VAL(lo + r * (hi - lo));
}

static Value lenNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_ARRAY(args[0])) return NUMBER_VAL(0);
    return NUMBER_VAL((double)AS_ARRAY(args[0])->count);
}

static Value arrayPushNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    ObjArray* array = AS_ARRAY(args[0]);
    if (array->count >= array->capacity) {
        int newCap = array->capacity < 8 ? 8 : array->capacity * 2;
        array->items    = GROW_ARRAY(Value, array->items, array->capacity, newCap);
        array->capacity = newCap;
    }
    array->items[array->count++] = args[1];
    return NIL_VAL;
}

static Value arrayPopNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    ObjArray* array = AS_ARRAY(args[0]);
    if (array->count == 0) return NIL_VAL;
    return array->items[--array->count];
}

static Value arrayFillNative(int argCount, Value* args) {
    (void)argCount;
    int size = (int)AS_NUMBER(args[0]);
    Value fill = args[1];
    ObjArray* array = newArray();
    
    push(OBJ_VAL(array));
    if (size > 0) {
        array->items    = GROW_ARRAY(Value, array->items, 0, size);
        array->capacity = size;
        for (int i = 0; i < size; i++) array->items[i] = fill;
        array->count = size;
    }
    pop();
    return OBJ_VAL(array);
}

static Value arrayCopyNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    ObjArray* src = AS_ARRAY(args[0]);
    ObjArray* dst = newArray();
    push(OBJ_VAL(dst));
    if (src->count > 0) {
        dst->items    = GROW_ARRAY(Value, dst->items, 0, src->count);
        dst->capacity = src->count;
        dst->count    = src->count;
        for (int i = 0; i < src->count; i++) dst->items[i] = src->items[i];
    }
    pop();
    return OBJ_VAL(dst);
}

static Value arrayNewNative(int argCount, Value* args) {
    (void)argCount;
    int size = (int)AS_NUMBER(args[0]);
    ObjArray* array = newArray();
    push(OBJ_VAL(array));
    if (size > 0) {
        array->items    = GROW_ARRAY(Value, array->items, 0, size);
        array->capacity = size;
        for (int i = 0; i < size; i++) array->items[i] = NIL_VAL;
        array->count = size;
    }
    pop();
    return OBJ_VAL(array);
}
static Value matRows(int argCount, Value* args) {
    (void)argCount;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    ObjArray* array = AS_ARRAY(args[0]);
    return NUMBER_VAL((double)array->count);
}
static Value matCols(int argCount, Value* args) {
    (void)argCount;
    if (!IS_ARRAY(args[0])) return NIL_VAL;
    ObjArray* array = AS_ARRAY(args[0]);
    if (array->count == 0) return NUMBER_VAL(0);
    if (!IS_ARRAY(array->items[0])) return NIL_VAL;
    ObjArray* row = AS_ARRAY(array->items[0]);
    return NUMBER_VAL((double)row->count);
}
static Value matGetNative(int argCount, Value* args) {
    (void)argCount;
    ObjArray* outer = AS_ARRAY(args[0]);
    int r = (int)AS_NUMBER(args[1]);
    int c = (int)AS_NUMBER(args[2]);
    ObjArray* row = AS_ARRAY(outer->items[r]);
    return row->items[c];
}
static Value matSetNative(int argCount, Value* args) {
    (void)argCount;
    ObjArray* outer = AS_ARRAY(args[0]);
    int r = (int)AS_NUMBER(args[1]);
    int c = (int)AS_NUMBER(args[2]);
    Value val = args[3];
    ObjArray* row = AS_ARRAY(outer->items[r]);
    row->items[c] = val;
    return row->items[c];
}
static Value matrix2dNative(int argCount, Value* args) {
    (void)argCount;
    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);

    ObjArray* outer = newArray();
    push(OBJ_VAL(outer));  

    for (int i = 0; i < rows; i++) {
        ObjArray* row = newArray();
        push(OBJ_VAL(row));  

        for (int j = 0; j < cols; j++) {
            
            Value zero = NUMBER_VAL(0);
            if (row->count >= row->capacity) {
                int newCap = row->capacity < 8 ? 8 : row->capacity * 2;
                row->items    = GROW_ARRAY(Value, row->items, row->capacity, newCap);
                row->capacity = newCap;
            }
            row->items[row->count++] = zero;
        }

        
        if (outer->count >= outer->capacity) {
            int newCap = outer->capacity < 8 ? 8 : outer->capacity * 2;
            outer->items    = GROW_ARRAY(Value, outer->items, outer->capacity, newCap);
            outer->capacity = newCap;
        }
        outer->items[outer->count++] = OBJ_VAL(row);

        pop();  
    }

    pop();  
    return OBJ_VAL(outer);
}
static Value fileOpenNative(int argCount, Value* args) {
    if (argCount < 1 || argCount > 2) {
        runtimeError("fileOpen expects 1-2 arguments (path, mode)");
        return NIL_VAL;
    }
    
    // Get filename
    if (!IS_STRING(args[0])) {
        runtimeError("fileOpen: filename must be a string");
        return NIL_VAL;
    }
    const char* filename = AS_CSTRING(args[0]);
    
    // Get mode (default to "r")
    const char* mode = "r";
    if (argCount == 2) {
        if (!IS_STRING(args[1])) {
            runtimeError("fileOpen: mode must be a string");
            return NIL_VAL;
        }
        mode = AS_CSTRING(args[1]);
    }
    
    // Open file
    FILE* file = fopen(filename, mode);
    if (file == NULL) {
        // Return nil and let the script check with fileIsOpen
        return NIL_VAL;
    }
    
    // Create file object
    ObjFile* file_obj = newFile(file, mode);
    return OBJ_VAL(file_obj);
}

static Value fileCloseNative(int argCount, Value* args) {
    if (argCount != 1) {
        runtimeError("fileClose expects 1 argument");
        return NIL_VAL;
    }
    
    if (!IS_FILE(args[0])) {
        runtimeError("fileClose expects a file object");
        return NIL_VAL;
    }
    
    ObjFile* file = AS_FILE(args[0]);
    if (file->is_Open && file->file != NULL) {
        fclose(file->file);
        file->file = NULL;
        file->is_Open = false;
    }
    
    return NIL_VAL;
}

static Value fileReadNative(int argCount, Value* args) {
    if (argCount != 1) {
        runtimeError("fileRead expects 1 argument");
        return NIL_VAL;
    }
    
    if (!IS_FILE(args[0])) {
        runtimeError("fileRead expects a file object");
        return NIL_VAL;
    }
    
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileRead: file is not open");
        return NIL_VAL;
    }
    
    // Read entire file
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
    if (argCount != 1) {
        runtimeError("fileReadLine expects 1 argument");
        return NIL_VAL;
    }
    
    if (!IS_FILE(args[0])) {
        runtimeError("fileReadLine expects a file object");
        return NIL_VAL;
    }
    
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileReadLine: file is not open");
        return NIL_VAL;
    }
    
    // Read line (simple approach - you might want a growing buffer)
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), file->file) == NULL) {
        if (feof(file->file)) {
            return NIL_VAL;  // EOF
        }
        runtimeError("fileReadLine: read error");
        return NIL_VAL;
    }
    
    return OBJ_VAL(copyString(buffer, strlen(buffer)));
}

static Value fileWriteNative(int argCount, Value* args) {
    if (argCount != 2) {
        runtimeError("fileWrite expects 2 arguments (file, string)");
        return NIL_VAL;
    }
    
    if (!IS_FILE(args[0])) {
        runtimeError("fileWrite: first argument must be a file");
        return NIL_VAL;
    }
    
    if (!IS_STRING(args[1])) {
        runtimeError("fileWrite: second argument must be a string");
        return NIL_VAL;
    }
    
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileWrite: file is not open");
        return NIL_VAL;
    }
    
    const char* text = AS_CSTRING(args[1]);
    fprintf(file->file, "%s", text);
    
    return NIL_VAL;
}

static Value fileWriteLineNative(int argCount, Value* args) {
    if (argCount != 2) {
        runtimeError("fileWriteLine expects 2 arguments (file, string)");
        return NIL_VAL;
    }
    
    if (!IS_FILE(args[0])) {
        runtimeError("fileWriteLine: first argument must be a file");
        return NIL_VAL;
    }
    
    if (!IS_STRING(args[1])) {
        runtimeError("fileWriteLine: second argument must be a string");
        return NIL_VAL;
    }
    
    ObjFile* file = AS_FILE(args[0]);
    if (!file->is_Open || file->file == NULL) {
        runtimeError("fileWriteLine: file is not open");
        return NIL_VAL;
    }
    
    const char* text = AS_CSTRING(args[1]);
    fprintf(file->file, "%s\n", text);
    
    return NIL_VAL;
}

static Value fileIsOpenNative(int argCount, Value* args) {
    if (argCount != 1) {
        runtimeError("fileIsOpen expects 1 argument");
        return NIL_VAL;
    }
    
    if (!IS_FILE(args[0])) {
        return BOOL_VAL(false);
    }
    
    ObjFile* file = AS_FILE(args[0]);
    return BOOL_VAL(file->is_Open && file->file != NULL);
}

static Value fileExistsNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_STRING(args[0])) {
        runtimeError("fileExists expects a string filename");
        return NIL_VAL;
    }
    
    const char* filename = AS_CSTRING(args[0]);
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}
static Value fileFlushNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_FILE(args[0])) return NIL_VAL;
    ObjFile* file = AS_FILE(args[0]);
    if (file->is_open && file->file != NULL) {
        fflush(file->file);  // Force write to disk
    }
    return NIL_VAL;
}
void registerNatives() {
    
    defineNative("windowShouldClose", windowShouldCloseNative);
    defineNative("pollEvents",        pollEventsNative);
    defineNative("swapBuffers",       swapBuffersNative);
    defineNative("clearColor",        clearColorNative);

    
    defineNative("sin",        sinNative);
    defineNative("cos",        cosNative);
    defineNative("tan",        tanNative);
    defineNative("asin",       asinNative);
    defineNative("acos",       acosNative);
    defineNative("atan",       atanNative);
    defineNative("atan2",      atan2Native);
    defineNative("sqrt",       sqrtNative);
    defineNative("pow",        powNative);
    defineNative("exp",        expNative);
    defineNative("log",        logNative);
    defineNative("log2",       log2Native);
    defineNative("log10",      log10Native);
    defineNative("abs",        absNative);
    defineNative("floor",      floorNative);
    defineNative("ceil",       ceilNative);
    defineNative("round",      roundNative);
    defineNative("hypot",      hypotNative);
    defineNative("numToString",toStringNative);
    defineNative("rand",       randNative);
    defineNative("randRange",  randRangeNative);

    
    defineNative("len",        lenNative);
    defineNative("arrayPush",  arrayPushNative);
    defineNative("arrayPop",   arrayPopNative);
    defineNative("arrayFill",  arrayFillNative);
    defineNative("arrayCopy",  arrayCopyNative);
    defineNative("arrayNew",   arrayNewNative);
    defineNative("matrix2d",   matrix2dNative);
    defineNative("matGet", matGetNative);
    defineNative("matSet", matSetNative);
    defineNative("matRows", matRows);
    defineNative("matCols", matCols);

    defineNative("fileOpen",      fileOpenNative);
    defineNative("fileClose",     fileCloseNative);
    defineNative("fileRead",      fileReadNative);
    defineNative("fileReadLine",  fileReadLineNative);
    defineNative("fileWrite",     fileWriteNative);
    defineNative("fileWriteLine", fileWriteLineNative);
    defineNative("fileIsOpen",    fileIsOpenNative);
    defineNative("fileExists",    fileExistsNative);
    defineNative("fileFlush",     fileFlushNative);
    srand((unsigned int)time(NULL));
}