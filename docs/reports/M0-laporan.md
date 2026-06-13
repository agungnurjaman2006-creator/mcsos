# Laporan Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan Pengembangan

## 1. Sampul
- Judul praktikum: Praktikum M0 — Baseline Requirements, Governance, dan Lingkungan Pengembangan Reproducible MCSOS 260502
- Nama mahasiswa: Agung Nurjaman
- NIM: 25832073010
- Kelas: Pendidikan Teknologi Informasi
- Dosen: Muhaemin Sidiq, S.Pd., M.Pd.
- Program Studi: Pendidikan Teknologi Informasi, Institut Pendidikan Indonesia
- Tanggal: 13 Juni 2026

## 2. Tujuan
Praktikum M0 bertujuan untuk:
1. Menyiapkan lingkungan pengembangan yang dapat direproduksi di Windows 11 x64 melalui WSL 2.
2. Membuat struktur repository MCSOS yang seragam dan terdokumentasi.
3. Memverifikasi toolchain freestanding untuk target x86_64.
4. Menyusun dokumen baseline requirements, governance, threat model, risk register, dan verification matrix.
5. Membuktikan bahwa lingkungan siap untuk masuk ke M1.

## 3. Dasar Teori Ringkas

### Host vs Target
Host adalah Windows 11 x64 tempat pengembangan dilakukan. Target adalah bare-metal x86_64
yang dijalankan di QEMU. Keduanya tidak boleh dicampur karena ABI dan asumsi runtime berbeda.

### WSL 2
WSL 2 adalah virtualisasi Linux penuh di atas Windows menggunakan Hyper-V. Memberikan
lingkungan Linux yang terisolasi dari filesystem Windows.

### Cross-compilation
Kernel tidak boleh dikompilasi seperti program Linux biasa. Flag --target=x86_64-unknown-none
memberitahu Clang bahwa output ditujukan untuk bare-metal x86_64 tanpa OS.

### ELF Object
ELF (Executable and Linkable Format) adalah format binary standar Linux. Object relocatable
(tipe REL) adalah hasil kompilasi sebelum linking; belum dapat dieksekusi langsung.

### QEMU dan OVMF
QEMU adalah emulator x86_64. OVMF adalah firmware UEFI untuk QEMU. Keduanya digunakan
pada milestone berikutnya untuk boot kernel.

### Reproducibility
Reproducibility berarti prosedur dapat diulang dari clean checkout dengan hasil yang dapat diaudit.
Dicapai dengan mencatat versi toolchain dan menggunakan script validasi.

### Evidence-first Engineering
Setiap klaim harus disertai bukti berupa command output, log, checksum, atau artefak yang
dapat diperiksa.

## 4. Lingkungan

| Komponen | Versi / output |
|---|---|
| Windows | Windows 11 x64 |
| WSL distro | Ubuntu (resolute) |
| Kernel Linux WSL | 6.6.87.2-microsoft-standard-WSL2 |
| Git | 2.53.0 |
| Clang | 21.1.8 |
| LLD | 21.1.8 |
| binutils/readelf | 2.46 |
| NASM | 3.01 |
| QEMU | 10.2.1 |
| GDB | 17.1 |
| Python | 3.14.4 | 

Metadata lengkap tersedia di build/meta/toolchain-versions.txt.

## 5. Desain Baseline

### Struktur Repository
Repository MCSOS ditempatkan di /home/agung/src/mcsos (filesystem Linux WSL).
Struktur direktori mengikuti panduan M0 dengan folder docs, tools, smoke, dan build.

### Dokumen Baseline
- system_requirements.md: 12 requirement dengan verification mapping
- assumptions_and_nongoals.md: 10 asumsi dan 8 non-goals
- ADR-0001: Keputusan toolchain WSL 2, Clang, QEMU, OVMF, Limine
- invariants.md: Invariants repository, toolchain, dokumentasi, dan evidence
- threat_model.md: Assets, actors, trust boundary, dan mitigasi awal
- risk_register.md: 10 risiko dengan probabilitas, dampak, dan mitigasi
- verification_matrix.md: Mapping requirement ke command dan evidence

### Assumptions Utama
Target adalah x86_64 bare-metal. Build di WSL 2. Emulator QEMU. Firmware OVMF.
Bahasa kernel freestanding C17. M0 tidak membuat kernel bootable.

### Threat Model Awal
Assets utama: source code, toolchain, build scripts, dokumentasi, generated artifacts.
Trust boundary utama: Windows host vs WSL Linux environment.
Mitigasi utama: repository di filesystem Linux, target eksplisit pada compiler, versi tool tercatat.

## 6. Langkah Kerja

1. Install WSL 2 di Windows 11 via PowerShell Administrator: wsl --install
2. Verifikasi WSL 2 dengan wsl --list --verbose (VERSION = 2)
3. Update paket Ubuntu: sudo apt update && sudo apt upgrade -y
4. Install paket toolchain: build-essential, clang, lld, llvm, binutils, nasm, qemu-system-x86, ovmf, gdb, dll
5. Setup identitas Git: user.name, user.email, defaultBranch main
6. Buat repository di ~/src/mcsos dan init Git
7. Buat struktur direktori baseline dengan mkdir -p
8. Buat tools/check_env.sh dan jalankan make meta
9. Buat smoke/freestanding.c dan jalankan make smoke
10. Buat semua dokumen baseline di docs/
11. Commit semua file ke Git

