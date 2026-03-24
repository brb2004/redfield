#include <math.h>
#include <string.h>
#include <stddef.h>
#include "vm.h"
#include "obj.h"
#include "memory.h"
#include "../table.h"
#include "stdlib.h"
void runtimeError(const char* format, ...);

// Disable GC during matrix ops to avoid invalidating pointers
#define MATRIX_OP_BEGIN size_t _savedNextGC = vm.nextGC; vm.nextGC = (size_t)-1;
#define MATRIX_OP_END   vm.nextGC = _savedNextGC;

static ObjMatrix* getMatrix(Value val) {
    if (IS_MATRIX(val)) return AS_MATRIX(val);
    if (IS_INSTANCE(val)) {
        // Walk fields table looking for "handle"
        Table* fields = &AS_INSTANCE(val)->fields;
        for (int i = 0; i < fields->capacity; i++) {
            Entry* entry = &fields->entries[i];
            if (entry->key != NULL &&
                entry->key->length == 6 &&
                memcmp(entry->key->chars, "handle", 6) == 0 &&
                IS_MATRIX(entry->value)) {
                return AS_MATRIX(entry->value);
            }
        }
    }
    return NULL;
}

static Value wrapMatrix(ObjMatrix* mat) {
    // mat must already be on the stack by caller
    Value matrixClass;
    // Find Matrix class without allocating a new string
    Table* globals = &vm.globals;
    for (int i = 0; i < globals->capacity; i++) {
        Entry* entry = &globals->entries[i];
        if (entry->key != NULL &&
            entry->key->length == 6 &&
            memcmp(entry->key->chars, "Matrix", 6) == 0 &&
            IS_CLASS(entry->value)) {
            matrixClass = entry->value;
            goto found;
        }
    }
    return OBJ_VAL(mat); // fallback
found:;
    ObjInstance* instance = newInstance(AS_CLASS(matrixClass));
    push(OBJ_VAL(instance));
    // Set handle field without allocating new string if possible
    ObjString* handleKey = NULL;
    Table* ifields = &instance->fields;
    // Find or create handle key
    for (int i = 0; i < globals->capacity; i++) {
        Entry* e = &globals->entries[i];
        if (e->key != NULL && e->key->length == 6 && memcmp(e->key->chars, "handle", 6) == 0) {
            handleKey = e->key; break;
        }
    }
    if (handleKey == NULL) handleKey = copyString("handle", 6);
    tableSet(ifields, handleKey, OBJ_VAL(mat));
    pop();
    return OBJ_VAL(instance);
}

static Value matrixNewNative(int argCount, Value* args) {
    (void)argCount;
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) { runtimeError("matrixNew: rows and cols must be numbers"); return NIL_VAL; }
    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);
    if (rows <= 0 || cols <= 0) { runtimeError("matrixNew: rows and cols must be positive"); return NIL_VAL; }
    return OBJ_VAL(newMatrix(rows, cols));
}

static Value matrixGetNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* matrix = getMatrix(args[0]);
    if (!matrix) { runtimeError("matrixGet: first argument must be a matrix"); return NIL_VAL; }
    int row = (int)AS_NUMBER(args[1]);
    int col = (int)AS_NUMBER(args[2]);
    if (row < 0 || row >= matrix->rows || col < 0 || col >= matrix->cols) {
        runtimeError("matrixGet: index out of bounds (%d,%d) in %dx%d", row, col, matrix->rows, matrix->cols);
        return NIL_VAL;
    }
    return matrix->data[row * matrix->cols + col];
}

static Value matrixSetNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* matrix = getMatrix(args[0]);
    if (!matrix) { runtimeError("matrixSet: first argument must be a matrix"); return NIL_VAL; }
    int row = (int)AS_NUMBER(args[1]);
    int col = (int)AS_NUMBER(args[2]);
    if (row < 0 || row >= matrix->rows || col < 0 || col >= matrix->cols) { runtimeError("matrixSet: index out of bounds"); return NIL_VAL; }
    matrix->data[row * matrix->cols + col] = args[3];
    return args[3];
}

