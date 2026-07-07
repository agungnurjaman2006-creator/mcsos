# ELF64 User Program Loader Awal, Process Image Plan, User Address-Space Contract, dan Kesiapan Transisi Userspace pada MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M11_Agung.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, subset POSIX-like
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `M11` |
| Judul praktikum | `ELF64 User Program Loader Awal, Process Image Plan, User Address-Space Contract, dan Kesiapan Transisi Userspace pada MCSOS 260502` |
| Jenis pengerjaan | `Individu` |
| Nama mahasiswa | `Agung Nurjaman` |
| NIM | `25832073010` |
| Kelas | `Pendidikan Teknologi Informasi` |
| Nama kelompok | `-` |
| Anggota kelompok | `-` |
| Tanggal praktikum | `2026-06-28` |
| Tanggal pengumpulan | `2026-06-28` |
| Repository | `/home/agung/src/mcsos` |
| Branch | `praktikum-m11-elf-user-loader` |
| Commit awal | `f017f4b` (M10) |
| Commit akhir | `ad07b27` (M11) |
| Status readiness yang diklaim | `Siap uji QEMU untuk loader ELF64 user awal dan process-image planning single-core — bukan siap produksi, bukan bukti isolasi user/kernel penuh` |

---

## 1. Sampul

# Laporan Praktikum M11
## ELF64 User Program Loader Awal, Process Image Plan, User Address-Space Contract, dan Kesiapan Transisi Userspace pada MCSOS 260502

Disusun oleh:

| Nama | NIM | Kelas | Peran |
|---|---|---|---|
| Agung Nurjaman | 25832073010 | Pendidikan Teknologi Informasi | Individu |

Dosen Pengampu: **Muhaemin Sidiq, S.Pd., M.Pd.**
Program Studi Pendidikan Teknologi Informasi
Institut Pendidikan Indonesia
Tahun Akademik 2025/2026

---

## 2. Pernyataan Orisinalitas dan Integritas Akademik

Saya menyatakan bahwa laporan ini disusun berdasarkan pekerjaan praktikum sendiri sesuai panduan yang diberikan. Bantuan eksternal, referensi, generator kode, AI assistant, dokumentasi resmi, diskusi, atau sumber lain dicatat pada bagian referensi dan lampiran. Saya tidak mengklaim hasil yang tidak dibuktikan oleh log, test, commit, atau artefak lain.

| Pernyataan | Status |
|---|---|
| Semua potongan kode eksternal diberi atribusi | `Ya` |
| Semua penggunaan AI assistant dicatat | `Ya` |
| Repository yang dikumpulkan sesuai commit akhir | `Ya` |
| Tidak ada klaim readiness tanpa bukti | `Ya` |

Catatan penggunaan bantuan eksternal:

```text
Panduan resmi praktikum M11 MCSOS 260502 digunakan sebagai referensi utama
dan kontrak implementasi untuk seluruh komponen (m11_elf_loader.h,
m11_elf_loader.c, m11_host_test.c, Makefile.m11, scripts/m11_preflight.sh,
dan pola integrasi kmain.c). Intel SDM Vol.3 digunakan sebagai referensi
teknis untuk privilege level x86_64, page table flags user/supervisor,
NX bit, dan model proteksi ring 0/ring 3. Dokumentasi Oracle Linker and
Libraries Guide dan System V ABI AMD64 digunakan sebagai referensi struktur
ELF64 (Elf64_Ehdr, Elf64_Phdr, PT_LOAD, PF_R/PF_W/PF_X, ET_EXEC, ET_DYN,
EM_X86_64). Dokumentasi Linux kernel ELF loader digunakan sebagai referensi
pembanding behavior loader modern. AI assistant (Claude) digunakan untuk:
(1) menulis draft awal m11_elf_loader.h sesuai kontrak panduan (struct
m11_elf64_ehdr, m11_elf64_phdr, m11_user_region, m11_segment_plan,
m11_process_image_plan, dan 17 kode error M11_ERR_*); (2) menulis draft
m11_elf_loader.c dengan validasi defensif (overflow check via
m11_add_overflow_u64, W^X enforcement, user range check, alignment check,
fail-closed plan reset saat error); (3) menulis 10 kasus host unit test
mencakup happy path dan 9 negative case; (4) menulis Makefile.m11 dan
m11_preflight.sh; (5) mendiagnosis masalah separator di Makefile.m11 yang
menggunakan tab (default Make) alih-alih '>' (RECIPEPREFIX dari Makefile
utama), diperbaiki via Python string replacement; (6) menulis Python patch
untuk menyisipkan m11_elf_smoke_test() ke kmain.c tanpa merusak kode M9
dan M10 yang sudah ada. Seluruh build, host unit test, audit nm/readelf/
objdump/sha256sum, QEMU smoke test, dan commit git dijalankan dan
diverifikasi sendiri oleh mahasiswa di WSL 2 miliknya. AI tidak digunakan
untuk mengubah kontrak fungsional di luar yang ditentukan panduan resmi.
```

---

## 3. Tujuan Praktikum

1. Memvalidasi image ELF64 secara defensif: magic, class, endianness, version, type (`ET_EXEC`/`ET_DYN`), machine (`EM_X86_64`), ukuran ELF header, ukuran program header entry, dan batas tabel program header.
2. Memvalidasi setiap segment `PT_LOAD` berdasarkan `p_offset`, `p_filesz`, `p_memsz`, `p_vaddr`, `p_align`, dan `p_flags`, termasuk deteksi overflow dan penolakan segment W+X.
3. Membangun `m11_process_image_plan` yang berisi entry point dan rencana pemetaan hingga `M11_MAX_LOAD_SEGMENTS` (8) segment, sebagai kontrak antara loader dan VMM/PMM.
4. Menolak segment yang berada di luar user virtual region (`0x400000..0x8000000000`) dan menolak entry point di luar region tersebut.
5. Membuktikan bahwa semua validasi adalah fail-closed: jika satu segment invalid, plan dikosongkan dan kode error negatif dikembalikan.
6. Menyediakan host unit test deterministik yang mencakup satu kasus valid dan sembilan kasus negatif tanpa memerlukan boot QEMU.
7. Mengompilasi source loader sebagai C17 freestanding target `x86_64-unknown-none-elf` dan membuktikan tidak ada dependency libc (`nm -u` kosong).
8. Mengintegrasikan loader ke kernel MCSOS dengan smoke test ELF sintetis di kmain yang mencetak plan ke serial log.
9. Mengumpulkan evidence reproducible (host test log, nm, readelf, objdump, sha256sum, QEMU serial log, manifest) ke `evidence/M11`.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan hubungan ELF header, program header, segment, section, dan process image | Bagian 6.1; struct `m11_elf64_ehdr` dan `m11_elf64_phdr` di header |
| Menjelaskan mengapa loader menggunakan program header, bukan section header | Bagian 6.1; pertanyaan analisis nomor 1 |
| Memvalidasi magic, class, endianness, version, type, machine, ukuran header | Fungsi `m11_validate_ident`, `m11_validate_phdr_bounds`; host test bad magic/machine lulus |
| Memvalidasi `PT_LOAD`: offset, filesz, memsz, vaddr, align, flags | Fungsi `m11_validate_load_segment`; host test 7 negative case lulus |
| Mendeteksi integer overflow pada kalkulasi offset dan address | `m11_add_overflow_u64` dipakai di semua kalkulasi batas; host test file range overflow lulus |
| Menolak segment di luar user virtual region | `m11_validate_user_range`; host test segment outside user range lulus |
| Menerapkan kebijakan W^X awal | Penolakan flags `PF_W|PF_X`; host test W+X rejected lulus |
| Menyusun process image plan | Struct `m11_process_image_plan`; host test valid plan fields lulus |
| Menulis host unit test dengan kasus valid dan negatif | `tests/m11/m11_host_test.c`; 11 kasus PASS |
| Mengompilasi source loader sebagai object freestanding | `build/m11_elf_loader.o`; `nm -u` kosong; `readelf -h` ELF64 |
| Mengaudit object dengan nm, readelf, objdump, checksum | `make -f Makefile.m11 m11-audit`; `[M11][PASS] audit lulus` |
| Mengintegrasikan loader ke kernel dan membuktikan log M11 di QEMU | Serial log `[M11] elf: ident ok`, `[M11] elf: plan ok`, `[M11] user image plan ready` |

---

## 5. Peta Milestone MCSOS

| Milestone | Fokus | Status dalam laporan |
|---|---|---|
| M0 | Requirements, governance, baseline arsitektur | `✓ selesai praktikum` |
| M1 | Toolchain reproducible, Git, QEMU, GDB, metadata build | `✓ selesai praktikum` |
| M2 | Boot image, kernel ELF64, early console | `✓ selesai praktikum` |
| M3 | Panic path, linker map, GDB, observability awal | `✓ selesai praktikum` |
| M4 | IDT, exception stub, trap dispatch | `✓ selesai praktikum` |
| M5 | PIC, PIT, hardware IRQ dispatch | `✓ selesai praktikum` |
| M6 | PMM bitmap frame allocator | `✓ selesai praktikum` |
| M7 | VMM awal, page table 4-level | `✓ selesai praktikum` |
| M8 | Kernel heap awal, allocator dinamis | `✓ selesai praktikum` |
| M9 | Kernel thread, scheduler kooperatif | `✓ selesai praktikum` |
| M10 | Syscall ABI, int 0x80 dispatcher | `✓ selesai praktikum` |
| M11 | ELF64 loader, process image plan, user address-space contract | `✓ selesai praktikum` |
| M12 | User address space aktif, ring 3 transisi | `[ ] tidak dibahas` |
| M13 | VFS, file descriptor, ramfs | `[ ] tidak dibahas` |
| M14 | Block layer, device model | `[ ] tidak dibahas` |
| M15 | Networking stack | `[ ] tidak dibahas` |
| M16 | Security model, hardening | `[ ] tidak dibahas` |
| M17 | SMP, scalability | `[ ] tidak dibahas` |
| M18 | Observability, release image, readiness review | `[ ] tidak dibahas` |

