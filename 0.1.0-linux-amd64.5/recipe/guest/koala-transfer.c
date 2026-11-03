/* koala-transfer: guest side of the W04A file-transfer protocol.
 *
 * Speaks a binary encoding of the W04A1 frames on stdin/stdout (see
 * docs/tasks/W04A2A.md). Every filesystem operation is descriptor-relative
 * and never follows symlinks; content is staged and published atomically
 * only after the manifest matches. */
#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif
#endif

#define MAX_FRAME 70000U
#define MAX_CONTENT 65536U
#define MAX_PATH_BYTES 4096U
#define MAX_COMPONENT 255U

enum { F_ENTRY = 1, F_CONTENT = 2, F_MANIFEST = 3, F_QUERY = 4, F_CANCEL = 5,
       F_ACK = 0x81, F_MANIFEST_ACK = 0x82, F_ERROR = 0x83 };
enum { K_DIR = 0, K_REGULAR = 1, K_SYMLINK = 2 };

/* ---------- SHA-256 ---------- */

typedef struct { uint32_t h[8]; uint64_t length; uint8_t block[64]; size_t used; } sha256_t;

static const uint32_t K256[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(sha256_t *s, const uint8_t *p) {
  uint32_t w[64], a, b, c, d, e, f, g, h;
  for (int i = 0; i < 16; i++)
    w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
           (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
  e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
    uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
  s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha256_init(sha256_t *s) {
  static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  memcpy(s->h, init, sizeof init);
  s->length = 0;
  s->used = 0;
}

static void sha256_update(sha256_t *s, const void *data, size_t n) {
  const uint8_t *p = data;
  s->length += n;
  while (n > 0) {
    size_t take = 64 - s->used < n ? 64 - s->used : n;
    memcpy(s->block + s->used, p, take);
    s->used += take; p += take; n -= take;
    if (s->used == 64) { sha256_block(s, s->block); s->used = 0; }
  }
}

static void sha256_final(sha256_t *s, uint8_t out[32]) {
  uint64_t bits = s->length * 8;
  uint8_t pad = 0x80, zero = 0, len[8];
  sha256_update(s, &pad, 1);
  while (s->used != 56) sha256_update(s, &zero, 1);
  for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
  sha256_update(s, len, 8);
  for (int i = 0; i < 8; i++) {
    out[4 * i] = (uint8_t)(s->h[i] >> 24); out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
    out[4 * i + 2] = (uint8_t)(s->h[i] >> 8); out[4 * i + 3] = (uint8_t)s->h[i];
  }
}

/* ---------- Failure handling ---------- */

static int staging_parent = -1;
static char staging_name[96];
static dev_t staging_dev;
/* The intent record removed by a failure before publish; kept after it. */
static int prepared_state = -1;
static char prepared_name[96];

static int remove_tree(int dirfd, const char *name, dev_t dev);

/* Removes what an unpublished receive created; nothing once published. */
static void cleanup_unpublished(void) {
  if (staging_parent >= 0) (void)remove_tree(staging_parent, staging_name, staging_dev);
  if (prepared_state >= 0) (void)unlinkat(prepared_state, prepared_name, 0);
}

static void write_all(int fd, const void *data, size_t n) {
  const uint8_t *p = data;
  while (n > 0) {
    ssize_t w = write(fd, p, n);
    if (w < 0 && errno == EINTR) continue;
    if (w <= 0) {
      /* The host channel is gone; nothing more to report, but staging
         must not outlive it. */
      cleanup_unpublished();
      _exit(3);
    }
    p += w; n -= (size_t)w;
  }
}

static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
static void put_u64(uint8_t *p, uint64_t v) {
  for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (56 - 8 * i));
}
static uint32_t get_u32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static uint64_t get_u64(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v = v << 8 | p[i];
  return v;
}

static void send_frame(uint8_t type, const void *payload, size_t n) {
  uint8_t header[5];
  put_u32(header, (uint32_t)(n + 1));
  header[4] = type;
  write_all(STDOUT_FILENO, header, 5);
  if (n) write_all(STDOUT_FILENO, payload, n);
}

/* Reports a stable error, removes only this transfer's staging, exits. */
__attribute__((noreturn)) static void fail(const char *code) {
  send_frame(F_ERROR, code, strlen(code));
  cleanup_unpublished();
  _exit(1);
}

#ifdef KOALA_TRANSFER_FAULTS
static void checkpoint(const char *name) {
  const char *fault = getenv("KOALA_TRANSFER_FAULT");
  const char *pause = getenv("KOALA_TRANSFER_PAUSE");
  if (pause && strcmp(pause, name) == 0) raise(SIGSTOP);
  if (fault && strcmp(fault, name) == 0) raise(SIGKILL);
}
#else
#define checkpoint(name) ((void)0)
#endif

/* ---------- Path rules ---------- */

static int valid_utf8(const uint8_t *s, size_t n) {
  size_t i = 0;
  while (i < n) {
    uint8_t c = s[i];
    if (c < 0x80) { i++; continue; }
    uint32_t cp; size_t len;
    if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; len = 2; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; len = 3; }
    else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; len = 4; }
    else return 0;
    if (i + len > n) return 0;
    for (size_t k = 1; k < len; k++) {
      if ((s[i + k] & 0xc0) != 0x80) return 0;
      cp = cp << 6 | (s[i + k] & 0x3f);
    }
    if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) ||
        cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return 0;
    i += len;
  }
  return 1;
}

/* Bytes forbidden anywhere in a path or link target. */
static int clean_bytes(const uint8_t *s, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (s[i] < 0x20 || s[i] == 0x7f || s[i] == '\\') return 0;
  return valid_utf8(s, n);
}

