# Source this file: shared cross-compile environment for Nintendo Switch (Horizon / libnx)
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITA64=$DEVKITPRO/devkitA64
export NXX_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)
export NXX_SYSROOT=$NXX_ROOT/build/sysroot
export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:$PATH

export NX_ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
export NX_CPPFLAGS="-D__SWITCH__ -I$NXX_SYSROOT/include -I$DEVKITPRO/portlibs/switch/include -I$DEVKITPRO/libnx/include"
export NX_LDFLAGS="-specs=$DEVKITPRO/libnx/switch.specs -L$NXX_SYSROOT/lib -L$DEVKITPRO/portlibs/switch/lib -L$DEVKITPRO/libnx/lib"

export PKG_CONFIG_DIR=
export PKG_CONFIG_SYSROOT_DIR=
export PKG_CONFIG_LIBDIR=$NXX_SYSROOT/lib/pkgconfig:$DEVKITPRO/portlibs/switch/lib/pkgconfig
