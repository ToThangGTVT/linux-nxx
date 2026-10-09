/* nxcompat: Switch application frontend for QEMU (see include/nxcompat/frontend.h) */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <switch.h>
#include "nxcompat/crash.h"
#include "nxcompat/frontend.h"

#define NXFE_MAX_ARGS 128

#define I386_DIR "sdmc:/switch/qemu-kitkat"
#define ARM_DIR "sdmc:/switch/qemu-kitkat-arm"

/* Android-x86 4.4-r5 on the PC machine */
static const char *const g_i386_args[] = {
    "-L", "romfs:/pc-bios",
    "-M", "pc",
    "-cpu", "qemu32,+sse3,+ssse3",
    "-m", "1024",
    "-smp", "1",
    "-vga", "std",
    "-usb", "-device", "usb-tablet",
    "-drive", "file=" I386_DIR "/android.qcow2,if=ide,index=0,media=disk,cache=writeback",
    "-kernel", I386_DIR "/kernel",
    "-initrd", I386_DIR "/initrd.img",
    /*
     * nomodeset + vga=788: VESA 800x600 framebuffer. pci=nocrs: the KitKat
     * kernel misreads QEMU's ACPI _CRS and moves the VGA BAR away from the
     * address vesafb draws to, leaving a black screen.
     */
    "-append", "root=/dev/ram0 androidboot.hardware=android_x86 "
               "SRC=/android-4.4-r5 DATA= nomodeset vga=788 pci=nocrs quiet",
    "-rtc", "base=localtime",
    "-nic", "none",
    "-display", "sdl",
    NULL
};

/* Android SDK armeabi-v7a API 19 on vexpress-a15 (see arm/ in the repo) */
static const char *const g_arm_args[] = {
    "-M", "vexpress-a15",
    "-cpu", "cortex-a15",
    /* vexpress-a15 maxes out at 2 GiB without LPAE; Dalvik heap is 128m/512m */
    "-m", "2048",
    "-smp", "1",
    "-kernel", ARM_DIR "/zImage",
    "-dtb", ARM_DIR "/vexpress.dtb",
    "-initrd", ARM_DIR "/ramdisk.img",
    "-append", "console=ttyAMA0 androidboot.hardware=ranchu "
               "androidboot.console=ttyAMA0 qemu=1 qemu.gles=0",
    "-drive", "if=none,id=system,format=qcow2,file=" ARM_DIR "/system.qcow2",
    "-drive", "if=none,id=cache,format=qcow2,file=" ARM_DIR "/cache.qcow2",
    "-drive", "if=none,id=data,format=qcow2,file=" ARM_DIR "/userdata.qcow2",
    /* virtio-mmio transports fill from the last one: reverse order gives vda=system */
    "-device", "virtio-blk-device,drive=data",
    "-device", "virtio-blk-device,drive=cache",
    "-device", "virtio-blk-device,drive=system",
    /* user-mode networking (libslirp over the Switch's own connection) */
    "-nic", "user,model=lan9118",
    "-display", "sdl",
    NULL
};

typedef struct {
    const char *target;
    const char *dir;
    const char *const *args;
} NxfeProfile;

static const NxfeProfile g_profiles[] = {
    { "i386", I386_DIR, g_i386_args },
    { "arm", ARM_DIR, g_arm_args },
};

static char *g_argv[NXFE_MAX_ARGS + 1];
static bool g_boosted;
static bool g_sockets;
static bool g_started;

static void nxfe_exit(void)
{
    if (g_boosted) {
        appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    }
    romfsExit();
    socketExit();
    if (g_started) {
        /*
         * QEMU's threads are still alive and their stacks and the JIT buffer
         * still lock heap pages. Returning to hbloader would leave them
         * running inside the next homebrew (the menu then crashes with
         * 2168-0002), so end the whole process instead.
         */
        fflush(stdout);
        svcExitProcess();
    }
}

/* Block on a text console until the user presses + */
static void nxfe_fatal_screen(const char *msg)
{
    PadState pad;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    printf("QEMU KitKat\n\n%s\n\nPress + to exit.\n", msg);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
}

