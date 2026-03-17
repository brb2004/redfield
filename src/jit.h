#ifndef redfield_jit_h
#define redfield_jit_h

#ifdef __cplusplus
extern "C" {
#endif

#include "obj.h"
#include "chunk.h"

void jitInit();
void jitShutdown();
int jitExecute(ObjFunction* function);
void jitDefineGlobal(ObjString* name, Value value);
Value jitGetGlobal(ObjString* name);
void jitSetGlobal(ObjString* name, Value value);
#ifdef __cplusplus
}
#endif

#endif