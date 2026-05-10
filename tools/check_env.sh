#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
META_DIR="$ROOT_DIR/build/meta"

mkdir -p "$META_DIR"

fail=0

say() {
    printf '[M0] %s\n' "$*"
}

check_tool() {
    local tool="$1"

    if command -v "$tool" >/dev/null 2>&1; then
        printf '[OK]   %-24s %s\n' "$tool" "$(command -v "$tool")"
    else
        printf '[FAIL] %-24s not found\n' "$tool"
        fail=1
    fi
}

say "Repository root: $ROOT_DIR"

case "$ROOT_DIR" in
    /mnt/c/*|/mnt/d/*|/mnt/e/*)
        printf '[WARN] Repository appears to be on a Windows-mounted filesystem. Move it to ~/src/mcsos for this practicum.\n'
        ;;
    *)
        printf '[OK] Repository is not under /mnt/<drive>.\n'
        ;;
esac
