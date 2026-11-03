#!/bin/bash
# R05B1: builds the lean profile's guest kernel, Koala's own, the way
# Kata Containers builds its kernel: Linux 6.18.60 from pinned source,
# allnoconfig, then Kata 3.17.0's fragments (copied into kernel/kata-3.17.0,
# a selection as Kata's build-kernel.sh makes it, less DAX/pmem, memory
# hotplug and debug) and Koala's kernel-lean-x86_64.fragment, merged with the
# kernel's merge_config.sh. On x86_64 the result is an uncompressed ELF that
# Cloud Hypervisor boots through PVH; on arm64 (the Mac, #322) the kernel's
# Image, which Virtualization.framework boots. One recipe for both: the
# architecture picks Kata's directory, Koala's fragment and the output's name.
# The full profile's kernel is build-kernel.sh.
# Prints one PASS/FAIL line per check and a RESULT line.
#
#   build-lean-kernel.sh [x86_64|arm64]   (x86_64 by default; built natively)
set -euo pipefail

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
KARCH="${1:-x86_64}"
case "$KARCH" in
  x86_64) WORK="${L02_WORK:-$HERE/.work}/kernel-lean"; TARGET=vmlinux; BUILT=vmlinux; OUTPUT=vmlinux-lean-x86_64 ;;
  arm64) WORK="${L02_WORK:-$HERE/.work}/kernel-lean-arm64"; TARGET=Image; BUILT=arch/arm64/boot/Image; OUTPUT=Image-lean-arm64 ;;
  *) echo "FAIL the architecture is x86_64 or arm64, not $KARCH"; echo "RESULT 0 passed, 1 failed"; exit 1 ;;
esac
BUILDER="ubuntu@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac"  # 24.04, as build-kernel.sh

# The newest long-term release on 2026-11-03; its digest is in kernel.org's
# signed sha256sums.asc (Kernel.org checksum autosigner, B886 8C80 … A6B1).
SOURCE_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.60.tar.xz"
SOURCE_SHA="3c2f44ef6acc021a7ad444d9a2e6ec1ef488ff09368793d0004a6080f69d621e"
# Per architecture, the SHA-256 of `sha256sum common/*.conf ARCH/*.conf` in
# kernel/kata-3.17.0 (the Mac adds arm64's with Kata's arm64 fragments).
FRAGMENTS_SHA_x86_64="edf3b563d07738afe4feebe1ad03fe73f6faf8c829aa671098e0f9cd7d159660"
FRAGMENTS_SHA_arm64="b0ad0b4229e163f9ff53948ba9f726a291adb0944ebad4bb432b5002474c9f8e"  # MAC08

pass=0 fail=0
check() { if [ "$2" = ok ]; then echo "PASS $1"; pass=$((pass + 1)); else echo "FAIL $1${3:+: $3}"; fail=$((fail + 1)); fi; }
digest() { sha256sum "$1" | awk '{print $1}'; }
finish() { echo "RESULT $pass passed, $fail failed"; [ "$fail" -eq 0 ]; }

mkdir -p "$WORK"
if [ ! -f "$WORK/source.tar.xz" ]; then curl -fsSL "$SOURCE_URL" -o "$WORK/source.tar.xz.partial" && mv "$WORK/source.tar.xz.partial" "$WORK/source.tar.xz"; fi
[ "$(digest "$WORK/source.tar.xz")" = "$SOURCE_SHA" ] && check "kernel source sha256 (6.18.60)" ok \
  || { check "kernel source sha256 (6.18.60)" no "$(digest "$WORK/source.tar.xz")"; finish; }
PINNED="FRAGMENTS_SHA_$KARCH"
FRAGS="$(cd "$HERE/kernel/kata-3.17.0" && sha256sum common/*.conf "$KARCH"/*.conf | sha256sum | cut -d' ' -f1)"
[ -n "${!PINNED}" ] && [ "$FRAGS" = "${!PINNED}" ] && check "Kata 3.17.0's $KARCH fragments are the pinned ones" ok \
  || { check "Kata 3.17.0's $KARCH fragments are the pinned ones" no "$FRAGS"; finish; }
[ -f "$HERE/kernel-lean-$KARCH.fragment" ] && check "Koala's kernel-lean-$KARCH.fragment exists" ok \
  || { check "Koala's kernel-lean-$KARCH.fragment exists" no; finish; }
rm -f "$WORK/$OUTPUT" "$WORK/config.built" "$WORK/missing.txt"

