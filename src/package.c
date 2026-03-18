#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include "cJSON.h"


char* rfFetch(const char* url, const char* method, const char* body);

static void getRedfieldDir(char* out, size_t size) {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) home = ".";
    snprintf(out, size, "%s/.redfield", home);
}
static int fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}
static void mkdirRecursive(const char* path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}
static int saveToFile(const char* path, const char* data) {
    FILE* file = fopen(path, "w");
    if (!file) return 0;
    fputs(data, file);
    fclose(file);
    return 1;
}
char* resolvePackage(const char* name) {
    char cacheDir[1024];
    char cachePath[2048];
    char dir[512];
    getRedfieldDir(dir, sizeof(dir));
    snprintf(cacheDir, sizeof(cacheDir), "%s/packages/%s", dir, name);
    snprintf(cachePath, sizeof(cachePath), "%s/init.rf", cacheDir);
    if (fileExists(cachePath)) return strdup(cachePath);
    char url[512];
    snprintf(url, sizeof(url), "https://raw.githubusercontent.com/brb2004/redfield-registry/main/%s/init.rf", name);
    char* result = rfFetch(url, "GET", NULL);
    if (!result) return NULL;
    if (strncmp(result, "404", 3) == 0) {
        fprintf(stderr, "Package '%s' not found\n", name);
        free(result);
        return NULL;
    }
    mkdirRecursive(cacheDir);
    saveToFile(cachePath, result);
    free(result);
    return strdup(cachePath);
}