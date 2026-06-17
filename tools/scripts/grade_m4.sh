#!/usr/bin/env bash
set -euo pipefail
score=0

make clean >/dev/null
make audit >/dev/null
score=$((score + 60))

tools/scripts/m4_audit_elf.sh build/kernel.elf >/dev/null
score=$((score + 20))

if [[ -f build/m4-qemu-serial.log ]]; then
    grep -q '\[M4\]' build/m4-qemu-serial.log && score=$((score + 10))
fi

[[ -f evidence/M4/manifest.txt ]] && score=$((score + 10))

echo "M4_LOCAL_SCORE=$score/100"