static int valid_component(const char *s, size_t n) {
  if (n == 0 || n > MAX_COMPONENT) return 0;
  if ((n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.')) return 0;
  return memchr(s, '/', n) == NULL && clean_bytes((const uint8_t *)s, n);
}

/* Relative transfer path: valid components joined by single slashes. */
static int valid_path(const char *s, size_t n) {
  if (n == 0 || n > MAX_PATH_BYTES || !clean_bytes((const uint8_t *)s, n)) return 0;
  size_t start = 0;
  for (size_t i = 0; i <= n; i++) {
    if (i == n || s[i] == '/') {
      if (!valid_component(s + start, i - start)) return 0;
      start = i + 1;
    }
  }
  return 1;
}

static const char *last_slash(const char *s, size_t n) {
  while (n-- > 0) if (s[n] == '/') return s + n;
  return NULL;
}

static size_t component_count(const char *path) {
  size_t count = 1;
  for (const char *p = path; *p; p++) if (*p == '/') count++;
  return count;
}

/* A link target must be relative and stay inside the root from its directory. */
static int valid_target(const char *target, size_t n, const char *link_path) {
  if (n == 0 || n > MAX_PATH_BYTES || target[0] == '/' || !clean_bytes((const uint8_t *)target, n))
    return 0;
  long depth = (long)component_count(link_path) - 1;
  size_t start = 0;
  for (size_t i = 0; i <= n; i++) {
    if (i == n || target[i] == '/') {
      size_t len = i - start;
      const char *part = target + start;
      start = i + 1;
      if (len == 0 || (len == 1 && part[0] == '.')) continue;
      if (len == 2 && part[0] == '.' && part[1] == '.') {
        if (--depth < 0) return 0;
      } else {
        depth++;
      }
    }
  }
  return 1;
}

static int valid_id(const char *id) {
  size_t n = strlen(id);
  if (n == 0 || n > 64) return 0;
  for (size_t i = 0; i < n; i++)
    if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'z') || id[i] == '-'))
      return 0;
  return 1;
}

static int parse_u64(const char *text, uint64_t *out) {
  if (!*text) return 0;
  uint64_t v = 0;
  for (const char *p = text; *p; p++) {
    if (*p < '0' || *p > '9' || v > (UINT64_MAX - 9) / 10) return 0;
    v = v * 10 + (uint64_t)(*p - '0');
  }
  *out = v;
  return 1;
}

/* ---------- Descriptor-relative filesystem helpers ---------- */

static int open_dir_at(int dirfd, const char *name) {
  return openat(dirfd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
}

/* Opens an absolute normalized directory path one component at a time.
   Any symlink component is refused. */
static int open_absolute_dir(const char *path) {
  if (path[0] != '/') return -1;
  int fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0 || path[1] == '\0') return fd;
  if (!valid_path(path + 1, strlen(path + 1))) { close(fd); errno = EINVAL; return -1; }
  char buffer[MAX_PATH_BYTES + 1];
  snprintf(buffer, sizeof buffer, "%s", path + 1);
  for (char *part = strtok(buffer, "/"); part; part = strtok(NULL, "/")) {
    int next = open_dir_at(fd, part);
    close(fd);
    if (next < 0) return -1;
    fd = next;
  }
  return fd;
}

/* Splits an absolute destination into an opened parent and its final name. */
static int open_destination_parent(const char *dest, char *name, size_t name_size) {
  size_t n = strlen(dest);
  if (n < 2 || dest[0] != '/' || n > MAX_PATH_BYTES || !valid_path(dest + 1, n - 1)) return -1;
  const char *slash = strrchr(dest, '/');
  if (strlen(slash + 1) >= name_size) return -1;
  snprintf(name, name_size, "%s", slash + 1);
  char parent[MAX_PATH_BYTES + 1];
  size_t parent_len = (size_t)(slash - dest);
  if (parent_len == 0) parent_len = 1;
  memcpy(parent, dest, parent_len);
  parent[parent_len] = '\0';
  return open_absolute_dir(parent);
}

/* Removes NAME under DIRFD without following links or crossing filesystems. */
static int remove_tree(int dirfd, const char *name, dev_t dev) {
  struct stat st;
  if (fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT ? 0 : -1;
  if (!S_ISDIR(st.st_mode)) return unlinkat(dirfd, name, 0);
  if (st.st_dev != dev) return -1;
  int fd = open_dir_at(dirfd, name);
  if (fd < 0) return -1;
  struct stat opened;
  if (fstat(fd, &opened) != 0 || opened.st_ino != st.st_ino || opened.st_dev != st.st_dev) {
    close(fd);
    return -1;
  }
  if (opened.st_uid == geteuid()) (void)fchmod(fd, 0700);
  int result = 0;
  for (int removed = 1; removed && result == 0;) {
    removed = 0;
    int listing = dup(fd);
    DIR *dir = listing >= 0 ? fdopendir(listing) : NULL;
    if (!dir) { if (listing >= 0) close(listing); result = -1; break; }
    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
      if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) continue;
      if (remove_tree(fd, item->d_name, dev) != 0) { result = -1; break; }
      removed = 1;
    }
    closedir(dir);
  }
  close(fd);
  if (result != 0) return -1;
  return unlinkat(dirfd, name, AT_REMOVEDIR);
}

/* Opens the directory holding PATH's last component, relative to ROOT. */
static int open_entry_parent(int root, const char *path, const char **last) {
  char buffer[MAX_PATH_BYTES + 1];
  snprintf(buffer, sizeof buffer, "%s", path);
  char *slash = strrchr(buffer, '/');
  *last = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
  int fd = dup(root);
  if (!slash || fd < 0) return fd;
  *slash = '\0';
  for (char *part = strtok(buffer, "/"); part; part = strtok(NULL, "/")) {
    int next = open_dir_at(fd, part);
    close(fd);
    if (next < 0) return -1;
    fd = next;
  }
  return fd;
}

/* ---------- Entry table and manifest ---------- */

typedef struct {
  char *path;
  char *target;
  uint8_t kind;
  uint32_t mode;
  uint64_t size;
  uint8_t digest[32];
} entry_t;

static entry_t *entries;
static size_t entry_count, entry_capacity;
static size_t *slots; /* open-addressing index into entries, 0 = empty */
static size_t slot_capacity;

static uint64_t hash_path(const char *s, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; i++) { h ^= (uint8_t)s[i]; h *= 1099511628211ULL; }
  return h;
}

static entry_t *find_entry(const char *path, size_t n) {
  if (!slot_capacity) return NULL;
  for (size_t i = hash_path(path, n) & (slot_capacity - 1);; i = (i + 1) & (slot_capacity - 1)) {
    if (!slots[i]) return NULL;
    entry_t *e = &entries[slots[i] - 1];
    if (strlen(e->path) == n && memcmp(e->path, path, n) == 0) return e;
  }
}

static void index_entry(size_t index) {
  const char *p = entries[index].path;
  for (size_t i = hash_path(p, strlen(p)) & (slot_capacity - 1);; i = (i + 1) & (slot_capacity - 1))
    if (!slots[i]) { slots[i] = index + 1; return; }
}

