#!/usr/bin/env bash
set -euo pipefail

echo "[M11] Preflight lingkungan dan artefak M0-M10"

for tool in git make clang nm readelf objdump sha256sum; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[FAIL] tool tidak ditemukan: $tool" >&2; exit 1
    fi
    echo "[OK] $tool -> $(command -v "$tool")"
done

clang --version | head -n1
make --version | head -n1

for f in include/mcsos/user/m11_elf_loader.h \
          kernel/user/m11_elf_loader.c \
          tests/m11/m11_host_test.c \
          Makefile.m11; do
    if [[ ! -f "$f" ]]; then
        echo "[FAIL] file tidak ditemukan: $f" >&2; exit 1
    fi
    echo "[OK] file: $f"
done

echo "[M11] menjalankan host test..."
make -f Makefile.m11 CC=clang m11-host-test

echo "[M11] menjalankan audit..."
make -f Makefile.m11 CC=clang m11-audit

if [[ -s build/m11_nm_undefined.txt ]]; then
    echo "[FAIL] undefined symbol ditemukan" >&2; exit 1
fi
grep -q 'ELF64' build/m11_readelf_header.txt
grep -q 'm11_elf64_plan_load' build/m11_objdump.txt

echo "[OK] commit: $(git rev-parse --short HEAD)"
echo "[PASS] M11 preflight selesai."
