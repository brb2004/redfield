#include "common.h"
#include "chunk.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "glad/glad.h"  
#include <GLFW/glfw3.h> 
#include "natives.h"
#include "redfield_stdlib.h"
#include <curl/curl.h>
GLFWwindow* window = NULL;

static void repl() {
  char line[1024];
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line);
  }
}
static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}
static void runFile(const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(source);
  free(source); 

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

void initOpenGL() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(800, 600, "Redfield", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

}
int main(int argc, const char* argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);
    initVM();
    registerNatives();
    interpret(REDFIELD_STDLIB);
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        if (strcmp(argv[1], "update") == 0) {
            char registryPath[512];
            const char* home = getenv("HOME");
            if (!home) home = getenv("USERPROFILE");
            if (!home) home = ".";
            snprintf(registryPath, sizeof(registryPath), "%s/.redfield/registry.json", home);
            remove(registryPath);
            printf("Registry cache cleared. Packages will be re-fetched on next import.\n");
        } else {
            vm.currentFilePath = argv[1];
            runFile(argv[1]);
        }
    } else {
        fprintf(stderr, "Usage: redfield [path]\n");
        fprintf(stderr, "       redfield update\n");
        exit(64);
    }

  freeVM();
  glfwTerminate();
  curl_global_cleanup();
    return 0;
}