static entry_t *add_entry(void) {
  if (entry_count == entry_capacity) {
    entry_capacity = entry_capacity ? entry_capacity * 2 : 64;
    entries = realloc(entries, entry_capacity * sizeof *entries);
    if (!entries) fail("TRANSFER_BUDGET");
  }
  if ((entry_count + 1) * 2 > slot_capacity) {
    free(slots);
    slot_capacity = slot_capacity ? slot_capacity * 2 : 256;
    while ((entry_count + 1) * 2 > slot_capacity) slot_capacity *= 2;
    slots = calloc(slot_capacity, sizeof *slots);
    if (!slots) fail("TRANSFER_BUDGET");
    for (size_t i = 0; i < entry_count; i++) index_entry(i);
  }
  memset(&entries[entry_count], 0, sizeof *entries);
  return &entries[entry_count++];
}

static void commit_entry(void) { index_entry(entry_count - 1); }

static int compare_paths(const void *a, const void *b) {
  return strcmp(entries[*(const size_t *)a].path, entries[*(const size_t *)b].path);
}

static void hex(const uint8_t digest[32], char out[65]) {
  for (int i = 0; i < 32; i++) snprintf(out + 2 * i, 3, "%02x", digest[i]);
}

static int unhex(const char in[64], uint8_t out[32]) {
  for (int i = 0; i < 32; i++) {
    int v = 0;
    for (int k = 0; k < 2; k++) {
      char c = in[2 * i + k];
      if (c >= '0' && c <= '9') v = v * 16 + (c - '0');
      else if (c >= 'a' && c <= 'f') v = v * 16 + (c - 'a' + 10);
      else return 0;
    }
    out[i] = (uint8_t)v;
  }
  return 1;
}

static const char *kind_name(uint8_t kind) {
  return kind == K_DIR ? "directory" : kind == K_REGULAR ? "regular" : "symlink";
}

/* The W04A1 canonical manifest digest. */
static void manifest_digest(uint8_t out[32], uint64_t *total_bytes) {
  size_t *order = malloc((entry_count ? entry_count : 1) * sizeof *order);
  if (!order) fail("TRANSFER_BUDGET");
  for (size_t i = 0; i < entry_count; i++) order[i] = i;
  qsort(order, entry_count, sizeof *order, compare_paths);
  sha256_t s;
  sha256_init(&s);
  uint64_t bytes = 0;
  char line[2 * MAX_PATH_BYTES + 256], digest[65];
  for (size_t i = 0; i < entry_count; i++) {
    entry_t *e = &entries[order[i]];
    char size[24] = "-", sha[72] = "-";
    if (e->kind == K_REGULAR) {
      snprintf(size, sizeof size, "%llu", (unsigned long long)e->size);
      hex(e->digest, digest);
      snprintf(sha, sizeof sha, "sha256:%s", digest);
      bytes += e->size;
    }
    int n = snprintf(line, sizeof line, "%s\t%s\t%o\t%s\t%s\t%s\n", kind_name(e->kind), e->path,
                     (unsigned)e->mode, size, sha, e->kind == K_SYMLINK ? e->target : "-");
    sha256_update(&s, line, (size_t)n);
  }
  int n = snprintf(line, sizeof line, "total\t%llu\t%llu\n", (unsigned long long)entry_count,
                   (unsigned long long)bytes);
  sha256_update(&s, line, (size_t)n);
  sha256_final(&s, out);
  *total_bytes = bytes;
  free(order);
}

/* ---------- Frame input ---------- */

static uint8_t frame[MAX_FRAME];

/* Returns the frame length, 0 on clean EOF, or fails on a malformed frame. */
static size_t read_frame(void) {
  uint8_t header[4];
  size_t got = 0;
  while (got < 4) {
    ssize_t r = read(STDIN_FILENO, header + got, 4 - got);
    if (r < 0 && errno == EINTR) continue;
    if (r == 0 && got == 0) return 0;
    if (r <= 0) fail("TRANSFER_CHANNEL_LOST");
    got += (size_t)r;
  }
  uint32_t length = get_u32(header);
  if (length == 0 || length > MAX_FRAME) fail("TRANSFER_PROTOCOL");
  for (uint32_t done = 0; done < length;) {
    ssize_t r = read(STDIN_FILENO, frame + done, (size_t)(length - done));
    if (r < 0 && errno == EINTR) continue;
    if (r <= 0 || (size_t)r > (size_t)(length - done)) fail("TRANSFER_CHANNEL_LOST");
    done += (uint32_t)r;
  }
  return length;
}

/* ---------- Receive ---------- */

typedef struct {
  uint64_t max_entries, max_total, max_entry;
  uint64_t received, declared;
  int open_fd;
  entry_t *open_entry;
  uint64_t open_received;
  sha256_t open_hash;
} receive_t;

/* W04C: a file root is staged as `file` inside the staging directory. */
#define FILE_ROOT_NAME "file"
static int file_root;
static struct stat file_root_identity;

static void send_ack(const receive_t *r) {
  uint8_t payload[16];
  put_u64(payload, r->received);
  put_u64(payload + 8, (uint64_t)entry_count);
  send_frame(F_ACK, payload, sizeof payload);
}

static void finish_file(receive_t *r) {
  uint8_t digest[32];
  sha256_final(&r->open_hash, digest);
  if (memcmp(digest, r->open_entry->digest, 32) != 0) fail("TRANSFER_CHECKSUM");
  /* L10F: never group- or other-writable, whatever the sender asked. */
  if (fchmod(r->open_fd, (mode_t)(r->open_entry->mode & ~022u)) != 0 || fsync(r->open_fd) != 0)
    fail("TRANSFER_IO");
  checkpoint("file-fsynced");
  close(r->open_fd);
  r->open_fd = -1;
  r->open_entry = NULL;
}

