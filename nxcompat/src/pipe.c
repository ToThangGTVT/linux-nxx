/*
 * nxcompat: pipe() plus poll()/select()/fcntl() that understand it.
 *
 * Horizon has no kernel pipes. glib (GWakeup) and QEMU (EventNotifier,
 * main-loop wakeups) rely on them, so pipes are emulated in-process as a
 * newlib devoptab device. libnx's own poll/select/fcntl only accept BSD
 * sockets; they are wrapped (ld --wrap) so pipe fds are handled here and
 * socket fds are forwarded to libnx.
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/iosupport.h>
#include <sys/select.h>

#define NXC_PIPE_CAPACITY 65536
/* While sockets are being polled we must re-check them periodically */
#define NXC_SOCKET_POLL_SLICE_MS 2

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    uint8_t buf[NXC_PIPE_CAPACITY];
    size_t head;
    size_t count;
    int readers;
    int writers;
} NxcPipe;

typedef struct {
    NxcPipe *pipe;
    bool writer;
    int flags;
} NxcPipeEnd;

static pthread_mutex_t g_wake_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_wake_cond = PTHREAD_COND_INITIALIZER;
static uint64_t g_wake_gen;

static pthread_once_t g_pipe_once = PTHREAD_ONCE_INIT;
static int g_pipe_dev = -1;

static void nxc_wake_pollers(void)
{
    pthread_mutex_lock(&g_wake_lock);
    g_wake_gen++;
    pthread_cond_broadcast(&g_wake_cond);
    pthread_mutex_unlock(&g_wake_lock);
}

static int pipe_close_r(struct _reent *r, void *fd)
{
    NxcPipeEnd *end = fd;
    NxcPipe *p = end->pipe;
    bool last;

    (void)r;
    pthread_mutex_lock(&p->lock);
    if (end->writer) {
        p->writers--;
    } else {
        p->readers--;
    }
    last = p->readers == 0 && p->writers == 0;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);

    if (last) {
        pthread_cond_destroy(&p->cond);
        pthread_mutex_destroy(&p->lock);
        free(p);
    }
    nxc_wake_pollers();
    return 0;
}

static ssize_t pipe_write_r(struct _reent *r, void *fd, const char *ptr, size_t len)
{
    NxcPipeEnd *end = fd;
    NxcPipe *p = end->pipe;
    size_t done = 0;

    if (!end->writer) {
        r->_errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&p->lock);
    while (done < len) {
        if (p->readers == 0) {
            pthread_mutex_unlock(&p->lock);
            r->_errno = EPIPE;
            return done ? (ssize_t)done : -1;
        }
        if (p->count == NXC_PIPE_CAPACITY) {
            if (done || (end->flags & O_NONBLOCK)) {
                break;
            }
            pthread_cond_wait(&p->cond, &p->lock);
            continue;
        }
        size_t tail = (p->head + p->count) % NXC_PIPE_CAPACITY;
        size_t chunk = NXC_PIPE_CAPACITY - p->count;
        if (chunk > NXC_PIPE_CAPACITY - tail) {
            chunk = NXC_PIPE_CAPACITY - tail;
        }
        if (chunk > len - done) {
            chunk = len - done;
        }
        memcpy(p->buf + tail, ptr + done, chunk);
        p->count += chunk;
        done += chunk;
        pthread_cond_broadcast(&p->cond);
    }
    pthread_mutex_unlock(&p->lock);

    if (done == 0) {
        r->_errno = EAGAIN;
        return -1;
    }
    nxc_wake_pollers();
    return done;
}

static ssize_t pipe_read_r(struct _reent *r, void *fd, char *ptr, size_t len)
{
    NxcPipeEnd *end = fd;
    NxcPipe *p = end->pipe;
    size_t done = 0;

    if (end->writer) {
        r->_errno = EBADF;
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    pthread_mutex_lock(&p->lock);
    while (p->count == 0) {
        if (p->writers == 0) {
            pthread_mutex_unlock(&p->lock);
            return 0; /* EOF */
        }
        if (end->flags & O_NONBLOCK) {
            pthread_mutex_unlock(&p->lock);
            r->_errno = EAGAIN;
            return -1;
        }
        pthread_cond_wait(&p->cond, &p->lock);
    }
    while (done < len && p->count) {
        size_t chunk = NXC_PIPE_CAPACITY - p->head;
        if (chunk > p->count) {
            chunk = p->count;
        }
        if (chunk > len - done) {
            chunk = len - done;
        }
        memcpy(ptr + done, p->buf + p->head, chunk);
        p->head = (p->head + chunk) % NXC_PIPE_CAPACITY;
        p->count -= chunk;
        done += chunk;
    }
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);

    nxc_wake_pollers();
    return done;
}

