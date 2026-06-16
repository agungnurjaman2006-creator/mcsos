#!/usr/bin/env bash
set -Eeuo pipefail
ISO="${1:-build/mcsos.iso}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}"
OVMF_VARS="${OVMF_VARS:-build/OVMF_VARS.fd}"
exec qemu-system-x86_64 \
  -machine q35 \
  -m 256M \
  -smp 1 \
  -cpu qemu64 \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$OVMF_VARS" \
  -cdrom "$ISO" \
  -boot d \
  -serial mon:stdio \
  -display none \
  -no-reboot \
  -no-shutdown \
  -s -S
