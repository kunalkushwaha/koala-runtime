#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define SOCKET_PATH "/run/koala-host-exec.sock"
#define PROTOCOL_V2_MAGIC 0x4b4f4132U
#define PROTOCOL_V3_MAGIC 0x4b4f4133U
#define PROTOCOL_V4_MAGIC 0x4b4f4134U
#define MAX_COMMAND (1024U * 1024U)
#define MAX_FIELD (64U * 1024U)
#define MAX_FIELDS 1024U
#define MAX_OUTPUT (2U * 1024U * 1024U)
#define MAX_RUNTIME_MS (300U * 1000U)
/* v4: at most the wrapper's secret-variable limit. */
#define MAX_SECRETS 32U
#define EXEC_PATH "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

static volatile sig_atomic_t control_socket = -1;

static void forward_control_signal(int signal_number) {
  int fd = (int)control_socket;
  unsigned char value = (unsigned char)signal_number;
  if (fd >= 0) {
    ssize_t ignored = write(fd, &value, 1);
    (void)ignored;
  }
}

/* v4 also forwards SIGUSR2, the worker's escalation: the wrapper kills the
   workload group and still records the result. */
static int set_control_signal_handlers(int forward_usr2) {
  int signals[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR2};
  size_t count = forward_usr2 ? 5 : 4;
  struct sigaction action = {.sa_handler = forward_control_signal};
  sigemptyset(&action.sa_mask);
  for (size_t i = 0; i < count; i++) {
    if (sigaction(signals[i], &action, NULL) != 0) return -1;
  }
  return 0;
}

static int transfer(int from, int to, size_t count) {
  char buffer[16384];
  while (count > 0) {
    size_t wanted = count < sizeof(buffer) ? count : sizeof(buffer);
    ssize_t n = read(from, buffer, wanted);
    if (n == 0) return -1;
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    size_t offset = 0;
    while (offset < (size_t)n) {
      ssize_t written = write(to, buffer + offset, (size_t)n - offset);
      if (written < 0) {
        if (errno == EINTR) continue;
        return -1;
      }
      offset += (size_t)written;
    }
    count -= (size_t)n;
  }
  return 0;
}

static int read_exact(int fd, void *buffer, size_t count) {
  size_t offset = 0;
  while (offset < count) {
    ssize_t n = read(fd, (char *)buffer + offset, count - offset);
    if (n == 0) return -1;
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    offset += (size_t)n;
  }
  return 0;
}

static int write_exact(int fd, const void *buffer, size_t count) {
  size_t offset = 0;
  while (offset < count) {
    ssize_t n = write(fd, (const char *)buffer + offset, count - offset);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    offset += (size_t)n;
  }
  return 0;
}

static char **read_structured_command(int client, uint32_t count, uint32_t *populated_out) {
  if (count < 7 || count > MAX_FIELDS) return NULL;
  char **command = calloc((size_t)count + 3, sizeof(char *));
  if (command == NULL) return NULL;
  command[0] = "/usr/local/sbin/koala-exec-status";
  command[1] = "run";
  size_t total = 0;
  uint32_t populated = 0;
  for (; populated < count; populated++) {
    uint32_t encoded_length;
    if (read_exact(client, &encoded_length, sizeof(encoded_length)) != 0) break;
    uint32_t length = ntohl(encoded_length);
    if (length == 0 || length > MAX_FIELD || total > MAX_COMMAND - length) break;
    command[populated + 2] = calloc((size_t)length + 1, 1);
    if (command[populated + 2] == NULL ||
        read_exact(client, command[populated + 2], length) != 0) {
      if (command[populated + 2] != NULL) populated++;
      break;
    }
    total += length;
  }
  *populated_out = populated;
  if (populated == count) return command;
  for (uint32_t i = 0; i < populated; i++) free(command[i + 2]);
  free(command);
  return NULL;
}

static void free_structured_command(char **command, uint32_t populated) {
  if (command == NULL) return;
  for (uint32_t i = 0; i < populated; i++) free(command[i + 2]);
  free(command);
}