static void receive_entry(receive_t *r, size_t length, int staging) {
  if (r->open_entry) fail("TRANSFER_OFFSET"); /* the previous file is incomplete */
  if (length < 1 + 1 + 4 + 8 + 32 + 2 + 2) fail("TRANSFER_PROTOCOL");
  const uint8_t *p = frame + 1;
  uint8_t kind = p[0];
  uint32_t mode = get_u32(p + 1);
  uint64_t size = get_u64(p + 5);
  const uint8_t *digest = p + 13;
  size_t path_len = (size_t)p[45] << 8 | p[46];
  if (47 + path_len + 2 > length - 1) fail("TRANSFER_PROTOCOL");
  const char *path = (const char *)p + 47;
  size_t target_len = (size_t)p[47 + path_len] << 8 | p[48 + path_len];
  if (49 + path_len + target_len != length - 1) fail("TRANSFER_PROTOCOL");
  const char *target = (const char *)p + 49 + path_len;

  static const uint8_t zero[32];
  if (kind > K_SYMLINK || mode > 0777) fail("TRANSFER_INVALID_ENTRY");
  /* W04C: an empty path is a file root, valid only as the first and only
     entry and only for a regular file. */
  if (file_root) fail("TRANSFER_INVALID_PATH");
  if (path_len == 0) {
    if (kind != K_REGULAR || entry_count != 0) fail("TRANSFER_INVALID_PATH");
    file_root = 1;
  } else if (!valid_path(path, path_len)) fail("TRANSFER_INVALID_PATH");
  if (kind != K_REGULAR && (size != 0 || memcmp(digest, zero, 32) != 0))
    fail("TRANSFER_INVALID_ENTRY");
  if ((kind == K_SYMLINK) != (target_len > 0)) fail("TRANSFER_INVALID_ENTRY");
  if (kind == K_SYMLINK && mode != 0777) fail("TRANSFER_INVALID_ENTRY");
  if (kind == K_REGULAR && size > r->max_entry) fail("TRANSFER_INVALID_ENTRY");
  if (find_entry(path, path_len)) fail("TRANSFER_INVALID_ENTRY");
  const char *slash = last_slash(path, path_len);
  if (slash) {
    entry_t *parent = find_entry(path, (size_t)(slash - path));
    if (!parent || parent->kind != K_DIR) fail("TRANSFER_INVALID_ENTRY");
  }
  if (entry_count + 1 > r->max_entries) fail("TRANSFER_BUDGET");
  if (kind == K_REGULAR && size > r->max_total - r->declared) fail("TRANSFER_BUDGET");

  entry_t *e = add_entry();
  e->kind = kind; e->mode = mode; e->size = size;
  memcpy(e->digest, digest, 32);
  e->path = strndup(path, path_len);
  e->target = kind == K_SYMLINK ? strndup(target, target_len) : NULL;
  if (!e->path || (kind == K_SYMLINK && !e->target)) fail("TRANSFER_BUDGET");
  if (kind == K_SYMLINK && !valid_target(e->target, target_len, e->path))
    fail("TRANSFER_INVALID_ENTRY");
  commit_entry();

  const char *last = FILE_ROOT_NAME;
  int parent = file_root ? dup(staging) : open_entry_parent(staging, e->path, &last);
  if (parent < 0) fail("TRANSFER_INVALID_PATH");
  if (kind == K_DIR) {
    if (mkdirat(parent, last, 0700) != 0) fail(errno == EEXIST ? "TRANSFER_INVALID_ENTRY" : "TRANSFER_IO");
  } else if (kind == K_SYMLINK) {
    if (symlinkat(e->target, parent, last) != 0)
      fail(errno == EEXIST ? "TRANSFER_INVALID_ENTRY" : "TRANSFER_IO");
  } else {
    int fd = openat(parent, last, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) fail(errno == EEXIST ? "TRANSFER_INVALID_ENTRY" : "TRANSFER_IO");
    if (file_root && fstat(fd, &file_root_identity) != 0) fail("TRANSFER_IO");
    r->declared += size;
    r->open_fd = fd;
    r->open_entry = e;
    r->open_received = 0;
    sha256_init(&r->open_hash);
    if (size == 0) finish_file(r);
  }
  close(parent);
  checkpoint("entry-created");
  send_ack(r);
}

static void receive_content(receive_t *r, size_t length) {
  if (length < 1 + 16 + 1 || length - 17 > MAX_CONTENT) fail("TRANSFER_PROTOCOL");
  uint64_t transfer_offset = get_u64(frame + 1), entry_offset = get_u64(frame + 9);
  size_t n = length - 17;
  if (!r->open_entry || transfer_offset != r->received || entry_offset != r->open_received ||
      n > r->open_entry->size - r->open_received)
    fail("TRANSFER_OFFSET");
  const uint8_t *data = frame + 17;
  for (size_t written = 0; written < n;) {
    ssize_t w = write(r->open_fd, data + written, n - written);
    if (w < 0 && errno == EINTR) continue;
    if (w <= 0) fail("TRANSFER_IO");
    written += (size_t)w;
  }
  checkpoint("content-written");
  sha256_update(&r->open_hash, data, n);
  r->open_received += n;
  r->received += n;
  if (r->open_received == r->open_entry->size) finish_file(r);
  send_ack(r);
}

/* Moves FROM_DIR/FROM_NAME to PARENT/NAME without replacing, or by an atomic
   exchange when REPLACE; the exchanged-out object is left at the source. */
static int publish(int from_dir, const char *from_name, int parent, const char *name, int replace) {
#ifdef __linux__
  if (syscall(SYS_renameat2, from_dir, from_name, parent, name, RENAME_NOREPLACE) == 0) return 0;
  if (errno != EEXIST || !replace) return -1;
  return (int)syscall(SYS_renameat2, from_dir, from_name, parent, name, RENAME_EXCHANGE);
#else
  if (renameatx_np(from_dir, from_name, parent, name, RENAME_EXCL) == 0) return 0;
  if (errno != EEXIST || !replace) return -1;
  return renameatx_np(from_dir, from_name, parent, name, RENAME_SWAP);
#endif
}

static int open_state_dir(const char *path) {
  if (path[0] != '/' || strlen(path) > MAX_PATH_BYTES) return -1;
  char buffer[MAX_PATH_BYTES + 1];
  snprintf(buffer, sizeof buffer, "%s", path);
  int fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  for (char *part = strtok(buffer, "/"); fd >= 0 && part; part = strtok(NULL, "/")) {
    if (mkdirat(fd, part, 0700) != 0 && errno != EEXIST) { close(fd); return -1; }
    int next = open_dir_at(fd, part);
    close(fd);
    fd = next;
  }
  return fd;
}

/* Writes STATE_DIR/name durably through a temporary sibling. */
static void write_state_file(int state, const char *name, const char *text, size_t n) {
  char temporary[128];
  snprintf(temporary, sizeof temporary, "%s.tmp", name);
  (void)unlinkat(state, temporary, 0);
  int fd = openat(state, temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) fail("TRANSFER_IO");
  write_all(fd, text, n);
  if (fsync(fd) != 0 || close(fd) != 0 || renameat(state, temporary, state, name) != 0 ||
      fsync(state) != 0)
    fail("TRANSFER_IO");
}

