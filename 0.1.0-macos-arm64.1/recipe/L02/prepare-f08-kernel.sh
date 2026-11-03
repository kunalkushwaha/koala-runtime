#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
OUTPUT_ROOT="${KOALA_F08_OUT:-$ROOT/.artifacts/f08}"
CONTAINERIZATION="${KOALA_CONTAINERIZATION_CHECKOUT:-$ROOT/.artifacts/f02/build/checkouts/containerization}"
KERNEL_ROOT="$OUTPUT_ROOT/kernel"
WORK="$KERNEL_ROOT/work"
SOURCE="$KERNEL_ROOT/source.tar.xz"
OUTPUT="$KERNEL_ROOT/vmlinux-arm64"
STAMP="$KERNEL_ROOT/build-input-sha256.txt"
# R05B3 on the Mac (MAC11): Linux 6.18.60, the lean kernel's release (the
# same verified source as build-lean-kernel.sh; kernel.org's signed
# sha256sums.asc), with containerization's config-arm64 and build.sh
# unchanged and digest-checked. 6.18.5's output was e94c7bee84da….
SOURCE_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.60.tar.xz"
SOURCE_SHA="3c2f44ef6acc021a7ad444d9a2e6ec1ef488ff09368793d0004a6080f69d621e"
OUTPUT_SHA="d6e8ac1101238970377ef57992b425393c6739706f1a57dbf5c6e192f55e9822"  # MAC11: the 6.18.60 full kernel
# What the built config must keep (MAC11): every CPU mitigation and
# hardening option of the 6.18.5 full kernel (its 29, MAC08/#322), and the
# options Docker and Koala need (as Linux's build-kernel.sh checks). A
# renamed or dropped option on the way to a new release fails the build.
REQUIRED_OPTIONS=(CPU_MITIGATIONS UNMAP_KERNEL_AT_EL0 MITIGATE_SPECTRE_BRANCH_HISTORY
  ARM64_PAN ARM64_EPAN ARM64_E0PD ARM64_PTR_AUTH ARM64_PTR_AUTH_KERNEL ARM64_BTI ARM64_MTE ARM64_GCS
  ARM64_POE ARM64_USE_LSE_ATOMICS RANDOMIZE_BASE RANDOMIZE_KSTACK_OFFSET RANDOMIZE_MODULE_REGION_FULL
  STACKPROTECTOR STACKPROTECTOR_STRONG STACKPROTECTOR_PER_TASK FORTIFY_SOURCE SLAB_FREELIST_HARDENED
  LIST_HARDENED DEBUG_LIST BUG_ON_DATA_CORRUPTION STRICT_KERNEL_RWX VMAP_STACK SECCOMP SECCOMP_FILTER
  CGROUPS CGROUP_BPF CGROUP_CPUACCT CFS_BANDWIDTH MEMCG CGROUP_PIDS NAMESPACES UTS_NS IPC_NS PID_NS
  NET_NS VETH BRIDGE BRIDGE_NETFILTER NETFILTER NF_CONNTRACK NF_NAT NF_TABLES NFT_NAT NFT_MASQ
  IP_NF_IPTABLES OVERLAY_FS EXT4_FS VIRTIO_BLK VIRTIO_NET VSOCKETS VIRTIO_VSOCKETS VIRTIO_BALLOON
  VIRTIO_FS DEVTMPFS_MOUNT)
CONFIG_SHA="dedb03de6d686534efbca3a90bac304721e818bd96bc421b4d2213724a4e260b"
BUILD_SHA="5e0046a7ed1c8050595c8f4b37056bc86c030d073b9a63b5b5ca2b7fc5da4ed3"
DOCKERFILE_SHA="2671d76eceb0926834559f000582d49f1b78e44c830309e4ed72c3c1c1a9f581"
SOURCES_LIST_SHA="a51719fced24ac227acb5a88acf9a3f9f6f5a5a58f4b35a7e5ad908de16e2561"
LOCALVERSION="-koala-f08-bc994b88df46"
BUILD_TIMESTAMP="2026-01-09T00:00:00Z"

mkdir -p "$KERNEL_ROOT"

check_sha() {
  local expected="$1"
  local file="$2"
  local actual
  actual="$(shasum -a 256 "$file" | awk '{print $1}')"
  if [[ "$actual" != "$expected" ]]; then
    echo "F08 kernel input digest mismatch for $file: expected $expected, got $actual" >&2
    exit 1
  fi
}

