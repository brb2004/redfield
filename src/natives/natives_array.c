#include <stdlib.h>
#include "../vm.h"
#include "../obj.h"
#include "../memory.h"


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
static Value arrayGetNative(int argCount, Value* args) {
    if (argCount != 2) { runtimeError("arrayGet expects 2 arguments: array, index"); return NIL_VAL; }
    if (!IS_ARRAY(args[0])) { runtimeError("arrayGet: first argument must be an array"); return NIL_VAL; }
    if (!IS_NUMBER(args[1])) { runtimeError("arrayGet: second argument must be a number"); return NIL_VAL; }
    ObjArray* array = AS_ARRAY(args[0]);
    int index = (int)AS_NUMBER(args[1]);
    if (index < 0 || index >= array->count) { runtimeError("arrayGet: index out of bounds"); return NIL_VAL; }
    return array->items[index];
}
static Value arrayLengthNative(int argCount, Value* args) {
    if (argCount != 1) { runtimeError("arrayLength expects 1 argument"); return NIL_VAL; }
    if (!IS_ARRAY(args[0])) { runtimeError("arrayLength: argument must be an array"); return NIL_VAL; }
    return NUMBER_VAL(AS_ARRAY(args[0])->count);
}
void registerArrayNatives() {
    defineNative("len",       lenNative);
    defineNative("arrayPush", arrayPushNative);
    defineNative("arrayPop",  arrayPopNative);
    defineNative("arrayFill", arrayFillNative);
    defineNative("arrayCopy", arrayCopyNative);
    defineNative("arrayNew",  arrayNewNative);
    defineNative("arrayGet",   arrayGetNative);
    defineNative("arrayLength", arrayLengthNative);
}