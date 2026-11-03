#!/bin/bash
# L02C2 (ADR 0006): builds the full (Docker) root for linux/amd64 on Linux,
# the way scripts/prepare-f08.sh builds it for arm64 on macOS: F08's Guest
# context with the amd64 package lock (runtime-packages-amd64.lock), every
# .deb fetched and verified by SHA-256, the vminitd and vmexec of L02A's
# x86_64 initfs (debugfs), `docker build`, `docker export`, then the same
# ContainerizationEXT4 packing as F08 (RootPack). Prints one PASS/FAIL line per
# check and a RESULT line.
#
# KOALA_F08_DOCKER_FLAGS is passed to `docker build` (e.g. --no-cache).
set -uo pipefail

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$HERE/../.." && pwd)"
WORK="${L02_WORK:-$HERE/.work}"
OUT="$WORK/full"
GUEST="$ROOT/Experiments/F08/Guest"
SWIFT="swift@sha256:c2f3ac2eb806ce43f68859c3d52bd3bb1cfc88672fb3a948b76e136d98e39278"  # 6.3.3-noble
RUNNER="koala-l02-runner:1"
IMAGE="koala-l02c2-root:ubuntu-24.04-full-amd64-v1"
DISK_MIB=8192

pass=0 fail=0
check() { if [ "$2" = ok ]; then echo "PASS $1"; pass=$((pass + 1)); else echo "FAIL $1${3:+: $3}"; fail=$((fail + 1)); fi; }
finish() { echo "RESULT $pass passed, $fail failed"; [ "$fail" -eq 0 ]; exit $?; }

[ -s "$WORK/initfs/initfs.ext4" ] || { check "L02A initfs present" no "run build-initfs.sh first"; finish; }
mkdir -p "$OUT"

# 1. RootPack (F08's ext4 calls), built in the pinned Swift image.
docker volume create koala-l02c2-build >/dev/null
docker run --rm -v "$HERE/RootPack":/src:ro -v koala-l02c2-build:/build -v "$OUT":/out "$SWIFT" bash -c '
  set -e
  apt-get update -qq >/dev/null; apt-get install -y -qq libarchive-dev libbz2-dev liblzma-dev libssl-dev zlib1g-dev >/dev/null
  rm -rf /tmp/p; cp -a /src /tmp/p && cd /tmp/p
  swift build -c release --build-path /build > /out/rootpack-build.log 2>&1
  cp /build/release/RootPack /out/RootPack
' && check "RootPack build" ok || { check "RootPack build" no "see $OUT/rootpack-build.log"; finish; }
rootpack() { docker run --rm -v "$OUT":/out -v "$WORK/initfs":/initfs:ro "$RUNNER" /out/RootPack "$@"; }

# 2. The build context: F08's Guest files with the amd64 lock, verified debs, the agent.
CONTEXT="$OUT/context"
rm -rf "$CONTEXT"; mkdir -p "$CONTEXT"
(cd "$GUEST" && tar --exclude=./debs --exclude=./runtime-packages.lock --exclude=./runtime-packages-amd64.lock -cf - .) | tar -xf - -C "$CONTEXT"
cp "$GUEST/runtime-packages-amd64.lock" "$CONTEXT/runtime-packages.lock"
python3 -I "$ROOT/scripts/runtime_packages.py" fetch --arch amd64 "$CONTEXT/debs" \
  && check "every locked amd64 .deb fetched and SHA-256 verified" ok || { check "fetch amd64 debs" no; finish; }
# debugfs, not EXT4Reader.readFile: containerization 0.47.0's reader drops the
# holes of sparse files (its extent list has no logical block numbers), and
# upstream's initfs stores vminitd and vmexec sparse (L02C3 finding). Each copy
# must equal L02A's built binary.
docker run --rm -v "$WORK/initfs":/initfs:ro -v "$CONTEXT":/out "$RUNNER" sh -c '
  debugfs -R "dump -p /sbin/vminitd /out/vminitd" /initfs/initfs.ext4 2>/dev/null
  debugfs -R "dump -p /sbin/vmexec /out/vmexec" /initfs/initfs.ext4 2>/dev/null
  chown 1000:1000 /out/vminitd /out/vmexec; chmod 0755 /out/vminitd /out/vmexec' \
  && check "vminitd and vmexec extracted from L02A's x86_64 initfs (debugfs)" ok || { check "extract agent" no; finish; }
for binary in vminitd vmexec; do
  [ "$(sha256sum < "$CONTEXT/$binary")" = "$(sha256sum < "$WORK/initfs/$binary")" ] \
    && check "$binary equals L02A's build (sha256 $(sha256sum < "$CONTEXT/$binary" | cut -c1-12)…)" ok \
    || check "$binary equals L02A's build" no "$(stat -c %s "$CONTEXT/$binary") vs $(stat -c %s "$WORK/initfs/$binary") bytes"
