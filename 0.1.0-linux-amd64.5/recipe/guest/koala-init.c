// R05B2: the guest's first program. Sets every virtio disk's readahead to
// 128 KB (Linux 6.18 defaults to twice the device's largest request, 8 MB on
// Cloud Hypervisor, so a lean guest cached ~84 MB more than it needed; L18),
// then becomes the real init: the program named after "--" on the kernel
// command line (/sbin/vminitd for the lean profile, /sbin/init for the full
// one), with the rest of its arguments. The kernel passes what follows "--"
// as init's arguments, without the "--" itself, so the real init is the
// first argument that is an absolute path (a stray bare kernel word isn't);
// a literal "--" is honoured too (for running it by hand).
//
// It must never stop a boot: a disk it can't open or set is skipped, and a
// missing or failed init falls back to /sbin/vminitd, then /sbin/init. It
// writes nothing but the readahead and needs no mounted file system beyond
// the kernel's own /dev (CONFIG_DEVTMPFS_MOUNT).
//
// Freestanding, with no C library: it runs as PID 1 before /proc is mounted,
// and the lean init file system's proc/self/exe is a relative link to
// sbin/vminitd, on which glibc's static start-up asserts and kills init (found
// booting R05B2's first build). Six system calls, for x86_64 and arm64.
//
//   koala-init [--] [INIT [ARG...]]
//
// Build: cc -static -nostdlib -ffreestanding -fno-stack-protector -O2 -o koala-init koala-init.c

#define O_RDONLY 0
#define O_NONBLOCK 04000
#define O_CLOEXEC 02000000
#define BLKRASET 0x1262         /* _IO(0x12, 98) */
#define READAHEAD_SECTORS 256   /* 128 KB in 512-byte sectors */

#if defined(__x86_64__)
#define SYS_write 1
#define SYS_open_at 257
#define SYS_close 3
#define SYS_ioctl 16
#define SYS_execve 59
#define SYS_exit 60
static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return ret;
}
static long syscall4(long n, long a, long b, long c, long d) {
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}
#elif defined(__aarch64__)
#define SYS_write 64
#define SYS_open_at 56
#define SYS_close 57
#define SYS_ioctl 29
#define SYS_execve 221
#define SYS_exit 93
static long syscall4(long n, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static long syscall3(long n, long a, long b, long c) { return syscall4(n, a, b, c, 0); }
#else
#error "koala-init: x86_64 or aarch64 only"
#endif

#define AT_FDCWD -100

static unsigned long length(const char *text) {
    unsigned long n = 0;
    while (text[n]) n++;
    return n;
}

static void say(const char *a, const char *b) {
    syscall3(SYS_write, 2, (long)"koala-init: ", 12);
    syscall3(SYS_write, 2, (long)a, (long)length(a));
    syscall3(SYS_write, 2, (long)b, (long)length(b));
}

static void set_readahead(void) {
    char path[] = "/dev/vda";
    for (char letter = 'a'; letter <= 'z'; letter++) {
        path[7] = letter;
        long fd = syscall4(SYS_open_at, AT_FDCWD, (long)path, O_RDONLY | O_CLOEXEC | O_NONBLOCK, 0);
        if (fd < 0) continue;
        if (syscall3(SYS_ioctl, fd, BLKRASET, READAHEAD_SECTORS) != 0) say(path, ": readahead not set\n");
        syscall3(SYS_close, fd, 0, 0);
    }
}

static int same(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// argv and envp from the kernel's initial stack; then never return.
__attribute__((used)) void koala_init_main(long *stack) {
    long argc = stack[0];
    char **argv = (char **)(stack + 1);
    char **envp = argv + argc + 1;
    set_readahead();
    long start = 0;
    for (long i = 1; i < argc && start == 0; i++) {
        if (same(argv[i], "--")) start = i + 1 < argc ? i + 1 : -1;
        else if (argv[i][0] == '/') start = i;
    }
    if (start > 0) {
        syscall3(SYS_execve, (long)argv[start], (long)&argv[start], (long)envp);
        say(argv[start], " could not be started\n");
    }
    static char *const fallbacks[] = {"/sbin/vminitd", "/sbin/init"};
    for (unsigned i = 0; i < 2; i++) {
        char *const fallback_argv[] = {fallbacks[i], 0};
        syscall3(SYS_execve, (long)fallbacks[i], (long)fallback_argv, (long)envp);
    }
    say("no init could be started", "\n");
    syscall3(SYS_exit, 127, 0, 0);
    for (;;) {}
}

// The entry point: pass the initial stack pointer (argc, argv..., envp...).
#if defined(__x86_64__)
__asm__(".globl _start\n_start:\n  mov %rsp, %rdi\n  and $-16, %rsp\n  call koala_init_main\n  hlt\n");
#else
__asm__(".globl _start\n_start:\n  mov x0, sp\n  bl koala_init_main\n  brk #0\n");
#endif