static Value matrixRowsNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* m = getMatrix(args[0]);
    if (!m) { runtimeError("matrixRows expects a matrix"); return NIL_VAL; }
    return NUMBER_VAL(m->rows);
}

static Value matrixColsNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* m = getMatrix(args[0]);
    if (!m) { runtimeError("matrixCols expects a matrix"); return NIL_VAL; }
    return NUMBER_VAL(m->cols);
}

static Value matrixAddNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); ObjMatrix* b = getMatrix(args[1]);
    if (!a || !b) { MATRIX_OP_END runtimeError("matrixAdd: arguments must be matrices"); return NIL_VAL; }
    if (a->rows != b->rows || a->cols != b->cols) { MATRIX_OP_END runtimeError("matrixAdd: dimension mismatch (%dx%d vs %dx%d)", a->rows, a->cols, b->rows, b->cols); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) result->data[i] = NUMBER_VAL(AS_NUMBER(a->data[i]) + AS_NUMBER(b->data[i]));
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixSubNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); ObjMatrix* b = getMatrix(args[1]);
    if (!a || !b) { MATRIX_OP_END runtimeError("matrixSub: arguments must be matrices"); return NIL_VAL; }
    if (a->rows != b->rows || a->cols != b->cols) { MATRIX_OP_END runtimeError("matrixSub: dimension mismatch (%dx%d vs %dx%d)", a->rows, a->cols, b->rows, b->cols); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) result->data[i] = NUMBER_VAL(AS_NUMBER(a->data[i]) - AS_NUMBER(b->data[i]));
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixScaleNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a || !IS_NUMBER(args[1])) { MATRIX_OP_END runtimeError("matrixScale expects a matrix and a number"); return NIL_VAL; }
    double scalar = AS_NUMBER(args[1]);
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) result->data[i] = NUMBER_VAL(AS_NUMBER(a->data[i]) * scalar);
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixMulNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); ObjMatrix* b = getMatrix(args[1]);
    if (!a || !b) { MATRIX_OP_END runtimeError("matrixMul: arguments must be matrices"); return NIL_VAL; }
    if (a->cols != b->rows) { MATRIX_OP_END runtimeError("matrixMul: incompatible dimensions (%dx%d * %dx%d)", a->rows, a->cols, b->rows, b->cols); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, b->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->rows; i++)
        for (int j = 0; j < b->cols; j++) {
            double sum = 0;
            for (int k = 0; k < a->cols; k++)
                sum += AS_NUMBER(a->data[i * a->cols + k]) * AS_NUMBER(b->data[k * b->cols + j]);
            result->data[i * result->cols + j] = NUMBER_VAL(sum);
        }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixTransposeNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { MATRIX_OP_END runtimeError("matrixTranspose expects a matrix"); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->cols, a->rows);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->rows; i++)
        for (int j = 0; j < a->cols; j++)
            result->data[j * result->cols + i] = a->data[i * a->cols + j];
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixElementwiseMultiplyNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); ObjMatrix* b = getMatrix(args[1]);
    if (!a || !b) { MATRIX_OP_END runtimeError("matrixElementwiseMultiply expects two matrices"); return NIL_VAL; }
    if (a->rows != b->rows || a->cols != b->cols) { MATRIX_OP_END runtimeError("matrixElementwiseMultiply: dimension mismatch"); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) result->data[i] = NUMBER_VAL(AS_NUMBER(a->data[i]) * AS_NUMBER(b->data[i]));
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixSumNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { runtimeError("matrixSum expects a matrix"); return NIL_VAL; }
    double sum = 0;
    for (int i = 0; i < a->count; i++) sum += AS_NUMBER(a->data[i]);
    return NUMBER_VAL(sum);
}