# A missing or other source (an earlier release's) is fetched again.
if [[ ! -f "$SOURCE" ]] || [[ "$(shasum -a 256 "$SOURCE" | awk '{print $1}')" != "$SOURCE_SHA" ]]; then
  curl --fail --location --output "$SOURCE.tmp" "$SOURCE_URL"
  mv "$SOURCE.tmp" "$SOURCE"
fi
check_sha "$SOURCE_SHA" "$SOURCE"

UPSTREAM_KERNEL="$CONTAINERIZATION/kernel"
for file in config-arm64 build.sh image/Dockerfile image/sources.list; do
  if [[ ! -f "$UPSTREAM_KERNEL/$file" ]]; then
    echo "missing pinned Containerization kernel input: $UPSTREAM_KERNEL/$file" >&2
    exit 1
  fi
done
check_sha "$CONFIG_SHA" "$UPSTREAM_KERNEL/config-arm64"
check_sha "$BUILD_SHA" "$UPSTREAM_KERNEL/build.sh"
check_sha "$DOCKERFILE_SHA" "$UPSTREAM_KERNEL/image/Dockerfile"
check_sha "$SOURCES_LIST_SHA" "$UPSTREAM_KERNEL/image/sources.list"

INPUT_SHA="$(printf '%s\n' \
  "$SOURCE_SHA" "$CONFIG_SHA" "$BUILD_SHA" "$DOCKERFILE_SHA" \
  "$SOURCES_LIST_SHA" "$LOCALVERSION" "$BUILD_TIMESTAMP" | shasum -a 256 | awk '{print $1}')"
if [[ -s "$OUTPUT" && -f "$STAMP" ]] &&
  [[ "$(tr -d '\n' < "$STAMP")" == "$INPUT_SHA" ]]; then
  check_sha "$OUTPUT_SHA" "$OUTPUT"
  shasum -a 256 "$OUTPUT"
  exit 0
fi

rm -rf "$WORK"
mkdir -p "$WORK/image"
cp "$SOURCE" "$WORK/source.tar.xz"
cp "$UPSTREAM_KERNEL/config-arm64" "$WORK/config-arm64"
cp "$UPSTREAM_KERNEL/build.sh" "$WORK/build.sh"
cp "$UPSTREAM_KERNEL/image/Dockerfile" "$WORK/image/Dockerfile"
cp "$UPSTREAM_KERNEL/image/sources.list" "$WORK/image/sources.list"
chmod 0755 "$WORK/build.sh"

IMAGE="koala-f08-kernel-builder:${INPUT_SHA:0:12}"
docker build --platform linux/arm64 --provenance=false --tag "$IMAGE" "$WORK/image"
docker run --platform linux/arm64 \
  --cpus "${KOALA_F08_KERNEL_CPUS:-8}" \
  --memory "${KOALA_F08_KERNEL_MEMORY:-12g}" \
  --rm \
  --volume "$WORK:/kernel" \
  --env "LOCALVERSION=$LOCALVERSION" \
  --env TARGET_ARCH=arm64 \
  --env KBUILD_BUILD_TIMESTAMP="$BUILD_TIMESTAMP" \
  --env KBUILD_BUILD_USER=koala \
  --env KBUILD_BUILD_HOST=builder \
  --workdir /kernel \
  "$IMAGE" /bin/bash -c './build.sh && cp /kbuild/.config /kernel/config.built'

test -s "$WORK/vmlinux-arm64"
missing=()
for option in "${REQUIRED_OPTIONS[@]}"; do
  grep -Eq "^CONFIG_${option}=(y|m)$" "$WORK/config.built" || missing+=("$option")
done
if (( ${#missing[@]} )); then
  echo "F08 kernel: options off in the built config: ${missing[*]}" >&2
  exit 1
fi
echo "F08 kernel: all ${#REQUIRED_OPTIONS[@]} required options on ($(grep -m1 '^# Linux/arm64' "$WORK/config.built"))" >&2
cp "$WORK/config.built" "$KERNEL_ROOT/config.built"
cp "$WORK/vmlinux-arm64" "$OUTPUT"
check_sha "$OUTPUT_SHA" "$OUTPUT"
printf '%s\n' "$INPUT_SHA" > "$STAMP"
shasum -a 256 "$OUTPUT"