done
for binary in vminitd vmexec; do
  machine="$(od -An -tx2 -j18 -N2 "$CONTEXT/$binary" | tr -d ' ')"
  [ "$machine" = 003e ] && check "$binary is x86-64 (ELF machine 0x3e)" ok || check "$binary architecture" no "ELF machine 0x$machine"
done

# 3. docker build (offline package install from the lock, builder self-test), export.
# shellcheck disable=SC2086  # a word list, as in prepare-f08.sh
docker build ${KOALA_F08_DOCKER_FLAGS:-} --platform linux/amd64 --provenance=false --tag "$IMAGE" "$CONTEXT" \
  > "$OUT/docker-build.log" 2>&1 \
  && check "docker build --platform linux/amd64 (locked install, builder self-test)" ok \
  || { check "docker build" no "see $OUT/docker-build.log"; finish; }
ROOT_TAR="$OUT/ubuntu-24.04-full-amd64-v1.tar"
name="koala-l02c2-export-$$"
docker create --platform linux/amd64 --name "$name" "$IMAGE" >/dev/null
rm -f "$ROOT_TAR"
docker export --output "$ROOT_TAR" "$name"; docker rm -f "$name" >/dev/null
[ -s "$ROOT_TAR" ] && check "root exported ($(du -h "$ROOT_TAR" | cut -f1))" ok || { check "docker export" no; finish; }

# 4. ext4, as F08 packs it, and the image's package lock read back from it.
rm -f "$OUT/root-$DISK_MIB.ext4"
rootpack pack "/out/$(basename "$ROOT_TAR")" "/out/root-$DISK_MIB.ext4" "$DISK_MIB" /out/package-lock.txt \
  && check "root-$DISK_MIB.ext4 packed (ContainerizationEXT4, as F08), without /.dockerenv (C23)" ok || { check "pack ext4" no; finish; }

# 5. The image's installed packages against the lock.
packages="$(python3 -I - "$GUEST/runtime-packages-amd64.lock" "$OUT/package-lock.txt" <<'EOF'
import sys
lock = [l.rstrip("\n").split("\t") for l in open(sys.argv[1]) if l.strip() and not l.startswith("#")]
image = {tuple(l.rstrip("\n").split("\t")) for l in open(sys.argv[2]) if l.strip()}
installed = {(n, v) for n, v, a in image}
root = [(f[1], f[2], f[3]) for f in lock if f[0] == "root"]
missing = [r for r in root if (r[0], r[1]) not in installed]
foreign = sorted(n for n, v, a in image if a not in ("amd64", "all"))
builder_only = sorted(f[1] for f in lock if f[0] == "builder" and (f[1], f[2]) in installed
                      and f[1] not in {r[0] for r in root})
def line(ok, text): print(("PASS " if ok else "FAIL ") + text)
line(not missing, f"all {len(root)} root-stage lock entries are installed at their locked versions"
     + (f": missing {missing}" if missing else ""))
line(not foreign, f"every one of the image's {len(image)} packages is amd64 or all" + (f": {foreign}" if foreign else ""))
print(f"  builder-stage packages also in the image (from the base image): {builder_only or 'none'}")
EOF
)"
echo "$packages"
pass=$((pass + $(grep -c '^PASS' <<<"$packages"))); fail=$((fail + $(grep -c '^FAIL' <<<"$packages")))
[ -n "$packages" ] || check "package comparison ran" no

# 6. A content manifest of the root (path, type, mode, owner, size, content or
#    link digest; no timestamps), to compare builds.
manifest="$(python3 -I - "$ROOT_TAR" <<'EOF'
import hashlib, sys, tarfile
digest = hashlib.sha256()
with tarfile.open(sys.argv[1]) as tar:
    for member in sorted(tar.getmembers(), key=lambda m: m.name):
        content = ""
        if member.isfile():
            content = hashlib.sha256(tar.extractfile(member).read()).hexdigest()
        elif member.issym() or member.islnk():
            content = member.linkname
        digest.update(f"{member.name}\t{member.type}\t{member.mode:o}\t{member.uid}\t{member.gid}\t{member.size}\t{content}\n".encode())
print(digest.hexdigest())
EOF
)"
echo "root content manifest sha256 $manifest"
echo "root-$DISK_MIB.ext4 sha256 $(sha256sum "$OUT/root-$DISK_MIB.ext4" | awk '{print $1}')"
echo "package-lock.txt sha256 $(sha256sum "$OUT/package-lock.txt" | awk '{print $1}') ($(wc -l < "$OUT/package-lock.txt") packages)"
echo "$manifest" > "$OUT/content-manifest.sha256"
finish
