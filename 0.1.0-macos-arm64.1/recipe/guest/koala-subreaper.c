#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fputs("usage: koala-subreaper PROGRAM [ARG...]\n", stderr);
    return 64;
  }
  if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) == -1) {
    perror("koala-subreaper: prctl");
    return 70;
  }
  execv(argv[1], &argv[1]);
  perror("koala-subreaper: execv");
  return 126;
}
