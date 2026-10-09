/* nxcompat: crash logger (see include/nxcompat/crash.h) */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <switch.h>
#include "nxcompat/crash.h"

#define STACK_SCAN_WORDS 2048
#define MAX_FRAMES 48
#define MAX_SCAN_HITS 48

/* libnx runs the handler on this stack; its default 1 KiB is too small for snprintf */
alignas(16) u8 __nx_exception_stack[0x8000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

/* Start of the app image (switch.ld); the ELF is linked at 0, so addr - base = ELF address */
extern char __code_start[];

static int g_log_fd = -1;
static u64 g_text_start, g_text_end;

void nxc_crash_init(int log_fd)
{
    MemoryInfo mi;
    u32 pi;

    g_log_fd = log_fd;
    if (R_SUCCEEDED(svcQueryMemory(&mi, &pi, (u64)__code_start))) {
        g_text_start = mi.addr;
        g_text_end = mi.addr + mi.size;
    }
}

/*
 * Write straight to the log descriptor: the crashed thread may hold the
 * stdio or malloc lock, so FILE functions could deadlock here.
 */
static void out(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(buf) - 1) {
        n = sizeof(buf) - 1;
    }
    if (n > 0) {
        write(g_log_fd, buf, n);
    }
}

static bool in_text(u64 a)
{
    return a >= g_text_start && a < g_text_end;
}

/* "elf+0x..." for code in the app, the raw address otherwise */
static const char *addr_str(char *buf, size_t len, u64 a)
{
    if (in_text(a)) {
        snprintf(buf, len, "elf+0x%lx", a - g_text_start);
    } else {
        snprintf(buf, len, "0x%016lx", a);
    }
    return buf;
}

static bool readable(u64 a, size_t len, MemoryInfo *mi)
{
    u32 pi;

    return R_SUCCEEDED(svcQueryMemory(mi, &pi, a)) && mi->type != MemType_Unmapped &&
           (mi->perm & Perm_R) && a + len <= mi->addr + mi->size;
}

static const char *exception_name(const ThreadExceptionDump *ctx)
{
    switch (ctx->esr >> 26) {
    case 0x00: return "undefined instruction";
    case 0x0e: return "illegal execution state";
    case 0x15: return "SVC";
    case 0x20: case 0x21: return "instruction abort";
    case 0x22: return "misaligned PC";
    case 0x24: case 0x25: return "data abort";
    case 0x26: return "misaligned SP";
    case 0x2f: return "SError";
    case 0x3c: return "BRK";
    }
    switch (ctx->error_desc) {
    case ThreadExceptionDesc_BadSVC: return "bad SVC";
    case ThreadExceptionDesc_Trap: return "trap";
    }
    return "other";
}

static void describe_region(const char *what, u64 a)
{
    MemoryInfo mi;
    u32 pi;

    if (R_SUCCEEDED(svcQueryMemory(&mi, &pi, a))) {
        out("  %-3s in region 0x%016lx +0x%lx type 0x%x perm %c%c%c\n", what, mi.addr, mi.size,
            mi.type, (mi.perm & Perm_R) ? 'r' : '-', (mi.perm & Perm_W) ? 'w' : '-',
            (mi.perm & Perm_X) ? 'x' : '-');
    }
}

static void backtrace_fp(u64 fp)
{
    MemoryInfo mi;
    char b[32];

    out("backtrace (frame pointers):\n");
    for (int i = 0; i < MAX_FRAMES && fp && !(fp & 7) && readable(fp, 16, &mi); i++) {
        const u64 *frame = (const u64 *)fp;

        if (!frame[1]) {
            break;
        }
        out("  #%-2d %s\n", i, addr_str(b, sizeof(b), frame[1]));
        if (frame[0] <= fp) {
            break; /* frames must move up the stack */
        }
        fp = frame[0];
    }
}

