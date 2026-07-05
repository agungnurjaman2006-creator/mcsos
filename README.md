# MCSOS 260502
MCSOS 260502 adalah proyek sistem operasi pendidikan bertahap untuk target
x86_64 dengan host pengembangan Windows 11 x64 melalui WSL 2.

Status saat ini: M16 — MCSFS1J (write-ahead journal, replay, fsck) terintegrasi
ke kernel boot melalui block layer sync wrapper, QEMU smoke test passing.

Target awal:
- Arsitektur: x86_64
- Emulator: QEMU system x86_64
- Firmware emulator: OVMF / UEFI
- Bahasa kernel awal: freestanding C17 dan assembly x86_64 minimal
- Kernel model awal: monolithic educational kernel dengan boundary modular internal

Riwayat milestone (M0-M16):
- M0-M2 : baseline requirements, toolchain readiness, boot image awal
- M3-M4 : panic logging, IDT dan exception trap path
- M5-M6 : PIC/PIT/IRQ, bitmap physical memory manager (PMM)
- M7-M8 : virtual memory manager (VMM), kernel heap allocator (kmem)
- M9-M10: kernel thread scheduler (FIFO), syscall ABI (int 0x80)
- M11   : ELF64 user loader
- M12   : spinlock, cooperative mutex, lock-order validator
- M13   : VFS minimal, RAMFS, FD table, syscall file I/O
- M14   : block device layer, RAM block driver, buffer cache
- M15   : MCSFS1 (filesystem persisten minimal)
- M16   : MCSFS1J (write-ahead journal, replay, fsck)

Perintah awal (build umum):
make check
make smoke

Verifikasi per milestone: sebagian besar milestone (M8, M9, M13, dst) sudah
memiliki target langsung di Makefile utama, contoh:
make m8-all
make m9-all
make m13-all

Beberapa milestone memiliki Makefile terpisah dan BELUM di-include ke
Makefile utama, sehingga harus dipanggil eksplisit dengan flag -f, contoh:
make -f Makefile.m11 m11-all
make -f Makefile.m12 m12-all
make -f Makefile.m14 m14-all
make -f Makefile.m15 m15-all

Catatan: pastikan variabel environment CC tidak di-override ke compiler
selain clang (jalankan `echo $CC` untuk mengecek), karena sebagian target
freestanding membutuhkan flag khusus clang (--target=x86_64-unknown-none-elf).

Dokumen utama:
docs/requirements/system_requirements.md
docs/requirements/assumptions_and_nongoals.md
docs/adr/ADR-0001-toolchain-and-boot-baseline.md
docs/security/threat_model.md
docs/governance/risk_register.md
docs/testing/verification_matrix.md

Catatan readiness: keberhasilan M0 hanya berarti lingkungan dan baseline
proyek siap diperiksa. M0 tidak membuktikan kernel dapat boot.
