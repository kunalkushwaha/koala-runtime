#!/bin/bash
# L02A (ADR 0006): builds the x86_64 guest kernel from pinned inputs:
# containerization 0.47.0's config-x86_64 plus Koala's fragment
# (kernel-x86_64.fragment, L02B), merged with the kernel's merge_config.sh. Gates
# on the F08 options and the fragment's. Prints one PASS/FAIL line per check and
# a RESULT line.
set -euo pipefail

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
WORK="${L02_WORK:-$HERE/.work}/kernel"
VOLUME="${L02_SOURCES_VOLUME:-koala-linux-build}"   # holds the resolved SwiftPM checkouts
BUILDER="ubuntu@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac"  # 24.04

# R05B3: Linux 6.18.60, the lean kernel's release (build-lean-kernel.sh; its
# digest is in kernel.org's signed sha256sums.asc).
SOURCE_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.60.tar.xz"
SOURCE_SHA="3c2f44ef6acc021a7ad444d9a2e6ec1ef488ff09368793d0004a6080f69d621e"
CONFIG_SHA="683681bb8b46e70d0cd57a8213ef326de1161df25b37b8293f3747f309c314d8"  # config-x86_64, 0.47.0
BUILD_SHA="5e0046a7ed1c8050595c8f4b37056bc86c030d073b9a63b5b5ca2b7fc5da4ed3"   # build.sh, as F08
# F08's required options (scripts/validate_f08.py); each must be =y or =m.
OPTIONS=(CGROUPS CGROUP_BPF CGROUP_CPUACCT CFS_BANDWIDTH MEMCG CGROUP_PIDS NAMESPACES UTS_NS
         IPC_NS PID_NS NET_NS SECCOMP SECCOMP_FILTER VETH BRIDGE BRIDGE_NETFILTER NETFILTER
         NF_CONNTRACK NF_NAT NF_TABLES NFT_NAT NFT_MASQ IP_NF_IPTABLES OVERLAY_FS EXT4_FS
         VIRTIO_BLK VIRTIO_NET VSOCKETS VIRTIO_VSOCKETS VIRTIO_BALLOON
         # Koala's fragment (L02B): SMP under Cloud Hypervisor, AMD hosts, KVM guest.
         CPU_SUP_AMD HYPERVISOR_GUEST PARAVIRT KVM_GUEST X86_X2APIC
         # R05B3: booted uncompressed through PVH; the CPU mitigations on.
         PVH CPU_MITIGATIONS)

pass=0 fail=0
check() { if [ "$2" = ok ]; then echo "PASS $1"; pass=$((pass + 1)); else echo "FAIL $1${3:+: $3}"; fail=$((fail + 1)); fi; }
digest() { sha256sum "$1" | awk '{print $1}'; }
finish() { echo "RESULT $pass passed, $fail failed"; [ "$fail" -eq 0 ]; }

mkdir -p "$WORK"
# A missing or other source (an earlier release's) is fetched again.
if [ ! -f "$WORK/source.tar.xz" ] || [ "$(digest "$WORK/source.tar.xz")" != "$SOURCE_SHA" ]; then
  curl -fsSL "$SOURCE_URL" -o "$WORK/source.tar.xz.partial" && mv "$WORK/source.tar.xz.partial" "$WORK/source.tar.xz"
fi
[ "$(digest "$WORK/source.tar.xz")" = "$SOURCE_SHA" ] && check "kernel source sha256" ok || { check "kernel source sha256" no "$(digest "$WORK/source.tar.xz")"; finish; }

docker run --rm -v "$VOLUME":/sources:ro -v "$WORK":/out "$BUILDER" \
  cp /sources/checkouts/containerization/kernel/config-x86_64 /sources/checkouts/containerization/kernel/build.sh /out/
cp "$HERE/kernel-x86_64.fragment" "$WORK/kernel-x86_64.fragment"
[ "$(digest "$WORK/config-x86_64")" = "$CONFIG_SHA" ] && check "config-x86_64 sha256" ok || check "config-x86_64 sha256" no "$(digest "$WORK/config-x86_64")"
[ "$(digest "$WORK/build.sh")" = "$BUILD_SHA" ] && check "build.sh sha256" ok || check "build.sh sha256" no "$(digest "$WORK/build.sh")"
[ "$fail" -eq 0 ] || finish

# Reproducible: fixed timestamp, user and host; upstream build.sh unchanged.
docker run --rm -v "$WORK":/kernel "$BUILDER" bash -c '
  set -e
  apt-get update -qq >/dev/null
  apt-get install -y -qq build-essential flex bison bc libelf-dev libssl-dev cpio xz-utils kmod >/dev/null
  export KBUILD_BUILD_TIMESTAMP="2026-01-01 00:00:00 UTC" KBUILD_BUILD_USER=koala KBUILD_BUILD_HOST=koala LOCALVERSION=""
  # Upstream build.sh copies /kernel/config-x86_64 as the .config, so the
  # merged config (upstream + fragment) is written there first; upstream
  # build.sh itself stays unchanged and digest-checked.
  mkdir -p /merge && tar -xf /kernel/source.tar.xz -C /merge --strip-components=1 linux-6.18.60/scripts/kconfig/merge_config.sh
  cp /kernel/config-x86_64 /kernel/config-x86_64.upstream
  cd /tmp && bash /merge/scripts/kconfig/merge_config.sh -m -O /tmp /kernel/config-x86_64.upstream /kernel/kernel-x86_64.fragment > /kernel/merge.log 2>&1
  cp /tmp/.config /kernel/config-x86_64
  TARGET_ARCH=x86_64 bash /kernel/build.sh > /kernel/build.log 2>&1
  cp /kernel/config-x86_64.upstream /kernel/config-x86_64
  cp /kbuild/.config /kernel/config.built
  # R05B3: the uncompressed kernel that build.sh'"'"'s make also builds, which
  # Cloud Hypervisor boots through PVH without decompressing.
  cp /kbuild/vmlinux /kernel/vmlinux-x86_64
' && check "kernel build" ok || { check "kernel build" no "see $WORK/build.log"; finish; }

[ -s "$WORK/vmlinuz-x86_64" ] && check "vmlinuz-x86_64 produced" ok || check "vmlinuz-x86_64 produced" no
file "$WORK/vmlinux-x86_64" | grep -q "ELF 64-bit.*x86-64" && check "vmlinux-x86_64: a 64-bit x86-64 ELF (R05B3)" ok \
  || check "vmlinux-x86_64: a 64-bit x86-64 ELF (R05B3)" no
# R05B3: no CPU mitigation of the 6.18.5 kernel lost on the way to 6.18.60.
mitigations=$(grep -cE "^CONFIG_MITIGATION_[A-Z0-9_]+=y$" "$WORK/config.built")
[ "$mitigations" -ge 23 ] && check "CPU mitigations on: $mitigations (6.18.5: 23)" ok \
  || check "CPU mitigations on: $mitigations (6.18.5: 23)" no
for option in "${OPTIONS[@]}"; do
  grep -Eq "^CONFIG_${option}=(y|m)$" "$WORK/config.built" && check "CONFIG_$option" ok || check "CONFIG_$option" no "not set in the built config"
done
echo "kernel sha256 $(digest "$WORK/vmlinux-x86_64") (vmlinux-x86_64, shipped); vmlinuz-x86_64 $(digest "$WORK/vmlinuz-x86_64")"
echo "built config sha256 $(digest "$WORK/config.built")"
echo "fragment sha256 $(digest "$WORK/kernel-x86_64.fragment")"
grep -m1 "^# Linux/x86" "$WORK/config.built" || true
finish