static void write_record(int state, const char *id, const uint8_t digest[32]) {
  char name[96], text[96], digest_hex[65];
  snprintf(name, sizeof name, "%s.committed", id);
  hex(digest, digest_hex);
  int n = snprintf(text, sizeof text, "v1 committed sha256:%s\n", digest_hex);
  write_state_file(state, name, text, (size_t)n);
}

/* Before publish: the digest and the staging directory's identity. After a
   crash, `recover` proves publication by finding that identity at the
   destination name, so status never disagrees with the destination. */
static void write_prepared(int state, const uint8_t digest[32], const struct stat *staging) {
  char text[160], digest_hex[65];
  hex(digest, digest_hex);
  int n = snprintf(text, sizeof text, "v1 prepared sha256:%s %llu %llu\n", digest_hex,
                   (unsigned long long)staging->st_dev, (unsigned long long)staging->st_ino);
  prepared_state = state;
  write_state_file(state, prepared_name, text, (size_t)n);
}

/* The destination parent path must still name the directory held open; a
   parent renamed or swapped during the transfer is never published into. */
static void require_parent_unmoved(const char *dest, int parent) {
  char name[MAX_COMPONENT + 1];
  int again = open_destination_parent(dest, name, sizeof name);
  struct stat held, current;
  int same = again >= 0 && fstat(parent, &held) == 0 && fstat(again, &current) == 0 &&
             held.st_dev == current.st_dev && held.st_ino == current.st_ino;
  if (again >= 0) close(again);
  if (!same) fail("TRANSFER_PARENT_MOVED");
}

/* L10F: at commit, with every entry known. A link's target, resolved from
   the link's directory, must not pass through another link of this transfer:
   each link's own check (valid_target) is then the whole truth, as a chain
   such as `up -> ../..` followed by `k -> up/../x` could otherwise leave the
   tree. Ending at another link is fine (that link is checked itself). */
static int target_crosses_link(const entry_t *link) {
  char resolved[2 * MAX_PATH_BYTES + 2];
  size_t used = 0;
  const char *slash = strrchr(link->path, '/');
  if (slash) {
    used = (size_t)(slash - link->path);
    memcpy(resolved, link->path, used);
  }
  const char *part = link->target;
  while (*part) {
    const char *end = strchr(part, '/');
    size_t len = end ? (size_t)(end - part) : strlen(part);
    if (len == 0 || (len == 1 && part[0] == '.')) {
      /* nothing */
    } else if (len == 2 && part[0] == '.' && part[1] == '.') {
      while (used > 0 && resolved[used - 1] != '/') used--;
      if (used > 0) used--; /* the slash */
    } else {
      if (used + 1 + len >= sizeof resolved) return 1;
      if (used > 0) resolved[used++] = '/';
      memcpy(resolved + used, part, len);
      used += len;
      entry_t *e = find_entry(resolved, used);
      if (e && e->kind == K_SYMLINK && end && end[1] != 0) return 1;
    }
    if (!end) break;
    part = end + 1;
  }
  return 0;
}

