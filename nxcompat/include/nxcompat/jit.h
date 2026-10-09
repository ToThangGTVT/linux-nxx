/*
 * nxcompat: JIT code memory for Horizon, kept free of libnx headers so it
 * can be used from code (QEMU) whose names clash with <switch.h>.
 */
#ifndef NXCOMPAT_JIT_H
#define NXCOMPAT_JIT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create @size bytes of JIT memory mapped twice: writable at *@rw and
 * executable at *@rx (same physical pages). Requires CodeMemory support
 * (HOS 4.0+ with a loader that grants it, e.g. hbloader on Atmosphere).
 * Returns 0 on success or the libnx Result code on failure.
 */
unsigned int nxc_jit_alloc(size_t size, void **rw, void **rx);

#ifdef __cplusplus
}
#endif

#endif
