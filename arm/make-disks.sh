#!/bin/bash
# Build the ARM KitKat disk set from the Android SDK armeabi-v7a API 19 image:
#   out/sdcard/switch/qemu-kitkat-arm/{zImage,vexpress.dtb,ramdisk.img,system,cache,userdata .qcow2}
# Disk order matches fstab.ranchu: vda=system vdb=cache vdc=data
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
E2=$(brew --prefix e2fsprogs)/sbin
SDK=$ROOT/images/arm/sysimg/armeabi-v7a
OUT=$ROOT/out/sdcard/switch/qemu-kitkat-arm
DATA_SIZE=${DATA_SIZE:-2G}

if [ ! -f "$SDK/system.img" ]; then
  mkdir -p "$ROOT/images/arm"
  curl -fsSL -o "$ROOT/images/arm/armeabi-v7a-19_r05.zip" \
    https://dl.google.com/android/repository/sys-img/android/armeabi-v7a-19_r05.zip
  unzip -o -q "$ROOT/images/arm/armeabi-v7a-19_r05.zip" -d "$ROOT/images/arm/sysimg"
fi
[ -f "$ROOT/build/arm/zImage" ] || "$ROOT/arm/build-kernel.sh"

mkdir -p "$OUT"
cp "$ROOT/build/arm/zImage" "$OUT/zImage"
cp "$ROOT/build/arm/vexpress-v2p-ca15-tc1.dtb" "$OUT/vexpress.dtb"
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
