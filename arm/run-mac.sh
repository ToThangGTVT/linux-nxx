#!/bin/bash
# Preview ARM KitKat (SDK armeabi-v7a image + our 3.18 kernel) on QEMU vexpress-a15, TCG only.
#   arm/run-mac.sh            cocoa window
#   arm/run-mac.sh --headless no window
# virtio-mmio transports are filled from the last one, so devices are listed in
# reverse: the guest then sees vda=system vdb=cache vdc=data (fstab.ranchu).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=$ROOT/out/sdcard/switch/qemu-kitkat-arm
W=$ROOT/build/armtest
mkdir -p "$W"
DISPLAY_ARGS=(-display cocoa,zoom-to-fit=on -name "Android KitKat ARM (Switch preview)")
[ "$1" = "--headless" ] && DISPLAY_ARGS=(-display none)
exec qemu-system-arm -M vexpress-a15 -cpu cortex-a15 -m 1024 -smp 1 \
  -kernel "$D/zImage" -dtb "$D/vexpress.dtb" -initrd "$D/ramdisk.img" \
  -append "console=ttyAMA0 androidboot.hardware=ranchu androidboot.console=ttyAMA0 qemu=1 qemu.gles=0" \
  -drive if=none,id=system,format=raw,file="$D/system.img" \
  -drive if=none,id=cache,format=raw,file="$D/cache.img" \
  -drive if=none,id=data,format=raw,file="$D/userdata.img" \
  -device virtio-blk-device,drive=data -device virtio-blk-device,drive=cache -device virtio-blk-device,drive=system \
  "${DISPLAY_ARGS[@]}" \
  -chardev socket,id=ser0,path="$W/serial.sock",server=on,wait=off,logfile="$W/serial.log" -serial chardev:ser0 \
  -monitor unix:"$W/mon.sock",server,nowait
