/*
 * nxcompat: Switch application frontend for QEMU (no libnx headers, so it
 * can be included from QEMU sources).
 */
#ifndef NXCOMPAT_FRONTEND_H
#define NXCOMPAT_FRONTEND_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bring up libnx services and logging, enable CPU boost, and, when launched
 * without arguments (from hbmenu), replace argc/argv with <app dir>/args.txt
 * or the built-in defaults for @target (QEMU's TARGET_NAME: "i386" or "arm").
 * The app directory is sdmc:/switch/qemu-kitkat (i386) or
 * sdmc:/switch/qemu-kitkat-arm (arm) and also holds the disks and qemu.log.
 * Returns 0 to continue or -1 if QEMU cannot run.
 */
int nxfe_init(int *argc, char ***argv, const char *target);

/* Show the system software keyboard; returns 0 and fills @out on success */
int nxfe_text_input(char *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif
