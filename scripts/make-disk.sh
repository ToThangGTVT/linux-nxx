#!/bin/bash
# Build out/sdcard/switch/qemu-kitkat/{android.qcow2,kernel,initrd.img} from an Android-x86 ISO.
# The disk is a bare ext4 filesystem (no partition table, no bootloader):
# QEMU boots the kernel directly and Android-x86's initrd finds SRC on the disk.
# Newer ext4 features are disabled: the KitKat kernel (4.0.9) cannot mount them.
#   usage: scripts/make-disk.sh images/android-x86-4.4-r5.iso [size, default 4G]
set -e
ISO=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
SIZE=${2:-4G}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
E2=$(brew --prefix e2fsprogs)/sbin
SRC_DIR=android-4.4-r5
WORK=$ROOT/build/disk
OUT=$ROOT/out/sdcard/switch/qemu-kitkat

rm -rf "$WORK" && mkdir -p "$WORK/iso" "$WORK/root/$SRC_DIR/data" "$OUT"
tar -xf "$ISO" -C "$WORK/iso" kernel initrd.img ramdisk.img system.sfs isolinux/isolinux.cfg
cp "$WORK/iso/ramdisk.img" "$WORK/iso/system.sfs" "$WORK/root/$SRC_DIR/"

"$E2/mke2fs" -q -F -t ext4 -O ^metadata_csum_seed,^orphan_file,^metadata_csum -L android -E root_owner=0:0 -d "$WORK/root" "$WORK/android.img" "$SIZE"
# mke2fs -d copies the host uid/gid; Android expects root-owned files
for p in "/$SRC_DIR" "/$SRC_DIR/data" "/$SRC_DIR/ramdisk.img" "/$SRC_DIR/system.sfs"; do
  "$E2/debugfs" -w -R "set_inode_field $p uid 0" "$WORK/android.img" >/dev/null 2>&1
  "$E2/debugfs" -w -R "set_inode_field $p gid 0" "$WORK/android.img" >/dev/null 2>&1
done
"$E2/e2fsck" -fy "$WORK/android.img" >/dev/null || true

qemu-img convert -O qcow2 "$WORK/android.img" "$OUT/android.qcow2"
rm "$WORK/android.img"
cp "$WORK/iso/kernel" "$WORK/iso/initrd.img" "$OUT/"
echo "--- isolinux.cfg boot entries:"
grep -iE "append|kernel" "$WORK/iso/isolinux/isolinux.cfg" | head
ls -la "$OUT"
