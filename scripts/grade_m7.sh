#!/usr/bin/env bash
set -euo pipefail

make clean >/dev/null 2>&1 || true
mkdir -p build/evidence

make check 2>&1 | tee build/evidence/m7_make_check.log

readelf -h build/vmm.o  > build/evidence/m7_vmm_readelf_header.txt
readelf -S build/vmm.o  > build/evidence/m7_vmm_readelf_sections.txt
nm -u build/vmm.o       > build/evidence/m7_vmm_nm_undefined.txt
objdump -dr build/vmm.o > build/evidence/m7_vmm_objdump.txt

if [ -s build/evidence/m7_vmm_nm_undefined.txt ]; then
  echo "[FAIL] unresolved symbol ditemukan pada build/vmm.o" >&2; exit 1
fi

grep -q "invlpg" build/evidence/m7_vmm_objdump.txt
grep -q "cr3"    build/evidence/m7_vmm_objdump.txt

echo "[PASS] static grade M7 selesai"