static void commit(receive_t *r, size_t length, int staging, int parent, const char *name,
                   int replace, int state, const char *id, const char *dest) {
  if (length != 1 + 32 + 16) fail("TRANSFER_PROTOCOL");
  if (r->open_entry) fail("TRANSFER_OFFSET");
  uint8_t digest[32];
  uint64_t bytes;
  manifest_digest(digest, &bytes);
  if (memcmp(digest, frame + 1, 32) != 0 || get_u64(frame + 33) != entry_count ||
      get_u64(frame + 41) != bytes || bytes != r->received)
    fail("TRANSFER_MANIFEST");
  for (size_t i = 0; i < entry_count; i++) {
    if (entries[i].kind == K_SYMLINK && target_crosses_link(&entries[i])) fail("TRANSFER_INVALID_ENTRY");
  }
  /* Children follow their parents, so reverse order finishes children first. */
  for (size_t i = entry_count; i-- > 0;) {
    if (entries[i].kind != K_DIR) continue;
    const char *last;
    int dir_parent = open_entry_parent(staging, entries[i].path, &last);
    int fd = dir_parent >= 0 ? open_dir_at(dir_parent, last) : -1;
    if (fd < 0 || fchmod(fd, (mode_t)(entries[i].mode & ~022u)) != 0 || fsync(fd) != 0) fail("TRANSFER_IO");
    close(fd);
    close(dir_parent);
  }
  if (fchmod(staging, 0755) != 0 || fsync(staging) != 0) fail("TRANSFER_IO");
  checkpoint("dirs-fsynced");
  checkpoint("before-publish");
  require_parent_unmoved(dest, parent);
  /* The staging name must still be the directory written above; a swapped
     name (for example a symlink) is never published. */
  struct stat opened, named;
  if (fstat(staging, &opened) != 0 ||
      fstatat(parent, staging_name, &named, AT_SYMLINK_NOFOLLOW) != 0 ||
      !S_ISDIR(named.st_mode) || named.st_ino != opened.st_ino || named.st_dev != opened.st_dev) {
    staging_parent = -1; /* do not touch whatever now holds the name */
    fail("TRANSFER_STAGING");
  }
  /* A file root publishes the staged file, which must still be the one
     written; a tree publishes the staging directory itself. */
  if (file_root) {
    struct stat staged;
    if (fstatat(staging, FILE_ROOT_NAME, &staged, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(staged.st_mode) ||
        staged.st_ino != file_root_identity.st_ino || staged.st_dev != file_root_identity.st_dev)
      fail("TRANSFER_STAGING");
    opened = staged;
  }
  snprintf(prepared_name, sizeof prepared_name, "%s.prepared", id);
  write_prepared(state, digest, &opened);
  checkpoint("after-prepare");
  int published = file_root ? publish(staging, FILE_ROOT_NAME, parent, name, replace)
                            : publish(parent, staging_name, parent, name, replace);
  if (published != 0) fail(errno == EEXIST || errno == ENOTEMPTY ? "TRANSFER_EXISTS" : "TRANSFER_IO");
  /* Published: from here on failure must not remove the new destination or
     the intent record that proves it. The staging name now holds the
     replaced tree, if any. */
  staging_parent = -1;
  prepared_state = -1;
  checkpoint("after-publish");
  if (fsync(parent) != 0) fail("TRANSFER_IO");
  write_record(state, id, digest);
  checkpoint("after-record");
  (void)unlinkat(state, prepared_name, 0);
  (void)remove_tree(parent, staging_name, staging_dev);
  (void)fsync(parent);
  send_frame(F_MANIFEST_ACK, digest, 32);
}

static int run_receive(char **argv) {
  const char *dest = argv[0], *policy = argv[1], *id = argv[2];
  receive_t r = {.open_fd = -1};
  int replace;
  if (strcmp(policy, "fail-if-exists") == 0) replace = 0;
  else if (strcmp(policy, "replace") == 0) replace = 1;
  else return 2;
  if (!valid_id(id) || !parse_u64(argv[3], &r.max_entries) || !parse_u64(argv[4], &r.max_total) ||
      !parse_u64(argv[5], &r.max_entry))
    return 2;
  char name[MAX_COMPONENT + 1];
  int parent = open_destination_parent(dest, name, sizeof name);
  if (parent < 0) fail("TRANSFER_INVALID_PATH");
  int state = open_state_dir(argv[6]);
  if (state < 0) fail("TRANSFER_IO");
  char record[96];
  struct stat st;
  snprintf(record, sizeof record, "%s.committed", id);
  if (fstatat(state, record, &st, AT_SYMLINK_NOFOLLOW) == 0) fail("TRANSFER_TERMINAL");
  /* An earlier attempt may have published; only `recover` may decide. */
  snprintf(record, sizeof record, "%s.prepared", id);
  if (fstatat(state, record, &st, AT_SYMLINK_NOFOLLOW) == 0) fail("TRANSFER_TERMINAL");
  if (fstat(parent, &st) != 0) fail("TRANSFER_IO");
  snprintf(staging_name, sizeof staging_name, ".koala-transfer-%s", id);
  staging_dev = st.st_dev;
  if (remove_tree(parent, staging_name, staging_dev) != 0) fail("TRANSFER_IO");
  if (mkdirat(parent, staging_name, 0700) != 0) fail("TRANSFER_IO");
  staging_parent = parent;
  int staging = open_dir_at(parent, staging_name);
  if (staging < 0) fail("TRANSFER_IO");

  for (;;) {
    size_t length = read_frame();
    if (length == 0) fail("TRANSFER_CHANNEL_LOST");
    switch (frame[0]) {
    case F_ENTRY: receive_entry(&r, length, staging); break;
    case F_CONTENT: receive_content(&r, length); break;
    case F_QUERY:
      if (length != 1) fail("TRANSFER_PROTOCOL");
      send_ack(&r);
      break;
    case F_CANCEL: fail("TRANSFER_CANCELLED");
    case F_MANIFEST:
      commit(&r, length, staging, parent, name, replace, state, id, dest);
      return 0;
    default: fail("TRANSFER_PROTOCOL");
    }
  }
}

/* ---------- Send ---------- */

typedef struct {
  uint64_t max_entries, max_total, max_entry;
  uint64_t sent;
  dev_t dev;
} send_t;

/* Between frames, honours a host cancel or disconnect without blocking. */
static void poll_host(void) {
  struct pollfd p = {.fd = STDIN_FILENO, .events = POLLIN};
  if (poll(&p, 1, 0) <= 0) return;
  size_t length = read_frame();
  if (length == 0) fail("TRANSFER_CHANNEL_LOST");
  if (frame[0] == F_CANCEL) fail("TRANSFER_CANCELLED");
  if (frame[0] != F_ACK) fail("TRANSFER_PROTOCOL");
}

static void send_entry_frame(const entry_t *e) {
  size_t path_len = strlen(e->path), target_len = e->target ? strlen(e->target) : 0;
  uint8_t payload[1 + 4 + 8 + 32 + 2 + MAX_PATH_BYTES + 2 + MAX_PATH_BYTES];
  payload[0] = e->kind;
  put_u32(payload + 1, e->mode);
  put_u64(payload + 5, e->size);
  memcpy(payload + 13, e->digest, 32);
  payload[45] = (uint8_t)(path_len >> 8); payload[46] = (uint8_t)path_len;
  memcpy(payload + 47, e->path, path_len);
  payload[47 + path_len] = (uint8_t)(target_len >> 8);
  payload[48 + path_len] = (uint8_t)target_len;
  memcpy(payload + 49 + path_len, e->target ? e->target : "", target_len);
  send_frame(F_ENTRY, payload, 49 + path_len + target_len);
}

static int compare_names(const void *a, const void *b) {
  return strcmp(*(char *const *)a, *(char *const *)b);
}

static void send_regular(send_t *s, int dirfd, const char *name, const struct stat *walked,
                         entry_t *e) {
  if (walked->st_nlink > 1) fail("TRANSFER_INVALID_ENTRY");
  int fd = openat(dirfd, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  struct stat st;
  if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_ino != walked->st_ino ||
      st.st_dev != walked->st_dev)
    fail("TRANSFER_INVALID_ENTRY");
  uint64_t size = (uint64_t)st.st_size;
  if (size > s->max_entry) fail("TRANSFER_INVALID_ENTRY");
  if (size > s->max_total - s->sent) fail("TRANSFER_BUDGET");
  static uint8_t buffer[MAX_CONTENT];
  sha256_t hash;
  sha256_init(&hash);
  uint64_t counted = 0;
  for (;;) {
    ssize_t n = read(fd, buffer, sizeof buffer);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) fail("TRANSFER_IO");
    if (n == 0) break;
    sha256_update(&hash, buffer, (size_t)n);
    counted += (uint64_t)n;
  }
  if (counted != size) fail("TRANSFER_CHECKSUM");
  sha256_final(&hash, e->digest);
  e->size = size;
  e->mode = st.st_mode & 0777;
  checkpoint("send-hashed");
  send_entry_frame(e);
  if (lseek(fd, 0, SEEK_SET) != 0) fail("TRANSFER_IO");
  sha256_init(&hash);
  uint64_t offset = 0;
  uint8_t content[16 + MAX_CONTENT];
  for (;;) {
    ssize_t n = read(fd, content + 16, MAX_CONTENT);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) fail("TRANSFER_IO");
    if (n == 0) break;
    if ((uint64_t)n > size - offset) fail("TRANSFER_CHECKSUM"); /* the file grew */
    put_u64(content, s->sent);
    put_u64(content + 8, offset);
    sha256_update(&hash, content + 16, (size_t)n);
    poll_host();
    send_frame(F_CONTENT, content, 16 + (size_t)n);
    offset += (uint64_t)n;
    s->sent += (uint64_t)n;
  }
  uint8_t again[32];
  sha256_final(&hash, again);
  if (offset != size || memcmp(again, e->digest, 32) != 0) fail("TRANSFER_CHECKSUM");
  close(fd);
}

