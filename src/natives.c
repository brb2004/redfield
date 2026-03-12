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
    if (file->is_Open && file->file != NULL) {
        fflush(file->file); 
    }
    return NIL_VAL;
}
static Value readCSV(int argCount, Value* args) {
    if (argCount != 1 || !IS_STRING(args[0])) {
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
    if (fgets(line, sizeof(line), file)) {
        ObjArray* headers = newArray();
        push(OBJ_VAL(headers));
        
        char* token = strtok(line, ",\n");
        while (token) {
            size_t len = strlen(token);
            if (len > 0 && (token[len-1] == '\n' || token[len-1] == '\r')) {
                token[len-1] = '\0';
            }
            
            Value header = OBJ_VAL(copyString(token, strlen(token)));
            
            if (headers->count >= headers->capacity) {
                int newCap = headers->capacity < 8 ? 8 : headers->capacity * 2;
                headers->items = GROW_ARRAY(Value, headers->items, headers->capacity, newCap);
                headers->capacity = newCap;
            }
            headers->items[headers->count++] = header;
            
            token = strtok(NULL, ",\n");
        }
        
        if (rows->count >= rows->capacity) {
            int newCap = rows->capacity < 8 ? 8 : rows->capacity * 2;
            rows->items = GROW_ARRAY(Value, rows->items, rows->capacity, newCap);
            rows->capacity = newCap;
        }
        rows->items[rows->count++] = OBJ_VAL(headers);
        
        pop();
    }
    

    while (fgets(line, sizeof(line), file)) {
        ObjArray* row = newArray();
        push(OBJ_VAL(row));
        
        char* token = strtok(line, ",\n");
        while (token) {
            size_t len = strlen(token);
            if (len > 0 && (token[len-1] == '\n' || token[len-1] == '\r')) {
                token[len-1] = '\0';
            }
            
            char* endptr;
            double num = strtod(token, &endptr);
            Value cell;
            if (*endptr == '\0') {
                cell = NUMBER_VAL(num);
            } else {
                cell = OBJ_VAL(copyString(token, strlen(token)));
            }
            
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
static Value matrixNewNative(int argCount, Value* args) {
    if (argCount != 2) {
        runtimeError("matrixNew expects 2 arguments (rows, cols)");
        return NIL_VAL;
    }
    
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        runtimeError("matrixNew: rows and cols must be numbers");
        return NIL_VAL;
    }
    
    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);
    
    if (rows <= 0 || cols <= 0) {
        runtimeError("matrixNew: rows and cols must be positive");
        return NIL_VAL;
    }
    
    ObjMatrix* matrix = newMatrix(rows, cols);
    return OBJ_VAL(matrix);
}

static Value matrixGetNative(int argCount, Value* args) {
    if (argCount != 3) {
        runtimeError("matrixGet expects 3 arguments (matrix, row, col)");
        return NIL_VAL;
    }
    
    if (!IS_MATRIX(args[0])) {
        runtimeError("matrixGet: first argument must be a matrix");
        return NIL_VAL;
    }
    
    if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
        runtimeError("matrixGet: row and col must be numbers");
        return NIL_VAL;
    }
    
    ObjMatrix* matrix = AS_MATRIX(args[0]);
    int row = (int)AS_NUMBER(args[1]);
    int col = (int)AS_NUMBER(args[2]);
    
    if (row < 0 || row >= matrix->rows || col < 0 || col >= matrix->cols) {
        runtimeError("matrixGet: index out of bounds");
        return NIL_VAL;
    }
    
    int index = row * matrix->cols + col;
    return matrix->data[index];
}

static Value matrixSetNative(int argCount, Value* args) {
    if (argCount != 4) {
        runtimeError("matrixSet expects 4 arguments (matrix, row, col, value)");
        return NIL_VAL;
    }
    
    if (!IS_MATRIX(args[0])) {
        runtimeError("matrixSet: first argument must be a matrix");
        return NIL_VAL;
    }
    
    if (!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
        runtimeError("matrixSet: row and col must be numbers");
        return NIL_VAL;
    }
    
    ObjMatrix* matrix = AS_MATRIX(args[0]);
    int row = (int)AS_NUMBER(args[1]);
    int col = (int)AS_NUMBER(args[2]);
    Value value = args[3];
    
    if (row < 0 || row >= matrix->rows || col < 0 || col >= matrix->cols) {
        runtimeError("matrixSet: index out of bounds");
        return NIL_VAL;
    }
    
    int index = row * matrix->cols + col;
    matrix->data[index] = value;
    return value;
}

static Value matrixRowsNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixRows expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* matrix = AS_MATRIX(args[0]);
    return NUMBER_VAL(matrix->rows);
}

