//
// Created by hariharan on 8/8/22.
//
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <dftracer/dftracer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int bar();
void foo() {
  DFTRACER_C_FUNCTION_START();
  DFTRACER_C_FUNCTION_UPDATE_INT("key", 0);
  DFTRACER_C_FUNCTION_UPDATE_STR("key", "0");
  usleep(1000);
  DFTRACER_C_REGION_START(CUSTOM);
  DFTRACER_C_REGION_UPDATE_INT(CUSTOM, "key", 0);
  DFTRACER_C_REGION_UPDATE_STR(CUSTOM, "key", "0");
  bar();
  usleep(1000);
  DFTRACER_C_REGION_START(CUSTOM_BLOCK);
  usleep(1000);
  DFTRACER_C_REGION_END(CUSTOM_BLOCK);
  DFTRACER_C_REGION_END(CUSTOM);
  DFTRACER_C_FUNCTION_END();
}

int main(int argc, char* argv[]) {
  int init = 0;
  if (argc > 2) {
    if (strcmp(argv[2], "1") == 0) {
      DFTRACER_C_INIT(NULL, NULL, NULL);
      init = 1;
    }
  }
  DFTRACER_C_METADATA(meta, "key", "value");
  char filename[1024];
  sprintf(filename, "%s/demofile_c.txt", argv[1]);
  foo();
  FILE* fh = fopen(filename, "w+");
  fwrite("hello", sizeof("hello"), 1, fh);
  int child_pid = fork();  // fork a duplicate process
  if (child_pid == 0) {
    // we are the child process (fork returns 0 in child)
    // Do not printf here: child and parent share stdout; if the pipe to ctest is full,
    // the child can block on printf and never reach execv, so the parent blocks in waitpid.
    // Clear LD_PRELOAD and tracer env so exec'd program (e.g. /bin/ls) does not
    // load the preload library; otherwise ls exit runs preload destructors and can block.
    unsetenv("LD_PRELOAD");
    unsetenv("DFTRACER_ENABLE");
    unsetenv("DFTRACER_INIT");
    unsetenv("DFTRACER_LOG_FILE");
    unsetenv("DFTRACER_DATA_DIR");
    unsetenv("DFTRACER_CHRONOLOG_HOST");
    unsetenv("DFTRACER_CHRONOLOG_PORT");
    char* arr[] = {"ls", "-l", NULL};
    execv("/bin/ls", arr);
    // execv only returns on failure; exit without atexit/cleanup to avoid blocking
    _exit(127);
  }
  int pid = getpid();
  int child_ppid = getppid();
  printf("child_pid:%d ppid:%d pid:%d\n", child_pid, child_ppid, pid);
  int status = -1;
  waitpid(child_pid, &status, WEXITED);
  fclose(fh);
  if (init) {
    DFTRACER_C_FINI();
  }
  return 0;
}

// Add definition for bar() to fix the error
int bar() {
  // Dummy implementation
  return 0;
}
