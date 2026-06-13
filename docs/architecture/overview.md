# MCSOS 260502 — Architecture Overview

## Target
- Arsitektur: x86_64 (AMD64 / Intel 64)
- Host: Windows 11 x64
- Lingkungan build: WSL 2 Linux filesystem
- Emulator: QEMU system x86_64
- Firmware: OVMF (UEFI)
- Bootloader: Limine binary release

## Model Kernel
Kernel monolitik pendidikan dengan boundary modular dan POSIX-like subset.

## Bahasa
Freestanding C17 dengan inline assembly x86_64 minimal.

## Non-goals
- Tidak ada userspace pada M2
- Tidak ada memory manager pada M2
- Tidak ada interrupt handler pada M2
- Tidak ada hardware bring-up fisik pada M2

## Readiness Gate
Klaim keberhasilan hanya boleh: "siap uji QEMU tahap boot awal".
