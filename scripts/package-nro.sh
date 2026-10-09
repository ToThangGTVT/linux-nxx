#!/bin/bash
# Package the Switch QEMU builds as .nro apps:
#   out/qemu-kitkat.nro      qemu-system-i386 (romfs holds the x86 firmware)
#   out/qemu-kitkat-arm.nro  qemu-system-arm  (no firmware needed)
# Each .nro gets its unstripped ELF next to it (out/<name>.elf) for resolving
# crash addresses: aarch64-none-elf-addr2line -fipC -e out/<name>.elf <offset>
#   scripts/package-nro.sh [i386|arm|all]   (default: all)
set -e
. "$(dirname "$0")/env.sh"
BUILD=$NXX_ROOT/build
OUT=$NXX_ROOT/out
QEMU_SRC=$NXX_ROOT/src/qemu-11.1.2
WHICH=${1:-all}
mkdir -p "$OUT"

# package <qemu binary> <nro name> <title> [romfs dir]
package() {
  local elf=$1 name=$2 title=$3 romfs=$4
  local args=(--nacp="$BUILD/$name.nacp")
  [ -n "$romfs" ] && args+=(--romfsdir="$romfs")
  [ -f "$NXX_ROOT/assets/icon.jpg" ] && args+=(--icon="$NXX_ROOT/assets/icon.jpg")
  nacptool --create "$title" "linux-nxx" "0.2.2" "$BUILD/$name.nacp"
  elf2nro "$elf" "$OUT/$name.nro" "${args[@]}" >/dev/null
  cp "$elf" "$OUT/$name.elf"
  ls -la "$OUT/$name.nro" "$OUT/$name.elf"
}

if [ "$WHICH" = all ] || [ "$WHICH" = i386 ]; then
  ROMFS=$BUILD/romfs-i386
  mkdir -p "$ROMFS/pc-bios"
  for f in bios-256k.bin vgabios-stdvga.bin kvmvapic.bin linuxboot_dma.bin multiboot_dma.bin; do
    cp "$QEMU_SRC/pc-bios/$f" "$ROMFS/pc-bios/"
  done
  package "$BUILD/qemu/qemu-system-i386" qemu-kitkat "QEMU KitKat" "$ROMFS"
fi

if [ "$WHICH" = all ] || [ "$WHICH" = arm ]; then
  package "$BUILD/qemu/qemu-system-arm" qemu-kitkat-arm "QEMU KitKat ARM"
fi
