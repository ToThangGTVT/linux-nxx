#!/bin/bash
# Build the ARM KitKat disk set from the Android SDK armeabi-v7a API 19 image:
#   out/sdcard/switch/qemu-kitkat-arm/{zImage,vexpress.dtb,ramdisk.img,system,cache,userdata .qcow2}
# Disk order matches fstab.ranchu: vda=system vdb=cache vdc=data
# The kernel comes from build/arm (arm/build-kernel.sh) or, if not built
# locally, from the latest GitHub release.
# Needs: curl, unzip, gzip, cpio, qemu-img, e2fsprogs >= 1.43 (mke2fs -d)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RELEASE_URL=${RELEASE_URL:-https://github.com/ToThangGTVT/linux-nxx/releases/latest/download}
# e2fsprogs: Homebrew keeps it keg-only on macOS; elsewhere it is on PATH
if command -v brew >/dev/null && [ -d "$(brew --prefix e2fsprogs 2>/dev/null)/sbin" ]; then
  E2=$(brew --prefix e2fsprogs)/sbin
else
  E2=$(dirname "$(command -v mke2fs || echo /sbin/mke2fs)")
fi
for tool in curl unzip gzip cpio qemu-img "$E2/mke2fs" "$E2/e2fsck" "$E2/resize2fs"; do
  command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
done
SDK=$ROOT/images/arm/sysimg/armeabi-v7a
OUT=$ROOT/out/sdcard/switch/qemu-kitkat-arm
DATA_SIZE=${DATA_SIZE:-2G}

if [ ! -f "$SDK/system.img" ]; then
  mkdir -p "$ROOT/images/arm"
  curl -fsSL -o "$ROOT/images/arm/armeabi-v7a-19_r05.zip" \
    https://dl.google.com/android/repository/sys-img/android/armeabi-v7a-19_r05.zip
  unzip -o -q "$ROOT/images/arm/armeabi-v7a-19_r05.zip" -d "$ROOT/images/arm/sysimg"
fi
mkdir -p "$OUT"
if [ -f "$ROOT/build/arm/zImage" ]; then
  cp "$ROOT/build/arm/zImage" "$OUT/zImage"
  cp "$ROOT/build/arm/vexpress-v2p-ca15-tc1.dtb" "$OUT/vexpress.dtb"
else
  echo "kernel not built locally; downloading from $RELEASE_URL"
  curl -fsSL -o "$OUT/zImage" "$RELEASE_URL/zImage"
  curl -fsSL -o "$OUT/vexpress.dtb" "$RELEASE_URL/vexpress.dtb"
fi
"$ROOT/arm/patch-ramdisk.sh" >/dev/null

W=$ROOT/build/arm-disks
mkdir -p "$W"
cp "$SDK/userdata.img" "$W/userdata.img"
"$E2/e2fsck" -fy "$W/userdata.img" >/dev/null || true
"$E2/resize2fs" -f "$W/userdata.img" "$DATA_SIZE" >/dev/null
# The 3.18 kernel predates metadata_csum_seed and orphan_file
rm -f "$W/cache.img"
"$E2/mke2fs" -q -F -t ext4 -O ^metadata_csum_seed,^orphan_file -L cache "$W/cache.img" 256M

# qcow2 keeps the SD card footprint to what is actually used
qemu-img convert -O qcow2 "$SDK/system.img" "$OUT/system.qcow2"
qemu-img convert -O qcow2 "$W/cache.img" "$OUT/cache.qcow2"
qemu-img convert -O qcow2 "$W/userdata.img" "$OUT/userdata.qcow2"
rm -f "$W/cache.img" "$W/userdata.img"
ls -la "$OUT"
echo "Tip: boot once with arm/run-mac.sh so the first-boot dexopt is not done on the Switch."
