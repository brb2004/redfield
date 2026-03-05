#ifndef redfield_compiler_h
#define redfield_compiler_h

#include "vm.h"
#include "obj.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif