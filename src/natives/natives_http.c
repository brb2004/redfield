#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "vm.h"
#include "obj.h"
#include "memory.h"

void runtimeError(const char* format, ...);
char *rfFetch(const char* url, const char* method, const char* body);
typedef struct {
    char*  data;
    size_t size;
} ResponseBuffer;

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t incoming = size * nmemb;
    ResponseBuffer* buf = (ResponseBuffer*)userdata;
    buf->data = (char*)realloc(buf->data, buf->size + incoming + 1);
    if (!buf->data) {
        runtimeError("Failed to allocate memory for HTTP response");
        return 0;
    }
    memcpy(buf->data + buf->size, ptr, incoming);
    buf->size += incoming;
    buf->data[buf->size] = '\0';
    return incoming;
}

char* rfFetch(const char* url, const char* method, const char* body) {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    if (!curl) return NULL;

    ResponseBuffer buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        free(buf.data);
        buf.data = NULL;
    }
    curl_easy_cleanup(curl);
    return buf.data;
}
static Value httpGetNative(int argCount, Value* args) {
    if (!IS_STRING(args[0])) {
        runtimeError("Expected a string URL as argument");
        return NIL_VAL;
    }
    char* result = rfFetch(AS_STRING(args[0])->chars, "GET", NULL);
    if (result == NULL) {
        return NIL_VAL;
    }
    ObjString* s = copyString(result, strlen(result));
    Value resultValue = OBJ_VAL(s);
    return resultValue;
}

static Value httpPostNative(int argCount, Value* args) {
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        runtimeError("Expected a string URL and a string body as arguments");
        return NIL_VAL;
    }
    char* result = rfFetch(AS_STRING(args[0])->chars, "POST", AS_STRING(args[1])->chars);
    if (result == NULL) {
        return NIL_VAL;
    }
    ObjString* s = copyString(result, strlen(result));
    Value resultValue = OBJ_VAL(s);
    return resultValue; 
}

void registerHttpNatives()
{
    defineNative("httpGet", httpGetNative);
    defineNative("httpPost", httpPostNative);
}