/* nxcompat: JIT code memory (see include/nxcompat/jit.h) */
#include <switch.h>
#include "nxcompat/jit.h"

/* QEMU allocates one code buffer for the process lifetime */
static Jit g_jit;

unsigned int nxc_jit_alloc(size_t size, void **rw, void **rx)
{
    Result rc;

    size = (size + 0xfff) & ~(size_t)0xfff;
    rc = jitCreate(&g_jit, size);
    if (R_FAILED(rc)) {
        return rc;
    }
    /*
     * Only CodeMemory gives simultaneous RW and RX aliases; the legacy
     * SetProcessMemoryPermission type toggles one mapping and cannot work
     * with TCG's split-wx model.
     */
    if (g_jit.type != JitType_CodeMemory) {
        jitClose(&g_jit);
        return MAKERESULT(Module_Libnx, LibnxError_JitUnavailable);
    }
    rc = jitTransitionToExecutable(&g_jit);
    if (R_FAILED(rc)) {
        jitClose(&g_jit);
        return rc;
    }
    *rw = jitGetRwAddr(&g_jit);
    *rx = jitGetRxAddr(&g_jit);
    return 0;
}
