#!/bin/bash
# Repack the SDK ramdisk with our changes into out/sdcard/switch/qemu-kitkat-arm/ramdisk.img
#   DEBUG_BUSYBOX=1 adds a static busybox at /sbin/busybox (nslookup, wget, ...)
#   DEBUG_SHELL=1   also runs it as a root shell on the console
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK=$ROOT/images/arm/sysimg/armeabi-v7a
R=$ROOT/build/arm-ramdisk
OUT=$ROOT/out/sdcard/switch/qemu-kitkat-arm

rm -rf "$R" && mkdir -p "$R"
(cd "$R" && gzip -dc "$SDK/ramdisk.img" | cpio -id --quiet)

# Dalvik heap limits normally come from the emulator's qemu-props (via qemud),
# which upstream QEMU cannot provide; without them apps get 16 MB and the
# launcher dies with OOM when opening the app drawer.
cat >> "$R/default.prop" <<'PROP'
dalvik.vm.heapstartsize=8m
dalvik.vm.heapgrowthlimit=128m
dalvik.vm.heapsize=512m
PROP

# Networking: init.goldfish.sh brings eth0 up as 10.0.2.15 (QEMU user-mode
# networking). On the real emulator the RIL's mobile data connection then
# hands DNS to netd; there is no RIL here, so tell netd directly.
cat > "$R/init.ranchu.net.sh" <<'SH'
#!/system/bin/sh
# Wait for netd, then make eth0 the default DNS interface (QEMU slirp DNS at 10.0.2.3)
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    ndc resolver setifdns eth0 "" 10.0.2.3 && ndc resolver setdefaultif eth0 && exit 0
    sleep 3
done
SH
chmod 0750 "$R/init.ranchu.net.sh"
cat >> "$R/init.ranchu.rc" <<'RC'

service ranchu-net /system/bin/sh /init.ranchu.net.sh
    class main
    user root
    group root
    oneshot
RC

if [ "${DEBUG_BUSYBOX:-0}" = 1 ] || [ "${DEBUG_SHELL:-0}" = 1 ]; then
  BB=$ROOT/images/arm/tools/busybox-armv7l
  mkdir -p "$(dirname "$BB")"
  [ -f "$BB" ] || curl -fsSL -o "$BB" https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-armv7l
  install -m 0755 "$BB" "$R/sbin/busybox"
fi
if [ "${DEBUG_SHELL:-0}" = 1 ]; then
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
