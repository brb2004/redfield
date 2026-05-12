#include <stdio.h>
#include <string.h>
#include "vm.h"
#include "obj.h"
#include "memory.h"

void runTimeError(const char* format, ...);

static Value readLineNative(int argCount, Value* args)
{
    (void)argCount; (void)args;
    char buffer[65536];
    if(fgets(buffer, sizeof(buffer), stdin) == NULL)
    {
        return NIL_VAL;
    }
    int len = (int)strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') len--;
    if (len > 0 && buffer[len-1] == '\r') len --;
    return OBJ_VAL(copyString(buffer, len));
} 

static Value writeNative(int argCount, Value* args)
{
    if (argCount < 1 || !IS_STRING(args[0]))
    {
        runTimeError("write expects some shit idk");
        return NIL_VAL;
    }
    fputs(AS_CSTRING(args[0]), stdout);
    fflush(stdout);
    return NIL_VAL;
}
static Value writeLineNative(int argCount, Value*args)
{
    if(argCount < 1 || !IS_STRING(args[0]))
    {
        runtimeError("idk man it broke");
        return NIL_VAL;
    }
    fputs(AS_CSTRING(args[0]), stdout);
    fputc('\n', stdout);
    fflush(stdout);
    return NIL_VAL;
}

static Value writeErrNative(int argCount, Value* args)
{
    if(argCount < 1 || !IS_STRING(args[0]))
    {
        runtimeError("shits broke");
        return NIL_VAL;
    }
    fputs(AS_CSTRING(args[0]), stdout);
    fputc('\n', stderr);
    fflush(stderr);
    return NIL_VAL;
}

void registerIONatives()
{
    defineNative("readLine", readLineNative);
    defineNative("write", writeNative);
    defineNative("writeLine", writeLineNative);
    defineNative("writeErr", writeErrNative);
}