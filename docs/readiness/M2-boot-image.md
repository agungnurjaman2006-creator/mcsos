# Readiness Review M2 - Boot Image dan Early Serial Console

## Identitas
- Proyek: MCSOS 260502
- Praktikum: M2
- Target: x86_64, QEMU, OVMF, Limine
- Nama/Kelompok: Agung
- Commit hash: 9aad140569f807d7b2a80ec4211ac8e5d01657e6
- Tanggal: 2026-06-14

## Ringkasan Status
Status yang diajukan: **siap uji QEMU tahap M2**

Alasan ringkas: Kernel ELF64 x86_64 berhasil dikompilasi dengan Clang 21.1.8
freestanding, dilink dengan LLD menggunakan linker script higher-half
(0xffffffff80000000), dibungkus dalam ISO bootable dengan Limine v11.x-binary,
dijalankan pada QEMU q35 + OVMF, dan menghasilkan tiga marker serial wajib
pada build/qemu-serial.log. Seluruh 9 artefak wajib tersedia dan terverifikasi
oleh make grade.

## Evidence Matrix
| Evidence | Lokasi | Status | Catatan |
|---|---|---|---|
| Preflight M2 | `build/meta/m2-preflight.txt` | PASS | Semua tool OK, OVMF ditemukan |
| Kernel ELF | `build/kernel.elf` | PASS | ELF64 x86_64, entry 0xffffffff80000000 |
| Kernel map | `build/kernel.map` | PASS | Symbol boundary tersedia |
| readelf header | `build/inspect/readelf-header.txt` | PASS | Class ELF64, Machine x86-64 |
| readelf PHDR | `build/inspect/readelf-program-headers.txt` | PASS | 2 LOAD segments: .text, .rodata |
| objdump | `build/inspect/objdump-disassembly.txt` | PASS | kmain, serial_init, outb/inb terdisassembly |
| nm symbols | `build/inspect/nm-symbols.txt` | PASS | kmain, serial_init, serial_write ada |
| ISO | `build/mcsos.iso` | PASS | 4.2MB, Limine BIOS+UEFI terinstall |
| ISO checksum | `build/mcsos.iso.sha256` | PASS | 26d6fb253c8dbf619ce44b06eb4c9d3e4e05899b9b453bc22b8ae154bedc7771 |
| Serial log | `build/qemu-serial.log` | PASS | 3 marker wajib muncul |
| Git commit | `9aad140569f807d7b2a80ec4211ac8e5d01657e6` | PASS | Branch master, pesan jelas |

## Invariants yang Diperiksa
1. Kernel adalah ELF64 x86_64. **TERPENUHI** — readelf membuktikan Class ELF64,
   Machine Advanced Micro Devices X86-64.
2. Entry point sesuai linker script. **TERPENUHI** — entry point 0xffffffff80000000
   sesuai ENTRY(kmain) dan `. = 0xffffffff80000000` di linker.ld.
3. Kernel tidak memakai hosted libc. **TERPENUHI** — flag -ffreestanding -nostdlib,
   tidak ada dependency libc pada nm output.
4. Source dikompilasi dengan `-ffreestanding` dan `-mno-red-zone`. **TERPENUHI** —
   tercatat di CFLAGS Makefile dan build output.
5. Serial console tersedia sebelum subsistem kompleks. **TERPENUHI** — serial_init
   dipanggil pertama kali di kmain sebelum operasi lain.
6. Kernel tidak kembali setelah `kmain`. **TERPENUHI** — halt_forever() dengan
   attribute noreturn dan cli;hlt loop mencegah return.
7. Output QEMU disimpan sebagai log file. **TERPENUHI** — build/qemu-serial.log
   berisi marker dan output Limine/OVMF.

## Failure Modes yang Diuji atau Dianalisis
| Failure mode | Pernah terjadi? | Diagnosis | Perbaikan |
|---|---|---|---|
| Toolchain salah | Tidak | — | — |
| OVMF tidak ditemukan | Tidak | — | OVMF tersedia di /usr/share/OVMF/ |
| Limine gagal fetch | Tidak | — | Jaringan stabil, clone berhasil |
| ISO gagal dibuat | Tidak | — | xorriso tersedia dan berjalan normal |
| QEMU log kosong | Tidak | — | Serial diarahkan ke file dengan benar |
| Entry point salah | Tidak | — | linker.ld ENTRY(kmain) benar |
| Reboot loop | Tidak | — | halt_forever() mencegah return |
| CRLF script | Tidak | — | File dibuat langsung di WSL |

## Keputusan Readiness
- [x] Lulus M2: siap uji QEMU tahap M2.
- [ ] Belum lulus M2: perlu perbaikan.

## Catatan Reviewer
Jalur boot OVMF -> Limine -> kernel.elf -> kmain -> serial_init ->
serial_write -> halt_forever terbukti berjalan pada QEMU q35 dengan
firmware OVMF_CODE_4M.fd. Limine v11.x-binary (revision
5be26a73d7b7b4d4477d18be94e1d16e615adf56) digunakan sebagai bootloader.
Kernel M2 sengaja minimal: tidak ada memory manager, IDT, interrupt handler,
scheduler, syscall, userspace, filesystem, atau network stack. Status ini
adalah siap uji QEMU tahap M2, bukan siap produksi dan bukan siap
hardware umum.