static Value matrixColsNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixCols expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* matrix = AS_MATRIX(args[0]);
    return NUMBER_VAL(matrix->cols);
}

static Value matrixAddNative(int argCount, Value* args) {
    if (argCount != 2) {
        runtimeError("matrixAdd expects 2 matrices");
        return NIL_VAL;
    }
    
    if (!IS_MATRIX(args[0]) || !IS_MATRIX(args[1])) {
        runtimeError("matrixAdd: arguments must be matrices");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjMatrix* b = AS_MATRIX(args[1]);
    
    if (a->rows != b->rows || a->cols != b->cols) {
        runtimeError("matrixAdd: matrices must have same dimensions");
        return NIL_VAL;
    }
    
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    
    for (int i = 0; i < a->count; i++) {
        if (!IS_NUMBER(a->data[i]) || !IS_NUMBER(b->data[i])) {
            runtimeError("matrixAdd: all elements must be numbers");
            return NIL_VAL;
        }
        double av = AS_NUMBER(a->data[i]);
        double bv = AS_NUMBER(b->data[i]);
        result->data[i] = NUMBER_VAL(av + bv);
    }
    
    return OBJ_VAL(result);
}

static Value matrixMulNative(int argCount, Value* args) {
    if (argCount != 2) {
        runtimeError("matrixMul expects 2 matrices");
        return NIL_VAL;
    }
    
    if (!IS_MATRIX(args[0]) || !IS_MATRIX(args[1])) {
        runtimeError("matrixMul: arguments must be matrices");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjMatrix* b = AS_MATRIX(args[1]);
    
    if (a->cols != b->rows) {
        runtimeError("matrixMul: incompatible dimensions (%dx%d * %dx%d)",
                     a->rows, a->cols, b->rows, b->cols);
        return NIL_VAL;
    }
    
    ObjMatrix* result = newMatrix(a->rows, b->cols);
    
    // Matrix multiplication
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < b->cols; j++) {
            double sum = 0;
            for (int k = 0; k < a->cols; k++) {
                Value av = a->data[i * a->cols + k];
                Value bv = b->data[k * b->cols + j];
                
                if (!IS_NUMBER(av) || !IS_NUMBER(bv)) {
                    runtimeError("matrixMul: all elements must be numbers");
                    return NIL_VAL;
                }
                
                sum += AS_NUMBER(av) * AS_NUMBER(bv);
            }
            result->data[i * result->cols + j] = NUMBER_VAL(sum);
        }
    }
    
    return OBJ_VAL(result);
}
static Value matrixTransposeNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixTranspose expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjMatrix* result = newMatrix(a->cols, a->rows);
    
    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            int a_index = i * a->cols + j;
            int r_index = j * result->cols + i;
            result->data[r_index] = a->data[a_index];
        }
    }
    
    return OBJ_VAL(result);
}

static Value matrixElementwiseMultiplyNative(int argCount, Value* args) {
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_MATRIX(args[1])) {
        runtimeError("matrixElementwiseMultiply expects two matrices");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjMatrix* b = AS_MATRIX(args[1]);
    
    if (a->rows != b->rows || a->cols != b->cols) {
        runtimeError("matrixElementwiseMultiply: matrices must have same dimensions");
        return NIL_VAL;
    }
    
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    
    for (int i = 0; i < a->count; i++) {
        if (!IS_NUMBER(a->data[i]) || !IS_NUMBER(b->data[i])) {
            runtimeError("matrixElementwiseMultiply: all elements must be numbers");
            return NIL_VAL;
        }
        double av = AS_NUMBER(a->data[i]);
        double bv = AS_NUMBER(b->data[i]);
        result->data[i] = NUMBER_VAL(av * bv);
    }
    
    return OBJ_VAL(result);
}

