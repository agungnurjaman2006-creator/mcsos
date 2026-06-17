#!/usr/bin/env bash
set -euo pipefail
iso="${1:-build/mcsos.iso}"
log="${2:-build/m4-qemu-serial.log}"

[[ -f "$iso" ]] || { echo "[M4][FAIL] ISO tidak ditemukan: $iso" >&2; exit 1; }
command -v qemu-system-x86_64 >/dev/null || { echo "[M4][FAIL] qemu-system-x86_64 tidak ditemukan" >&2; exit 1; }

mkdir -p "$(dirname "$log")"
timeout 20s qemu-system-x86_64 \
    -machine q35 -cpu max -m 256M \
    -cdrom "$iso" -boot d \
    -serial file:"$log" \
    -display none -no-reboot -no-shutdown || true

grep -q '\[M4\] IDT loaded' "$log" || { echo "[M4][FAIL] Log tidak menunjukkan IDT loaded" >&2; exit 1; }
grep -q '\[M4\] IDT and exception dispatch path installed' "$log" || { echo "[M4][FAIL] Log tidak menunjukkan milestone M4 siap uji" >&2; exit 1; }
echo "[M4][PASS] QEMU smoke test lulus. Log: $log"
