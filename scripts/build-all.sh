#!/bin/bash
# Full build: sources -> deps -> QEMU -> out/qemu-kitkat-arm.nro
#   QEMU_TARGETS=arm-softmmu,i386-softmmu also builds out/qemu-kitkat.nro (x86)
set -e
D=$(dirname "$0")
"$D/fetch-sources.sh"
"$D/build-deps.sh"
"$D/configure-qemu.sh" >/dev/null
. "$D/env.sh"
TARGETS=${QEMU_TARGETS:-arm-softmmu}
BINS=() NROS=()
for t in ${TARGETS//,/ }; do
  BINS+=("qemu-system-${t%-softmmu}")
  NROS+=("${t%-softmmu}")
done
ninja -C "$NXX_ROOT/build/qemu" "${BINS[@]}"
for n in "${NROS[@]}"; do "$D/package-nro.sh" "$n"; done
