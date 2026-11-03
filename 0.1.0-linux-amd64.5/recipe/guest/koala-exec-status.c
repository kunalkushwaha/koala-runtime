#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define RESULT_DIRECTORY "/var/lib/koala/executions"
#define MAX_FIELDS 1024U
#define MAX_FIELD_BYTES (64U * 1024U)
#define MAX_PAYLOAD_BYTES (1024U * 1024U)

extern char **environ;
static volatile sig_atomic_t execution_group = -1;

static void forward_signal(int signal_number) {
  pid_t group = (pid_t)execution_group;
  if (group > 0) (void)kill(-group, signal_number);
}

/* SIGKILL cannot be forwarded, so the host escalates with SIGUSR2: the
   wrapper kills the whole workload group and still records its result. */
static void kill_group(int signal_number) {
  (void)signal_number;
  pid_t group = (pid_t)execution_group;
  if (group > 0) (void)kill(-group, SIGKILL);
}

static int install_forwarders(void) {
  int signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM};
  struct sigaction action = {.sa_handler = forward_signal};
  sigemptyset(&action.sa_mask);
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
    if (sigaction(signals[i], &action, NULL) != 0) return -1;
  }
  struct sigaction escalate = {.sa_handler = kill_group};
  sigemptyset(&escalate.sa_mask);
  if (sigaction(SIGUSR2, &escalate, NULL) != 0) return -1;
  return 0;
}

static void restore_forwarded_signals(void) {
  int signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR2};
  struct sigaction action = {.sa_handler = SIG_DFL};
  sigemptyset(&action.sa_mask);
  for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
    (void)sigaction(signals[i], &action, NULL);
  }
}

static int parse_u32(const char *value, uint32_t *result) {
  char *end = NULL;
  errno = 0;
  unsigned long parsed = strtoul(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed > UINT32_MAX) return -1;
  *result = (uint32_t)parsed;
  return 0;
}

static int valid_id(const char *value) {
  if (strlen(value) != 36) return 0;
  for (size_t i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (value[i] != '-') return 0;
    } else if (!((value[i] >= '0' && value[i] <= '9') ||
                 (value[i] >= 'a' && value[i] <= 'f'))) {
      return 0;
    }
  }
  return 1;
}

#define MAX_SECRET_VARIABLES 32

/* A secret variable name: POSIX, never PATH or KOALA_* (W06B2). */
static int valid_secret_name(const char *name) {
  size_t length = strlen(name);
  if (length == 0 || length > 128 || isdigit((unsigned char)name[0])) return 0;
  for (size_t i = 0; i < length; i++)
    if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) return 0;
  return strcmp(name, "PATH") != 0 && strncmp(name, "KOALA_", 6) != 0;
}

static int valid_environment(const char *value) {
  const char *separator = strchr(value, '=');
  return separator != NULL && separator != value;
}

static int ensure_result_directory(void) {
  if (mkdir("/var/lib/koala", 0700) != 0 && errno != EEXIST) return -1;
  if (mkdir(RESULT_DIRECTORY, 0700) != 0 && errno != EEXIST) return -1;
  struct stat info;
  if (lstat(RESULT_DIRECTORY, &info) != 0 || !S_ISDIR(info.st_mode) ||
      info.st_uid != 0 || (info.st_mode & 0077) != 0) return -1;
  return 0;
}