Batas cakupan praktikum:

```text
M11 mencakup: validasi ELF64 header dan program header (ident, type,
machine, version, ehsize, phentsize, phbounds), validasi setiap PT_LOAD
(flags W^X, memsz>=filesz, alignment power-of-two, overflow offset+filesz,
file range, user range), penyusunan m11_process_image_plan (entry point,
segment_count, array m11_segment_plan), fungsi m11_validate_user_range,
m11_error_name, host unit test 11 kasus, object freestanding audit (nm -u,
readelf ELF64, objdump, sha256sum), integrasi kernel smoke test, QEMU
smoke test, preflight script, dan evidence manifest.

M11 TIDAK mencakup: alokasi frame fisik untuk segment user, pemetaan page
user via VMM, copy file bytes ke page user, zero-fill BSS, aktivasi CR3
user, transisi ring 3, dynamic linker, PT_INTERP, TLS, auxiliary vector,
ASLR, demand paging, copy-on-write, signal, credential, file-backed mmap,
SMP exec, dan kompatibilitas Linux penuh.
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

```text
M11 membangun fondasi loader program user pada kernel MCSOS 260502.
Lima konsep utama:

1. ELF64 dan Hubungan Header/Program Header/Section:
   Executable and Linkable Format (ELF) menyimpan dua jenis metadata:
   section header (sudut pandang linker/debugger) dan program header
   (sudut pandang loader runtime). Loader harus menggunakan program header
   karena section header dapat distrip dari executable final. Program
   header table berisi deskriptor segment yang menentukan bagian file
   yang harus dimuat ke virtual memory. Untuk M11, hanya PT_LOAD yang
   relevan; PT_INTERP, PT_DYNAMIC, PT_NOTE, PT_PHDR diabaikan.

2. PT_LOAD dan Process Image Plan:
   Setiap entry PT_LOAD memiliki empat nilai ukuran/lokasi kritis:
   p_offset (posisi data di file), p_vaddr (alamat virtual tujuan),
   p_filesz (byte yang disalin dari file), p_memsz (byte di memori).
   Jika p_memsz > p_filesz, selisihnya adalah BSS yang harus di-zero.
   Loader M11 tidak langsung memetakan page; ia hanya membangun rencana
   (m11_process_image_plan) yang kemudian dikonsumsi oleh VMM/PMM.

3. User Virtual Region dan Address Space Contract:
   Kernel harus memastikan semua p_vaddr, p_vaddr+p_memsz, dan e_entry
   berada dalam region user yang telah ditetapkan. Region M11 adalah
   0x400000..0x8000000000. Loader menolak segment yang melampaui batas
   atas (kernel region), batas bawah (null page region), atau overflow.
   Ini mencegah pemetaan diam-diam ke alamat kernel atau HHDM.

4. W^X Baseline:
   Segment yang memiliki flag PF_W (writable) sekaligus PF_X (executable)
   ditolak. Ini adalah baseline keamanan minimal yang mencegah halaman data
   dapat dieksekusi dan halaman kode dapat ditimpa. Enforcement NX bit
   penuh, SMEP, SMAP, dan per-process W^X policy adalah pengayaan masa
   depan.

5. Fail-Closed dan Overflow Safety:
   Setiap kalkulasi batas menggunakan m11_add_overflow_u64 yang mendeteksi
   unsigned 64-bit overflow. Jika satu validasi gagal, m11_zero_plan
   dipanggil untuk membersihkan plan yang sudah diisi sebagian, lalu kode
   error negatif dikembalikan. Ini memastikan caller tidak pernah menerima
   plan parsial yang tidak valid.
```

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| ELF64 format | Struktur `m11_elf64_ehdr` dan `m11_elf64_phdr` harus sesuai ABI | Ukuran struct divalidasi dengan `e_ehsize` dan `e_phentsize` di loader |
| User virtual region | Semua segment harus berada dalam 0x400000..0x8000000000 | `m11_validate_user_range`; host test segment out of range lulus |
| W^X policy | Segment `PF_W|PF_X` ditolak untuk mencegah data executable | Host test W+X rejected lulus; kode `M11_ERR_FLAGS` |
| Integer overflow u64 | `p_offset+p_filesz` dan `p_vaddr+p_memsz` dapat overflow | `m11_add_overflow_u64`; host test file range outside image lulus |
| Power-of-two alignment | `p_align` harus 0, 1, atau power-of-two, dan offset/vaddr kongruen | `m11_is_power_of_two_u64`; host test bad alignment lulus |
| `-mno-red-zone` | Kernel freestanding tidak bergantung red-zone | Flag aktif di `TARGET_CFLAGS` Makefile.m11 |
| ET_EXEC vs ET_DYN | Dua jenis executable yang diterima loader M11 | Validasi `e_type` di `m11_elf64_plan_load` |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk loader; C17 host untuk unit test |
| Runtime | Tanpa hosted libc; tidak ada `memset`, `memcpy`, `printf` di loader |
| ABI | `x86_64-unknown-none-elf`, `-mno-red-zone`, `-ffreestanding`, `-fno-builtin` |
| Compiler flags | `-ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone` |
| No-libc enforcement | `m11_zero_plan` menggunakan loop eksplisit, bukan `memset`; `nm -u` kosong membuktikan |
| Risiko UB | Cast `(const struct m11_elf64_ehdr *)image` valid hanya jika image aligned; diasumsikan caller menyuplai buffer aligned |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Intel SDM Vol.3 | Chapter 5 (Protection), privilege levels, page flags | Fondasi user/kernel isolation dan W^X |
| [2] | System V ABI AMD64 | Chapter 4 (Object Files), ELF header, program header | Format `Elf64_Ehdr`, `Elf64_Phdr`, `PT_LOAD`, flag PF_* |
| [3] | Oracle Linker and Libraries Guide | Chapter 7 (Program Loading and Dynamic Linking) | Semantik PT_LOAD, p_filesz vs p_memsz, zero-fill BSS |
| [4] | Linux Kernel ELF Documentation | `fs/binfmt_elf.c` | Pembanding behavior ELF loader modern |
| [5] | QEMU Documentation | GDB usage, `-no-reboot`, serial | Konfigurasi QEMU smoke test M11 |
| [6] | LLVM/Clang Reference | `-ffreestanding`, `-fno-builtin` | Flags kompilasi freestanding loader |
| [7] | GNU ld/LLD | Linker script, ELF sections | Layout kernel ELF64 yang menampung loader |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu |
| Target ISA | x86_64 |
| Target ABI | `x86_64-unknown-none-elf` |
| Emulator | QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3) |
| Boot path | Limine bootloader (third_party/limine), dilanjutkan dari M2–M10 |
| Build system | GNU Make 4.4.1 |
| Bahasa utama | C17 freestanding |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker | Ubuntu LLD 21.1.8 (compatible with GNU linkers) |
| Host compiler (unit test) | clang (via `$(CC)` tanpa flag freestanding) |
| Binutils | GNU nm, readelf, objdump, sha256sum (GNU Binutils for Ubuntu 2.46) |

### 7.2 Versi Toolchain

```text
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
lld=Ubuntu LLD 21.1.8 (compatible with GNU linkers)
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
make=GNU Make 4.4.1
```

Diambil dari `evidence/M11/manifest.txt`, commit `ad07b27`.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | `Ya` |
| Remote repository | `-` (lokal) |
| Branch | `praktikum-m11-elf-user-loader` |
| Commit hash awal | `f017f4b` (M10 selesai) |
| Commit hash akhir | `ad07b27` (M11 selesai) |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── include/
│   └── mcsos/
│       ├── kmem.h            (M8)
│       ├── syscall.h         (M10)
│       └── user/
│           └── m11_elf_loader.h  (baru M11)
├── kernel/
│   ├── user/
│   │   └── m11_elf_loader.c  (baru M11)
│   └── core/
│       └── kmain.c           (diperbarui M11)
├── tests/
│   └── m11/
│       └── m11_host_test.c   (baru M11)
├── scripts/
│   └── m11_preflight.sh      (baru M11)
├── Makefile.m11              (baru M11)
└── evidence/
    └── M11/                  (baru M11)
        ├── m11_host_test.log
        ├── m11_nm_undefined.txt
        ├── m11_readelf_header.txt
        ├── m11_objdump.txt
        ├── m11_sha256.txt
        ├── m11_preflight.log
        ├── m11_qemu_serial.log
        └── manifest.txt
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis | Alasan | Risiko |
|---|---|---|---|
| `include/mcsos/user/m11_elf_loader.h` | Baru | Definisi publik: 17 kode error, struct ELF64/phdr/user_region/segment_plan/process_image_plan, deklarasi 3 fungsi | Rendah — header-only, tanpa logika |
| `kernel/user/m11_elf_loader.c` | Baru | Implementasi loader: overflow check, validasi ident/phbounds/load_segment, plan builder, m11_error_name | Tinggi — kesalahan validasi batas menyebabkan pembacaan di luar image atau pemetaan salah |
| `tests/m11/m11_host_test.c` | Baru | 11 kasus host unit test: 1 valid + 10 negative | Rendah — hanya dipakai di host test |
| `Makefile.m11` | Baru | Target m11-host-test, m11-freestanding, m11-audit, m11-all, m11-clean | Rendah — terisolasi dari Makefile utama |
| `scripts/m11_preflight.sh` | Baru | Validasi tool, file, host test, dan audit sebelum integrasi kernel | Rendah |
| `kernel/core/kmain.c` | Ubah | Tambah `#include "mcsos/user/m11_elf_loader.h"`, fungsi `m11_elf_smoke_test()`, pemanggilan setelah `m10_syscall_bootstrap()` | Sedang — ELF sintetis yang salah dapat memicu KERNEL_PANIC sebelum idle loop |

