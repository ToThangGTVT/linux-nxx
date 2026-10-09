/*
 * nxcompat: newlib only provides sigsetjmp/siglongjmp for Cygwin/RTEMS.
 * Horizon has no signal masks, so they are plain setjmp/longjmp.
 */
#include_next <setjmp.h>

#ifndef NXCOMPAT_SETJMP_H
#define NXCOMPAT_SETJMP_H

#if !defined(__CYGWIN__) && !defined(__rtems__)
typedef jmp_buf sigjmp_buf;
#define sigsetjmp(env, savemask) setjmp(env)
#define siglongjmp(env, val) longjmp(env, val)
#endif

#endif