static int send_descriptors(int socket_fd, const int *descriptors, size_t count) {
  char marker = 'F';
  struct iovec iov = {.iov_base = &marker, .iov_len = 1};
  union {
    struct cmsghdr align;
    char bytes[CMSG_SPACE(5 * sizeof(int))];
  } control = {0};
  if (count > 5) return -1;
  struct msghdr message = {0};
  message.msg_iov = &iov;
  message.msg_iovlen = 1;
  message.msg_control = control.bytes;
  message.msg_controllen = CMSG_SPACE(count * sizeof(int));
  struct cmsghdr *header = CMSG_FIRSTHDR(&message);
  header->cmsg_level = SOL_SOCKET;
  header->cmsg_type = SCM_RIGHTS;
  header->cmsg_len = CMSG_LEN(count * sizeof(int));
  memcpy(CMSG_DATA(header), descriptors, count * sizeof(int));
  return sendmsg(socket_fd, &message, MSG_NOSIGNAL) == 1 ? 0 : -1;
}

static int send_standard_fds(int socket_fd) {
  int descriptors[3] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
  return send_descriptors(socket_fd, descriptors, 3);
}

/* Receives exactly COUNT descriptors; any other number is refused. Received
   descriptors are closed on refusal. */
static int receive_descriptors(int socket_fd, int *descriptors, size_t count) {
  char marker = 0;
  struct iovec iov = {.iov_base = &marker, .iov_len = 1};
  union {
    struct cmsghdr align;
    char bytes[CMSG_SPACE(5 * sizeof(int))];
  } control = {0};
  if (count > 5) return -1;
  struct msghdr message = {0};
  message.msg_iov = &iov;
  message.msg_iovlen = 1;
  message.msg_control = control.bytes;
  message.msg_controllen = sizeof(control.bytes);
  ssize_t received;
  do { received = recvmsg(socket_fd, &message, MSG_CMSG_CLOEXEC); }
  while (received < 0 && errno == EINTR);
  struct cmsghdr *header = received == 1 ? CMSG_FIRSTHDR(&message) : NULL;
  if (header != NULL && header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_RIGHTS &&
      header->cmsg_len == CMSG_LEN(count * sizeof(int)) && marker == 'F' &&
      !(message.msg_flags & (MSG_CTRUNC | MSG_TRUNC)) && CMSG_NXTHDR(&message, header) == NULL) {
    memcpy(descriptors, CMSG_DATA(header), count * sizeof(int));
    return 0;
  }
  if (header != NULL && header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_RIGHTS &&
      header->cmsg_len > CMSG_LEN(0)) {
    size_t arrived = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
    for (size_t i = 0; i < arrived && i < 5; i++) {
      int stray;
      memcpy(&stray, CMSG_DATA(header) + i * sizeof(int), sizeof(int));
      close(stray);
    }
  }
  return -1;
}

static int receive_standard_fds(int socket_fd, int descriptors[3]) {
  return receive_descriptors(socket_fd, descriptors, 3);
}

static int validate_standard_fds(const int descriptors[3], int terminal) {
  for (int i = 0; i < 3; i++) {
    struct stat info;
    if (fstat(descriptors[i], &info) != 0 || S_ISREG(info.st_mode) ||
        S_ISDIR(info.st_mode)) return -1;
  }
  if (terminal && (!isatty(descriptors[0]) || !isatty(descriptors[1]))) return -1;
  return 0;
}

static int command_status(int status) {
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return 125;
}

