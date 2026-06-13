# Risk Register MCSOS 260502 — M0

| ID | Risiko | Probabilitas | Dampak | Mitigasi | Owner | Trigger review |
|---|---|---:|---:|---|---|---|
| R-M0-001 | WSL tidak aktif atau memakai WSL 1 | Medium | High | Verifikasi `wsl --list --verbose`; konversi ke WSL 2 | Toolchain engineer | `VERSION` bukan 2 |
| R-M0-002 | Repository berada di `/mnt/c` | High | Medium | Pindahkan ke `~/src/mcsos`; check script warning | Koordinator | `pwd` menunjukkan `/mnt/c` |
| R-M0-003 | QEMU tidak tersedia | Medium | High | Pasang `qemu-system-x86`; catat versi | Toolchain engineer | `command -v qemu-system-x86_64` gagal |
| R-M0-004 | OVMF path berbeda | Medium | Medium | Cari dengan `find /usr/share`; jangan hardcode tanpa verifikasi | Toolchain engineer | `OVMF_CODE.fd` tidak ditemukan |
| R-M0-005 | Compiler menghasilkan target host | Medium | High | Pakai `--target`; inspeksi `readelf` | Verification engineer | `Machine` bukan x86-64 |
| R-M0-006 | Dokumen requirement tidak testable | Medium | Medium | Verification matrix wajib | Documentation engineer | Requirement tanpa evidence |
| R-M0-007 | Kelompok tidak sinkron branch | Medium | Medium | Kebijakan branch dan pull sebelum commit | Koordinator | Konflik merge berulang |
| R-M0-008 | Mahasiswa menghapus log error | Medium | Medium | Laporan wajib mencantumkan failure mode | Semua | Error tidak tercatat |
| R-M0-009 | Build bergantung pada package version tidak tercatat | Medium | High | `make meta` sebelum submit | Verification engineer | Metadata kosong |
| R-M0-010 | Scope M0 melebar menjadi implementasi kernel | Medium | Medium | Ikuti non-goals; tunda kernel ke M1/M2 | Koordinator | Ada kode kernel fungsional tanpa kontrak |
