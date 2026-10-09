#!/bin/bash
# Preview the Switch configuration on macOS with Homebrew QEMU (TCG only, same args as the .nro).
# Uses an overlay so out/sdcard/.../android.qcow2 stays pristine.
#   scripts/run-mac.sh            # cocoa window
#   scripts/run-mac.sh --reset    # discard the overlay and start fresh
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
K=$ROOT/out/sdcard/switch/qemu-kitkat
W=$ROOT/build/mactest
mkdir -p "$W"
[ "$1" = "--reset" ] && rm -f "$W/test.qcow2"
[ -f "$W/test.qcow2" ] || qemu-img create -q -f qcow2 -b "$K/android.qcow2" -F qcow2 "$W/test.qcow2"
exec qemu-system-i386 -accel tcg -M pc -cpu qemu32,+sse3,+ssse3 -m 1024 -smp 1 \
  -vga std -usb -device usb-tablet \
  -drive file="$W/test.qcow2",if=ide,index=0,media=disk,cache=writeback \
  -kernel "$K/kernel" -initrd "$K/initrd.img" \
  -append "root=/dev/ram0 androidboot.hardware=android_x86 SRC=/android-4.4-r5 DATA= nomodeset vga=788 pci=nocrs console=ttyS0 quiet" \
  -rtc base=localtime -nic none \
  -display cocoa,zoom-to-fit=on -name "Android KitKat (Switch preview)" \
  -monitor unix:"$W/mon.sock",server,nowait \
  -serial unix:"$W/serial.sock",server,nowait