static int execute_and_respond(int client, char *const command[]) {
  char stdout_path[] = "/run/koala-host-exec.stdout.XXXXXX";
  char stderr_path[] = "/run/koala-host-exec.stderr.XXXXXX";
  int stdout_fd = mkstemp(stdout_path);
  int stderr_fd = mkstemp(stderr_path);
  if (stdout_fd < 0 || stderr_fd < 0) {
    if (stdout_fd >= 0) close(stdout_fd);
    if (stderr_fd >= 0) close(stderr_fd);
    return 1;
  }
  unlink(stdout_path);
  unlink(stderr_path);

  pid_t pid = fork();
  if (pid == 0) {
    (void)setpgid(0, 0);
    if (dup2(stdout_fd, STDOUT_FILENO) < 0 || dup2(stderr_fd, STDERR_FILENO) < 0) _exit(126);
    close(stdout_fd);
    close(stderr_fd);
    execv(command[0], command);
    _exit(127);
  }
  if (pid < 0) {
    close(stdout_fd);
    close(stderr_fd);
    return 1;
  }
  (void)setpgid(pid, pid);

  int wait_status = 0;
  int forced_status = 0;
  unsigned int elapsed_ms = 0;
  for (;;) {
    pid_t waited = waitpid(pid, &wait_status, WNOHANG);
    if (waited == pid) break;
    if (waited < 0 && errno != EINTR) {
      close(stdout_fd);
      close(stderr_fd);
      return 1;
    }
    struct stat stdout_info;
    struct stat stderr_info;
    if (fstat(stdout_fd, &stdout_info) != 0 || fstat(stderr_fd, &stderr_info) != 0) {
      (void)kill(-pid, SIGKILL);
      (void)waitpid(pid, &wait_status, 0);
      close(stdout_fd);
      close(stderr_fd);
      return 1;
    }
    if (stdout_info.st_size > MAX_OUTPUT || stderr_info.st_size > MAX_OUTPUT) forced_status = 125;
    else if (elapsed_ms >= MAX_RUNTIME_MS) forced_status = 124;
    if (forced_status != 0) {
      (void)kill(-pid, SIGKILL);
      while (waitpid(pid, &wait_status, 0) < 0 && errno == EINTR) {}
      break;
    }
    usleep(10000);
    elapsed_ms += 10;
  }

  off_t stdout_size = lseek(stdout_fd, 0, SEEK_END);
  off_t stderr_size = lseek(stderr_fd, 0, SEEK_END);
  if (stdout_size > MAX_OUTPUT) stdout_size = MAX_OUTPUT;
  if (stderr_size > MAX_OUTPUT) stderr_size = MAX_OUTPUT;
  if (stdout_size < 0 || stderr_size < 0 || lseek(stdout_fd, 0, SEEK_SET) < 0 ||
      lseek(stderr_fd, 0, SEEK_SET) < 0) {
    close(stdout_fd);
    close(stderr_fd);
    return 1;
  }
  uint32_t header[3] = {
      htonl((uint32_t)(forced_status != 0 ? forced_status : command_status(wait_status))),
      htonl((uint32_t)stdout_size), htonl((uint32_t)stderr_size)};
  int result = write_exact(client, header, sizeof(header));
  if (result == 0) result = transfer(stdout_fd, client, (size_t)stdout_size);
  if (result == 0) result = transfer(stderr_fd, client, (size_t)stderr_size);
  close(stdout_fd);
  close(stderr_fd);
  return result == 0 ? 0 : 1;
}

static int handle_v2(int client) {
  uint32_t encoded_count;
  if (read_exact(client, &encoded_count, sizeof(encoded_count)) != 0) return 1;
  uint32_t count = ntohl(encoded_count);
  uint32_t populated = 0;
  char **command = read_structured_command(client, count, &populated);
  int result = command == NULL ? 1 : execute_and_respond(client, command);
  free_structured_command(command, populated);
  return result;
}

/* v4: the workload joins the requesting control container's mount namespace
   and root, so it keeps that container's volumes and shares, while staying in
   this service's (systemd's) PID, IPC, UTS and cgroup namespaces. */
struct isolation {
  int namespace_fd;
  int root_fd;
  int terminal;
  char **environment;
};

static int enter_isolation(const struct isolation *isolation, int terminal_fd) {
  if (isolation->terminal) {
    if (setsid() < 0 || ioctl(terminal_fd, TIOCSCTTY, 1) != 0) return -1;
  } else {
    (void)setpgid(0, 0);
  }
  if (setns(isolation->namespace_fd, CLONE_NEWNS) != 0 || fchdir(isolation->root_fd) != 0 ||
      chroot(".") != 0 || chdir("/") != 0) return -1;
  close(isolation->namespace_fd);
  close(isolation->root_fd);
  return 0;
}