static Value matrixApplyNative(int argCount, Value* args) {
    if (argCount != 2 || !IS_MATRIX(args[0]) || !IS_CLOSURE(args[1])) {
        runtimeError("matrixApply expects a matrix and a function");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjClosure* func = AS_CLOSURE(args[1]);
    
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    
    push(OBJ_VAL(result));
    push(OBJ_VAL(func));
    
    for (int i = 0; i < a->count; i++) {
        if (!IS_NUMBER(a->data[i])) {
            runtimeError("matrixApply: all elements must be numbers");
            pop();
            pop();
            return NIL_VAL;
        }
        
        push(OBJ_VAL(func));
        push(a->data[i]);
        if (!call(func, 1)) { 
            pop();
            pop();
            return NIL_VAL;
        }
        Value val = pop();
        result->data[i] = val;
    }
    
    pop();
    pop();
    return OBJ_VAL(result);
}
static Value matrixSumNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixSum expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    double sum = 0;
    
    for (int i = 0; i < a->count; i++) {
        if (!IS_NUMBER(a->data[i])) {
            runtimeError("matrixSum: all elements must be numbers");
            return NIL_VAL;
        }
        sum += AS_NUMBER(a->data[i]);
    }
    
    return NUMBER_VAL(sum);
}

static Value matrixMeanNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixMean expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    double sum = 0;
    
    for (int i = 0; i < a->count; i++) {
        if (!IS_NUMBER(a->data[i])) {
            runtimeError("matrixMean: all elements must be numbers");
            return NIL_VAL;
        }
        sum += AS_NUMBER(a->data[i]);
    }
    
    return NUMBER_VAL(sum / a->count);
}

static Value matrixRowSumNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixRowSum expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjMatrix* result = newMatrix(a->rows, 1);
    
    for (int i = 0; i < a->rows; i++) {
        double sum = 0;
        for (int j = 0; j < a->cols; j++) {
            int index = i * a->cols + j;
            if (!IS_NUMBER(a->data[index])) {
                runtimeError("matrixRowSum: all elements must be numbers");
                return NIL_VAL;
            }
            sum += AS_NUMBER(a->data[index]);
        }
        result->data[i] = NUMBER_VAL(sum);
    }
    
    return OBJ_VAL(result);
}

static Value matrixColSumNative(int argCount, Value* args) {
    if (argCount != 1 || !IS_MATRIX(args[0])) {
        runtimeError("matrixColSum expects a matrix");
        return NIL_VAL;
    }
    
    ObjMatrix* a = AS_MATRIX(args[0]);
    ObjMatrix* result = newMatrix(1, a->cols);
    
    for (int j = 0; j < a->cols; j++) {
        double sum = 0;
        for (int i = 0; i < a->rows; i++) {
            int index = i * a->cols + j;
            if (!IS_NUMBER(a->data[index])) {
                runtimeError("matrixColSum: all elements must be numbers");
                return NIL_VAL;
            }
            sum += AS_NUMBER(a->data[index]);
        }
        result->data[j] = NUMBER_VAL(sum);
    }
    
    return OBJ_VAL(result);
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
    defineNative("readCSV",       readCSV);
    defineNative("matrixNew",     matrixNewNative);
    defineNative("matrixGet",     matrixGetNative);
    defineNative("matrixSet",     matrixSetNative);
    defineNative("matrixRows",    matrixRowsNative);
    defineNative("matrixCols",    matrixColsNative);
    defineNative("matrixAdd",     matrixAddNative);
    defineNative("matrixMul",     matrixMulNative);
    defineNative("matrixTranspose",          matrixTransposeNative);
    defineNative("matrixElementwiseMultiply", matrixElementwiseMultiplyNative);
    defineNative("matrixApply",               matrixApplyNative);
    defineNative("matrixSum",                 matrixSumNative);
    defineNative("matrixMean",                matrixMeanNative);
    defineNative("matrixRowSum",              matrixRowSumNative);
    defineNative("matrixColSum",              matrixColSumNative);
    srand((unsigned int)time(NULL));
}