static Value matrixMeanNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { runtimeError("matrixMean expects a matrix"); return NIL_VAL; }
    double sum = 0;
    for (int i = 0; i < a->count; i++) sum += AS_NUMBER(a->data[i]);
    return NUMBER_VAL(sum / a->count);
}

static Value matrixRowSumNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { MATRIX_OP_END runtimeError("matrixRowSum expects a matrix"); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, 1);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->rows; i++) {
        double sum = 0;
        for (int j = 0; j < a->cols; j++) sum += AS_NUMBER(a->data[i * a->cols + j]);
        result->data[i] = NUMBER_VAL(sum);
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixColSumNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { MATRIX_OP_END runtimeError("matrixColSum expects a matrix"); return NIL_VAL; }
    ObjMatrix* result = newMatrix(1, a->cols);
    push(OBJ_VAL(result));
    for (int j = 0; j < a->cols; j++) {
        double sum = 0;
        for (int i = 0; i < a->rows; i++) sum += AS_NUMBER(a->data[i * a->cols + j]);
        result->data[j] = NUMBER_VAL(sum);
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixSigmoidNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) { double x = AS_NUMBER(a->data[i]); result->data[i] = NUMBER_VAL(1.0/(1.0+exp(-x))); }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}


static Value matrixSigmoidDerivFromOutputNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) {
        double s = AS_NUMBER(a->data[i]);
        result->data[i] = NUMBER_VAL(s * (1.0 - s));
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixReluNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) { double x = AS_NUMBER(a->data[i]); result->data[i] = NUMBER_VAL(x > 0 ? x : 0); }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixReluDerivNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) { double x = AS_NUMBER(a->data[i]); result->data[i] = NUMBER_VAL(x > 0 ? 1.0 : 0.0); }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixRandomNative(int argCount, Value* args) {
    if (argCount != 4) { runtimeError("matrixRandom expects 4 arguments: rows, cols, min, max"); return NIL_VAL; }
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2]) || !IS_NUMBER(args[3])) {
        runtimeError("matrixRandom expects all arguments to be numbers"); return NIL_VAL;
    }
    int rows = (int)AS_NUMBER(args[0]);
    int cols = (int)AS_NUMBER(args[1]);
    double min = AS_NUMBER(args[2]);
    double max = AS_NUMBER(args[3]);
    if (rows <= 0 || cols <= 0) { runtimeError("matrixRandom: rows and cols must be positive"); return NIL_VAL; }
    if (min > max) { runtimeError("matrixRandom: min cannot be greater than max"); return NIL_VAL; }
    ObjMatrix* result = newMatrix(rows, cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < result->count; i++) {
        double r = ((double)rand() / RAND_MAX) * (max - min) + min;
        result->data[i] = NUMBER_VAL(r);
    }
    Value wrapped = wrapMatrix(result);
    pop();
    return wrapped;
}
static Value matrixTanhNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) {
        double x = AS_NUMBER(a->data[i]);
        result->data[i] = NUMBER_VAL(tanh(x));
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}

static Value matrixTanhDerivFromOutputNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) {
        double s = AS_NUMBER(a->data[i]);
        result->data[i] = NUMBER_VAL(1.0 - s * s);
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixSqrtNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) {
        double x = AS_NUMBER(a->data[i]);
        result->data[i] = NUMBER_VAL(sqrt(x));
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixAddScalarNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a || !IS_NUMBER(args[1])) { MATRIX_OP_END return NIL_VAL; }
    double scalar = AS_NUMBER(args[1]);
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++)
        result->data[i] = NUMBER_VAL(AS_NUMBER(a->data[i]) + scalar);
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixDivideNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]); ObjMatrix* b = getMatrix(args[1]);
    if (!a || !b) { MATRIX_OP_END return NIL_VAL; }
    if (a->rows != b->rows || a->cols != b->cols) { MATRIX_OP_END runtimeError("matrixDivide: dimension mismatch"); return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++)
        result->data[i] = NUMBER_VAL(AS_NUMBER(a->data[i]) / AS_NUMBER(b->data[i]));
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixSoftmaxNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    double sum = 0;
    for (int i = 0; i < a->count; i++) {
        double x = exp(AS_NUMBER(a->data[i]));
        result->data[i] = NUMBER_VAL(x);
        sum += x;
    }
    for (int i = 0; i < a->count; i++)
        result->data[i] = NUMBER_VAL(AS_NUMBER(result->data[i]) / sum);
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixArgmaxNative(int argCount, Value* args) {
    (void)argCount;
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { runtimeError("matrixArgmax expects a matrix"); return NIL_VAL; }
    int maxIndex = 0;
    double maxValue = AS_NUMBER(a->data[0]);
    for (int i = 1; i < a->count; i++) {
        double val = AS_NUMBER(a->data[i]);
        if (val > maxValue) { maxValue = val; maxIndex = i; }
    }
    return NUMBER_VAL(maxIndex);
}

