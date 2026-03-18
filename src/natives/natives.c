#include "natives.h"
 
void registerCoreNatives();
void registerArrayNatives();
void registerMatrixNatives();
void registerFileNatives();
void registerGLNatives();
void registerHttpNatives();
 
void registerNatives() {
    registerGLNatives();
    registerCoreNatives();
    registerArrayNatives();
    registerMatrixNatives();
    registerFileNatives();
    registerHttpNatives();
}