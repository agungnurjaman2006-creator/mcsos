# Readiness Review M1 - Toolchain Reproducible

## Identitas
- Nama mahasiswa: Agung Nurjaman
- NIM: 25832073010
- Kelas: PTI
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia
- Tanggal: 2026-06-13
- Commit hash: 4484e354218cc40a720840555f73db5908e919fe

## Ringkasan hasil
Seluruh target M1 berhasil dijalankan dari clean checkout. Lingkungan WSL 2 Ubuntu 26.04
dengan toolchain Clang 21.1.8, LLD 21.1.8, QEMU 10.2.1, dan OVMF telah terverifikasi.
Proof object freestanding x86_64 ELF berhasil dikompilasi tanpa undefined symbol.
Hash reproducibility identik pada dua run berturut-turut. Lingkungan dinyatakan siap lanjut M2.

## Evidence checklist
| Evidence | Path | Status | Catatan |
|---|---|---|---|
| Toolchain versions | `build/meta/toolchain-versions.txt` | Ada | Clang 21.1.8, GCC 15.2, QEMU 10.2.1 |
| Host readiness | `build/meta/host-readiness.txt` | Ada | i3-1005G1, 1.8GiB RAM, 1007G disk |
| QEMU capabilities | `build/meta/qemu-capabilities.txt` | Ada | q35 dan OVMF terdeteksi |
| Freestanding object | `build/proof/freestanding_probe.o` | Ada | ELF64 x86-64 relocatable |
| Freestanding ELF | `build/proof/freestanding_probe.elf` | Ada | ELF64 x86-64 executable |
| ELF header | `build/proof/readelf-header.txt` | Ada | Machine: Advanced Micro Devices X86-64 |
| ELF sections | `build/proof/readelf-sections.txt` | Ada | .text, .bss, .symtab tersedia |
| Disassembly | `build/proof/objdump-disassembly.txt` | Ada | mcsos_toolchain_probe terdisassembly |
| Undefined symbol report | `build/proof/nm-undefined.txt` | Ada | Kosong, tidak ada undefined symbol |
| Reproducibility hash | `build/repro/sha256-run1.txt`, `build/repro/sha256-run2.txt` | Ada | Hash identik pada kedua run |

## Acceptance criteria M1
| Kriteria | Lulus/Gagal | Bukti |
|---|---|---|
| Repository berada di filesystem Linux WSL | Lulus | `/home/agung/src/mcsos` |
| Semua tool wajib tersedia | Lulus | `make check` OK semua tool |
| `make meta` berhasil | Lulus | `build/meta/toolchain-versions.txt` terisi |
| `make check` berhasil | Lulus | Tidak ada baris ERROR |
| `make proof` berhasil | Lulus | Object dan ELF terbentuk, nm-undefined kosong |
| `make qemu-probe` berhasil | Lulus | q35 dan OVMF terdeteksi |
| `make repro` berhasil | Lulus | Hash run1 = run2 |
| `make test` berhasil dari clean checkout | Lulus | `OK: M1 test suite passed` |
| `nm-undefined.txt` kosong | Lulus | Tidak ada undefined symbol |
| Hasil `readelf` menunjukkan ELF64 x86_64 | Lulus | Machine: Advanced Micro Devices X86-64 |

## Known limitations
- Belum ada cross GCC x86_64-elf-gcc sebagai alternatif Clang
- Belum ada CI/CD otomatis untuk make test
- Belum ada boot image kernel
- Pengujian hanya pada emulator, belum pada hardware fisik

## Risiko dan mitigasi
1. **Compiler salah target** → mitigasi: readelf dijalankan setiap build untuk verifikasi Machine field
2. **OVMF tidak tersedia** → mitigasi: qemu_probe.sh mendeteksi path OVMF sebelum M2 dimulai
3. **Repository berpindah ke mount Windows** → mitigasi: check_toolchain.sh menolak path /mnt/* secara otomatis
4. **Nondeterminism build** → mitigasi: repro_check.sh membandingkan hash dua run dan gagal jika berbeda
5. **Paket distro berubah versi** → mitigasi: toolchain-versions.txt mencatat versi aktual setiap run

## Readiness decision
- [ ] Belum siap lanjut M2.
- [ ] Siap lanjut M2 dengan catatan.
- [x] Siap lanjut M2.

Alasan keputusan: Seluruh acceptance criteria M1 terpenuhi. make test lulus dari clean
checkout. Toolchain freestanding x86_64 terverifikasi. QEMU q35 dan OVMF tersedia.
Hash reproducibility identik. Tidak ada undefined symbol pada proof ELF.