static int pipe_fstat_r(struct _reent *r, void *fd, struct stat *st)
{
    (void)r; (void)fd;
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFIFO | 0600;
    return 0;
}

static const devoptab_t g_pipe_devoptab = {
    .name = "nxcpipe",
    .structSize = sizeof(NxcPipeEnd),
    .close_r = pipe_close_r,
    .write_r = pipe_write_r,
    .read_r = pipe_read_r,
    .fstat_r = pipe_fstat_r,
};

static void nxc_pipe_register(void)
{
    AddDevice(&g_pipe_devoptab);
    g_pipe_dev = FindDevice("nxcpipe:");
}

static NxcPipeEnd *nxc_pipe_end(int fd)
{
    __handle *h;

    if (g_pipe_dev < 0) {
        return NULL;
    }
    h = __get_handle(fd);
    if (!h || (int)h->device != g_pipe_dev) {
        return NULL;
    }
    return h->fileStruct;
}

static bool nxc_is_socket(int fd)
{
    __handle *h = __get_handle(fd);

    return h && devoptab_list[h->device] &&
           strcmp(devoptab_list[h->device]->name, "soc") == 0;
}

int pipe(int fds[2])
{
    NxcPipe *p;
    int rfd, wfd;

    pthread_once(&g_pipe_once, nxc_pipe_register);
    if (g_pipe_dev < 0) {
        errno = ENFILE;
        return -1;
    }

    p = calloc(1, sizeof(*p));
    if (!p) {
        errno = ENOMEM;
        return -1;
    }
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->cond, NULL);
    p->readers = 1;
    p->writers = 1;

    rfd = __alloc_handle(g_pipe_dev);
    if (rfd < 0) {
        goto fail;
    }
    wfd = __alloc_handle(g_pipe_dev);
    if (wfd < 0) {
        __release_handle(rfd);
        goto fail;
    }
    *(NxcPipeEnd *)__get_handle(rfd)->fileStruct = (NxcPipeEnd){ p, false, 0 };
    *(NxcPipeEnd *)__get_handle(wfd)->fileStruct = (NxcPipeEnd){ p, true, 0 };
    fds[0] = rfd;
    fds[1] = wfd;
    return 0;

fail:
    pthread_cond_destroy(&p->cond);
    pthread_mutex_destroy(&p->lock);
    free(p);
    errno = EMFILE;
    return -1;
}

/* ---- fcntl ---- */

int __real_fcntl(int fd, int cmd, ...);

