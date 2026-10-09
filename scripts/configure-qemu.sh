#!/bin/bash
# Configure QEMU (i386-softmmu) for Nintendo Switch into build/qemu
set -e
. "$(dirname "$0")/env.sh"
mkdir -p "$NXX_ROOT/build/qemu"
cd "$NXX_ROOT/build/qemu"
export PKG_CONFIG=$NXX_ROOT/scripts/nx-pkg-config
export SDL2_CONFIG=$DEVKITPRO/portlibs/switch/bin/sdl2-config
"$NXX_ROOT/src/qemu-11.1.2/configure" \
  --cross-prefix=aarch64-none-elf- --host-cc=clang \
  --target-list=i386-softmmu \
  --extra-cflags="$NX_ARCH $NX_CPPFLAGS" \
  --extra-ldflags="$NX_ARCH $NX_LDFLAGS $($PKG_CONFIG --libs nxcompat)" \
  --without-default-features --enable-tcg --enable-sdl --enable-pixman \
  --disable-werror "$@"
