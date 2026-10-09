/*
 * nxcompat: POSIX bits that newlib/libnx lack on Horizon.
 *
 * Horizon has no processes, users or signals in the POSIX sense, so the
 * corresponding calls are harmless stubs. Memory mapping only supports
 * anonymous memory, which is what QEMU needs (stacks, guest RAM).
 */
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <switch.h>

#define NX_PAGE_SIZE 0x1000

/* ---- sys/mman.h ---- */

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    void *p;

    (void)addr; (void)prot; (void)off;
    if (!(flags & MAP_ANONYMOUS) || fd != -1 || (flags & MAP_FIXED)) {
        errno = ENODEV;
        return MAP_FAILED;
    }
    len = (len + NX_PAGE_SIZE - 1) & ~(size_t)(NX_PAGE_SIZE - 1);
    p = memalign(NX_PAGE_SIZE, len);
    if (!p) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    memset(p, 0, len);
    return p;
}

int munmap(void *addr, size_t len)
{
    (void)len;
    free(addr);
    return 0;
}

int mprotect(void *addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }
int madvise(void *addr, size_t len, int advice) { (void)addr; (void)len; (void)advice; return 0; }
int msync(void *addr, size_t len, int flags) { (void)addr; (void)len; (void)flags; return 0; }
int mlock(const void *addr, size_t len) { (void)addr; (void)len; return 0; }
int munlock(const void *addr, size_t len) { (void)addr; (void)len; return 0; }
int mlockall(int flags) { (void)flags; return 0; }
int munlockall(void) { return 0; }
int shm_open(const char *name, int oflag, mode_t mode) { (void)name; (void)oflag; (void)mode; errno = ENOSYS; return -1; }
int shm_unlink(const char *name) { (void)name; errno = ENOSYS; return -1; }

/* ---- stdlib.h: newlib's libc lacks posix_memalign ---- */

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    void *p;

    if (alignment < sizeof(void *) || (alignment & (alignment - 1))) {
        return EINVAL;
    }
    p = memalign(alignment, size ? size : 1);
    if (!p) {
        return ENOMEM;
    }
    *memptr = p;
    return 0;
}

/* ---- sys/uio.h ---- */

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        ssize_t n = read(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            return total ? total : n;
        }
        total += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return total;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        ssize_t n = write(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            return total ? total : n;
        }
        total += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return total;
}

ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    ssize_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        ssize_t n = pread(fd, iov[i].iov_base, iov[i].iov_len, offset + total);
        if (n < 0) {
            return total ? total : n;
        }
        total += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return total;
}

ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
    ssize_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        ssize_t n = pwrite(fd, iov[i].iov_base, iov[i].iov_len, offset + total);
        if (n < 0) {
            return total ? total : n;
        }
        total += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return total;
}

/* ---- sys/utsname.h ---- */

int uname(struct utsname *buf)
{
    memset(buf, 0, sizeof(*buf));
    strcpy(buf->sysname, "Horizon");
    strcpy(buf->nodename, "switch");
    strcpy(buf->release, "1.0");
    strcpy(buf->version, "libnx");
    strcpy(buf->machine, "aarch64");
    return 0;
}

/* ---- unistd.h ---- */

int getpagesize(void) { return NX_PAGE_SIZE; }

long sysconf(int name)
{
    u64 mem = 0;

    switch (name) {
    case _SC_PAGESIZE:
        return NX_PAGE_SIZE;
    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN:
        /* Core 3 belongs to the system; homebrew gets cores 0-2 */
        return 3;
    case _SC_CLK_TCK:
        return 100;
    case _SC_OPEN_MAX:
        return 256;
    case _SC_PHYS_PAGES:
        svcGetInfo(&mem, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        return (long)(mem / NX_PAGE_SIZE);
    case _SC_AVPHYS_PAGES: {
        u64 used = 0;
        svcGetInfo(&mem, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
        return (long)((mem - used) / NX_PAGE_SIZE);
    }
#ifdef _SC_HOST_NAME_MAX
    case _SC_HOST_NAME_MAX:
        return 255;
#endif
    default:
        errno = EINVAL;
        return -1;
    }
}

uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }
pid_t getppid(void) { return 0; }

pid_t fork(void) { errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { (void)path; (void)argv; errno = ENOSYS; return -1; }
int execvp(const char *file, char *const argv[]) { (void)file; (void)argv; errno = ENOSYS; return -1; }
pid_t waitpid(pid_t pid, int *status, int options) { (void)pid; (void)status; (void)options; errno = ECHILD; return -1; }

/* ---- signal.h: Horizon has no POSIX signals ---- */

int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{
    (void)sig; (void)act;
    if (old) {
        memset(old, 0, sizeof(*old));
    }
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
    (void)how; (void)set;
    if (old) {
        sigemptyset(old);
    }
    return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *old)
{
    return sigprocmask(how, set, old);
}

int sigaltstack(const stack_t *ss, stack_t *old)
{
    (void)ss;
    if (old) {
        memset(old, 0, sizeof(*old));
        old->ss_flags = SS_DISABLE;
    }
    return 0;
}

int pthread_kill(pthread_t thread, int sig)
{
    (void)thread; (void)sig;
    return 0;
}

/*
 * ---- pthread ----
 * libnx threads default to a small stack and are all pinned to the
 * process's default core. Give every thread a usable stack, allow it on all
 * three application cores and spread preferred cores so the vCPU thread does
 * not compete with the main loop for core 0.
 */

#define NXC_MIN_THREAD_STACK (2 * 1024 * 1024)
#define NXC_APP_CORE_MASK 0x7

typedef struct {
    void *(*fn)(void *);
    void *arg;
} NxcThreadStart;

static int g_next_core = 1;

static void *nxc_thread_trampoline(void *opaque)
{
    NxcThreadStart start = *(NxcThreadStart *)opaque;
    int core = __atomic_fetch_add(&g_next_core, 1, __ATOMIC_RELAXED) % 3;

    free(opaque);
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, NXC_APP_CORE_MASK);
    return start.fn(start.arg);
}

int __real_pthread_create(pthread_t *t, const pthread_attr_t *attr,
                          void *(*fn)(void *), void *arg);

int __wrap_pthread_create(pthread_t *t, const pthread_attr_t *attr,
                          void *(*fn)(void *), void *arg)
{
    pthread_attr_t a;
    NxcThreadStart *start;
    size_t sz = 0;
    int ret;

    start = malloc(sizeof(*start));
    if (!start) {
        return EAGAIN;
    }
    start->fn = fn;
    start->arg = arg;

    if (attr) {
        a = *attr;
    } else {
        pthread_attr_init(&a);
    }
    pthread_attr_getstacksize(&a, &sz);
    if (sz < NXC_MIN_THREAD_STACK) {
        pthread_attr_setstacksize(&a, NXC_MIN_THREAD_STACK);
    }
    ret = __real_pthread_create(t, &a, nxc_thread_trampoline, start);
    if (ret) {
        free(start);
    }
    if (!attr) {
        pthread_attr_destroy(&a);
    }
    return ret;
}
