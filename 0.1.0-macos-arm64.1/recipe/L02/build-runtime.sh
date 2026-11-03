#!/bin/bash
# Rebuild every runtime file from pinned inputs and record how (R01A).
#
#   scripts/build-runtime.sh [OUTPUT]
#
# OUTPUT (default .artifacts/runtime/<UTC>-<id>) must not exist. Needs macOS on
# Apple silicon, Docker and the network (the package cache makes reruns offline
# for packages). Options, as environment variables:
#   KOALA_RUNTIME_NO_CACHE=1        build images with docker --no-cache
#   KOALA_RUNTIME_REBUILD_KERNEL=1  rebuild the full kernel instead of reusing a
#                                   cached build (it is pinned to its digest)
# Side effect: like the harnesses, this refreshes .artifacts/f08/runtime.
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
OUT="${1:-$ROOT/.artifacts/runtime/$(date -u +%Y%m%dT%H%M%SZ)-$(uuidgen | cut -c1-8 | tr 'A-Z' 'a-z')}"
F08="$ROOT/.artifacts/f08"
NO_CACHE="${KOALA_RUNTIME_NO_CACHE:+--no-cache}"
# R05B1 on the Mac (MAC08): the lean profile's kernel is Koala's own, Linux
# 6.18.60 built Kata's way (Experiments/L02/build-lean-kernel.sh arm64), no
# longer Kata's prebuilt binary. Reproducible; the build fails unless it
# matches this pin.
LEAN_KERNEL_OUTPUT_SHA="d3e12c7273eb564e7e5c0ccaee34f6dd2bb1ca70bce39738a58d8ec967c3c0d0"  # Image-lean-arm64

[[ "$(uname -s)/$(uname -m)" == Darwin/arm64 ]] || { echo "build-runtime: needs macOS on Apple silicon" >&2; exit 1; }
docker version >/dev/null 2>&1 || { echo "build-runtime: Docker is not running" >&2; exit 1; }
[[ ! -e "$OUT" ]] || { echo "build-runtime: $OUT already exists" >&2; exit 1; }
mkdir -p "$OUT/lean" "$OUT/full" "$OUT/guest-tools" "$OUT/content" "$OUT/logs"
echo "build-runtime: $OUT" >&2

# Every locked package, verified, into the Docker context.
python3 "$SCRIPT_DIR/runtime_packages.py" fetch "$ROOT/Experiments/F08/Guest/debs" | tee "$OUT/logs/packages.txt" >&2

# 1. Lean: our kernel (built and checked against its pin), vminit and alpine
#    (by digest).
"$ROOT/Experiments/L02/build-lean-kernel.sh" arm64 >"$OUT/logs/lean-kernel.txt" 2>&1 \
  || { echo "build-runtime: the lean kernel build failed; see $OUT/logs/lean-kernel.txt" >&2; exit 1; }
LEAN_KERNEL="$ROOT/Experiments/L02/.work/kernel-lean-arm64/Image-lean-arm64"
[[ "$(shasum -a 256 "$LEAN_KERNEL" | cut -d' ' -f1)" == "$LEAN_KERNEL_OUTPUT_SHA" ]] \
  || { echo "build-runtime: the lean kernel does not match its pinned digest $LEAN_KERNEL_OUTPUT_SHA" >&2; exit 1; }
# Registry pulls can drop the connection; a failed attempt leaves nothing behind.
for attempt in 1 2 3; do
  rm -rf "$OUT/lean"
  KOALA_F02_KERNEL="$LEAN_KERNEL" "$SCRIPT_DIR/prepare-f02.sh" "$OUT/lean" >>"$OUT/logs/lean.txt" 2>&1 && break
  [[ "$attempt" == 3 ]] && { echo "build-runtime: lean step failed; see $OUT/logs/lean.txt" >&2; exit 1; }
  echo "build-runtime: lean step attempt $attempt failed; retrying" >&2; sleep 10
done
rm -rf "$OUT/lean/oci-store"

# 2. Full kernel, reused only when it matches its pinned output digest.
if [[ -n "${KOALA_RUNTIME_REBUILD_KERNEL:-}" ]]; then rm -f "$F08/kernel/build-input-sha256.txt"; fi
"$SCRIPT_DIR/prepare-f08-kernel.sh" >"$OUT/logs/full-kernel.txt" 2>&1

# 3. Full root from the locked packages.
KOALA_F02_ARTIFACTS="$OUT/lean" KOALA_F08_FORCE=1 KOALA_F08_DOCKER_FLAGS="$NO_CACHE" \
  "$SCRIPT_DIR/prepare-f08.sh" >"$OUT/logs/full-root.txt" 2>&1
cp -c "$F08/runtime/root-8192.ext4" "$OUT/full/root-8192.ext4"
cp "$F08/runtime/package-lock.txt" "$OUT/full/package-lock.txt"
cp "$F08/kernel/vmlinux-arm64" "$OUT/full/vmlinux-arm64"
python3 "$SCRIPT_DIR/runtime_provenance.py" tar-manifest \
  "$F08/downloads/ubuntu-24.04-full-arm64-v1.tar" "$OUT/content/full-root.tsv"