static int execute_streamed(int client, char *const command[], int descriptors[3],
                            const struct isolation *isolation) {
  pid_t pid = fork();
  if (pid == 0) {
    if (isolation == NULL) (void)setpgid(0, 0);
    else if (enter_isolation(isolation, descriptors[0]) != 0) _exit(126);
    if (dup2(descriptors[0], STDIN_FILENO) < 0 ||
        dup2(descriptors[1], STDOUT_FILENO) < 0 ||
        dup2(descriptors[2], STDERR_FILENO) < 0) _exit(126);
    for (int i = 0; i < 3; i++) {
      if (descriptors[i] > STDERR_FILENO) close(descriptors[i]);
    }
    close(client);
    if (isolation == NULL) execv(command[0], command);
    else execve(command[0], command, isolation->environment);
    _exit(127);
  }
  for (int i = 0; i < 3; i++) close(descriptors[i]);
  if (pid < 0) return 1;
  if (isolation == NULL) (void)setpgid(pid, pid);
  /* v3 asks a disconnected client's wrapper to stop; v4 escalates, so the
     workload group is killed and its result still recorded. */
  int on_loss = isolation == NULL ? SIGTERM : SIGUSR2;

  int wait_status = 0;
  for (;;) {
    pid_t waited = waitpid(pid, &wait_status, WNOHANG);
    if (waited == pid) break;
    if (waited < 0 && errno != EINTR) return 1;
    struct pollfd poll_descriptor = {.fd = client, .events = POLLIN | POLLHUP};
    int polled;
    do { polled = poll(&poll_descriptor, 1, 50); }
    while (polled < 0 && errno == EINTR);
    if (polled < 0) return 1;
    if (polled > 0 && (poll_descriptor.revents & POLLIN)) {
      unsigned char requested;
      ssize_t count = read(client, &requested, 1);
      if (count == 1 && (requested == SIGHUP || requested == SIGINT ||
                         requested == SIGQUIT || requested == SIGTERM ||
                         (requested == SIGUSR2 && isolation != NULL))) {
        (void)kill(pid, requested);
      } else if (count <= 0) {
        (void)kill(pid, on_loss);
      }
    }
    if (polled > 0 && (poll_descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))) {
      (void)kill(pid, on_loss);
    }
  }
  uint32_t status = htonl((uint32_t)command_status(wait_status));
  return write_exact(client, &status, sizeof(status)) == 0 ? 0 : 1;
}

static int handle_v3(int client) {
  uint32_t encoded[2];
  if (read_exact(client, encoded, sizeof(encoded)) != 0) return 1;
  uint32_t count = ntohl(encoded[0]);
  uint32_t terminal = ntohl(encoded[1]);
  if (terminal > 1) return 1;
  uint32_t populated = 0;
  char **command = read_structured_command(client, count, &populated);
  if (command == NULL) return 1;
  int descriptors[3] = {-1, -1, -1};
  int result = receive_standard_fds(client, descriptors);
  if (result == 0) result = validate_standard_fds(descriptors, (int)terminal);
  if (result == 0) result = execute_streamed(client, command, descriptors, NULL);
  else for (int i = 0; i < 3; i++) if (descriptors[i] >= 0) close(descriptors[i]);
  free_structured_command(command, populated);
  return result;
}

/* A secret variable name: POSIX, never PATH or KOALA_* (the wrapper's rule). */
static int valid_secret_name(const char *name, size_t length) {
  if (length == 0 || length > 128 || isdigit((unsigned char)name[0])) return 0;
  for (size_t i = 0; i < length; i++)
    if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) return 0;
  return !(length == 4 && strncmp(name, "PATH", 4) == 0) && strncmp(name, "KOALA_", 6) != 0;
}

static void free_environment(char **environment) {
  if (environment == NULL) return;
  for (size_t i = 1; environment[i] != NULL; i++) {
    memset(environment[i], 0, strlen(environment[i]));
    free(environment[i]);
  }
  free(environment);
}

/* PATH, COUNT secret NAME=VALUE entries read from the client, and
   KOALA_SECRET_VARIABLES naming them (when there are any). */
