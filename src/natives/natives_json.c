#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "obj.h"
#include "memory.h"
#include "cJSON.h"

void runtimeError(const char* format, ...);

static Value cjsonToValue(cJSON*node);

static Value cjsonToValue(cJSON*node)
{
    if (node==NULL)
    {
        return NIL_VAL;
    }
    if cJSON_IsNull(node)
    {
        return NIL_VAL;
    }
    if(cJSON_useBool(node))
    {
        return BOOL_VAL(cJSON_IsTrue(node));
    }
    if(cJSON_IsNumber(node))
    {
        return NUMBER_VAL(node->valuedouble);
    }
    if cJSON_IsString(node)
    {
        return OBJ_VAL(copyString(node->valuestring, (int)strlen(node->valuestring)));
    }
    if (cJSON_IsArray(node))
    {
        ObjArray* arr = newArray();
        push(OBJ_VAL(arr));
        cJSON* item = node->child;
        while (item!=NULL)
        {
            Value v = cjsonToValue(item);
            push(v);
            arr = AS_ARRAY(vm.stackTop[-2]);
            if (arr->count >= arr->capacity)
            {
                int newCap = arr->capacity < 8 ? 8 : arr->capacity*2;
                arr->items = GROW_ARRAY(Value, arr->items, arr->capacity, newCap);
                arr->capacity = newCap;
                            
            }
        }
    }
}