static int write_all(int fd, const void *buffer, size_t count) {
  size_t offset = 0;
  while (offset < count) {
    ssize_t written = write(fd, (const char *)buffer + offset, count - offset);
    if (written < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    offset += (size_t)written;
  }
  return 0;
}

static int persist_result(const char *id, const char *kind, int value) {
  char final_path[PATH_MAX];
  char temporary_path[PATH_MAX];
  int final_length = snprintf(final_path, sizeof(final_path), "%s/%s.result",
                              RESULT_DIRECTORY, id);
  int temporary_length = snprintf(temporary_path, sizeof(temporary_path),
                                  "%s/.%s.%ld.tmp", RESULT_DIRECTORY, id,
                                  (long)getpid());
  if (final_length <= 0 || (size_t)final_length >= sizeof(final_path) ||
      temporary_length <= 0 || (size_t)temporary_length >= sizeof(temporary_path)) return -1;

  char record[96];
  int record_length = snprintf(record, sizeof(record), "v1 %s %d\n", kind, value);
  if (record_length <= 0 || (size_t)record_length >= sizeof(record)) return -1;
  int fd = open(temporary_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd < 0) return -1;
  int result = 0;
  if (write_all(fd, record, (size_t)record_length) != 0 || fsync(fd) != 0 || close(fd) != 0) {
    result = -1;
  }
  if (result == 0 && rename(temporary_path, final_path) != 0) result = -1;
  if (result == 0) {
    int directory = open(RESULT_DIRECTORY, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0 || fsync(directory) != 0) result = -1;
    if (directory >= 0) close(directory);
  }
  if (result != 0) unlink(temporary_path);
  return result;
}

static int run(int argc, char **argv) {
  if (argc < 9 || strcmp(argv[1], "run") != 0 || !valid_id(argv[2])) return 64;
  uint32_t uid_value, gid_value, environment_count, argument_count;
  if (parse_u32(argv[3], &uid_value) != 0 || parse_u32(argv[4], &gid_value) != 0 ||
      parse_u32(argv[6], &environment_count) != 0 ||
      parse_u32(argv[7], &argument_count) != 0 || argument_count == 0 ||
      environment_count > MAX_FIELDS || argument_count > MAX_FIELDS ||
      (uint64_t)8 + environment_count + argument_count != (uint64_t)argc) return 64;

  size_t payload_bytes = 0;
  for (int i = 2; i < argc; i++) {
    size_t length = strlen(argv[i]);
    if (length > MAX_FIELD_BYTES || payload_bytes > MAX_PAYLOAD_BYTES - length) return 64;
    payload_bytes += length;
  }
  for (uint32_t i = 0; i < environment_count; i++) {
    if (!valid_environment(argv[8 + i])) return 64;
  }
  if (ensure_result_directory() != 0) return 70;

  char final_path[PATH_MAX];
  if (snprintf(final_path, sizeof(final_path), "%s/%s.result", RESULT_DIRECTORY,
               argv[2]) >= (int)sizeof(final_path)) return 70;
  if (access(final_path, F_OK) == 0) return 0;

  int errors[2];
  if (pipe2(errors, O_CLOEXEC) != 0) return 70;
  if (install_forwarders() != 0) {
    close(errors[0]);
    close(errors[1]);
    return 70;
  }
  pid_t child = fork();
  if (child == 0) {
    restore_forwarded_signals();
    (void)setpgid(0, 0);
    close(errors[0]);
    int failure = 0;
    /* Secret variables arrive in this helper's own environment, named by
       KOALA_SECRET_VARIABLES, never in argv. They are copied before the
       environment is cleared and applied after the request's variables. */
    char *secret_names[MAX_SECRET_VARIABLES], *secret_values[MAX_SECRET_VARIABLES];
    int secret_count = 0;
    const char *list = getenv("KOALA_SECRET_VARIABLES");
    char *names = list ? strdup(list) : NULL;
    if (list && !names) failure = ENOMEM;
    for (char *save = NULL, *name = names ? strtok_r(names, ",", &save) : NULL;
         failure == 0 && name; name = strtok_r(NULL, ",", &save)) {
      const char *value = getenv(name);
      if (!valid_secret_name(name) || !value || secret_count == MAX_SECRET_VARIABLES) {
        failure = EINVAL;
        break;
      }
      secret_names[secret_count] = name;
      secret_values[secret_count] = strdup(value);
      if (!secret_values[secret_count]) failure = ENOMEM; else secret_count++;
    }
    if (failure == 0 && (setgroups(0, NULL) != 0 || setgid((gid_t)gid_value) != 0 ||
        setuid((uid_t)uid_value) != 0 || chdir(argv[5]) != 0 || clearenv() != 0)) {
      failure = errno;
    }
    for (uint32_t i = 0; failure == 0 && i < environment_count; i++) {
      char *separator = strchr(argv[8 + i], '=');
      *separator = '\0';
      if (setenv(argv[8 + i], separator + 1, 1) != 0) failure = errno;
      *separator = '=';
    }
    for (int i = 0; failure == 0 && i < secret_count; i++) {
      if (setenv(secret_names[i], secret_values[i], 1) != 0) failure = errno;
      memset(secret_values[i], 0, strlen(secret_values[i]));
      free(secret_values[i]);
    }
    if (failure == 0) {
      char **command = &argv[8 + environment_count];
      execvpe(command[0], command, environ);
      failure = errno;
    }
    (void)write_all(errors[1], &failure, sizeof(failure));
    _exit(127);
  }
  close(errors[1]);
  if (child < 0) {
    int failure = errno;
    close(errors[0]);
    return persist_result(argv[2], "start_failed", failure) == 0 ? 0 : 70;
  }
  (void)setpgid(child, child);
  if (isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, child) == 0) {
    (void)kill(-child, SIGCONT);
  }
  execution_group = (sig_atomic_t)child;

  int execution_error = 0;
  ssize_t error_size;
  do { error_size = read(errors[0], &execution_error, sizeof(execution_error)); }
  while (error_size < 0 && errno == EINTR);
  close(errors[0]);
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) return 70;
  }
  execution_group = -1;
  if (error_size == (ssize_t)sizeof(execution_error)) {
    return persist_result(argv[2], "start_failed", execution_error) == 0 ? 0 : 70;
  }
  if (error_size != 0) return 70;
  if (WIFEXITED(status)) {
    return persist_result(argv[2], "exit", WEXITSTATUS(status)) == 0 ? 0 : 70;
  }
  if (WIFSIGNALED(status)) {
    return persist_result(argv[2], "signal", WTERMSIG(status)) == 0 ? 0 : 70;
  }
  return 70;
}

int main(int argc, char **argv) {
  int result = run(argc, argv);
  if (result == 64) {
    fputs("usage: koala-exec-status run ID UID GID CWD ENV_COUNT ARG_COUNT [ENV...] ARGV...\n",
          stderr);
  }
  return result;
}