/* Frame pointers can be missing, so also list stack words that point into the app's code */
static void stack_scan(u64 sp)
{
    MemoryInfo mi;
    char b[32];
    int hits = 0;

    out("stack scan (words pointing into the app's code, may include stale values):\n");
    if (!readable(sp, 8, &mi)) {
        return;
    }
    u64 end = mi.addr + mi.size;
    for (u64 a = sp; a + 8 <= end && a < sp + STACK_SCAN_WORDS * 8 && hits < MAX_SCAN_HITS; a += 8) {
        u64 v = *(const u64 *)a;

        if (in_text(v)) {
            out("  sp+0x%-5lx %s\n", a - sp, addr_str(b, sizeof(b), v));
            hits++;
        }
    }
}

Result __real_svcBreak(u32 reason, uintptr_t address, uintptr_t size);
void __real_abort(void);
void __real_exit(int status);

static void dump_here(void)
{
    u64 fp = (u64)__builtin_frame_address(0);

    backtrace_fp(fp);
    stack_scan(fp);
    out("*** end of crash dump\n");
}

/*
 * libnx reports fatal service errors (diagAbortWithResult, fatalThrow) with
 * svcBreak, which never reaches the exception handler: log it first.
 */
Result __wrap_svcBreak(u32 reason, uintptr_t address, uintptr_t size)
{
    if (g_log_fd >= 0 && !(reason & BreakReason_NotificationOnlyFlag)) {
        static const char *const names[] = { "panic", "assert", "user" };
        u32 r = reason & 0xff;

        out("\n*** CRASH: svcBreak (%s, reason 0x%x)", r < 3 ? names[r] : "other", reason);
        if (size == sizeof(Result) && address) {
            Result rc = *(const Result *)address;

            out(", result 0x%x (%04u-%04u)", rc, 2000 + R_MODULE(rc), R_DESCRIPTION(rc));
        }
        out("\n");
        dump_here();
    }
    return __real_svcBreak(reason, address, size);
}

/*
 * newlib's abort() quietly exits to hbmenu. Log where it came from, then
 * break so the app shows an error and Atmosphère writes a crash report.
 */
void __wrap_abort(void)
{
    Result rc = MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen);

    if (g_log_fd >= 0) {
        out("\n*** CRASH: abort()\n");
        dump_here();
    }
    __real_svcBreak(BreakReason_Panic, (uintptr_t)&rc, sizeof(rc));
    __real_abort();
}

/* QEMU often quits without a message: log the status and who called exit() */
void __wrap_exit(int status)
{
    if (g_log_fd >= 0) {
        out("\n*** exit(%d)\n", status);
        dump_here();
    }
    __real_exit(status);
}

void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
    char b1[32], b2[32];
    u64 tid = 0;

    if (g_log_fd >= 0) {
        svcGetThreadId(&tid, CUR_THREAD_HANDLE);
        out("\n*** CRASH: %s in thread %lu (error_desc 0x%x, esr 0x%08x, far 0x%016lx)\n",
            exception_name(ctx), tid, ctx->error_desc, ctx->esr, ctx->far.x);
        out("app code at 0x%016lx-0x%016lx; resolve elf+OFFSET with:\n"
            "  aarch64-none-elf-addr2line -fipC -e qemu-kitkat-arm.elf OFFSET...\n",
            g_text_start, g_text_end);
        out("pc %s  lr %s\n", addr_str(b1, sizeof(b1), ctx->pc.x), addr_str(b2, sizeof(b2), ctx->lr.x));
        out("sp 0x%016lx  fp 0x%016lx  pstate 0x%08x\n", ctx->sp.x, ctx->fp.x, ctx->pstate);
        for (int i = 0; i < 29; i += 3) {
            out("x%-2d 0x%016lx", i, ctx->cpu_gprs[i].x);
            for (int j = i + 1; j < i + 3 && j < 29; j++) {
                out("  x%-2d 0x%016lx", j, ctx->cpu_gprs[j].x);
            }
            out("\n");
        }
        describe_region("pc", ctx->pc.x);
        describe_region("lr", ctx->lr.x);
        describe_region("far", ctx->far.x);
        describe_region("sp", ctx->sp.x);
        backtrace_fp(ctx->fp.x);
        stack_scan(ctx->sp.x);
        out("*** end of crash dump\n");
    }

    /* Not handled: the kernel crashes the app as usual and Atmosphère writes its report */
    svcReturnFromException(MAKERESULT(Module_Kernel, KernelError_UnhandledUserInterrupt));
}