# Reproducible: fixed timestamp, user and host; the architecture explicit
# (allnoconfig on an x86_64 host is otherwise 32-bit).
docker run --rm -e KARCH="$KARCH" -e TARGET="$TARGET" -e BUILT="$BUILT" -e OUTPUT="$OUTPUT" \
  -v "$WORK":/work -v "$HERE/kernel/kata-3.17.0":/kata:ro -v "$HERE":/l02:ro "$BUILDER" bash -c '
  set -e
  apt-get update -qq >/dev/null
  apt-get install -y -qq build-essential flex bison bc libelf-dev libssl-dev xz-utils >/dev/null
  export KBUILD_BUILD_TIMESTAMP="2026-01-01 00:00:00 UTC" KBUILD_BUILD_USER=koala KBUILD_BUILD_HOST=koala LOCALVERSION="" ARCH="$KARCH"
  mkdir -p /k && tar -xf /work/source.tar.xz -C /k --strip-components=1 && cd /k
  # As Kata: the common fragments not tagged !ARCH and the architecture'"'"'s;
  # less DAX/pmem, debug and hotplug (whose PCI and CPU parts are in our
  # fragment, without memory hotplug).
  COMMON=$(grep -L -e "!$KARCH" /kata/common/*.conf | grep -v -e /dax.conf -e /pmem.conf -e /hotplug.conf -e /debug.conf)
  ARCH_FRAGS=$(ls /kata/$KARCH/*.conf | grep -v -e /hotplug.conf)
  FRAGS="$COMMON $ARCH_FRAGS /l02/kernel-lean-$KARCH.fragment"
  scripts/kconfig/merge_config.sh -n $FRAGS > /work/merge.log 2>&1
  for f in $FRAGS; do
    grep -hE "^CONFIG_[A-Z0-9_]+=[ym]" "$f" | while IFS== read -r k v; do
      grep -qx "$k=$v" .config || grep -qx "$k=y" .config || echo "$k=$v ($(basename $f))"
    done
  done | sort -u > /work/missing.txt
  make -j"$(nproc)" "$TARGET" > /work/build.log 2>&1
  cp "$BUILT" "/work/$OUTPUT" && cp .config /work/config.built
  chown '"$(id -u):$(id -g)"' "/work/$OUTPUT" /work/config.built /work/missing.txt /work/merge.log /work/build.log
' && check "lean kernel build" ok || { check "lean kernel build" no "see $WORK/build.log"; finish; }

if [ "$KARCH" = x86_64 ]; then
  file "$WORK/$OUTPUT" | grep -q "ELF 64-bit.*x86-64" && check "a 64-bit x86-64 ELF" ok \
    || check "a 64-bit x86-64 ELF" no "$(file -b "$WORK/$OUTPUT" | cut -c1-60)"
  ARCH_OPTIONS="PVH HOTPLUG_PCI HOTPLUG_PCI_PCIE PCIEPORTBUS CPU_SUP_AMD X86_X2APIC KVM_GUEST RANDOMIZE_MEMORY"
else
  file "$WORK/$OUTPUT" | grep -q "ARM64 boot executable Image" && check "an arm64 kernel Image" ok \
    || check "an arm64 kernel Image" no "$(file -b "$WORK/$OUTPUT" | cut -c1-60)"
  ARCH_OPTIONS=""
fi
for option in $ARCH_OPTIONS VIRTIO_BLK VIRTIO_NET VIRTIO_CONSOLE VIRTIO_VSOCKETS \
              VIRTIO_FS VIRTIO_BALLOON EXT4_FS OVERLAY_FS CGROUPS NAMESPACES SECCOMP DEVTMPFS_MOUNT \
              RANDOMIZE_BASE STACKPROTECTOR_STRONG FORTIFY_SOURCE HARDENED_USERCOPY \
              STRICT_KERNEL_RWX VMAP_STACK SLAB_FREELIST_HARDENED BUG_ON_DATA_CORRUPTION SECCOMP_FILTER; do
  grep -qx "CONFIG_$option=y" "$WORK/config.built" && check "CONFIG_$option=y" ok || check "CONFIG_$option=y" no
done
grep -q "^CONFIG_MEMORY_HOTPLUG=y" "$WORK/config.built" && check "no memory hotplug" no || check "no memory hotplug" ok
# Every option the fragment sets =y is on in the build, on both
# architectures (MAC08's rule, adopted for x86_64 in R05B4A): a renamed or
# unmet option would otherwise drop out silently, as Kata's 6.12 mitigation
# names did.
off=$(grep -E "^CONFIG_[A-Z0-9_]+=y" "$HERE/kernel-lean-$KARCH.fragment" | while read -r o; do
  grep -qx "$o" "$WORK/config.built" || echo "${o%=y}"; done)
[ -z "$off" ] && check "every option the $KARCH fragment sets is on ($(grep -cE "^CONFIG_[A-Z0-9_]+=y" "$HERE/kernel-lean-$KARCH.fragment"); mitigations $(grep -cE "^CONFIG_MITIGATION_[A-Z0-9_]+=y" "$WORK/config.built"))" ok \
  || check "every option the $KARCH fragment sets is on" no "$(echo $off)"
echo "NOTE $(wc -c <"$WORK/$OUTPUT" | tr -d ' ') bytes, $(grep -c "=y$" "$WORK/config.built") options =y, $(wc -l <"$WORK/missing.txt") requested options missing (missing.txt)"
echo "NOTE $OUTPUT sha256 $(digest "$WORK/$OUTPUT")"
finish