### 8.3 Ringkasan Diff

```text
git log --oneline praktikum-m11-elf-user-loader
ad07b27 M11: implement ELF64 user loader, host test, audit, and QEMU integration
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI, int 0x80 dispatcher, and kernel integration
29c0595 (praktikum/m9-kernel-thread-scheduler) M9: implement kernel thread, FIFO scheduler, and x86_64 context switch
```

File baru pada commit M11 (ad07b27):
- `include/mcsos/user/m11_elf_loader.h`
- `kernel/user/m11_elf_loader.c`
- `tests/m11/m11_host_test.c`
- `Makefile.m11`
- `scripts/m11_preflight.sh`
- `evidence/M11/*` (8 file artefak + manifest)

File yang diubah:
- `kernel/core/kmain.c` — tambah include, smoke test function, pemanggilan

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

```text
Kernel M10 memiliki syscall ABI, dispatcher, scheduler, heap, VMM, dan PMM,
tetapi tidak memiliki mekanisme untuk memahami format program user. Tanpa
loader ELF, kernel tidak dapat mengetahui di alamat berapa kode program
harus berada, berapa banyak memori yang dibutuhkan, atau di mana entry
point berada. M11 menyelesaikan masalah ini dengan membangun parser dan
validator ELF64 yang defensif, serta menyusun process image plan sebagai
dokumen formal yang menghubungkan parser ke VMM dan PMM. Dengan M11, kernel
memiliki fondasi untuk menentukan apa yang perlu dipetakan, meskipun
pemetaan aktual dan transisi ring 3 ditunda ke M12.
```

### 9.2 Keputusan Desain

| Keputusan | Alternatif | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Loader hanya membangun plan, tidak langsung memetakan | Langsung panggil VMM di dalam loader | Memisahkan tanggung jawab; loader tidak perlu tahu interface VMM M7 | Caller bertanggung jawab mengeksekusi plan; loader dapat diuji di host tanpa hardware |
| Fail-closed: `m11_zero_plan` saat error | Biarkan plan parsial | Caller tidak boleh menerima plan setengah jadi yang tidak valid | Setiap error menghasilkan plan kosong + kode negatif; tidak ada state ambigu |
| `M11_MAX_LOAD_SEGMENTS = 8` | Nilai lebih besar atau dinamis | Cukup untuk ELF pendidikan; array statik di struct menghindari alokasi heap di loader | Executable dengan lebih dari 8 PT_LOAD ditolak dengan `M11_ERR_SEGCOUNT` |
| Validasi `e_ehsize` dan `e_phentsize` eksak | Terima yang ≥ ukuran struct | ABI menetapkan ukuran tepat; jika berbeda, kemungkinan mismatch toolchain atau corrupt | Loader menolak ELF yang dibuat dengan struct layout berbeda |
| Region user 0x400000..0x8000000000 di host test | Nilai kernel nyata | Host test menggunakan nilai realistis yang umum pada ELF Linux x86_64 | Pada kernel nyata, batas harus diselaraskan dengan layout VMM M7 |
| W^X: tolak PF_W|PF_X bersamaan | Izinkan, biarkan VMM menangani | Fail-closed di layer loader lebih aman; jangan bergantung VMM untuk enforce W^X | Executable dengan segment W+X ditolak sejak awal |
| `m11_zero_plan` pakai loop eksplisit | `memset` | Tidak ada libc di freestanding; loop eksplisit membuktikan tidak ada dependency tersembunyi | Sedikit lebih verbose tapi `nm -u` kosong |

### 9.3 Diagram Alur Loader

```text
m11_elf64_plan_load(image, image_size, region, out_plan):
  1. NULL check → M11_ERR_NULL
  2. m11_zero_plan(out_plan)
  3. image_size < sizeof(ehdr) → M11_ERR_SIZE
  4. m11_validate_ident(eh):
     - magic[0..3] != 0x7f 'E' 'L' 'F' → M11_ERR_MAGIC
     - ident[4] != ELFCLASS64 → M11_ERR_CLASS
     - ident[5] != ELFDATA2LSB → M11_ERR_ENDIAN
     - ident[6] atau e_version != EV_CURRENT → M11_ERR_VERSION
  5. e_type != ET_EXEC && != ET_DYN → M11_ERR_TYPE
  6. e_machine != EM_X86_64 → M11_ERR_MACHINE
  7. e_ehsize != sizeof(ehdr) → M11_ERR_EHSIZE
  8. m11_validate_phdr_bounds(eh, image_size):
     - e_phnum == 0 → M11_ERR_PHBOUNDS
     - e_phentsize != sizeof(phdr) → M11_ERR_PHENTSIZE
     - e_phoff + e_phnum*e_phentsize overflow atau > image_size → M11_ERR_PHBOUNDS
  9. m11_validate_user_range(region, e_entry, 1) → M11_ERR_ENTRY
  10. Loop setiap program header:
      - p_type != PT_LOAD: skip
      - segment_count >= MAX_LOAD_SEGMENTS: zero_plan → M11_ERR_SEGCOUNT
      - m11_validate_load_segment(ph, image_size, region):
        a. (p_flags & ~(R|W|X)) != 0 → M11_ERR_FLAGS
        b. (p_flags & W) && (p_flags & X) → M11_ERR_FLAGS  [W^X]
        c. p_memsz < p_filesz → M11_ERR_SEGBOUNDS
        d. p_align > 1: !power_of_two → M11_ERR_ALIGN
                        vaddr%align != offset%align → M11_ERR_ALIGN
        e. p_offset+p_filesz overflow atau > image_size → M11_ERR_SEGBOUNDS
        f. m11_validate_user_range(region, p_vaddr, p_memsz) → M11_ERR_SEGRANGE
      - Isi out_plan->segments[segment_count++]
  11. segment_count == 0 → M11_ERR_SEGCOUNT
  12. return M11_OK
```

### 9.4 Kontrak Antarmuka

| Fungsi | Precondition | Postcondition | Error path |
|---|---|---|---|
| `m11_elf64_plan_load` | `image != NULL`, `out_plan != NULL`, `image_size >= sizeof(ehdr)` | `out_plan` terisi dengan plan valid jika return `M11_OK`; dizeroi jika return error | 17 kode error negatif; plan selalu di-zero saat error |
| `m11_validate_user_range` | `region.base < region.limit`, `size > 0` | Return `M11_OK` jika `base..base+size` ⊂ `region.base..region.limit` tanpa overflow | `M11_ERR_SEGRANGE` untuk semua kasus tidak valid |
| `m11_error_name` | `code` adalah nilai yang valid atau tidak dikenal | String nama kode error | `"M11_ERR_UNKNOWN"` untuk kode tidak dikenal |

### 9.5 Invariants

| Kode | Invariant | Implementasi | Bukti |
|---|---|---|---|
| I1 | Magic, class, endian, version valid | `m11_validate_ident` | Host test bad magic lulus `M11_ERR_MAGIC` |
| I2 | Machine hanya `EM_X86_64` | Cek `e_machine == 62` | Host test bad machine lulus `M11_ERR_MACHINE` |
| I3 | `e_ehsize == sizeof(m11_elf64_ehdr)` | Cek eksak | Review kode; ELF sintetis di smoke test mengisi nilai benar |
| I4 | PH table bounded: `e_phoff + e_phnum*e_phentsize` tidak overflow dan tidak keluar image | `m11_validate_phdr_bounds` | Host test bounds |
| I5 | `p_offset + p_filesz` tidak overflow dan ≤ image_size | `m11_add_overflow_u64` + cek batas | Host test file range outside image lulus |
| I6 | `p_memsz >= p_filesz` | Cek `ph->p_memsz < ph->p_filesz` | Host test memsz below filesz lulus |
| I7 | `p_vaddr..p_vaddr+p_memsz` berada dalam user region | `m11_validate_user_range` | Host test segment outside user range lulus |
| I8 | `e_entry` berada dalam user region | `m11_validate_user_range(region, e_entry, 1)` | Host test entry outside user range lulus |
| I9 | `p_align` adalah 0/1 atau power-of-two dan kongruen | `m11_is_power_of_two_u64` + kongruensi | Host test bad alignment lulus |
| I10 | Segment tidak boleh W+X | `(flags & PF_W) && (flags & PF_X) → ERR_FLAGS` | Host test W+X rejected lulus |
| I11 | Fail-closed: jika satu segment invalid, plan dizeroi | `m11_zero_plan(out_plan)` sebelum return error | Review kode `m11_elf64_plan_load`; semua negative test tidak menghasilkan plan parsial |

