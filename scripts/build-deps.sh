#!/bin/bash
# Cross-compile QEMU's dependencies for Switch into build/sysroot:
# pixman, libffi, pcre2, libiconv, glib (patched), then libnxcompat.
set -e
. "$(dirname "$0")/env.sh"
SRC=$NXX_ROOT/src
B=$NXX_ROOT/build
"$NXX_ROOT/scripts/gen-cross.sh" >/dev/null
CROSS=$B/switch-cross.ini
AUTOTOOLS=(--host=aarch64-none-elf --prefix="$NXX_SYSROOT" --disable-shared --enable-static)

step() { echo "==> $*"; }

step pixman
meson setup --reconfigure "$B/pixman" "$SRC/pixman-0.44.2" --cross-file "$CROSS" --prefix="$NXX_SYSROOT" \
  -Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled -Dopenmp=disabled -Da64-neon=enabled >/dev/null
ninja -C "$B/pixman" install >/dev/null

step libffi
mkdir -p "$B/libffi" && cd "$B/libffi"
"$SRC/libffi-3.4.6/configure" "${AUTOTOOLS[@]}" --disable-docs --disable-multi-os-directory \
  CFLAGS="$NX_ARCH -O2" CPPFLAGS="$NX_CPPFLAGS" LDFLAGS="$NX_LDFLAGS" LIBS="-lnx" >/dev/null
make -j8 >/dev/null && make install >/dev/null

step pcre2
mkdir -p "$B/pcre2" && cd "$B/pcre2"
"$SRC/pcre2-10.44/configure" "${AUTOTOOLS[@]}" --enable-pcre2-8 --disable-pcre2-16 --disable-pcre2-32 --disable-jit \
  --disable-pcre2grep-libz --disable-pcre2grep-libbz2 --disable-pcre2test-libreadline \
  CFLAGS="$NX_ARCH -O2" CPPFLAGS="$NX_CPPFLAGS" LDFLAGS="$NX_LDFLAGS" LIBS="-lnx" >/dev/null
make -j8 libpcre2-8.la >/dev/null
make install-libLTLIBRARIES install-includeHEADERS install-nodist_includeHEADERS install-pkgconfigDATA >/dev/null

step libiconv
# GCC 16 defaults to C23, which rejects libiconv's K&R declarations
mkdir -p "$B/libiconv" && cd "$B/libiconv"
"$SRC/libiconv-1.17/configure" "${AUTOTOOLS[@]}" --disable-nls \
  CFLAGS="$NX_ARCH -O2 -std=gnu17" CPPFLAGS="$NX_CPPFLAGS" LDFLAGS="$NX_LDFLAGS" LIBS="-lnx" >/dev/null
make lib/localcharset.h >/dev/null
make -j8 -C libcharset >/dev/null && make -j8 -C lib >/dev/null
make -C lib install >/dev/null
cp include/iconv.h.inst "$NXX_SYSROOT/include/iconv.h"

step glib
meson setup --reconfigure "$B/glib" "$SRC/glib-2.82.5" --cross-file "$CROSS" --prefix="$NXX_SYSROOT" \
  -Dtests=false -Dintrospection=disabled -Dnls=disabled -Dselinux=disabled -Dxattr=false -Dlibmount=disabled \
  -Dman-pages=disabled -Ddocumentation=false -Dsysprof=disabled -Dlibelf=disabled -Dglib_debug=disabled \
  -Dglib_assert=false -Dglib_checks=false -Dinstalled_tests=false -Doss_fuzz=disabled >/dev/null
ninja -C "$B/glib" install >/dev/null

step nxcompat
make -C "$NXX_ROOT/nxcompat" install >/dev/null
echo "deps installed into $NXX_SYSROOT"
