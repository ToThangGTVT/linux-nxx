/* nxcompat: <sys/uio.h> for Horizon */
#ifndef NXCOMPAT_SYS_UIO_H
#define NXCOMPAT_SYS_UIO_H

#include <sys/types.h>
#include <sys/_iovec.h>

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

#ifdef __cplusplus
extern "C" {
#endif

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);

#ifdef __cplusplus
}
#endif

#endif