### 9.6 Security Boundary M11

| Risiko | Level | Mitigasi pada M11 | Status |
|---|---|---|---|
| Integer overflow `offset + filesz` | Tinggi | `m11_add_overflow_u64` di semua kalkulasi batas | Terverifikasi host test |
| Pemetaan ke kernel region | Tinggi | `m11_validate_user_range` menolak alamat di luar 0x400000..0x8000000000 | Terverifikasi host test |
| Segment W+X (executable data) | Tinggi | Penolakan eksplisit `PF_W|PF_X` dengan `M11_ERR_FLAGS` | Terverifikasi host test |
| Plan parsial pada error parsial | Tinggi | `m11_zero_plan` dipanggil sebelum setiap `return` error setelah plan mulai diisi | Review kode |
| `p_memsz < p_filesz` | Sedang | Penolakan eksplisit `M11_ERR_SEGBOUNDS` | Terverifikasi host test |
| Alignment congruence false | Sedang | Cek `(p_vaddr % p_align) != (p_offset % p_align)` | Terverifikasi host test |
| Entry point di kernel region | Sedang | Validasi `e_entry` via `m11_validate_user_range(region, e_entry, 1)` | Terverifikasi host test |
| Dependency libc di loader | Sedang | `-ffreestanding -fno-builtin`; `nm -u` kosong | Terverifikasi `evidence/M11/m11_nm_undefined.txt` kosong |
| Lebih dari 8 PT_LOAD | Rendah | `M11_ERR_SEGCOUNT` setelah `segment_count >= MAX_LOAD_SEGMENTS` | Documented behavior |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Buat Branch M11 dan Direktori

```bash
git switch -c praktikum-m11-elf-user-loader
mkdir -p kernel/user include/mcsos/user tests/m11 scripts build
git branch --show-current
```

Output: `praktikum-m11-elf-user-loader`

### Langkah 2 — Buat Header `include/mcsos/user/m11_elf_loader.h`

Mendefinisikan 17 kode error `M11_ERR_*`, 5 struct (ehdr, phdr, user_region, segment_plan, process_image_plan), dan 3 deklarasi fungsi publik. Tidak ada include libc selain `<stddef.h>` dan `<stdint.h>`.

Verifikasi:

```text
grep -n "M11_ERR|m11_elf64_plan_load|m11_process_image_plan" include/mcsos/user/m11_elf_loader.h
→ 17 kode error, struct process_image_plan, deklarasi fungsi ada
```

### Langkah 3 — Buat Implementasi `kernel/user/m11_elf_loader.c`

Mengimplementasikan 6 fungsi: `m11_add_overflow_u64`, `m11_is_power_of_two_u64`, `m11_zero_plan`, `m11_validate_ident`, `m11_validate_phdr_bounds`, `m11_validate_load_segment`, `m11_validate_user_range`, `m11_elf64_plan_load`, `m11_error_name`.

Verifikasi:

```text
grep -n "^int m11_|^const char|^static int m11_" kernel/user/m11_elf_loader.c
→ 9 fungsi ditemukan
```

### Langkah 4 — Buat Host Unit Test `tests/m11/m11_host_test.c`

10 kasus negatif + 1 kasus valid: bad magic, bad machine, entry outside user range, memsz below filesz, file range outside image, bad alignment, segment outside user range, W+X rejected, null image pointer, dan valid plan fields.

### Langkah 5 — Buat Makefile.m11

Target: `m11-host-test`, `m11-freestanding`, `m11-audit`, `m11-all`, `m11-clean`. Bug ditemukan: separator tab vs `>` — diperbaiki via Python.

### Langkah 6 — Jalankan `make -f Makefile.m11 CC=clang m11-all`

```text
11 kasus PASS + [M11][PASS] audit lulus
```

### Langkah 7 — Buat dan Jalankan Preflight

```bash
./scripts/m11_preflight.sh | tee build/m11_preflight.log
```

Output: `[PASS] M11 preflight selesai.`

### Langkah 8 — Integrasi Kernel di `kmain.c`

Menambahkan `#include "mcsos/user/m11_elf_loader.h"` dan fungsi `m11_elf_smoke_test()` yang membangun ELF sintetis 12 KiB di `.bss`, memanggil `m11_elf64_plan_load`, dan mencetak plan ke serial log. Pemanggilan setelah `m10_syscall_bootstrap()`. Patch via Python.

### Langkah 9 — Build Kernel

```bash
make clean && make build
```

Output: 17 file dikompilasi termasuk `kernel/user/m11_elf_loader.o`. Link berhasil.

### Langkah 10 — QEMU Smoke Test

```bash
tools/scripts/make_iso.sh 2>/dev/null
qemu-system-x86_64 -machine q35 -cpu max -m 256M \
    -cdrom build/mcsos.iso -boot d \
    -serial file:build/m11_qemu_serial.log \
    -display none -no-reboot -no-shutdown || true
```

Log M11 di serial:

```text
[M11] elf: ident ok
[M11] elf: plan ok
[M11] user image plan ready
```

### Langkah 11 — Kumpulkan Evidence dan Commit

```bash
mkdir -p evidence/M11
# copy artefak, buat manifest
git add include/mcsos/user/m11_elf_loader.h kernel/user/m11_elf_loader.c \
        tests/m11/m11_host_test.c Makefile.m11 scripts/m11_preflight.sh \
        kernel/core/kmain.c evidence/M11
git commit -m "M11: implement ELF64 user loader, host test, audit, and QEMU integration"
# → commit ad07b27
```

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| C1 Preflight | `./scripts/m11_preflight.sh` | Tool dan file M0–M10 terdeteksi; host test dan audit lulus | `PASS` |
| C2 Host test | `make -f Makefile.m11 CC=clang m11-host-test` | `M11 host tests passed.` | `PASS` |
| C3 Freestanding compile | `make -f Makefile.m11 CC=clang m11-freestanding` | `build/m11_elf_loader.o` ada | `PASS` |
| C4 Audit object | `make -f Makefile.m11 CC=clang m11-audit` | nm kosong, ELF64, simbol ada, sha256 tersimpan | `PASS` |
| C5 Kernel build | `make clean && make build` | `build/kernel.elf` berhasil, `m11_elf_loader.o` masuk | `PASS` |
| C6 QEMU smoke test | QEMU command M11 | `[M11] user image plan ready` di serial log | `PASS` |
| C7 Git evidence | `git log --oneline -3` | Commit `ad07b27` ada | `PASS` |

---

## 12. Perintah Uji dan Validasi

### 12.1 Host Unit Test

```bash
make -f Makefile.m11 CC=clang m11-host-test
```

Hasil:

```text
PASS valid ELF64 image: M11_OK
PASS valid plan fields: entry=0x401000 segments=2
PASS bad magic: M11_ERR_MAGIC
PASS bad machine: M11_ERR_MACHINE
PASS entry outside user range: M11_ERR_ENTRY
PASS memsz below filesz: M11_ERR_SEGBOUNDS
PASS file range outside image: M11_ERR_SEGBOUNDS
PASS bad alignment: M11_ERR_ALIGN
PASS segment outside user range: M11_ERR_SEGRANGE
PASS W+X segment rejected: M11_ERR_FLAGS
PASS null image pointer: M11_ERR_NULL
M11 host tests passed.
```

Status: `PASS`

### 12.2 Audit Object Freestanding

```bash
make -f Makefile.m11 CC=clang m11-audit
cat build/m11_nm_undefined.txt    # harus kosong
cat build/m11_sha256.txt
grep "ELF64" build/m11_readelf_header.txt
grep "m11_elf64_plan_load" build/m11_objdump.txt | head -3
```

Hasil:

```text
[nm_undefined.txt — kosong]

[readelf_header.txt ringkas]
Class:   ELF64
Machine: Advanced Micro Devices X86-64
Type:    REL (Relocatable file)

[objdump — simbol loader]
0000000000000000 <m11_elf64_plan_load>: (ada di disassembly)

[sha256.txt]
<hash> build/m11_elf_loader.o
<hash> kernel/user/m11_elf_loader.c
<hash> include/mcsos/user/m11_elf_loader.h
<hash> tests/m11/m11_host_test.c

[M11][PASS] audit lulus
```

Status: `PASS`

### 12.3 Build Kernel

```bash
make clean && make build
```

Hasil: 17 file dikompilasi. `kernel/user/m11_elf_loader.o` masuk ke linker command. Link berhasil tanpa error/warning.

Status: `PASS`

### 12.4 QEMU Smoke Test

```bash
cat build/m11_qemu_serial.log | grep -E "M11|M10|M9|M8|kernel entered" | head -20
```

Hasil:

```text
MCSOS 260502 M4 kernel entered
[M6] PMM initialized
[M6] sample alloc/free roundtrip ok
[M7] VMM core initialized
[M7] VMM map/query/unmap smoke test passed
[M7] ready for QEMU smoke test and GDB audit
[M8] kmem initialized
[M8] heap probe alloc/free roundtrip ok
[M9] scheduler initialized
[M10] syscall init
[M10] syscall ping ok
[M10] syscall get_ticks ok
[M10] syscall smoke done
[M10] int 0x80 gate installed
[M10] int 0x80 ping ok
[M11] elf: ident ok
[M11] elf: plan ok
[M11] user image plan ready
[M9] thread A tick
[M9] thread B tick
```

Status: `PASS`

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan

| No. | Uji | Expected | Actual | Status | Evidence |
|---|---|---|---|---|---|
| 1 | Valid ELF64 image | `M11_OK` | `M11_OK` | `PASS` | Host test |
| 2 | Valid plan fields | `entry=0x401000, segments=2` | Sesuai | `PASS` | Host test |
| 3 | Bad magic (byte 0 = 0) | `M11_ERR_MAGIC` | `M11_ERR_MAGIC` | `PASS` | Host test |
| 4 | Bad machine (e_machine=3, EM_386) | `M11_ERR_MACHINE` | `M11_ERR_MACHINE` | `PASS` | Host test |
| 5 | Entry outside user range (0x1000) | `M11_ERR_ENTRY` | `M11_ERR_ENTRY` | `PASS` | Host test |
| 6 | memsz (4) < filesz (16) | `M11_ERR_SEGBOUNDS` | `M11_ERR_SEGBOUNDS` | `PASS` | Host test |
| 7 | File range outside image (p_offset=0x3000 > image) | `M11_ERR_SEGBOUNDS` | `M11_ERR_SEGBOUNDS` | `PASS` | Host test |
| 8 | Bad alignment (p_align=24, bukan power-of-two) | `M11_ERR_ALIGN` | `M11_ERR_ALIGN` | `PASS` | Host test |
| 9 | Segment outside user range (p_vaddr=0x800000000000) | `M11_ERR_SEGRANGE` | `M11_ERR_SEGRANGE` | `PASS` | Host test |
| 10 | W+X segment (PF_R\|PF_W\|PF_X) | `M11_ERR_FLAGS` | `M11_ERR_FLAGS` | `PASS` | Host test |
| 11 | Null image pointer | `M11_ERR_NULL` | `M11_ERR_NULL` | `PASS` | Host test |
| 12 | `nm -u` freestanding object | Kosong | Kosong | `PASS` | `m11_nm_undefined.txt` |
| 13 | `readelf -h` ELF64 | ELF64 x86-64 REL | ELF64 x86-64 REL | `PASS` | `m11_readelf_header.txt` |
| 14 | `objdump` memuat simbol loader | `m11_elf64_plan_load` ada | Ada | `PASS` | `m11_objdump.txt` |
| 15 | Checksum artefak tersimpan | sha256sum 4 file | 4 hash tersimpan | `PASS` | `m11_sha256.txt` |
| 16 | Build kernel dengan loader | Berhasil tanpa error | Berhasil | `PASS` | `make clean && make build` |
| 17 | QEMU: `[M11] elf: ident ok` | Ada | Ada | `PASS` | `m11_qemu_serial.log` |
| 18 | QEMU: `[M11] user image plan ready` | Ada | Ada | `PASS` | `m11_qemu_serial.log` |
| 19 | M9 thread A/B masih berjalan setelah M11 | `[M9] thread A/B tick` muncul | Muncul | `PASS` | `m11_qemu_serial.log` |
| 20 | Preflight script | `[PASS] M11 preflight selesai.` | Lulus | `PASS` | `m11_preflight.log` |

### 13.2 Log Serial QEMU Penuh (Bagian Penting)

```text
MCSOS 260502 M4 kernel entered
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M6] PMM initialized
[M6] sample alloc/free roundtrip ok
[M7] VMM core initialized
[M7] VMM map/query/unmap smoke test passed
[M7] ready for QEMU smoke test and GDB audit
[M8] kmem initialized
[M8] heap probe alloc/free roundtrip ok
[M9] scheduler initialized
[M10] syscall init
[M10] syscall ping ok
[M10] syscall get_ticks ok
[M10] syscall smoke done
[M10] int 0x80 gate installed
[M10] int 0x80 ping ok
[M11] elf: ident ok
elf_phnum=0x0000000000000002
elf_entry=0x0000000000401000
seg0_vaddr=0x0000000000400000
seg0_flags=0x0000000000000005
seg1_vaddr=0x0000000000401000
seg1_flags=0x0000000000000006
[M11] elf: plan ok
[M11] user image plan ready
[M9] thread A tick
[M9] thread B tick
... [berlanjut]
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| `m11_host_test.log` | `evidence/M11/m11_host_test.log` | Output 11 kasus host unit test |
| `m11_nm_undefined.txt` | `evidence/M11/m11_nm_undefined.txt` | Bukti `nm -u` kosong |
| `m11_readelf_header.txt` | `evidence/M11/m11_readelf_header.txt` | Bukti ELF64 x86-64 REL freestanding |
| `m11_objdump.txt` | `evidence/M11/m11_objdump.txt` | Disassembly dengan simbol loader |
| `m11_sha256.txt` | `evidence/M11/m11_sha256.txt` | Checksum 4 artefak |
| `m11_preflight.log` | `evidence/M11/m11_preflight.log` | Output preflight script |
| `m11_qemu_serial.log` | `evidence/M11/m11_qemu_serial.log` | Serial log QEMU penuh M4→M11 |
| `manifest.txt` | `evidence/M11/manifest.txt` | Manifest toolchain dan daftar file |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

```text
Host unit test berhasil karena implementasi loader mengikuti kontrak
panduan secara tepat: validasi dilakukan dalam urutan yang deterministik
(ident → type → machine → ehsize → phbounds → entry → loop PT_LOAD),
dan setiap validasi mengembalikan kode error yang spesifik. Helper
m11_add_overflow_u64 memastikan semua kalkulasi batas aman dari overflow.

Audit freestanding lulus (nm -u kosong) karena loader tidak menggunakan
fungsi libc apapun. m11_zero_plan menggunakan loop manual; tidak ada
printf, memset, atau fungsi runtime. Flag -ffreestanding -fno-builtin
memastikan compiler tidak menyisipkan builtin tersembunyi.

Bug Makefile.m11 (separator tab vs '>') terdeteksi langsung dari pesan
error Make: "missing separator". Penyebab: file ditulis dengan tab
standar sementara Makefile utama memakai .RECIPEPREFIX := >, dan dua
Makefile tidak berbagi konfigurasi. Solusi: Python replace semua baris
bertab dengan '>'.

Build kernel berhasil karena SRC_C := $(shell find kernel src -name '*.c')
sudah mencakup kernel/user/ secara otomatis tanpa perubahan Makefile.

QEMU smoke test berhasil karena ELF sintetis di .bss (12 KiB di-zero
oleh bootloader) sudah dalam kondisi valid setelah field diisi manual.
Log menunjukkan seg0_flags=0x5 (R|X) dan seg1_flags=0x6 (R|W) sesuai
yang diisi di m11_elf_smoke_test. M9 thread A/B masih berjalan setelah
M11 membuktikan loader tidak merusak scheduler atau IDT.
```

### 14.2 Analisis Kegagalan dan Bug yang Ditemukan

**Bug 1: Separator Makefile.m11 (tab vs >)**

```text
Gejala: "Makefile.m11:24: *** missing separator. Stop."
Penyebab: Makefile.m11 ditulis dengan tab sebagai separator recipe,
  sedangkan Makefile utama menggunakan .RECIPEPREFIX := > sehingga tab
  tidak dikenali sebagai separator recipe oleh Make. Dua file Makefile
  berbeda tidak berbagi RECIPEPREFIX.
Perbaikan: Python string replace:
  (1) tambahkan '.RECIPEPREFIX := >' sebelum .PHONY
  (2) ganti semua baris yang diawali tab dengan '>'
Deteksi: pesan error Make eksplisit.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai? | Penjelasan |
|---|---|---|---|
| Loader menggunakan program header, bukan section header | Loop hanya pada `e_phnum` entries di program header table | Sesuai | Section header (`e_shoff`, `e_shnum`, `e_shstrndx`) ada di struct tapi tidak dipakai |
| `p_memsz >= p_filesz` untuk BSS | Penolakan jika `p_memsz < p_filesz` dengan `M11_ERR_SEGBOUNDS` | Sesuai | Terverifikasi host test |
| Overflow `offset + filesz` | `m11_add_overflow_u64` sebelum cek batas | Sesuai | Terverifikasi host test file range |
| W^X policy | Penolakan `PF_W|PF_X` dengan `M11_ERR_FLAGS` | Sesuai | Terverifikasi host test |
| Alignment power-of-two dan kongruensi | `m11_is_power_of_two_u64` + `(vaddr%align) == (offset%align)` | Sesuai | Terverifikasi host test bad alignment |
| Fail-closed plan | `m11_zero_plan` sebelum setiap `return` error setelah plan diisi | Sesuai | Review kode; tidak ada plan parsial pada error |
| `e_entry` dalam user region | `m11_validate_user_range(region, e_entry, 1)` | Sesuai | Terverifikasi host test entry outside range |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Perbaikan |
|---|---|---|---|
| Makefile.m11 separator error | `missing separator. Stop.` | Tab vs `>` di file Makefile | Python replace: tambah RECIPEPREFIX, ganti tab dengan `>` |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Gejala | Penyebab | Mitigasi |
|---|---|---|---|
| `M11_ERR_MAGIC` pada ELF valid | Host test gagal kasus valid | Image corrupt atau pointer salah | Dump 16 byte pertama; verifikasi `0x7f 45 4c 46` |
| `M11_ERR_PHBOUNDS` | PH table keluar image | `e_phoff` atau `e_phnum` salah | Cetak ukuran image, e_phoff, e_phentsize, e_phnum |
| `M11_ERR_ALIGN` pada ELF valid | Alignment congruence salah | `p_vaddr % p_align != p_offset % p_align` | Verifikasi dengan linker script; cek output ld |
| Undefined symbol di freestanding object | `nm -u` tidak kosong | Penggunaan `memset`/`memcpy`/`printf` tersembunyi | Hapus semua penggunaan libc; gunakan loop eksplisit |
| `readelf` bukan ELF64 | Compiler target salah | `CC=clang` tanpa `--target=x86_64-unknown-none-elf` | Gunakan `CC=clang` dengan TARGET_CFLAGS eksplisit |
| QEMU panic saat `m11_elf_smoke_test` | Kernel panic sebelum log M11 | ELF sintetis tidak valid atau field tidak diisi benar | Verifikasi setiap field di `m11_elf_smoke_test`; gunakan GDB breakpoint |
| M9 thread tidak berjalan setelah M11 | Scheduler hang atau panic | m11_elf_smoke_test merusak stack atau register | Periksa apakah smoke test mengalokasikan memory statis di .bss (aman) |