static char **read_secret_environment(int client, uint32_t count) {
  if (count > MAX_SECRETS) return NULL;
  char **environment = calloc((size_t)count + 3, sizeof(char *));
  if (environment == NULL) return NULL;
  environment[0] = EXEC_PATH;
  size_t names_length = strlen("KOALA_SECRET_VARIABLES=") + 1, total = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t encoded_length;
    char *entry = NULL;
    if (read_exact(client, &encoded_length, sizeof(encoded_length)) == 0) {
      uint32_t length = ntohl(encoded_length);
      if (length > 0 && length <= MAX_FIELD && total <= MAX_COMMAND - length &&
          (entry = calloc((size_t)length + 1, 1)) != NULL &&
          read_exact(client, entry, length) == 0) {
        total += length;
        char *separator = memchr(entry, '=', length);
        if (separator == NULL || memchr(entry, '\0', length) != NULL ||
            !valid_secret_name(entry, (size_t)(separator - entry))) {
          memset(entry, 0, length);
          free(entry);
          entry = NULL;
        } else {
          names_length += (size_t)(separator - entry) + 1;
        }
      } else if (entry != NULL) {
        memset(entry, 0, length);
        free(entry);
        entry = NULL;
      }
    }
    if (entry == NULL) {
      free_environment(environment);
      return NULL;
    }
    environment[i + 1] = entry;
  }
  if (count > 0) {
    char *names = calloc(names_length, 1);
    if (names == NULL) {
      free_environment(environment);
      return NULL;
    }
    strcpy(names, "KOALA_SECRET_VARIABLES=");
    for (uint32_t i = 0; i < count; i++) {
      if (i > 0) strcat(names, ",");
      strncat(names, environment[i + 1], (size_t)(strchr(environment[i + 1], '=') - environment[i + 1]));
    }
    environment[count + 1] = names;
  }
  return environment;
}

static int handle_v4(int client) {
  uint32_t encoded[3];
  if (read_exact(client, encoded, sizeof(encoded)) != 0) return 1;
  uint32_t count = ntohl(encoded[0]);
  uint32_t terminal = ntohl(encoded[1]);
  uint32_t secrets = ntohl(encoded[2]);
  if (terminal > 1 || secrets > MAX_SECRETS) return 1;
  uint32_t populated = 0;
  char **command = read_structured_command(client, count, &populated);
  if (command == NULL) return 1;
  struct isolation isolation = {.namespace_fd = -1, .root_fd = -1, .terminal = (int)terminal};
  isolation.environment = read_secret_environment(client, secrets);
  int descriptors[5] = {-1, -1, -1, -1, -1};
  int result = isolation.environment == NULL ? 1 : receive_descriptors(client, descriptors, 5);
  if (result == 0) result = validate_standard_fds(descriptors, (int)terminal);
  struct stat root;
  if (result == 0 && (fstat(descriptors[4], &root) != 0 || !S_ISDIR(root.st_mode))) result = -1;
  if (result == 0) {
    isolation.namespace_fd = descriptors[3];
    isolation.root_fd = descriptors[4];
    result = execute_streamed(client, command, descriptors, &isolation);
    close(descriptors[3]);
    close(descriptors[4]);
  } else {
    for (int i = 0; i < 5; i++) if (descriptors[i] >= 0) close(descriptors[i]);
  }
  free_environment(isolation.environment);
  free_structured_command(command, populated);
  return result == 0 ? 0 : 1;
}

static int handle_legacy(int client, uint32_t encoded_length) {
  uint32_t length = ntohl(encoded_length);
  if (length == 0 || length > MAX_COMMAND) return 1;
  char *text = calloc((size_t)length + 1, 1);
  if (text == NULL || read_exact(client, text, length) != 0) {
    free(text);
    return 1;
  }
  char *command[] = {"/bin/bash", "-lc", text, NULL};
  int result = execute_and_respond(client, command);
  free(text);
  return result;
}

static int handle_connection(int client) {
  uint32_t first;
  if (read_exact(client, &first, sizeof(first)) != 0) return 1;
  uint32_t magic = ntohl(first);
  if (magic == PROTOCOL_V4_MAGIC) return handle_v4(client);
  if (magic == PROTOCOL_V3_MAGIC) return handle_v3(client);
  return magic == PROTOCOL_V2_MAGIC ? handle_v2(client) : handle_legacy(client, first);
}

