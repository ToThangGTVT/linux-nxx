#!/bin/bash
# Full build: sources -> deps -> QEMU -> out/qemu-kitkat.nro
set -e
D=$(dirname "$0")
"$D/fetch-sources.sh"
"$D/build-deps.sh"
"$D/configure-qemu.sh" >/dev/null
. "$D/env.sh"
ninja -C "$NXX_ROOT/build/qemu" qemu-system-i386
"$D/package-nro.sh"