### 15.3 Triage yang Dilakukan

```text
Bug separator terdeteksi dari pesan error Make yang eksplisit. Diagnosis
dilakukan dengan membandingkan Makefile.m11 (tab) dengan Makefile utama
(.RECIPEPREFIX := >). Perbaikan via Python lebih aman daripada edit manual
karena tidak berisiko salah jumlah tab.

Tidak ada bug logika loader yang ditemukan. Semua 11 kasus host test lulus
tanpa modifikasi source. Ini konsisten dengan sumber panduan yang menyatakan
source sudah divalidasi di lingkungan penyusun.
```

### 15.4 Rancangan GDB untuk Debugging Loader

```text
Jika m11_elf_smoke_test gagal atau KERNEL_PANIC dipicu:

Terminal 1 (QEMU):
  qemu-system-x86_64 -machine q35 -m 256M -serial stdio \
    -no-reboot -no-shutdown -s -S -cdrom build/mcsos.iso

Terminal 2 (GDB):
  gdb build/kernel.elf
  (gdb) target remote localhost:1234
  (gdb) break m11_elf_smoke_test
  (gdb) break m11_elf64_plan_load
  (gdb) continue
  (gdb) print *eh           # inspeksi ELF header
  (gdb) print plan          # inspeksi plan hasil
  (gdb) info locals         # variabel lokal fungsi

Jika KERNEL_PANIC dengan kode error M11, nilai kode dapat dicocokkan
dengan m11_error_name untuk mengetahui validasi mana yang gagal.
```

---

## 16. Prosedur Rollback

| Skenario | Perintah | Status |
|---|---|---|
| Kembali ke baseline M10 | `git switch praktikum/m10-syscall-abi` | Belum diuji aktual; branch M10 masih ada di `f017f4b` |
| Nonaktifkan smoke test saja | Hapus pemanggilan `m11_elf_smoke_test()` dari `kmain.c` | Paling aman; loader tetap ada untuk host test |
| Hapus semua file M11 | `git restore kernel/core/kmain.c; git rm kernel/user/m11_elf_loader.c include/mcsos/user/m11_elf_loader.h tests/m11/m11_host_test.c Makefile.m11 scripts/m11_preflight.sh` | Belum diuji aktual |
| Bersihkan artefak | `make -f Makefile.m11 m11-clean` | Teruji — menghapus `build/m11_*` |

Catatan rollback:

```text
Branch M11 (praktikum-m11-elf-user-loader) terpisah dari branch M10
(praktikum/m10-syscall-abi). Rollback ke M10 dapat dilakukan dengan git
switch tanpa risiko kehilangan data M11. Rollback belum diuji aktual
karena M11 berhasil tanpa perlu rollback.
```

---

## 17. Keamanan dan Reliability

### 17.1 Keamanan

| Risiko | Mitigasi pada M11 | Known limitation |
|---|---|---|
| Overflow kalkulasi batas | `m11_add_overflow_u64` di semua kalkulasi | Tidak ada |
| Pemetaan ke kernel region | `m11_validate_user_range` wajib | User region masih hardcoded di smoke test; integrasi VMM nyata harus memakai batas dari layout M7 |
| W+X segment | Penolakan eksplisit `M11_ERR_FLAGS` | Enforcement NX bit di page table adalah M12+ |
| Plan parsial pada error | `m11_zero_plan` sebelum return error | Tidak ada |
| Dependency libc | `-ffreestanding -fno-builtin`; `nm -u` kosong | Tidak ada |
| Flag tidak dikenal di p_flags | `(flags & ~(R|W|X)) != 0 → ERR_FLAGS` | Tidak ada |

### 17.2 Reliability

| Aspek | Status M11 | Catatan |
|---|---|---|
| Fail-closed | ✅ `m11_zero_plan` saat error | Terverifikasi review kode |
| Overflow safety | ✅ `m11_add_overflow_u64` | Terverifikasi host test |
| W^X baseline | ✅ `M11_ERR_FLAGS` | Terverifikasi host test |
| Freestanding | ✅ `nm -u` kosong | Terverifikasi evidence |
| Build `-Werror` | ✅ Lulus | Semua warning dianggap error |
| M9/M10 tidak regresi | ✅ Thread A/B masih berjalan | Terverifikasi QEMU log |

---

## 18. Pembagian Kerja

Tidak berlaku — praktikum dikerjakan secara individu.

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Repository dapat dibangun dari clean checkout | `PASS` | `make clean && make build` berhasil |
| Hasil M0–M10 sudah diperiksa | `PASS` | `m11_preflight.log` menunjukkan semua marker terdeteksi |
| Header, implementasi, host test, dan Makefile tersedia | `PASS` | 4 file baru terkonfirmasi |
| Host unit test M11 lulus | `PASS` | `M11 host tests passed.` |
| Source loader dikompilasi sebagai C17 freestanding x86_64 | `PASS` | `build/m11_elf_loader.o` ada |
| `nm -u` untuk object loader kosong | `PASS` | `evidence/M11/m11_nm_undefined.txt` kosong |
| `readelf -h` menunjukkan ELF64 object | `PASS` | `evidence/M11/m11_readelf_header.txt` |
| `objdump` memuat simbol loader utama | `PASS` | `m11_elf64_plan_load` ada di disassembly |
| Checksum artefak disimpan | `PASS` | `evidence/M11/m11_sha256.txt` |
| Integrasi kernel tidak merusak panic path dan serial log | `PASS` | M9 thread A/B masih berjalan setelah M11 |
| QEMU smoke test dijalankan | `PASS` | `[M11] user image plan ready` di log |
| Semua perubahan dikomit | `PASS` | Commit `ad07b27` |
| Laporan menyertakan bukti lengkap | `PASS` | Laporan ini |

| Kriteria pengayaan | Status | Catatan |
|---|---|---|
| Negative test `e_phentsize` salah | `NA` | Implisit melalui penolakan ukuran tidak cocok |
| Negative test W+X | ✅ | Ditambahkan sebagai test 10 |
| Sorting/check overlap segment | `NA` | Belum diimplementasikan |
| Loader dari initrd nyata | `NA` | Smoke test menggunakan ELF sintetis di .bss |

---

## 20. Readiness Review

| Status | Definisi | Pilihan |
|---|---|---|
| Belum siap uji | Build/test belum stabil | `[ ]` |
| Siap uji QEMU | Build bersih, QEMU berjalan, log tersedia | `✓ ` |
| Siap demonstrasi praktikum | Siap ditunjukkan dengan bukti uji, failure mode, rollback | `[ ]` |
| Kandidat siap pakai terbatas | Setelah test, security review, dokumentasi lengkap | `[ ]` |

Alasan readiness:

```text
Build bersih untuk semua target (host test, freestanding object, kernel.elf).
Host unit test lulus 11 kasus mencakup happy path dan 10 negative case.
nm -u kosong. ELF64 x86-64 REL terverifikasi. Simbol m11_elf64_plan_load ada
di disassembly. Checksum 4 artefak tersimpan. Build kernel berhasil dengan
loader terintegrasi. QEMU smoke test menampilkan [M11] elf: ident ok, plan ok,
user image plan ready. M9/M10 tidak regresi setelah M11.

Status siap demonstrasi belum diklaim karena: (1) rollback belum diuji aktual;
(2) GDB session formal pada loader belum dilakukan; (3) user region masih
hardcoded, belum terintegrasi dengan layout VMM M7 nyata; (4) pemetaan page
user, copy file bytes, zero-fill BSS, dan transisi ring 3 belum ada (M12+).
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | User region hardcoded 0x400000..0x8000000000 | Tidak sinkron dengan layout VMM M7 nyata | Cukup untuk smoke test sintetis | M12 saat VMM user mapping diintegrasikan |
| 2 | Tidak ada alokasi frame dan pemetaan page user | Loader hanya menghasilkan plan, tidak mengeksekusi | Scope M11 memang hanya planning | M12 |
| 3 | ELF sintetis di `.bss`, bukan dari storage nyata | Tidak menguji jalur pembacaan file | Cukup untuk uji parser | M11 pengayaan: loader dari initrd |
| 4 | Rollback belum diuji aktual | Risiko M10 tidak berjalan setelah rollback | Branch terpisah | Sebelum demonstrasi |
| 5 | GDB session formal belum dilakukan | Tidak ada bukti GDB M11 | Rancangan ada di bagian 15.4 | Sebelum demonstrasi |
| 6 | W^X hanya di layer loader, belum di page table | NX bit page table belum ditegakkan | Enforcement NX di M12+ | M12 |

Keputusan akhir:

```text
Berdasarkan bukti 11 kasus host test PASS, nm -u kosong, ELF64 terverifikasi,
simbol loader ada di disassembly, checksum tersimpan, QEMU log [M11] user
image plan ready, dan M9/M10 tidak regresi, hasil praktikum M11 layak disebut
siap uji QEMU untuk ELF64 loader awal dan process-image planning single-core.
M11 tidak mengklaim siap demonstrasi atau siap produksi karena rollback belum
diuji, GDB formal belum dilakukan, dan pemetaan user page/transisi ring 3
belum ada.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---:|---:|---|---:|
| Kebenaran fungsional | 30 | Loader memvalidasi semua invariant I1–I11, plan benar pada kasus valid, fail-closed pada semua negative case | `29` |
| Kualitas desain dan invariants | 20 | 11 invariant terdokumentasi, fail-closed eksplisit, tidak ada dependency libc, interface integrasi jelas | `19` |
| Pengujian dan bukti | 20 | 11 kasus host test, nm-u, readelf, objdump, sha256sum, QEMU smoke test, preflight, manifest | `20` |
| Debugging dan failure analysis | 10 | Bug Makefile separator ditemukan dan diperbaiki, failure modes diantisipasi, rancangan GDB ada | `9` |
| Keamanan dan robustness | 10 | Overflow check, user range, W^X, fail-closed, malformed ELF handling, freestanding | `9` |
| Dokumentasi dan laporan | 10 | Laporan rapi, reproducible, referensi IEEE, commit hash, evidence lengkap | `9` |
| **Total** | **100** | | `95` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

