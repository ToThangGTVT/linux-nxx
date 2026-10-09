/* nxcompat: crash logger for Horizon (no libnx headers) */
#ifndef NXCOMPAT_CRASH_H
#define NXCOMPAT_CRASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Install the libnx exception handler. When any thread crashes it writes
 * the registers, a frame-pointer backtrace and a stack scan to @log_fd
 * (addresses inside the app are printed as ELF offsets for addr2line), then
 * hands the exception back to the system so Atmosphère still writes its own
 * crash report. abort() and svcBreak (libnx's diagAbortWithResult and
 * fatalThrow) are logged the same way (linked with --wrap); abort() then
 * breaks instead of quietly exiting. exit() logs its status and caller.
 * Pass -1 to only keep the system behaviour.
 */
void nxc_crash_init(int log_fd);

#ifdef __cplusplus
}
#endif

#endif
