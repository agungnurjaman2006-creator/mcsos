#!/usr/bin/env bash
set -euo pipefail
kernel="${1:-build/kernel.elf}"
[[ -f "$kernel" ]] || { echo "[M4][FAIL] kernel ELF tidak ditemukan: $kernel" >&2; exit 1; }

mkdir -p build
readelf -h "$kernel" > build/m4.readelf.header.txt
readelf -l "$kernel" > build/m4.readelf.programs.txt
readelf -S "$kernel" > build/m4.readelf.sections.txt
nm -n "$kernel"      > build/m4.syms.txt
objdump -d -Mintel "$kernel" > build/m4.disasm.txt

grep -q 'ELF64' build/m4.readelf.header.txt
grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' build/m4.readelf.header.txt
grep -q 'x86_64_idt_init'          build/m4.syms.txt
grep -q 'x86_64_trap_dispatch'     build/m4.syms.txt
grep -q 'x86_64_exception_stubs'   build/m4.syms.txt
grep -q 'isr_stub_14'              build/m4.syms.txt
grep -q 'lidt'                     build/m4.disasm.txt
grep -q 'iretq'                    build/m4.disasm.txt
! nm -u "$kernel" | grep .

echo "[M4][PASS] ELF, symbol, IDT, LIDT, dan IRETQ audit lulus untuk $kernel"