```text
Seluruh komponen wajib M11 berhasil:
- include/mcsos/user/m11_elf_loader.h: 17 kode error, 5 struct, 3 fungsi publik.
- kernel/user/m11_elf_loader.c: validasi defensif lengkap dengan overflow
  check, W^X, user range, alignment, fail-closed, m11_error_name.
- tests/m11/m11_host_test.c: 11 kasus PASS (1 valid + 10 negatif).
- make -f Makefile.m11 m11-all: host test PASS, nm-u kosong, ELF64 REL,
  simbol loader ada, sha256sum tersimpan, [M11][PASS] audit lulus.
- scripts/m11_preflight.sh: [PASS] M11 preflight selesai.
- Build kernel: kernel/user/m11_elf_loader.o masuk ke kernel.elf tanpa error.
- QEMU smoke test: [M11] elf: ident ok, elf: plan ok, user image plan ready.
- M9 thread A/B masih berjalan setelah M11 — tidak ada regresi.
- 1 bug Makefile separator ditemukan dan diperbaiki.
- 1 commit bersih ad07b27 di branch praktikum-m11-elf-user-loader.
- 8 artefak evidence + manifest di evidence/M11/.
```

### 22.2 Yang Belum Berhasil

```text
- Alokasi frame PMM dan pemetaan page user via VMM belum dilakukan —
  sesuai scope M11 yang hanya menghasilkan plan.
- User region masih hardcoded, belum terintegrasi dengan layout VMM M7.
- Loader dari storage nyata (initrd) belum diuji; smoke test menggunakan
  ELF sintetis di .bss.
- GDB session formal pada loader belum dilakukan.
- Rollback belum diuji aktual.
- Transisi ring 3 tidak ada — ini M12+.
```

### 22.3 Rencana Perbaikan

```text
Sebelum demonstrasi M11:
1. Uji rollback aktual ke commit f017f4b (M10) dan verifikasi M10 masih jalan.
2. Jalankan GDB session: break m11_elf_smoke_test, inspeksi plan setelah
   m11_elf64_plan_load, verifikasi entry dan segment_count.

Untuk M12+:
1. Integrasikan m11_process_image_plan dengan VMM M7: alokasi frame PMM
   per segment, map ke user virtual address, copy file bytes, zero-fill BSS.
2. Implementasikan user region dinamis yang sinkron dengan layout VMM M7
   (bukan hardcoded 0x400000..0x8000000000).
3. Setelah mapping lengkap, implementasikan transisi ring 3: setup GDT user
   segment, TSS, iretq ke user entry point.
4. Tambahkan negative test e_phentsize salah dan segment overlap check.
5. Implementasikan loader dari initrd/ramfs untuk menggantikan ELF sintetis.
```

---

## 23. Lampiran

### Lampiran A — Commit Log

```text
ad07b27 (HEAD -> praktikum-m11-elf-user-loader) M11: implement ELF64 user loader, host test, audit, and QEMU integration
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI, int 0x80 dispatcher, and kernel integration
29c0595 (praktikum/m9-kernel-thread-scheduler) M9: implement kernel thread, FIFO scheduler, and x86_64 context switch
d9fadc3 (praktikum-m8-kernel-heap) M8: implement kernel heap allocator, host test, audit, and QEMU integration
59ebb47 (m7-vmm-core) M7: add evidence manifest, QEMU log, and audit artifacts
```

### Lampiran B — Diff Ringkas

```text
File baru pada commit M11 (ad07b27):
  include/mcsos/user/m11_elf_loader.h — 86 baris header API
  kernel/user/m11_elf_loader.c        — 163 baris implementasi loader
  tests/m11/m11_host_test.c           — 155 baris host unit test
  Makefile.m11                        — 56 baris target M11
  scripts/m11_preflight.sh            — 35 baris preflight
  evidence/M11/                       — 8 file artefak + manifest

File yang diubah:
  kernel/core/kmain.c — +65 baris: include m11_elf_loader.h,
                        fungsi m11_elf_smoke_test(), pemanggilan setelah
                        m10_syscall_bootstrap()
```

### Lampiran C — Log Build (Ringkas)

```text
[make clean && make build — 17 file dikompilasi]
clang ... -c kernel/arch/x86_64/idt.c
clang ... -c kernel/arch/x86_64/pic.c
clang ... -c kernel/arch/x86_64/pit.c
clang ... -c kernel/core/kmain.c
clang ... -c kernel/core/log.c
clang ... -c kernel/core/panic.c
clang ... -c kernel/core/serial.c
clang ... -c kernel/core/trap.c
clang ... -c kernel/lib/memory.c
clang ... -c kernel/mcsos_thread.c
clang ... -c kernel/mm/kmem.c
clang ... -c kernel/syscall/syscall.c
clang ... -c kernel/user/m11_elf_loader.c    ← baru M11
clang ... -c src/pmm.c
clang ... -c src/vmm.c
clang ... -c kernel/arch/x86_64/isr.S
clang ... -c kernel/syscall/syscall_entry.S
clang ... -c arch/x86_64/context_switch.S
ld.lld -nostdlib -static ... -o build/kernel.elf [18 object files]
[Tidak ada error/warning]
```

### Lampiran D — Log Host Unit Test

```text
PASS valid ELF64 image: M11_OK
PASS valid plan fields: entry=0x401000 segments=2
PASS bad magic: M11_ERR_MAGIC
PASS bad machine: M11_ERR_MACHINE
PASS entry outside user range: M11_ERR_ENTRY
PASS memsz below filesz: M11_ERR_SEGBOUNDS
PASS file range outside image: M11_ERR_SEGBOUNDS
PASS bad alignment: M11_ERR_ALIGN
PASS segment outside user range: M11_ERR_SEGRANGE
PASS W+X segment rejected: M11_ERR_FLAGS
PASS null image pointer: M11_ERR_NULL
M11 host tests passed.
```

### Lampiran E — Log QEMU Penuh (Bagian M11)

```text
[M10] int 0x80 ping ok
[M11] elf: ident ok
elf_phnum=0x0000000000000002
elf_entry=0x0000000000401000
seg0_vaddr=0x0000000000400000
seg0_flags=0x0000000000000005
seg1_vaddr=0x0000000000401000
seg1_flags=0x0000000000000006
[M11] elf: plan ok
[M11] user image plan ready
[M9] thread A tick
[M9] thread B tick
```

### Lampiran F — Isi `evidence/M11/manifest.txt`

```text
MCSOS M11 evidence manifest
timestamp_utc=2026-06-28T19:25:56Z
commit=f017f4b3c5e9aab553996634c4678a2e0f546c33
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
lld=Ubuntu LLD 21.1.8 (compatible with GNU linkers)
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
m11_host_test.log
m11_nm_undefined.txt
m11_objdump.txt
m11_preflight.log
m11_qemu_serial.log
m11_readelf_header.txt
m11_sha256.txt
manifest.txt
```

### Lampiran G — Jawaban Pertanyaan Analisis

**1. Mengapa loader menggunakan program header, bukan section header?**
Section header adalah sudut pandang linker dan debugger — berisi metadata seperti nama section, atribut, dan relokasi yang dibutuhkan saat linking. Section header dapat distrip dari executable final (`strip --strip-sections`). Program header adalah sudut pandang loader runtime — berisi deskriptor segment yang menentukan bagian file yang harus dimuat ke virtual memory. Loader harus menggunakan program header karena itulah satu-satunya sumber informasi yang dijamin ada di executable final.

**2. Apa risiko jika `p_memsz < p_filesz` tidak ditolak?**
Loader akan mencoba menyalin lebih banyak byte dari file (`p_filesz`) ke memori yang lebih kecil (`p_memsz`), menyebabkan heap/stack overflow pada buffer tujuan. Ini dapat merusak metadata allocator, menimpa data kernel, atau menyebabkan arbitrary write jika penyerang mengendalikan nilai tersebut. M11 menolak kondisi ini dengan `M11_ERR_SEGBOUNDS`.