static Value matrixClipNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) { MATRIX_OP_END runtimeError("matrixClip expects a matrix and two numbers"); return NIL_VAL; }
    double mn = AS_NUMBER(args[1]);
    double mx = AS_NUMBER(args[2]);
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) {
        double x = AS_NUMBER(a->data[i]);
        if (x < mn) x = mn;
        else if (x > mx) x = mx;
        result->data[i] = NUMBER_VAL(x);
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
static Value matrixLogNative(int argCount, Value* args) {
    (void)argCount;
    MATRIX_OP_BEGIN
    ObjMatrix* a = getMatrix(args[0]);
    if (!a) { MATRIX_OP_END return NIL_VAL; }
    ObjMatrix* result = newMatrix(a->rows, a->cols);
    push(OBJ_VAL(result));
    for (int i = 0; i < a->count; i++) {
        double x = AS_NUMBER(a->data[i]);
        result->data[i] = NUMBER_VAL(log(x));
    }
    Value wrapped = wrapMatrix(result); pop();
    MATRIX_OP_END return wrapped;
}
void registerMatrixNatives() {
    defineNative("matrixNew",                 matrixNewNative);
    defineNative("matrixGet",                 matrixGetNative);
    defineNative("matrixSet",                 matrixSetNative);
    defineNative("matrixRows",                matrixRowsNative);
    defineNative("matrixCols",                matrixColsNative);
    defineNative("matrixAdd",                 matrixAddNative);
    defineNative("matrixSub",                 matrixSubNative);
    defineNative("matrixScale",               matrixScaleNative);
    defineNative("matrixMul",                 matrixMulNative);
    defineNative("matrixTranspose",           matrixTransposeNative);
    defineNative("matrixElementwiseMultiply", matrixElementwiseMultiplyNative);
    defineNative("matrixSum",                 matrixSumNative);
    defineNative("matrixMean",                matrixMeanNative);
    defineNative("matrixRowSum",              matrixRowSumNative);
    defineNative("matrixColSum",              matrixColSumNative);
    defineNative("matrixSigmoid",             matrixSigmoidNative);
    defineNative("matrixSigmoidDeriv",        matrixSigmoidDerivFromOutputNative);
    defineNative("matrixRelu",                matrixReluNative);
    defineNative("matrixReluDeriv",           matrixReluDerivNative);
    defineNative("matrixRandom",              matrixRandomNative);
    defineNative("matrixTanh",                matrixTanhNative);
    defineNative("matrixTanhDeriv",           matrixTanhDerivFromOutputNative);
    defineNative("matrixSqrt",                matrixSqrtNative);
    defineNative("matrixAddScalar",          matrixAddScalarNative);
    defineNative("matrixDivide",             matrixDivideNative);
    defineNative("matrixSoftmax",            matrixSoftmaxNative);
    defineNative("matrixArgmax",             matrixArgmaxNative);
    defineNative("matrixClip",               matrixClipNative);
    defineNative("matrixLog",                matrixLogNative);
}