/* Send stdout/stderr to nxlink or <dir>/qemu.log; returns a descriptor for the crash logger */
static int nxfe_open_log(const char *dir)
{
    char path[256];
    FILE *f;

    snprintf(path, sizeof(path), "%s/qemu.log", dir);
    if (__nxlink_host.s_addr != 0 && g_sockets) {
        return nxlinkStdio();
    }
    /* Truncate once, then append from both streams so neither overwrites the other */
    f = fopen(path, "w");
    if (f) {
        fclose(f);
    }
    if (freopen(path, "a", stdout)) {
        setvbuf(stdout, NULL, _IOLBF, 0);
    }
    if (!freopen(path, "a", stderr)) {
        return -1;
    }
    setvbuf(stderr, NULL, _IONBF, 0);
    return fileno(stderr);
}

/* Split @p into whitespace separated tokens; "double quotes" group words */
static int nxfe_split_line(char *p, int argc)
{
    while (*p && argc < NXFE_MAX_ARGS) {
        char *out, *start;

        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = out = p;
        bool quoted = false;
        while (*p && (quoted || !isspace((unsigned char)*p))) {
            if (*p == '"') {
                quoted = !quoted;
                p++;
                continue;
            }
            *out++ = *p++;
        }
        if (*p) {
            p++;
        }
        *out = '\0';
        g_argv[argc++] = strdup(start);
    }
    return argc;
}

/* Read arguments from @path, one or more per line; '#' starts a comment line */
static int nxfe_load_args(const char *path, int argc)
{
    FILE *f = fopen(path, "r");
    char line[1024];

    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f) && argc < NXFE_MAX_ARGS) {
        char *p = line;

        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '#' || *p == '\0') {
            continue;
        }
        argc = nxfe_split_line(p, argc);
    }
    fclose(f);
    return argc;
}

int nxfe_init(int *argc, char ***argv, const char *target)
{
    AppletType type = appletGetAppletType();
    const NxfeProfile *prof = &g_profiles[0];
    char path[256];
    int n;

    for (size_t i = 0; i < sizeof(g_profiles) / sizeof(g_profiles[0]); i++) {
        if (strcmp(g_profiles[i].target, target) == 0) {
            prof = &g_profiles[i];
        }
    }

    mkdir(prof->dir, 0777);
    romfsInit();
    atexit(nxfe_exit);

    if (type != AppletType_Application && type != AppletType_SystemApplication) {
        nxfe_fatal_screen("Applet mode only has ~400 MB of memory, which is not\n"
                          "enough for Android. Launch hbmenu by holding R while\n"
                          "starting a game (title takeover), then run this app.");
        return -1;
    }

    /* BSD sockets back QEMU's user-mode networking (libslirp) and nxlink */
    g_sockets = R_SUCCEEDED(socketInitializeDefault());
    nxc_crash_init(nxfe_open_log(prof->dir));
    if (!g_sockets) {
        printf("socketInitializeDefault failed; guest networking will not work\n");
    }

    /* Overclock to 1785 MHz while QEMU runs; restored at exit */
    if (R_SUCCEEDED(appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad))) {
        g_boosted = true;
    }

    g_started = true;
    if (*argc > 1) {
        return 0; /* explicit arguments, e.g. from nxlink */
    }

    g_argv[0] = (*argc > 0) ? (*argv)[0] : (char *)"qemu";
    snprintf(path, sizeof(path), "%s/args.txt", prof->dir);
    n = nxfe_load_args(path, 1);
    if (n < 0) {
        n = 1;
        for (const char *const *a = prof->args; *a && n < NXFE_MAX_ARGS; a++) {
            g_argv[n++] = (char *)*a;
        }
    }
    g_argv[n] = NULL;

    printf("qemu-system-%s args:", target);
    for (int i = 1; i < n; i++) {
        printf(" %s", g_argv[i]);
    }
    printf("\n");

    *argc = n;
    *argv = g_argv;
    return 0;
}

int nxfe_text_input(char *out, size_t len)
{
    SwkbdConfig kbd;
    Result rc;

    rc = swkbdCreate(&kbd, 0);
    if (R_FAILED(rc)) {
        return -1;
    }
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetGuideText(&kbd, "Text to type into Android");
    rc = swkbdShow(&kbd, out, len);
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) ? 0 : -1;
}