static int run_server(void) {
  int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (listener < 0) return 1;
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  if (strlen(SOCKET_PATH) >= sizeof(address.sun_path)) return 1;
  strcpy(address.sun_path, SOCKET_PATH);
  unlink(SOCKET_PATH);
  if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
      chmod(SOCKET_PATH, 0600) != 0 || listen(listener, 16) != 0) {
    perror("koala-host-exec: listen");
    return 1;
  }
  struct sigaction action = {.sa_handler = SIG_IGN, .sa_flags = SA_NOCLDWAIT};
  sigemptyset(&action.sa_mask);
  sigaction(SIGCHLD, &action, NULL);
  for (;;) {
    int client = accept(listener, NULL, NULL);
    if (client < 0) {
      if (errno == EINTR) continue;
      return 1;
    }
    pid_t pid = fork();
    if (pid == 0) {
      signal(SIGCHLD, SIG_DFL);
      close(listener);
      int result = handle_connection(client);
      close(client);
      _exit(result);
    }
    close(client);
    if (pid < 0) return 1;
  }
}

static int connect_socket(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return -1;
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  strcpy(address.sun_path, SOCKET_PATH);
  if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
    perror("koala-host-exec: connect");
    close(fd);
    return -1;
  }
  return fd;
}

static int read_response(int fd) {
  uint32_t header[3];
  if (read_exact(fd, header, sizeof(header)) != 0) return 70;
  int status = (int)ntohl(header[0]);
  size_t stdout_size = ntohl(header[1]);
  size_t stderr_size = ntohl(header[2]);
  if (stdout_size > MAX_OUTPUT || stderr_size > MAX_OUTPUT ||
      transfer(fd, STDOUT_FILENO, stdout_size) != 0 ||
      transfer(fd, STDERR_FILENO, stderr_size) != 0) return 70;
  return status > 255 ? 125 : status;
}

static int run_client(const char *text) {
  size_t length = strlen(text);
  if (length == 0 || length > MAX_COMMAND) return 64;
  int fd = connect_socket();
  if (fd < 0) return 69;
  uint32_t encoded_length = htonl((uint32_t)length);
  if (write_exact(fd, &encoded_length, sizeof(encoded_length)) != 0 ||
      write_exact(fd, text, length) != 0) {
    close(fd);
    return 70;
  }
  int result = read_response(fd);
  close(fd);
  return result;
}

static int run_client_v2(int argc, char **argv) {
  if (argc < 9 || (unsigned int)(argc - 2) > MAX_FIELDS) return 64;
  int fd = connect_socket();
  if (fd < 0) return 69;
  uint32_t header[2] = {htonl(PROTOCOL_V2_MAGIC), htonl((uint32_t)(argc - 2))};
  if (write_exact(fd, header, sizeof(header)) != 0) {
    close(fd);
    return 70;
  }
  size_t total = 0;
  for (int i = 2; i < argc; i++) {
    size_t length = strlen(argv[i]);
    if (length == 0 || length > MAX_FIELD || total > MAX_COMMAND - length) {
      close(fd);
      return 64;
    }
    uint32_t encoded_length = htonl((uint32_t)length);
    if (write_exact(fd, &encoded_length, sizeof(encoded_length)) != 0 ||
        write_exact(fd, argv[i], length) != 0) {
      close(fd);
      return 70;
    }
    total += length;
  }
  int result = read_response(fd);
  close(fd);
  return result;
}

static int write_structured_fields(int fd, int argc, char **argv, int first_field) {
  size_t total = 0;
  for (int i = first_field; i < argc; i++) {
    size_t length = strlen(argv[i]);
    if (length == 0 || length > MAX_FIELD || total > MAX_COMMAND - length) return -1;
    uint32_t encoded_length = htonl((uint32_t)length);
    if (write_exact(fd, &encoded_length, sizeof(encoded_length)) != 0 ||
        write_exact(fd, argv[i], length) != 0) return -1;
    total += length;
  }
  return 0;
}

static int run_client_v3(int argc, char **argv) {
  if (argc < 10 || (unsigned int)(argc - 3) > MAX_FIELDS ||
      (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0)) return 64;
  int fd = connect_socket();
  if (fd < 0) return 69;
  uint32_t header[3] = {
      htonl(PROTOCOL_V3_MAGIC), htonl((uint32_t)(argc - 3)),
      htonl((uint32_t)(argv[2][0] - '0'))};
  if (write_exact(fd, header, sizeof(header)) != 0 ||
      write_structured_fields(fd, argc, argv, 3) != 0 ||
      send_standard_fds(fd) != 0 || set_control_signal_handlers(0) != 0) {
    close(fd);
    return 70;
  }
  control_socket = (sig_atomic_t)fd;
  uint32_t encoded_status;
  int result = read_exact(fd, &encoded_status, sizeof(encoded_status)) == 0
      ? (int)ntohl(encoded_status) : 70;
  control_socket = -1;
  close(fd);
  return result > 255 ? 125 : result;
}