static void walk(send_t *s, int dirfd, const char *prefix) {
  int listing = dup(dirfd);
  DIR *dir = listing >= 0 ? fdopendir(listing) : NULL;
  if (!dir) fail("TRANSFER_IO");
  char **names = NULL;
  size_t count = 0, capacity = 0;
  struct dirent *item;
  while ((item = readdir(dir)) != NULL) {
    if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) continue;
    if (count == capacity) {
      capacity = capacity ? capacity * 2 : 16;
      names = realloc(names, capacity * sizeof *names);
      if (!names) fail("TRANSFER_BUDGET");
    }
    names[count] = strdup(item->d_name);
    if (!names[count++]) fail("TRANSFER_BUDGET");
  }
  closedir(dir);
  qsort(names, count, sizeof *names, compare_names);
  for (size_t i = 0; i < count; i++) {
    const char *name = names[i];
    if (!valid_component(name, strlen(name))) fail("TRANSFER_INVALID_PATH");
    char path[MAX_PATH_BYTES + 2];
    int n = *prefix ? snprintf(path, sizeof path, "%s/%s", prefix, name)
                    : snprintf(path, sizeof path, "%s", name);
    if (n < 0 || (size_t)n > MAX_PATH_BYTES) fail("TRANSFER_INVALID_PATH");
    struct stat st;
    if (fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) fail("TRANSFER_IO");
    if (st.st_dev != s->dev) fail("TRANSFER_INVALID_ENTRY");
    if (entry_count + 1 > s->max_entries) fail("TRANSFER_BUDGET");
    poll_host();
    entry_t *e = add_entry();
    e->path = strdup(path);
    if (!e->path) fail("TRANSFER_BUDGET");
    commit_entry();
    if (S_ISDIR(st.st_mode)) {
      e->kind = K_DIR;
      e->mode = st.st_mode & 0777;
      int child = open_dir_at(dirfd, name);
      struct stat opened;
      if (child < 0 || fstat(child, &opened) != 0 || opened.st_ino != st.st_ino)
        fail("TRANSFER_INVALID_ENTRY");
      send_entry_frame(e);
      walk(s, child, e->path);
      close(child);
    } else if (S_ISREG(st.st_mode)) {
      e->kind = K_REGULAR;
      send_regular(s, dirfd, name, &st, e);
    } else if (S_ISLNK(st.st_mode)) {
      char target[MAX_PATH_BYTES + 1];
      ssize_t len = readlinkat(dirfd, name, target, sizeof target);
      if (len <= 0 || (size_t)len > MAX_PATH_BYTES) fail("TRANSFER_INVALID_ENTRY");
      target[len] = '\0';
      if (!valid_target(target, (size_t)len, e->path)) fail("TRANSFER_INVALID_ENTRY");
      e->kind = K_SYMLINK;
      e->mode = 0777;
      e->target = strdup(target);
      if (!e->target) fail("TRANSFER_BUDGET");
      send_entry_frame(e);
    } else {
      fail("TRANSFER_INVALID_ENTRY"); /* FIFOs, sockets and devices */
    }
    free(names[i]);
  }
  free(names);
}

static int run_send(char **argv) {
  send_t s = {0};
  if (!valid_id(argv[1]) || !parse_u64(argv[2], &s.max_entries) ||
      !parse_u64(argv[3], &s.max_total) || !parse_u64(argv[4], &s.max_entry))
    return 2;
  struct stat st;
  /* W04C: a regular file is sent as a file root (one empty-path entry). */
  char name[MAX_COMPONENT + 1];
  int holder = strcmp(argv[0], "/") == 0 ? -1 : open_destination_parent(argv[0], name, sizeof name);
  if (holder >= 0 && fstatat(holder, name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(st.st_mode)) {
    s.dev = st.st_dev;
    if (s.max_entries < 1) fail("TRANSFER_BUDGET");
    entry_t *e = add_entry();
    e->path = strdup("");
    if (!e->path) fail("TRANSFER_BUDGET");
    commit_entry();
    e->kind = K_REGULAR;
    send_regular(&s, holder, name, &st, e);
    close(holder);
  } else {
    /* FIFOs, sockets and devices are refused as entries are; a symlinked
       root stays an invalid path (it is never followed). */
    int special = holder >= 0 && fstatat(holder, name, &st, AT_SYMLINK_NOFOLLOW) == 0 &&
                  !S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode);
    if (holder >= 0) close(holder);
    if (special) fail("TRANSFER_INVALID_ENTRY");
    int root = open_absolute_dir(argv[0]);
    if (root < 0 || fstat(root, &st) != 0) fail("TRANSFER_INVALID_PATH");
    s.dev = st.st_dev;
    walk(&s, root, "");
  }
  uint8_t payload[48];
  uint64_t bytes;
  manifest_digest(payload, &bytes);
  put_u64(payload + 32, (uint64_t)entry_count);
  put_u64(payload + 40, bytes);
  send_frame(F_MANIFEST, payload, sizeof payload);
  return 0;
}

static int run_abort(char **argv) {
  char name[MAX_COMPONENT + 1];
  if (!valid_id(argv[1])) return 2;
  int parent = open_destination_parent(argv[0], name, sizeof name);
  struct stat st;
  if (parent < 0 || fstat(parent, &st) != 0) return 1;
  snprintf(staging_name, sizeof staging_name, ".koala-transfer-%s", argv[1]);
  return remove_tree(parent, staging_name, st.st_dev) == 0 ? 0 : 1;
}

/* After the receiving agent died: records the commit iff the prepared
   staging identity is now at the destination name, then removes staging and
   the intent record. A missing or mismatched identity means not published. */
