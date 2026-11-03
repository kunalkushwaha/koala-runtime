#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
ARTIFACT_DIR="${1:-$ROOT/.artifacts/f02/runtime}"
DOWNLOAD_DIR="$ROOT/.artifacts/f02/downloads"
KERNEL="${KOALA_F02_KERNEL:-$DOWNLOAD_DIR/vmlinux.container}"
KATA_URL="https://github.com/kata-containers/kata-containers/releases/download/3.17.0/kata-static-3.17.0-arm64.tar.xz"
KATA_SHA256="647c7612e6edf789d5e14698c48c99d8bac15ad139ffaa1c8bb7d229f748d181"  # R01A pin

if [ ! -f "$KERNEL" ]; then
  mkdir -p "$DOWNLOAD_DIR/unpack"
  ARCHIVE="$DOWNLOAD_DIR/kata-static-3.17.0-arm64.tar.xz"
  if [ ! -f "$ARCHIVE" ]; then
    curl --fail --silent --show-error --location "$KATA_URL" --output "$ARCHIVE"
  fi
  if [ "$(shasum -a 256 "$ARCHIVE" | cut -d' ' -f1)" != "$KATA_SHA256" ]; then
    echo "Kata archive digest mismatch: $ARCHIVE" >&2
    exit 1
  fi
  tar -xJf "$ARCHIVE" -C "$DOWNLOAD_DIR/unpack"
  cp -L "$DOWNLOAD_DIR/unpack/opt/kata/share/kata-containers/vmlinux.container" "$KERNEL"
fi

KERNEL_SHA256="67bac9f416af4cdc9b151e4ba4962d6515e0ad7acc53816761cf964aa6af6ea0"  # R01A pin
if [ -z "${KOALA_F02_KERNEL:-}" ] && [ "$(shasum -a 256 "$KERNEL" | cut -d' ' -f1)" != "$KERNEL_SHA256" ]; then
  echo "lean kernel digest mismatch: $KERNEL" >&2
  exit 1
fi

F02_BIN="$($ROOT/scripts/build-f02.sh)"
"$F02_BIN" prepare --artifact-dir "$ARTIFACT_DIR" --kernel "$KERNEL"