## 7. Hasil Uji

| Pengujian | Command | Hasil | Pass/Fail |
|---|---|---|---|
| WSL version | `wsl --list --verbose` | VERSION 2 | Pass |
| Repository location | `pwd` | /home/agung/src/mcsos | Pass |
| Tool check | `bash tools/check_env.sh` | Semua 14 tool [OK] | Pass |
| Metadata | `cat build/meta/toolchain-versions.txt` | Versi tercatat | Pass |
| Smoke object | `make smoke` | ELF64 x86-64 REL | Pass |
| ELF header | `readelf -h build/smoke/freestanding.o` | Class ELF64, Machine x86-64, Type REL | Pass |
| QEMU version | `make qemu-version` | QEMU 10.2.1 | Pass |
| Git status | `git status` | Repository aktif | Pass |

## 8. Analisis
Kendala yang ditemui selama M0:
1. Password WSL lupa saat pertama kali setup. Diselesaikan dengan masuk sebagai root
   via wsl -u root dan menjalankan passwd agung.
2. Perintah ubuntu config --default-user root tidak dikenali karena executable ubuntu
   tidak ada di PATH. Diselesaikan dengan menggunakan wsl -u root sebagai alternatif.

Semua kendala berhasil diselesaikan dan tidak memengaruhi hasil akhir M0.

## 9. Keamanan dan Reliability

### Supply-chain Risk
Paket diinstall dari repository resmi Ubuntu via apt. Tidak ada paket dari sumber tidak dikenal.

### Toolchain Mismatch Risk
Dicegah dengan flag --target=x86_64-unknown-none pada kompilasi dan verifikasi readelf.

### Repository Path Risk
Repository ditempatkan di filesystem Linux WSL (/home/agung/src/mcsos), bukan /mnt/c.
Check script memberikan warning otomatis jika path salah.

### Log Integrity
Semua output command disimpan dan tidak dihapus dari laporan meskipun ada error.

## 10. Failure Modes dan Rollback

| Failure mode | Gejala | Diagnosis | Rollback/perbaikan |
|---|---|---|---|
| WSL bukan versi 2 | VERSION bernilai 1 | wsl --list --verbose | wsl --set-version Ubuntu 2 |
| Tool tidak ditemukan | command not found | command -v tool | sudo apt install paket |
| Repository di /mnt/c | Check script warning | pwd | Pindahkan ke ~/src/mcsos |
| Smoke object salah target | Machine bukan x86-64 | readelf -h | Perbaiki flag --target |
| OVMF tidak ditemukan | File tidak ada | find /usr/share -iname OVMF* | sudo apt install ovmf |
| Password WSL lupa | Authentication failed | - | wsl -u root lalu passwd agung |

## 11. Kesimpulan
Praktikum M0 telah diselesaikan dengan status siap uji lingkungan.

Seluruh acceptance criteria terpenuhi:
- WSL 2 aktif dan terverifikasi
- Semua tool wajib tersedia (14/14)
- Repository berada di filesystem Linux WSL
- Smoke test menghasilkan object ELF64 x86-64 relocatable
- Semua dokumen baseline tersedia
- Metadata toolchain tercatat

M0 TIDAK membuktikan kernel dapat boot. M0 TIDAK mengklaim sistem operasi siap pakai.
Status readiness: siap uji lingkungan, siap masuk M1 apabila seluruh acceptance evidence tersedia.

Syarat masuk M1: seluruh failure pada readiness review ditutup dan commit M0 tersedia.

## 12. Lampiran

### Output tools/check_env.sh
(tempel output make meta di sini)

### Isi build/meta/toolchain-versions.txt
(tempel output cat build/meta/toolchain-versions.txt di sini)

### Output readelf -h
(tempel output readelf -h build/smoke/freestanding.o di sini)

### Commit hash
43aaaed M0: initialize reproducible OS development baseline
43aaaed6c93b7e7b4ed4acf5e87f114903581b5d	

## 13. Referensi
[1] Microsoft, "How to install Linux on Windows with WSL," Microsoft Learn. Accessed: Jun 2026. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/install
[2] Microsoft, "Advanced settings configuration in WSL," Microsoft Learn. Accessed: Jun 2026. [Online]. Available: https://learn.microsoft.com/en-us/windows/wsl/wsl-config
[3] QEMU Project, "Invocation," QEMU Documentation. Accessed: Jun 2026. [Online]. Available: https://www.qemu.org/docs/master/system/invocation.html
[4] QEMU Project, "GDB usage," QEMU Documentation. Accessed: Jun 2026. [Online]. Available: https://www.qemu.org/docs/master/system/gdb.html
[5] LLVM Project, "Cross-compilation using Clang," Clang Documentation. Accessed: Jun 2026. [Online]. Available: https://clang.llvm.org/docs/CrossCompilation.html
