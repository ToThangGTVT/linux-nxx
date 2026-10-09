#!/bin/bash
# Repack the SDK ramdisk with our changes into out/sdcard/switch/qemu-kitkat-arm/ramdisk.img
#   DEBUG_SHELL=1 adds a static busybox shell on the console (/sbin/busybox sh)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK=$ROOT/images/arm/sysimg/armeabi-v7a
R=$ROOT/build/arm-ramdisk
OUT=$ROOT/out/sdcard/switch/qemu-kitkat-arm

rm -rf "$R" && mkdir -p "$R"
(cd "$R" && gzip -dc "$SDK/ramdisk.img" | cpio -id --quiet)

if [ "${DEBUG_SHELL:-0}" = 1 ]; then
  BB=$ROOT/images/arm/tools/busybox-armv7l
  [ -f "$BB" ] || curl -fsSL -o "$BB" https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-armv7l
  install -m 0755 "$BB" "$R/sbin/busybox"
  cat >> "$R/init.ranchu.rc" <<'RC'

service debugsh /sbin/busybox sh
    class core
    console
    user root
    group root
RC
fi

(cd "$R" && find . | LC_ALL=C sort | cpio -o -H newc -R 0:0 --quiet | gzip -9 > "$OUT/ramdisk.img")
ls -la "$OUT/ramdisk.img"