# 4. Guest helpers and busybox from the locked builder stage.
TAG="koala-runtime-tools:$(basename "$OUT")"
# shellcheck disable=SC2086
docker build $NO_CACHE --platform linux/arm64 --provenance=false --target subreaper-builder \
  --tag "$TAG" "$ROOT/Experiments/F08/Guest" >"$OUT/logs/guest-tools.txt" 2>&1
CONTAINER="$(docker create "$TAG")"
for tool in busybox koala-exec-status koala-transfer koala-host-exec koala-subreaper koala-init; do
  docker cp -q "$CONTAINER:/$tool" "$OUT/guest-tools/$tool"
done
docker rm "$CONTAINER" >/dev/null
docker run --rm "$TAG" dpkg-query -W -f='${Package}\t${Version}\t${Architecture}\n' busybox-static gcc-13 libc6-dev \
  >"$OUT/content/guest-tools-packages.tsv"
docker rmi "$TAG" >/dev/null

# R05B2 on the Mac (MAC09), as build-runtime-linux.sh: koala-init in the lean
# init file system, root's and 0755, where the kernel runs it first
# (init=/sbin/koala-init); the runtime then says so with guest protocol 1.1,
# and the manifest's init digest is the new image's. debugfs from the pinned
# Ubuntu image (e2fsprogs is in its base).
docker run --rm --platform linux/arm64 -v "$OUT:/out" "$(sed -n 's/^ARG UBUNTU_IMAGE=//p' "$ROOT/Experiments/F08/Guest/Dockerfile")" sh -c '
  debugfs -w -R "write /out/guest-tools/koala-init /sbin/koala-init" /out/lean/init.ext4 &&
  for field in "mode 0100755" "uid 0" "gid 0"; do debugfs -w -R "set_inode_field /sbin/koala-init $field" /out/lean/init.ext4; done &&
  debugfs -R "stat /sbin/koala-init" /out/lean/init.ext4' >"$OUT/logs/lean-koala-init.txt" 2>&1 \
  || { echo "build-runtime: koala-init into the lean init failed; see $OUT/logs/lean-koala-init.txt" >&2; exit 1; }
python3 -I - "$OUT" <<'PY'
import hashlib, json, sys
out = sys.argv[1]
path = f"{out}/lean/manifest.json"
manifest = json.load(open(path))
manifest["initSHA256"] = hashlib.sha256(open(f"{out}/lean/init.ext4", "rb").read()).hexdigest()
manifest["koalaInitSHA256"] = hashlib.sha256(open(f"{out}/guest-tools/koala-init", "rb").read()).hexdigest()
manifest["guestProtocolMaximum"] = {"major": 1, "minor": 1}
open(path, "w").write(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
PY

# R05B4A on the Mac (MAC10), as build-runtime-linux.sh: every ext4 image's
# bytes depend only on its content (a fixed UUID, hash seed and times;
# ext4_normalize.py), after the last write to each; the lean manifest's
# digests are the normalized images'. e2fsprogs 1.47 from the pinned Ubuntu
# image (Linux uses its runner image; the script takes either).
UBUNTU_RUNNER="$(sed -n 's/^ARG UBUNTU_IMAGE=//p' "$ROOT/Experiments/F08/Guest/Dockerfile")"
for image in lean/init.ext4:lean-init lean/base.ext4:lean-base lean/writable.ext4:lean-writable \
             full/root-8192.ext4:full-root; do
  python3 -I "$SCRIPT_DIR/ext4_normalize.py" "$OUT/${image%%:*}" "${image#*:}" --runner "$UBUNTU_RUNNER" \
    >"$OUT/logs/normalize-${image#*:}.txt" 2>&1 \
    || { echo "build-runtime: normalizing ${image%%:*} failed; see $OUT/logs/normalize-${image#*:}.txt" >&2; exit 1; }
done
python3 -I - "$OUT" <<'PY'
import hashlib, json, sys
out = sys.argv[1]
path = f"{out}/lean/manifest.json"
manifest = json.load(open(path))
sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
for key, file in (("initSHA256", "init"), ("baseSHA256", "base"), ("writableSHA256", "writable")):
    manifest[key] = sha(f"{out}/lean/{file}.ext4")
open(path, "w").write(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
PY

# 5. Base image packages (for lock coverage) and provenance.
docker run --rm --platform linux/arm64 "$(sed -n 's/^ARG UBUNTU_IMAGE=//p' "$ROOT/Experiments/F08/Guest/Dockerfile")" \
  dpkg-query -W -f='${Package}\t${Version}\t${Architecture}\n' | LC_ALL=C sort >"$OUT/content/base-packages.tsv"
# Provenance first: the licence step reads it (R01D, issue #84).
python3 "$SCRIPT_DIR/runtime_provenance.py" record "$OUT"
python3 "$SCRIPT_DIR/runtime_licenses.py" "$OUT" "$F08/downloads/ubuntu-24.04-full-arm64-v1.tar"  # R01B
python3 "$SCRIPT_DIR/validate_r01a.py" check "$OUT" | tee "$OUT/validator.txt" >&2
printf '%s\n' "$OUT"
