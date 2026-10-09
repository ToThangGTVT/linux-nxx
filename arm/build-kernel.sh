#!/bin/bash
# Build the Linux 3.18 kernel + DTB for Android KitKat on QEMU vexpress-a15 (in Docker)
#   output: build/arm/zImage, build/arm/vexpress-v2p-ca15-tc1.dtb
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
KVER=3.18.140
KSRC=$ROOT/build/kernel/linux-$KVER
OUT=$ROOT/build/arm
mkdir -p "$ROOT/build/kernel" "$OUT"
if [ ! -d "$KSRC" ]; then
  curl -fsSL "https://cdn.kernel.org/pub/linux/kernel/v3.x/linux-$KVER.tar.xz" | tar xJf - -C "$ROOT/build/kernel"
  for p in "$ROOT"/arm/kernel-patches/*.patch; do patch -d "$KSRC" -p1 -s < "$p"; done
fi

# Debian stretch ships gcc 6, old enough for a 3.18 kernel
docker build -q -t nxx-kbuild -f "$ROOT/arm/Dockerfile.kbuild" "$ROOT/arm" >/dev/null
docker run --rm -v "$ROOT:/w" -w /w/build/kernel/linux-$KVER nxx-kbuild bash -euc '
  export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
  make -s vexpress_defconfig
  scripts/kconfig/merge_config.sh -m .config /w/arm/android-vexpress.config >/dev/null
  make -s olddefconfig
  make -s -j$(nproc) zImage dtbs
  cp arch/arm/boot/zImage arch/arm/boot/dts/vexpress-v2p-ca15-tc1.dtb /w/build/arm/
'
for opt in LBDAF ANDROID_BINDER_IPC ASHMEM ANDROID_LOGGER ANDROID_INTF_ALARM_DEV PM_WAKELOCKS VIRTIO_MMIO VIRTIO_BLK FB_ARMCLCD SERIO_AMBAKMI HIGHMEM; do
  grep -q "^CONFIG_$opt=y" "$KSRC/.config" || echo "WARNING: CONFIG_$opt not enabled"
done
ls -la "$OUT"
