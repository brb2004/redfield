#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "../vm.h"
#include "../obj.h"
#include "../memory.h"

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
    defineNative("rand",       randNative);
    defineNative("randRange",  randRangeNative);
    srand((unsigned int)time(NULL));
}