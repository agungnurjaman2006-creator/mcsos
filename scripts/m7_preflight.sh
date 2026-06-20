#!/usr/bin/env bash
set -euo pipefail

echo "[M7-PREFLIGHT] pemeriksaan lingkungan dan hasil M0-M6"

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[FAIL] command tidak ditemukan: $1" >&2; exit 1
  fi
  echo "[OK] $1 -> $(command -v "$1")"
}

need_file() {
  if [ ! -f "$1" ]; then
    echo "[FAIL] file wajib tidak ada: $1" >&2; exit 1
  fi
  echo "[OK] file ada: $1"
}

need_cmd git
need_cmd make
need_cmd clang
need_cmd ld.lld
need_cmd readelf
need_cmd objdump
need_cmd nm
need_cmd qemu-system-x86_64

need_file include/pmm.h
need_file src/pmm.c
need_file include/vmm.h
need_file src/vmm.c
need_file tests/test_vmm_host.c
need_file Makefile

if ! grep -R "pmm_alloc_frame" include src >/dev/null 2>&1; then
  echo "[FAIL] API pmm_alloc_frame dari M6 tidak ditemukan" >&2; exit 1
fi
if ! grep -R "pmm_free_frame" include src >/dev/null 2>&1; then
  echo "[FAIL] API pmm_free_frame dari M6 tidak ditemukan" >&2; exit 1
fi
if ! grep -R "x86_64_trap_dispatch" kernel >/dev/null 2>&1; then
  echo "[WARN] dispatcher trap M4 belum ditemukan di kernel/"
else
  echo "[OK] dispatcher trap M4 terdeteksi"
fi

make clean >/dev/null 2>&1 || true
make check

if nm -u build/vmm.o | grep -v '^$'; then
  echo "[FAIL] build/vmm.o memiliki unresolved symbol" >&2; exit 1
fi

objdump -dr build/vmm.o > build/vmm.objdump.txt
grep -q "invlpg" build/vmm.objdump.txt || { echo "[FAIL] invlpg tidak terlihat" >&2; exit 1; }
grep -q "cr3"    build/vmm.objdump.txt || { echo "[FAIL] akses CR3 tidak terlihat" >&2; exit 1; }

echo "[PASS] M7 preflight selesai."
