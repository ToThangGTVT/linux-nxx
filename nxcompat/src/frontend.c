/* nxcompat: Switch application frontend for QEMU (see include/nxcompat/frontend.h) */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <switch.h>
#include "nxcompat/frontend.h"

#define NXFE_MAX_ARGS 128

static const char *const g_default_args[] = {
    "-L", "romfs:/pc-bios",
    "-M", "pc",
    "-cpu", "qemu32,+sse3,+ssse3",
    "-m", "1024",
    "-smp", "1",
    "-vga", "std",
    "-usb", "-device", "usb-tablet",
    "-drive", "file=" NXFE_DIR "/android.qcow2,if=ide,index=0,media=disk,cache=writeback",
    "-kernel", NXFE_DIR "/kernel",
    "-initrd", NXFE_DIR "/initrd.img",
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
};

static char *g_argv[NXFE_MAX_ARGS + 1];
static bool g_boosted;

static void nxfe_exit(void)
{
    if (g_boosted) {
        appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    }
    romfsExit();
    socketExit();
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

static void nxfe_open_log(void)
{
    if (__nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault())) {
        nxlinkStdio();
        return;
    }
    if (freopen(NXFE_DIR "/qemu.log", "w", stdout)) {
        setvbuf(stdout, NULL, _IOLBF, 0);
    }
    if (freopen(NXFE_DIR "/qemu.log", "a", stderr)) {
        setvbuf(stderr, NULL, _IONBF, 0);
    }
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

int nxfe_init(int *argc, char ***argv)
{
    AppletType type = appletGetAppletType();
    int n;

    mkdir(NXFE_DIR, 0777);
    romfsInit();
    atexit(nxfe_exit);

    if (type != AppletType_Application && type != AppletType_SystemApplication) {
        nxfe_fatal_screen("Applet mode only has ~400 MB of memory, which is not\n"
                          "enough for Android. Launch hbmenu by holding R while\n"
                          "starting a game (title takeover), then run this app.");
        return -1;
    }

    nxfe_open_log();

    /* Overclock to 1785 MHz while QEMU runs; restored at exit */
    if (R_SUCCEEDED(appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad))) {
        g_boosted = true;
    }

    if (*argc > 1) {
        return 0; /* explicit arguments, e.g. from nxlink */
    }

    g_argv[0] = (*argc > 0) ? (*argv)[0] : (char *)"qemu-system-i386";
    n = nxfe_load_args(NXFE_DIR "/args.txt", 1);
    if (n < 0) {
        n = 1;
        for (size_t i = 0; i < sizeof(g_default_args) / sizeof(g_default_args[0]); i++) {
            g_argv[n++] = (char *)g_default_args[i];
        }
    }
    g_argv[n] = NULL;

    printf("qemu args:");
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
