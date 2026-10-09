/* nxcompat: POSIX limits newlib leaves out */
#include_next <limits.h>

#ifndef NXCOMPAT_LIMITS_H
#define NXCOMPAT_LIMITS_H

#ifndef SSIZE_MAX
#define SSIZE_MAX __LONG_MAX__
#endif
#ifndef _POSIX_HOST_NAME_MAX
#define _POSIX_HOST_NAME_MAX 255
#endif
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#endif
