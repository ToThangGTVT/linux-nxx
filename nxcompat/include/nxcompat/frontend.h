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

/* App directory on the SD card: args.txt, disk images and qemu.log live here */
#define NXFE_DIR "sdmc:/switch/qemu-kitkat"

/*
 * Bring up libnx services and logging, enable CPU boost, and, when launched
 * without arguments (from hbmenu), replace argc/argv with NXFE_DIR/args.txt
 * or built-in defaults. Returns 0 to continue or -1 if QEMU cannot run.
 */
int nxfe_init(int *argc, char ***argv);

/* Show the system software keyboard; returns 0 and fills @out on success */
int nxfe_text_input(char *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif
