#!/bin/bash
# Package build/qemu/qemu-system-i386 as out/qemu-kitkat.nro (romfs holds the x86 firmware)
set -e
. "$(dirname "$0")/env.sh"
BUILD=$NXX_ROOT/build
OUT=$NXX_ROOT/out
ROMFS=$BUILD/romfs
QEMU_SRC=$NXX_ROOT/src/qemu-11.1.2

mkdir -p "$OUT" "$ROMFS/pc-bios"
for f in bios-256k.bin vgabios-stdvga.bin kvmvapic.bin linuxboot_dma.bin multiboot_dma.bin; do
  cp "$QEMU_SRC/pc-bios/$f" "$ROMFS/pc-bios/"
done

nacptool --create "QEMU KitKat" "linux-nxx" "0.1.0" "$BUILD/qemu-kitkat.nacp"
ICON_ARGS=()
[ -f "$NXX_ROOT/assets/icon.jpg" ] && ICON_ARGS=(--icon="$NXX_ROOT/assets/icon.jpg")
elf2nro "$BUILD/qemu/qemu-system-i386" "$OUT/qemu-kitkat.nro" \
  --nacp="$BUILD/qemu-kitkat.nacp" --romfsdir="$ROMFS" "${ICON_ARGS[@]}"
ls -la "$OUT/qemu-kitkat.nro"
