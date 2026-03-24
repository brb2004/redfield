#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "../vm/vm.h"
#include "obj.h"
#include "memory.h"

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
    double r  = (double)rand() / ((double)RAND_MAX + 1.0);
    return NUMBER_VAL(lo + r * (hi - lo));
}
static Value stringToNumNative(int argCount, Value* args) {
    (void)argCount;
    ObjString* str = AS_STRING(args[0]);
    return NUMBER_VAL(atof(str->chars));
}
static Value csvReadNative(int argCount, Value* args) {
    (void)argCount;
    ObjString* path = AS_STRING(args[0]);
    FILE* file = fopen(path->chars, "r");
    if (!file) return NIL_VAL;

    ObjArray* rows = newArray();
    push(OBJ_VAL(rows));

    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        // Strip newline
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len > 0 && line[len-1] == '\r') line[--len] = '\0';

        ObjArray* row = newArray();
        push(OBJ_VAL(row));

        char* p = line;
        while (1) {
            char* comma = strchr(p, ',');
            int fieldLen = comma ? (int)(comma - p) : (int)strlen(p);
            ObjString* field = copyString(p, fieldLen);
            if (row->count >= row->capacity) {
                int newCap = row->capacity < 8 ? 8 : row->capacity * 2;
                row->items = GROW_ARRAY(Value, row->items, row->capacity, newCap);
                row->capacity = newCap;
            }
            row->items[row->count++] = OBJ_VAL(field);
            if (!comma) break;
            p = comma + 1;
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
static Value modNative(int argCount, Value* args) {
    (void)argCount;
    double a = AS_NUMBER(args[0]);
    double b = AS_NUMBER(args[1]);
    return NUMBER_VAL(fmod(a, b));
}
void registerCoreNatives() {
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
    defineNative("stringToNum", stringToNumNative);
    defineNative("csvRead",    csvReadNative);
    defineNative("rand",       randNative);
    defineNative("randRange",  randRangeNative);
    defineNative("mod",        modNative);
    srand((unsigned int)time(NULL));
}