static int run_recover(char **argv) {
  const char *dest = argv[0], *id = argv[1];
  if (!valid_id(id)) return 2;
  int state = open_state_dir(argv[2]);
  if (state < 0) return 1;
  char name[MAX_COMPONENT + 1], committed[96];
  int parent = open_destination_parent(dest, name, sizeof name);
  snprintf(staging_name, sizeof staging_name, ".koala-transfer-%s", id);
  snprintf(prepared_name, sizeof prepared_name, "%s.prepared", id);
  snprintf(committed, sizeof committed, "%s.committed", id);
  struct stat st;
  if (fstatat(state, committed, &st, AT_SYMLINK_NOFOLLOW) != 0) {
    char text[161] = {0}, digest_hex[65] = {0};
    unsigned long long dev = 0, ino = 0;
    uint8_t digest[32];
    int fd = openat(state, prepared_name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    ssize_t n = fd >= 0 ? read(fd, text, sizeof text - 1) : -1;
    if (fd >= 0) close(fd);
    struct stat at;
    if (n > 0 && sscanf(text, "v1 prepared sha256:%64[0-9a-f] %llu %llu", digest_hex, &dev, &ino) == 3 &&
        strlen(digest_hex) == 64 && unhex(digest_hex, digest) && parent >= 0 &&
        fstatat(parent, name, &at, AT_SYMLINK_NOFOLLOW) == 0 &&
        (S_ISDIR(at.st_mode) || S_ISREG(at.st_mode)) && /* a tree or a file root (W04C) */
        (unsigned long long)at.st_dev == dev && (unsigned long long)at.st_ino == ino)
      write_record(state, id, digest);
  }
  if (parent >= 0 && fstat(parent, &st) == 0 && remove_tree(parent, staging_name, st.st_dev) != 0)
    return 1;
  (void)unlinkat(state, prepared_name, 0);
  /* Temporaries of a write interrupted by the crash. */
  char temporary[128];
  snprintf(temporary, sizeof temporary, "%s.tmp", prepared_name);
  (void)unlinkat(state, temporary, 0);
  snprintf(temporary, sizeof temporary, "%s.tmp", committed);
  (void)unlinkat(state, temporary, 0);
  return fsync(state) == 0 ? 0 : 1;
}

/* ---------- Secret files (W06B2) ---------- */

/* Exit codes for the secret modes; they never report secret bytes. */
enum { SECRET_OK = 0, SECRET_IO = 1, SECRET_USAGE = 2, SECRET_NOT_TMPFS = 3,
       SECRET_EXISTS = 4, SECRET_SIZE = 5 };
#define MAX_SECRET_BYTES 65536

#ifdef __linux__
static int is_tmpfs(int fd) {
  struct statfs info;
  return fstatfs(fd, &info) == 0 && info.f_type == 0x01021994; /* TMPFS_MAGIC */
}

/* Ensures /run/secrets is a 0700 directory on tmpfs, mounting a private
   tmpfs there when it is not; never follows links. */
static int run_secrets_dir(void) {
  int root = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (root < 0) return SECRET_IO;
  if (mkdirat(root, "run", 0755) != 0 && errno != EEXIST) return SECRET_IO;
  int run = openat(root, "run", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (run < 0) return SECRET_IO;
  if (mkdirat(run, "secrets", 0700) != 0 && errno != EEXIST) return SECRET_IO;
  int dir = openat(run, "secrets", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (dir < 0) return SECRET_IO;
  if (!is_tmpfs(dir)) {
    close(dir);
    if (mount("tmpfs", "/run/secrets", "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
              "mode=0700,size=8m") != 0)
      return SECRET_IO;
    dir = openat(run, "secrets", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dir < 0 || !is_tmpfs(dir)) return SECRET_NOT_TMPFS;
  }
  return fchmod(dir, 0700) == 0 ? SECRET_OK : SECRET_IO;
}

/* Creates DEST (mode 0400) from at most 64 KiB on stdin. The parent is
   opened component-wise without following links and must be tmpfs; an
   existing name is never replaced. The value is never in argv. */
static int run_install_secret(const char *dest) {
  char name[MAX_COMPONENT + 1];
  int parent = open_destination_parent(dest, name, sizeof name);
  if (parent < 0) return SECRET_USAGE;
  if (!is_tmpfs(parent)) return SECRET_NOT_TMPFS;
  static uint8_t value[MAX_SECRET_BYTES + 1];
  size_t length = 0;
  for (;;) {
    ssize_t n = read(STDIN_FILENO, value + length, sizeof value - length);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) { memset(value, 0, sizeof value); return SECRET_IO; }
    if (n == 0) break;
    length += (size_t)n;
    if (length > MAX_SECRET_BYTES) { memset(value, 0, sizeof value); return SECRET_SIZE; }
  }
  if (length == 0) return SECRET_SIZE;
  int fd = openat(parent, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0400);
  if (fd < 0) { memset(value, 0, sizeof value); return errno == EEXIST ? SECRET_EXISTS : SECRET_IO; }
  int status = SECRET_OK;
  for (size_t written = 0; written < length && status == SECRET_OK;) {
    ssize_t w = write(fd, value + written, length - written);
    if (w < 0 && errno == EINTR) continue;
    if (w <= 0) status = SECRET_IO; else written += (size_t)w;
  }
  memset(value, 0, sizeof value);
  if (fchmod(fd, 0400) != 0 || close(fd) != 0) status = SECRET_IO;
  if (status != SECRET_OK) (void)unlinkat(parent, name, 0);
  return status;
}
#else
static int run_secrets_dir(void) { return SECRET_USAGE; }
static int run_install_secret(const char *dest) { (void)dest; return SECRET_USAGE; }
#endif

int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);
  umask(077);
  if (argc == 9 && strcmp(argv[1], "receive") == 0) return run_receive(argv + 2);
  if (argc == 7 && strcmp(argv[1], "send") == 0) return run_send(argv + 2);
  if (argc == 4 && strcmp(argv[1], "abort") == 0) return run_abort(argv + 2);
  if (argc == 5 && strcmp(argv[1], "recover") == 0) return run_recover(argv + 2);
  if (argc == 2 && strcmp(argv[1], "secrets-dir") == 0) return run_secrets_dir();
  if (argc == 3 && strcmp(argv[1], "install-secret") == 0) return run_install_secret(argv[2]);
  fprintf(stderr,
          "usage: koala-transfer receive DEST fail-if-exists|replace ID MAX_ENTRIES MAX_TOTAL "
          "MAX_ENTRY STATE_DIR\n"
          "       koala-transfer send SRC ID MAX_ENTRIES MAX_TOTAL MAX_ENTRY\n"
          "       koala-transfer abort DEST ID\n"
          "       koala-transfer recover DEST ID STATE_DIR\n"
          "       koala-transfer secrets-dir\n"
          "       koala-transfer install-secret DEST < VALUE\n");
  return 2;
}