**3. Apa risiko jika `p_offset + p_filesz` tidak diperiksa overflow?**
Pada sistem 64-bit, `p_offset + p_filesz` dapat overflow kembali ke nilai kecil (misalnya 0). Loader kemudian akan menganggap segment berada di dalam image padahal tidak, dan mencoba membaca dari offset yang salah — berpotensi membaca di luar buffer image ke memori kernel. Fungsi `m11_add_overflow_u64` mendeteksi overflow ini sebelum kalkulasi digunakan.

**4. Mengapa `p_vaddr + p_memsz` harus berada dalam user region?**
Jika segment user melampaui batas user region ke dalam kernel higher-half, loader akan memerintahkan VMM untuk memetakan halaman yang seharusnya milik kernel sebagai user-accessible. Ini memungkinkan user program membaca atau menimpa struktur kernel seperti GDT, IDT, stack kernel, atau metadata page table, yang merupakan privilege escalation.

**5. Apa konsekuensi segment writable sekaligus executable?**
Segment W+X berarti user program dapat menulis shellcode ke halaman data lalu mengeksekusinya tanpa perlu mengeksploitasi vulnerability lain. Ini adalah kondisi yang sangat disukai attacker. W^X policy — memisahkan writable dari executable — memaksa attacker untuk mengeksploitasi dua kondisi berbeda secara bersamaan (mengubah content + mengubah eksekusi), jauh lebih sulit.

**6. Mengapa zero-fill `.bss` harus dilakukan setelah file bytes disalin?**
Jika zero-fill dilakukan sebelum copy, copy file bytes akan menimpa area yang sudah di-zero dan hasilnya benar. Namun jika zero-fill dilakukan setelah copy, dan area BSS (`p_memsz - p_filesz`) dimulai tepat setelah area file (`p_filesz`), zero-fill hanya mengenai area yang belum terisi. Urutan yang benar adalah: copy `p_filesz` byte dari file, lalu zero `p_memsz - p_filesz` byte setelahnya.

**7. Apa perbedaan `ET_EXEC` dan `ET_DYN` untuk loader pendidikan?**
`ET_EXEC` adalah executable dengan alamat virtual tetap (position-dependent); semua `p_vaddr` bersifat absolut dan tidak dapat direlokasi tanpa linker. `ET_DYN` adalah shared object atau position-independent executable (PIE); `p_vaddr` adalah offset yang dapat digeser oleh loader. Loader M11 menerima keduanya tetapi tidak mengimplementasikan relokasi untuk `ET_DYN`, sehingga PIE yang membutuhkan relokasi akan menghasilkan program yang berjalan di alamat salah.

**8. Mengapa dynamic linker dan relocation ditunda pada M11?**
Dynamic linker membutuhkan VFS untuk membuka shared library, symbol resolution dari library, GOT/PLT patching, dan koordinasi dengan kernel melalui auxiliary vector. Semua fondasi ini belum ada di MCSOS M11. Memaksa dynamic linker sebelum VFS, scheduler yang stabil, dan syscall yang lengkap akan menciptakan bug yang sangat sulit diisolasi. M11 memilih static ELF yang tidak membutuhkan relokasi untuk membatasi kompleksitas ke satu komponen baru (loader) per milestone.

**9. Bagaimana loader membersihkan frame jika mapping segment ke-2 gagal?**
Loader M11 hanya membangun plan dan tidak mengalokasikan frame. Pembersihan frame adalah tanggung jawab caller (kernel code yang mengeksekusi plan). Kontrak integrasi pada panduan M11 menyatakan: "Jika satu mapping gagal, seluruh process image harus dibatalkan dan frame yang sudah dialokasikan harus dilepas." Ini berarti kernel harus melacak frame yang sudah dialokasikan untuk setiap segment dan membebaskan semuanya jika ada mapping yang gagal sebelum rollback process image selesai.

**10. Apa bukti minimum sebelum MCSOS boleh mencoba transisi ring 3 penuh?**
Minimal: (1) loader ELF64 lulus semua validasi; (2) VMM dapat memetakan halaman user dengan bit user/supervisor benar dan NX enforcement; (3) GDT memiliki user code dan data segment selector; (4) TSS sudah diisi dengan kernel stack pointer; (5) IDT terhubung ke syscall handler via `int 0x80` atau `syscall`/`sysret`; (6) setidaknya satu syscall (seperti `write` atau `exit`) dapat melayani user program; (7) page fault handler dapat membedakan fault dari user mode dan mengembalikan SIGSEGV atau kill process tanpa merusak kernel; (8) seluruh rangkaian ini diuji dengan program user minimal (satu instruksi `int 0x80` dengan nomor syscall exit) di QEMU sebelum mencoba program lebih kompleks.

---

## 24. Daftar Referensi

```text
[1] Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's
    Manual, Volume 3: System Programming Guide," Intel Developer Documentation,
    2026. [Online]. Available:
    https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
    Accessed: Jun. 28, 2026.

[2] H. J. Lu et al., "System V Application Binary Interface — AMD64 Architecture
    Processor Supplement (With LP64 and ILP32 Programming Models)," Version 1.0,
    Jan. 2023. [Online]. Available:
    https://gitlab.com/x86-psABIs/x86-64-ABI
    Accessed: Jun. 28, 2026.

[3] Oracle Corporation, "Oracle Linker and Libraries Guide — Chapter 7: Program
    Loading and Dynamic Linking," Oracle Documentation, 2026. [Online]. Available:
    https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter6-83432.html
    Accessed: Jun. 28, 2026.

[4] The Linux Kernel Organization, "Linux Kernel Source: fs/binfmt_elf.c,"
    kernel.org, 2026. [Online]. Available:
    https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/fs/binfmt_elf.c
    Accessed: Jun. 28, 2026.

[5] QEMU Project, "GDB usage — QEMU documentation," QEMU System Emulation
    Documentation, 2026. [Online]. Available:
    https://www.qemu.org/docs/master/system/gdb.html
    Accessed: Jun. 28, 2026.

[6] LLVM Project, "Clang command line argument reference," LLVM Documentation,
    2026. [Online]. Available:
    https://clang.llvm.org/docs/ClangCommandLineReference.html
    Accessed: Jun. 28, 2026.

[7] Free Software Foundation, "Using LD, the GNU linker — Scripts," GNU
    Binutils Documentation, 2026. [Online]. Available:
    https://sourceware.org/binutils/docs/ld/Scripts.html
    Accessed: Jun. 28, 2026.
```

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder sudah diganti | `Ya` |
| Metadata laporan lengkap | `Ya` |
| Commit awal dan akhir dicatat | `Ya` |
| Perintah build dan test dapat dijalankan ulang | `Ya` |
| Log build dilampirkan | `Ya` |
| Log QEMU/test dilampirkan | `Ya` |
| Artefak penting tersedia di `evidence/M11` | `Ya` |
| Desain, invariants, dan failure modes dijelaskan | `Ya` |
| Security/reliability dibahas | `Ya` |
| Readiness review tidak berlebihan | `Ya` |
| Rubrik penilaian disiapkan | `Ya` |
| Referensi memakai format IEEE | `Ya` |
| Laporan disimpan sebagai Markdown | `Ya` |

---

## 26. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```text
ad07b27
```

Status akhir yang diklaim:

```text
Siap uji QEMU untuk loader ELF64 user awal dan process-image planning
single-core — bukan siap produksi, bukan bukti isolasi user/kernel penuh,
dengan known issues pada bagian 20 (user region hardcoded, tidak ada alokasi
frame dan mapping user page, ELF dari .bss bukan storage nyata, rollback
belum diuji aktual, GDB session formal belum dilakukan, W^X hanya di layer
loader belum di page table).
```

Ringkasan satu paragraf:

```text
Praktikum M11 MCSOS 260502 telah diselesaikan oleh Agung Nurjaman
(25832073010) secara individu, melanjutkan dari gate M10 yang solid (commit
f017f4b). ELF64 user program loader awal berhasil dibangun lengkap:
validasi defensif ident/type/machine/version/ehsize/phentsize/phbounds,
validasi setiap PT_LOAD (W^X, memsz>=filesz, alignment power-of-two dan
kongruen, overflow offset+filesz, file range, user range), penyusunan
m11_process_image_plan dengan fail-closed behavior (m11_zero_plan saat
error), fungsi m11_validate_user_range, dan m11_error_name untuk 17 kode
error. Host unit test lulus 11 kasus (1 valid + 10 negatif) tanpa boot QEMU.
Audit freestanding membuktikan nm -u kosong, ELF64 x86-64 REL, simbol
m11_elf64_plan_load ada di disassembly, dan sha256sum 4 artefak tersimpan.
Bug separator Makefile.m11 (tab vs '>') ditemukan dan diperbaiki via Python.
Build kernel berhasil dengan kernel/user/m11_elf_loader.o terintegrasi
otomatis. QEMU smoke test membuktikan [M11] elf: ident ok, [M11] elf: plan ok,
[M11] user image plan ready, dan M9 thread A/B masih berjalan normal setelah
M11. Commit M11 (ad07b27) tersimpan bersih di branch praktikum-m11-elf-user-
loader dengan 8 artefak evidence dan manifest toolchain. Status readiness yang
diklaim adalah siap uji QEMU untuk ELF64 loader awal dan process-image planning,
secara eksplisit bukan siap demonstrasi atau siap produksi, dengan enam known
issues yang didokumentasikan untuk ditindaklanjuti pada M12 dan demonstrasi.
```
