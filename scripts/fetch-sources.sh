#!/bin/bash
# Download upstream sources into src/ and apply the Horizon (Switch) patches
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/src"
cd "$ROOT/src"

fetch() {
  local url=$1 dir=$2
  [ -d "$dir" ] && { echo "have $dir"; return; }
  echo "fetch $url"
  curl -fsSL "$url" | tar xf - --no-same-owner
}

fetch https://download.qemu.org/qemu-11.1.2.tar.xz qemu-11.1.2
fetch https://cairographics.org/releases/pixman-0.44.2.tar.gz pixman-0.44.2
fetch https://github.com/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz libffi-3.4.6
fetch https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.44/pcre2-10.44.tar.bz2 pcre2-10.44
fetch https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz libiconv-1.17
fetch https://download.gnome.org/sources/glib/2.82/glib-2.82.5.tar.xz glib-2.82.5

apply() {
  local dir=$1 patch=$2
  if (cd "$dir" && patch -p1 -R --dry-run -s < "$patch" >/dev/null 2>&1); then
    echo "already patched $dir"
  else
    (cd "$dir" && patch -p1 -s < "$patch") && echo "patched $dir"
  fi
}
apply glib-2.82.5 "$ROOT/patches/glib-2.82.5-horizon.patch"
apply qemu-11.1.2 "$ROOT/patches/qemu-11.1.2-horizon.patch"