int __wrap_fcntl(int fd, int cmd, ...)
{
    NxcPipeEnd *end;
    va_list ap;
    long arg;

    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);

    if (nxc_is_socket(fd)) {
        return __real_fcntl(fd, cmd, arg);
    }
    if (!__get_handle(fd)) {
        errno = EBADF;
        return -1;
    }

    end = nxc_pipe_end(fd);
    switch (cmd) {
    case F_GETFL:
        if (end) {
            return end->flags | (end->writer ? O_WRONLY : O_RDONLY);
        }
        return O_RDWR;
    case F_SETFL:
        if (end) {
            end->flags = (int)arg & O_NONBLOCK;
        }
        return 0;
    case F_GETFD:
    case F_SETFD:
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

/* ---- poll / select ---- */

int __real_poll(struct pollfd *fds, nfds_t nfds, int timeout);

static int nxc_poll_scan(struct pollfd *fds, nfds_t nfds, bool *has_sockets)
{
    struct pollfd sock_fds[64];
    nfds_t sock_idx[64];
    nfds_t nsock = 0;
    int ready = 0;

    *has_sockets = false;
    for (nfds_t i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        NxcPipeEnd *end;

        fds[i].revents = 0;
        if (fd < 0) {
            continue;
        }
        if (!__get_handle(fd)) {
            fds[i].revents = POLLNVAL;
        } else if ((end = nxc_pipe_end(fd))) {
            NxcPipe *p = end->pipe;

            pthread_mutex_lock(&p->lock);
            if (end->writer) {
                if (p->readers == 0) {
                    fds[i].revents |= POLLERR;
                } else if (p->count < NXC_PIPE_CAPACITY) {
                    fds[i].revents |= fds[i].events & (POLLOUT | POLLWRNORM);
                }
            } else {
                if (p->count) {
                    fds[i].revents |= fds[i].events & (POLLIN | POLLRDNORM);
                }
                if (p->writers == 0) {
                    fds[i].revents |= POLLHUP;
                }
            }
            pthread_mutex_unlock(&p->lock);
        } else if (nxc_is_socket(fd) && nsock < 64) {
            sock_fds[nsock] = fds[i];
            sock_idx[nsock] = i;
            nsock++;
            continue;
        } else {
            /* Regular files and the console never block */
            fds[i].revents = fds[i].events &
                             (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM);
        }
        if (fds[i].revents) {
            ready++;
        }
    }

    if (nsock) {
        *has_sockets = true;
        if (__real_poll(sock_fds, nsock, 0) > 0) {
            for (nfds_t j = 0; j < nsock; j++) {
                fds[sock_idx[j]].revents = sock_fds[j].revents;
                if (sock_fds[j].revents) {
                    ready++;
                }
            }
        }
    }
    return ready;
}

static int64_t nxc_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int __wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int64_t deadline = timeout < 0 ? INT64_MAX : nxc_now_ms() + timeout;

    for (;;) {
        bool has_sockets;
        uint64_t gen;
        int ready;

        pthread_mutex_lock(&g_wake_lock);
        gen = g_wake_gen;
        pthread_mutex_unlock(&g_wake_lock);

        ready = nxc_poll_scan(fds, nfds, &has_sockets);
        if (ready || timeout == 0) {
            return ready;
        }

        int64_t now = nxc_now_ms();
        if (now >= deadline) {
            return 0;
        }
        int64_t wait_until = deadline;
        if (has_sockets && now + NXC_SOCKET_POLL_SLICE_MS < wait_until) {
            wait_until = now + NXC_SOCKET_POLL_SLICE_MS;
        }

        /* Sleep until a pipe changes state or the wait slice expires */
        pthread_mutex_lock(&g_wake_lock);
        while (g_wake_gen == gen) {
            struct timespec abs;
            int64_t left;

            if (wait_until == INT64_MAX) {
                pthread_cond_wait(&g_wake_cond, &g_wake_lock);
                continue;
            }
            left = wait_until - nxc_now_ms();
            if (left <= 0) {
                break;
            }
            clock_gettime(CLOCK_REALTIME, &abs);
            abs.tv_sec += left / 1000;
            abs.tv_nsec += (left % 1000) * 1000000;
            if (abs.tv_nsec >= 1000000000) {
                abs.tv_sec++;
                abs.tv_nsec -= 1000000000;
            }
            if (pthread_cond_timedwait(&g_wake_cond, &g_wake_lock, &abs) == ETIMEDOUT) {
                break;
            }
        }
        pthread_mutex_unlock(&g_wake_lock);
    }
}

int __wrap_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
                  struct timeval *tv)
{
    struct pollfd pfds[FD_SETSIZE];
    nfds_t n = 0;
    int timeout = tv ? (int)(tv->tv_sec * 1000 + tv->tv_usec / 1000) : -1;
    int ret;

    for (int fd = 0; fd < nfds && fd < FD_SETSIZE; fd++) {
        short ev = 0;

        if (rfds && FD_ISSET(fd, rfds)) {
            ev |= POLLIN;
        }
        if (wfds && FD_ISSET(fd, wfds)) {
            ev |= POLLOUT;
        }
        if (efds && FD_ISSET(fd, efds)) {
            ev |= POLLPRI;
        }
        if (ev) {
            pfds[n++] = (struct pollfd){ .fd = fd, .events = ev };
        }
    }

    ret = __wrap_poll(pfds, n, timeout);
    if (ret < 0) {
        return ret;
    }

    if (rfds) {
        FD_ZERO(rfds);
    }
    if (wfds) {
        FD_ZERO(wfds);
    }
    if (efds) {
        FD_ZERO(efds);
    }
    ret = 0;
    for (nfds_t i = 0; i < n; i++) {
        short re = pfds[i].revents;
        if (rfds && (re & (POLLIN | POLLHUP | POLLERR))) {
            FD_SET(pfds[i].fd, rfds);
            ret++;
        }
        if (wfds && (re & (POLLOUT | POLLERR))) {
            FD_SET(pfds[i].fd, wfds);
            ret++;
        }
        if (efds && (re & POLLPRI)) {
            FD_SET(pfds[i].fd, efds);
            ret++;
        }
    }
    return ret;
}
