#!/bin/bash
# Generate meson cross file with absolute paths
set -e
. "$(dirname "$0")/env.sh"
mkdir -p "$NXX_ROOT/build"
arr() { local out=""; for a in "$@"; do out+="'$a', "; done; echo "[${out%, }]"; }
cat > "$NXX_ROOT/build/switch-cross.ini" <<INI
[binaries]
c = '$DEVKITA64/bin/aarch64-none-elf-gcc'
cpp = '$DEVKITA64/bin/aarch64-none-elf-g++'
ar = '$DEVKITA64/bin/aarch64-none-elf-gcc-ar'
ranlib = '$DEVKITA64/bin/aarch64-none-elf-gcc-ranlib'
strip = '$DEVKITA64/bin/aarch64-none-elf-strip'
pkg-config = '$(command -v pkg-config)'

[built-in options]
c_args = $(arr $NX_ARCH $NX_CPPFLAGS -O2 -ffunction-sections)
cpp_args = $(arr $NX_ARCH $NX_CPPFLAGS -O2 -ffunction-sections)
c_link_args = $(arr $NX_ARCH $NX_LDFLAGS)
cpp_link_args = $(arr $NX_ARCH $NX_LDFLAGS)
default_library = 'static'

[properties]
pkg_config_libdir = '$PKG_CONFIG_LIBDIR'
needs_exe_wrapper = true

[host_machine]
system = 'horizon'
cpu_family = 'aarch64'
cpu = 'cortex-a57'
endian = 'little'
INI
echo "wrote $NXX_ROOT/build/switch-cross.ini"
