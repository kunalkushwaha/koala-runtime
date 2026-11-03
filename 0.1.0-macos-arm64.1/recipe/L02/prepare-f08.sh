#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
OUTPUT_ROOT="${KOALA_F08_OUT:-$ROOT/.artifacts/f08}"
F02_ROOT="${KOALA_F02_ARTIFACTS:-$ROOT/.artifacts/f02/runtime}"
ARTIFACT_DIR="$OUTPUT_ROOT/runtime"
KERNEL="$OUTPUT_ROOT/kernel/vmlinux-arm64"
CONTEXT="$OUTPUT_ROOT/guest-context"
DOWNLOADS="$OUTPUT_ROOT/downloads"
IMAGE="koala-f08-root:ubuntu-24.04-full-arm64-v1"
mkdir -p "$CONTEXT" "$DOWNLOADS"

"$SCRIPT_DIR/build-f08.sh" >/dev/null
"$SCRIPT_DIR/prepare-f08-kernel.sh" >/dev/null
BIN="$OUTPUT_ROOT/bin/koala-f08"

rm -rf "$CONTEXT"
mkdir -p "$CONTEXT"
python3 "$ROOT/scripts/runtime_packages.py" fetch "$ROOT/Experiments/F08/Guest/debs" >/dev/null  # locked packages (R01A)
cp -R "$ROOT"/Experiments/F08/Guest/. "$CONTEXT/"
"$BIN" extract-agent --init-block "$F02_ROOT/init.ext4" --output-dir "$CONTEXT"

INPUT_SHA="$({
  find "$CONTEXT" -type f -print | LC_ALL=C sort | xargs shasum -a 256
  shasum -a 256 "$KERNEL"
} | shasum -a 256 | awk '{print $1}')"
INPUT_STAMP="$ARTIFACT_DIR/input-sha256.txt"
if [[ -z "${KOALA_F08_FORCE:-}" && -f "$ARTIFACT_DIR/manifest.json" && -f "$INPUT_STAMP" ]] &&
  [[ "$(tr -d '\n' < "$INPUT_STAMP")" == "$INPUT_SHA" ]]; then
  "$BIN" validate --artifact-dir "$ARTIFACT_DIR"
  exit 0
fi
rm -rf "$ARTIFACT_DIR"

# shellcheck disable=SC2086  # KOALA_F08_DOCKER_FLAGS is a word list, e.g. --no-cache (R01A)
docker build ${KOALA_F08_DOCKER_FLAGS:-} \
  --platform linux/arm64 \
  --provenance=false \
  --tag "$IMAGE" \
  "$CONTEXT"

ROOT_TAR="$DOWNLOADS/ubuntu-24.04-full-arm64-v1.tar"
CONTAINER_NAME="koala-f08-export-$$"
cleanup() {
  docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
}
trap cleanup EXIT
docker create --platform linux/arm64 --name "$CONTAINER_NAME" "$IMAGE" >/dev/null
rm -f "$ROOT_TAR"
docker export --output "$ROOT_TAR" "$CONTAINER_NAME"
cleanup
trap - EXIT

"$BIN" prepare \
  --artifact-dir "$ARTIFACT_DIR" \
  --rootfs-tar "$ROOT_TAR" \
  --kernel "$KERNEL" \
  --agent-dir "$CONTEXT"
printf '%s\n' "$INPUT_SHA" > "$INPUT_STAMP"