/* Sends each variable named by this process's KOALA_SECRET_VARIABLES as a
   NAME=VALUE entry; values never enter any argv. */
static int write_secret_entries(int fd, uint32_t *count_out, int send) {
  const char *list = getenv("KOALA_SECRET_VARIABLES");
  uint32_t count = 0;
  if (list == NULL || *list == '\0') {
    *count_out = 0;
    return 0;
  }
  char *names = strdup(list);
  if (names == NULL) return -1;
  int result = 0;
  for (char *save = NULL, *name = strtok_r(names, ",", &save); name != NULL && result == 0;
       name = strtok_r(NULL, ",", &save)) {
    const char *value = getenv(name);
    size_t name_length = strlen(name);
    if (value == NULL || !valid_secret_name(name, name_length) || ++count > MAX_SECRETS) {
      result = -1;
      break;
    }
    size_t length = name_length + 1 + strlen(value);
    if (length > MAX_FIELD) {
      result = -1;
      break;
    }
    if (!send) continue;
    uint32_t encoded_length = htonl((uint32_t)length);
    if (write_exact(fd, &encoded_length, sizeof(encoded_length)) != 0 ||
        write_exact(fd, name, name_length) != 0 || write_exact(fd, "=", 1) != 0 ||
        write_exact(fd, value, strlen(value)) != 0) result = -1;
  }
  free(names);
  *count_out = count;
  return result;
}

static int run_client_v4(int argc, char **argv) {
  if (argc < 10 || (unsigned int)(argc - 3) > MAX_FIELDS ||
      (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0)) return 64;
  uint32_t secrets = 0;
  if (write_secret_entries(-1, &secrets, 0) != 0) return 64;
  int namespace_fd = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
  int root_fd = open("/", O_PATH | O_DIRECTORY | O_CLOEXEC);
  if (namespace_fd < 0 || root_fd < 0) {
    if (namespace_fd >= 0) close(namespace_fd);
    if (root_fd >= 0) close(root_fd);
    return 70;
  }
  int fd = connect_socket();
  if (fd < 0) {
    close(namespace_fd);
    close(root_fd);
    return 69;
  }
  uint32_t header[4] = {
      htonl(PROTOCOL_V4_MAGIC), htonl((uint32_t)(argc - 3)),
      htonl((uint32_t)(argv[2][0] - '0')), htonl(secrets)};
  int descriptors[5] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, namespace_fd, root_fd};
  uint32_t sent = 0;
  int failed = write_exact(fd, header, sizeof(header)) != 0 ||
      write_structured_fields(fd, argc, argv, 3) != 0 ||
      write_secret_entries(fd, &sent, 1) != 0 || sent != secrets ||
      send_descriptors(fd, descriptors, 5) != 0 || set_control_signal_handlers(1) != 0;
  close(namespace_fd);
  close(root_fd);
  if (failed) {
    close(fd);
    return 70;
  }
  control_socket = (sig_atomic_t)fd;
  uint32_t encoded_status;
  int result = read_exact(fd, &encoded_status, sizeof(encoded_status)) == 0
      ? (int)ntohl(encoded_status) : 70;
  control_socket = -1;
  close(fd);
  return result > 255 ? 125 : result;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "server") == 0) return run_server();
  if (argc == 3 && strcmp(argv[1], "client") == 0) return run_client(argv[2]);
  if (argc >= 9 && strcmp(argv[1], "client-v2") == 0) return run_client_v2(argc, argv);
  if (argc >= 10 && strcmp(argv[1], "client-v3") == 0) return run_client_v3(argc, argv);
  if (argc >= 10 && strcmp(argv[1], "client-v4") == 0) return run_client_v4(argc, argv);
  fputs("usage: koala-host-exec server | client COMMAND | client-v2 ID UID GID CWD ENV_COUNT ARG_COUNT [ENV...] ARGV... | client-v3|client-v4 TERMINAL ID UID GID CWD ENV_COUNT ARG_COUNT [ENV...] ARGV...\n", stderr);
  return 64;
}
