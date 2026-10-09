/* nxcompat: positional I/O and other unistd/dirent bits newlib lacks */
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

/*
 * newlib has no pread/pwrite. Emulate with lseek under one lock, so
 * concurrent callers (QEMU's block thread pool) don't race on the offset.
 */
static pthread_mutex_t g_pio_lock = PTHREAD_MUTEX_INITIALIZER;

ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
    ssize_t ret = -1;
    off_t old;

    pthread_mutex_lock(&g_pio_lock);
    old = lseek(fd, 0, SEEK_CUR);
    if (old >= 0 && lseek(fd, offset, SEEK_SET) >= 0) {
        ret = read(fd, buf, count);
        int saved = errno;
        lseek(fd, old, SEEK_SET);
        errno = saved;
    }
    pthread_mutex_unlock(&g_pio_lock);
    return ret;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    ssize_t ret = -1;
    off_t old;

    pthread_mutex_lock(&g_pio_lock);
    old = lseek(fd, 0, SEEK_CUR);
    if (old >= 0 && lseek(fd, offset, SEEK_SET) >= 0) {
        ret = write(fd, buf, count);
        int saved = errno;
        lseek(fd, old, SEEK_SET);
        errno = saved;
    }
    pthread_mutex_unlock(&g_pio_lock);
    return ret;
}

int getrusage(int who, struct rusage *usage)
{
    (void)who;
    memset(usage, 0, sizeof(*usage));
    return 0;
}

int sigwait(const sigset_t *set, int *sig)
{
    (void)set; (void)sig;
    return EINVAL;
}

DIR *fdopendir(int fd)
{
    (void)fd;
    errno = ENOSYS;
    return NULL;
}

int dirfd(DIR *dirp)
{
    (void)dirp;
    errno = ENOTSUP;
    return -1;
}

int daemon(int nochdir, int noclose)
{
    (void)nochdir; (void)noclose;
    errno = ENOSYS;
    return -1;
}
