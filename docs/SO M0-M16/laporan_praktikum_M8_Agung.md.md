# Kernel Heap Awal, Allocator Dinamis, Validasi Invariant, dan Integrasi Bertahap dengan PMM/VMM pada MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M8_Agung.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, subset POSIX-like
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `M8` |
| Judul praktikum | `Kernel Heap Awal, Allocator Dinamis, Validasi Invariant, dan Integrasi Bertahap dengan PMM/VMM pada MCSOS 260502` |
| Jenis pengerjaan | `Individu` |
| Nama mahasiswa | `Agung Nurjaman` |
| NIM | `25832073010` |
| Kelas | `Pendidikan Teknologi Informasi` |
| Nama kelompok | `-` |
| Anggota kelompok | `-` |
| Tanggal praktikum | `2026-06-21` |
| Tanggal pengumpulan | `2026-06-21` |
| Repository | `/home/agung/src/mcsos` |
| Branch | `praktikum-m8-kernel-heap` |
| Commit awal | `59ebb47` (M7) |
| Commit akhir | `d9fadc3` (M8) |
| Status readiness yang diklaim | `Siap uji QEMU untuk kernel heap awal — bukan siap produksi` |

---

## 1. Sampul

# Laporan Praktikum M8
## Kernel Heap Awal, Allocator Dinamis, Validasi Invariant, dan Integrasi Bertahap dengan PMM/VMM pada MCSOS 260502

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
Panduan resmi praktikum M8 MCSOS 260502 digunakan sebagai referensi utama
dan kontrak implementasi untuk seluruh komponen (kmem.h, kmem.c,
test_kmem.c, check_m8_kmem.sh, target Makefile M8, dan patch kmain.c).
Intel SDM Vol.3 dan AMD64 APM Vol.2 digunakan sebagai referensi arsitektur
x86_64, khususnya untuk alignment, pointer arithmetic, dan model memori
kernel freestanding. Dokumentasi Linux kernel memory allocation digunakan
sebagai referensi konsep first-fit free-list, split, coalesce, dan
strategi alokasi objek kecil. AI assistant (Claude) digunakan untuk:
(1) menulis draft awal kmem.h, kmem.c, test_kmem.c, check_m8_kmem.sh,
dan target Makefile M8 sesuai kontrak panduan; (2) membantu menulis
Python patch untuk menyisipkan include kmem.h, arena bootstrap, fungsi
m8_heap_bootstrap(), dan pemanggilan di kmain.c tanpa mengganggu kode M7
yang sudah ada; (3) mendiagnosis satu masalah operasional: build/m8/ hilang
setelah make clean sehingga evidence tidak dapat disalin; diselesaikan
dengan menjalankan make m8-all terlebih dahulu sebelum menyalin ke
evidence/M8/. AI tidak digunakan untuk mengubah kontrak fungsional di luar
yang ditentukan panduan resmi. Seluruh build, host unit test, audit
nm/objdump/readelf, QEMU smoke test, dan commit git dijalankan dan
diverifikasi sendiri oleh mahasiswa di WSL 2 miliknya.
```

---

## 3. Tujuan Praktikum

1. Membangun kernel heap awal berbasis first-fit free-list allocator dengan split, coalesce, alignment 16 byte, dan validasi magic header yang dapat dikompilasi sebagai C17 freestanding.
2. Mengimplementasikan API publik `kmem_init`, `kmem_alloc`, `kmem_calloc`, `kmem_free_checked`, `kmem_get_stats`, dan `kmem_validate` sesuai kontrak panduan M8.
3. Membuktikan invariant allocator secara eksplisit: alignment payload 16 byte, penolakan double-free, penolakan pointer di luar arena, validasi magic header, dan coalescing blok bebas bertetangga.
4. Menyediakan host unit test deterministik yang menguji alokasi dasar, zeroing `calloc`, overflow guard, double-free rejection, fragmentasi, dan coalescing tanpa perlu boot QEMU.
5. Mengaudit object freestanding agar bebas dependency host (`nm -u` kosong), membuktikan ELF64 x86-64, dan membuktikan semua simbol allocator ada di disassembly.
6. Mengintegrasikan heap ke kernel MCSOS menggunakan arena bootstrap statik `.bss` 64 KiB setelah PMM dan VMM siap, membuktikan log `[M8] kmem initialized` dan statistik heap muncul di serial QEMU.
7. Membedakan penggunaan PMM (frame fisik), VMM (pemetaan virtual), dan kernel heap (objek byte dinamis) sebagai tiga lapisan manajemen memori yang terpisah dan bertingkat.
8. Mengumpulkan evidence reproducible ke `evidence/M8` beserta manifest toolchain.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan perbedaan PMM, VMM, dan kernel heap | Bagian 6.1 dan 9.1; tabel tiga lapisan manajemen memori; log QEMU menunjukkan M6→M7→M8 berurutan |
| Mendesain free-list allocator dengan header, split, coalesce, dan statistik | `kernel/mm/kmem.c`; struktur `kmem_block_t` dengan `magic`, `size`, `free`, `prev`, `next` |
| Mengimplementasikan alignment 16 byte pada payload | `kmem_align_up_size` dan `kmem_align_up_ptr`; host test alignment assertion lulus |
| Mengimplementasikan split blok | `kmem_split_if_useful` di `kmem.c`; `KMEM_MIN_SPLIT = 32` mencegah split terlalu kecil |
| Mengimplementasikan coalesce blok free bertetangga | `kmem_coalesce_forward` di `kmem.c`; host test `free_count == 1` setelah free semua blok |
| Menolak double free secara eksplisit | `kmem_free_checked` memeriksa `block->free` dan mengembalikan `-4`; host test double-free lulus |
| Menolak pointer di luar arena | `kmem_ptr_in_heap` memeriksa batas `g_heap_base..g_heap_end`; host test pointer invalid ditolak |
| Mengaudit object freestanding tanpa undefined symbol | `nm -u build/m8/kmem.freestanding.o` kosong; `evidence/M8/nm_u.txt` kosong |
| Mengintegrasikan heap ke kernel nyata menggunakan arena statik | Log QEMU `[M8] kmem initialized` dengan statistik `kmem_total`, `kmem_free`, `kmem_largest`, `kmem_blocks` |
| Menjalankan `kmem_validate()` sebagai invariant check | Dipanggil di akhir `kmem_init` dan setiap `kmem_free_checked`; host test validate lulus |

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
| M8 | Kernel heap awal, allocator dinamis, integrasi PMM/VMM | `✓ selesai praktikum` |
| M9 | Thread, scheduler, synchronization | `[ ] tidak dibahas` |
| M10 | Syscall ABI dan user program loader | `[ ] tidak dibahas` |
| M11 | VFS, file descriptor, ramfs | `[ ] tidak dibahas` |
| M12 | Block layer dan device model | `[ ] tidak dibahas` |
| M13 | Persistent filesystem, recovery | `[ ] tidak dibahas` |
| M14 | Networking stack | `[ ] tidak dibahas` |
| M15 | Security model, capability/ACL, hardening | `[ ] tidak dibahas` |
| M16 | SMP, scalability, lock stress | `[ ] tidak dibahas` |
| M17 | Observability, release image, readiness review | `[ ] tidak dibahas` |

Batas cakupan praktikum:

```text
M8 mencakup: first-fit free-list allocator (kmem_init, kmem_alloc,
kmem_calloc, kmem_free_checked, kmem_get_stats, kmem_validate), alignment
16 byte, split blok (KMEM_MIN_SPLIT=32), coalesce blok free bertetangga,
magic header validation, double-free rejection, out-of-arena rejection,
overflow guard calloc, host unit test 4 skenario, audit freestanding
(nm -u, readelf ELF64, objdump), integrasi kernel dengan arena bootstrap
statik 64 KiB, QEMU smoke test, dan evidence manifest.

M8 TIDAK mencakup: page-backed heap growth melalui PMM/VMM, slab/cache
allocator, per-CPU allocator, allocator SMP-safe, reentrancy guard IRQ,
vmalloc, mmap, user heap, copy-on-write, garbage collection, NUMA-aware
allocation, DMA-constrained allocation, red zone guard, canary/shadow,
dan klaim keamanan produksi.
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

```text
M8 membangun lapisan ketiga manajemen memori kernel MCSOS di atas PMM
(frame fisik) dan VMM (pemetaan virtual). Lima konsep utama:

1. Tiga Lapisan Manajemen Memori Kernel:
   - PMM M6: mengelola frame fisik 4 KiB. Unit alokasi adalah frame.
     API: pmm_alloc_frame(), pmm_free_frame().
   - VMM M7: mengelola pemetaan virtual ke physical melalui page table
     4-level x86_64. Unit alokasi adalah halaman 4 KiB.
     API: vmm_map_page(), vmm_unmap_page(), vmm_query_page().
   - Kernel Heap M8: mengelola objek berukuran byte di atas arena virtual
     yang sudah terpetakan. Unit alokasi adalah byte dengan alignment.
     API: kmem_alloc(), kmem_calloc(), kmem_free_checked().
   Pemisahan ini wajib dijaga: heap tidak boleh mengubah page table
   langsung, dan PMM tidak boleh mengembalikan pointer objek kecil.

2. Free-List Allocator (First-Fit):
   Arena heap dibagi menjadi blok-blok yang dirangkai dalam linked list.
   Setiap blok memiliki header kmem_block_t yang menyimpan magic, ukuran
   payload, flag free/used, dan pointer prev/next. Alokasi mencari blok
   free pertama yang cukup besar (first-fit). Pembebasan mengembalikan
   flag free dan mencoba coalesce dengan blok free bertetangga.

3. Split dan Coalesce:
   Split: jika blok free lebih besar dari yang dibutuhkan (ditambah
   overhead header + KMEM_MIN_SPLIT), blok dipecah menjadi dua. Blok
   pertama dialokasikan, blok kedua menjadi blok free baru.
   Coalesce: saat blok dibebaskan, blok free bertetangga digabungkan
   menjadi satu blok free lebih besar. Ini mencegah fragmentasi eksternal
   yang tidak perlu.

4. Alignment 16 Byte:
   Payload yang dikembalikan kmem_alloc harus aligned 16 byte agar
   kompatibel dengan tipe data SSE/AVX dan struct yang membutuhkan
   natural alignment. kmem_align_up_size dan kmem_align_up_ptr
   menghitung padding yang diperlukan.

5. Magic Header dan Invariant Validation:
   Setiap blok memiliki field magic = KMEM_MAGIC (0x4d43534f53484541).
   kmem_validate() menelusuri seluruh linked list dan memeriksa:
   setiap blok berada dalam arena, magic valid, pointer prev/next
   konsisten, ukuran tidak melewati batas heap, dan cursor maju monoton.
   Ini membantu mendeteksi metadata corruption, use-after-free, dan
   heap overrun lebih awal.
```

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| Alignment 16 byte | SIMD, struct, dan ABI x86_64 membutuhkan alignment natural; payload heap harus memenuhi ini | Host test `assert(((uintptr_t)a & 15) == 0)` lulus untuk semua alokasi |
| `SIZE_MAX` dan overflow guard | `kmem_calloc(count, bytes)` harus mendeteksi `count * bytes` overflow sebelum alokasi | Host test `kmem_calloc((size_t)-1, 2) == NULL` lulus |
| Pointer arithmetic C freestanding | Manipulasi `unsigned char *` untuk navigasi antara header dan payload, serta antara blok bertetangga | `kmem_payload`, `kmem_header_from_payload`, `kmem_coalesce_forward` di `kmem.c` |
| `.bss` section kernel | Arena bootstrap 64 KiB ditempatkan di `.bss` yang sudah di-zero dan terpetakan oleh bootloader | `static unsigned char m8_boot_heap[65536] __attribute__((aligned(4096)))` di `kmain.c` |
| `-mno-red-zone` | Kernel freestanding tidak boleh bergantung pada red-zone karena exception/interrupt dapat terjadi kapan saja | Flag aktif di seluruh `COMMON_CFLAGS`; build lulus |
| `-ffreestanding -fno-builtin` | Compiler tidak boleh menghasilkan panggilan implisit ke libc `memset`, `memcpy`, atau `malloc` | `nm -u build/m8/kmem.freestanding.o` kosong |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding; tidak ada stdlib host kecuali pada host unit test |
| Runtime | Tanpa hosted libc; `memset` diganti dengan `kmem_memset` internal |
| ABI | `x86_64-unknown-none-elf`, `-mcmodel=kernel`, `-mno-red-zone` |
| Compiler flags kritis | `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, `-mno-red-zone`, `-Wall -Wextra -Werror` |
| Helper internal | `kmem_memset`, `kmem_align_up_size`, `kmem_align_up_ptr` menggantikan fungsi libc |
| Risiko undefined behavior | Cast `(kmem_block_t *)ptr` hanya valid jika ptr aligned dan dalam arena; kedua kondisi divalidasi sebelum cast |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Intel SDM Vol.3 | Memory Management, alignment, pointer model | Dasar alignment dan model memori x86_64 |
| [2] | AMD64 APM Vol.2 | System Programming, alignment | Referensi silang model memori |
| [3] | Linux Kernel Documentation | Memory allocation: kmalloc, vmalloc, page allocator | Referensi konsep strategi alokasi kernel |
| [4] | Linux Kernel Documentation | Slab allocator | Referensi konsep lanjutan untuk milestone berikutnya |
| [5] | GNU Make Manual | Target, dependency, phony | Target `m8-clean`, `m8-all`, `m8-audit` di Makefile |
| [6] | GNU ld/LLD Linker Scripts | SECTIONS, `.bss`, alignment | Layout arena bootstrap di `.bss` kernel |
| [7] | LLVM/Clang Reference | `-ffreestanding`, `-fno-builtin` | Flags kompilasi freestanding kernel |
| [8] | QEMU Documentation | Serial log, `-no-reboot`, `-no-shutdown` | Konfigurasi QEMU smoke test M8 |

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
| Boot path | Limine bootloader (third_party/limine), dilanjutkan dari M2–M7 |
| Build system | GNU Make 4.4.1 |
| Bahasa utama | C17 freestanding |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker | Ubuntu LLD 21.1.8 (compatible with GNU linkers) |
| Host compiler (unit test) | clang (via `$(CC)` tanpa flag freestanding) |

### 7.2 Versi Toolchain

```text
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
lld=Ubuntu LLD 21.1.8 (compatible with GNU linkers)
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
make=GNU Make 4.4.1
```

Diambil dari `evidence/M8/manifest.txt`, commit `d9fadc3`.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | `Ya` |
| Remote repository | `-` (lokal) |
| Branch | `praktikum-m8-kernel-heap` |
| Commit hash awal | `59ebb47` (M7 selesai) |
| Commit hash akhir | `d9fadc3` (M8 selesai) |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── include/
│   ├── types.h           (M6)
│   ├── pmm.h             (M6)
│   ├── vmm.h             (M7)
│   └── mcsos/
│       └── kmem.h        (baru M8)
├── src/
│   ├── pmm.c             (M6)
│   └── vmm.c             (M7)
├── kernel/
│   ├── mm/
│   │   └── kmem.c        (baru M8)
│   └── core/
│       └── kmain.c       (diperbarui M8)
├── tests/
│   ├── test_pmm_host.c   (M6)
│   ├── test_vmm_host.c   (M7)
│   └── test_kmem.c       (baru M8)
├── scripts/
│   ├── check_m6_static.sh
│   ├── m7_preflight.sh
│   ├── grade_m7.sh
│   └── check_m8_kmem.sh  (baru M8)
├── evidence/
│   └── M8/               (baru M8)
│       ├── test_kmem.log
│       ├── nm_u.txt
│       ├── readelf_h.txt
│       ├── kmem.objdump.txt
│       ├── m8-qemu-serial.log
│       └── manifest.txt
└── Makefile              (diperbarui M8)
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis | Alasan | Risiko |
|---|---|---|---|
| `include/mcsos/kmem.h` | Baru | API publik allocator: macro KMEM_ALIGN/MAGIC, struct kmem_stats, deklarasi 6 fungsi | Rendah — header-only, tidak ada logika |
| `kernel/mm/kmem.c` | Baru | Implementasi allocator: kmem_block_t, fungsi internal, dan 6 fungsi publik | Tinggi — pointer arithmetic yang salah menyebabkan korupsi heap |
| `tests/test_kmem.c` | Baru | Host unit test 4 skenario: basic alloc/free, calloc+overflow, double-free, fragmentasi+coalesce | Rendah — hanya dipakai di host test |
| `scripts/check_m8_kmem.sh` | Baru | Preflight otomatis: cek file, toolchain, freestanding object, host unit test | Rendah |
| `kernel/core/kmain.c` | Ubah | Tambah `#include "mcsos/kmem.h"`, arena `m8_boot_heap[65536]`, fungsi `m8_heap_bootstrap()`, panggil setelah `m7_vmm_init()` | Sedang — arena di `.bss` harus sudah terpetakan; urutan init harus dijaga |
| `Makefile` | Ubah | Tambah variabel `CFLAGS_M8_*`, `BUILD_M8`, dan target `m8-clean`, `m8-kmem-freestanding`, `m8-kmem-host-test`, `m8-audit`, `m8-all` | Rendah — target M8 terisolasi, tidak mempengaruhi target M0–M7 |

### 8.3 Ringkasan Diff

```text
git log --oneline praktikum-m8-kernel-heap
d9fadc3 (HEAD -> praktikum-m8-kernel-heap) M8: implement kernel heap allocator, host test, audit, and QEMU integration
59ebb47 (m7-vmm-core) M7: add evidence manifest, QEMU log, and audit artifacts
```

File baru pada commit M8 (d9fadc3):
- `include/mcsos/kmem.h`, `kernel/mm/kmem.c`, `tests/test_kmem.c`
- `scripts/check_m8_kmem.sh`
- `evidence/M8/*` (6 file artefak + manifest)

File yang diubah:
- `kernel/core/kmain.c` — tambah integrasi heap (arena + bootstrap + pemanggilan)
- `Makefile` — tambah target M8

---

## 9. Desain Teknis

### 9.1 Tiga Lapisan Manajemen Memori

| Lapisan | Unit alokasi | API utama | Tanggung jawab | Tidak boleh dilakukan |
|---|---|---|---|---|
| PMM M6 | Frame fisik 4 KiB | `pmm_alloc_frame()`, `pmm_free_frame()` | Memilih frame fisik usable dari bitmap | Mengembalikan pointer objek kecil langsung |
| VMM M7 | Halaman virtual 4 KiB | `vmm_map_page()`, `vmm_unmap_page()` | Membuat pemetaan virtual ke physical | Mengetahui layout objek heap |
| Kernel Heap M8 | Byte/objek dengan alignment | `kmem_alloc()`, `kmem_free_checked()` | Mengelola objek kecil dan sedang dalam arena | Mengubah page table tanpa kontrak VMM |

### 9.2 Masalah yang Diselesaikan

```text
Kernel M7 memiliki PMM dan VMM yang berfungsi, tetapi tidak memiliki
mekanisme alokasi objek berukuran byte. Setiap variabel kernel bersifat
statik sehingga tidak dapat dibuat atau dihancurkan secara dinamis saat
runtime. Struktur seperti deskriptor proses, metadata file, buffer I/O,
dan node timer membutuhkan alokasi dinamis. M8 menyelesaikan masalah ini
dengan first-fit free-list allocator yang bekerja pada arena virtual yang
sudah terpetakan, memberikan kernel kemampuan dasar kmem_alloc/kmem_free
tanpa bergantung pada libc.
```

### 9.3 Struktur Data Utama

```c
// Header setiap blok dalam free-list
typedef struct kmem_block {
    uint64_t magic;          // KMEM_MAGIC = 0x4d43534f53484541
    size_t size;             // ukuran payload (BUKAN termasuk header)
    int free;                // 1 = free, 0 = used
    uint32_t reserved;       // padding untuk alignment
    uint64_t reserved2;      // padding untuk alignment
    struct kmem_block *prev; // blok sebelumnya dalam list
    struct kmem_block *next; // blok berikutnya dalam list
} kmem_block_t;

// sizeof(kmem_block_t) harus kelipatan KMEM_ALIGN (16)
// agar payload yang mengikutinya juga aligned
```

Layout arena:

```text
g_heap_base                                              g_heap_end
|                                                                  |
[kmem_block_t | payload 16-aligned | padding?][kmem_block_t | ...][...]
```

### 9.4 Flowchart Operasi Utama

```text
kmem_alloc(bytes):
  1. g_initialized? bytes > 0?
  2. wanted = align_up(bytes, 16)
  3. Scan linked list: cari blok free dengan size >= wanted
  4. Jika ditemukan:
     a. split_if_useful(blok, wanted) — bagi jika sisa >= header + 32
     b. blok->free = 0
     c. return payload(blok)
  5. Tidak ada: return NULL

kmem_free_checked(ptr):
  1. ptr == NULL → return 0 (no-op)
  2. ptr dalam arena? aligned 16?
  3. header = ptr - sizeof(header); header dalam arena? magic valid?
  4. header->free? → return -4 (double-free)
  5. header->free = 1
  6. coalesce_forward(header)    — gabung dengan next free
  7. prev free? coalesce_forward(prev) — gabung dari prev
  8. return kmem_validate()

kmem_validate():
  1. Cek g_initialized, base, end, head valid
  2. head == heap_base?
  3. Telusuri list: setiap blok cek magic, prev pointer, batas arena, cursor monoton
  4. return 0 jika semua lulus, kode negatif jika gagal
```

### 9.5 Invariants

| Kode | Invariant | Implementasi | Bukti |
|---|---|---|---|
| KH-I1 | Setiap blok dalam arena: `g_heap_base <= block < g_heap_end` | `kmem_ptr_in_heap()` di `kmem_free_checked` dan `kmem_validate` | Host test pointer di luar arena ditolak |
| KH-I2 | `block->magic == KMEM_MAGIC` untuk blok aktif | Dicek di `kmem_alloc`, `kmem_free_checked`, `kmem_validate` | `kmem_validate` mengembalikan -6 jika magic salah |
| KH-I3 | Payload aligned 16 byte | `kmem_align_up_size(bytes, 16)` sebelum alokasi | Host test `(uintptr_t)ptr & 15 == 0` lulus |
| KH-I4 | `block->size` menyatakan kapasitas payload, bukan ukuran header | Seluruh kalkulasi ukuran memisahkan header dari payload | Review `kmem_split_if_useful` dan `kmem_coalesce_forward` |
| KH-I5 | Setiap blok memiliki status tepat satu: free atau used | `block->free` adalah 0 atau 1; double-free memeriksa ini | Host test double-free ditolak dengan kode -4 |
| KH-I6 | Dua blok free bertetangga dicoalesce saat free | `kmem_coalesce_forward` dipanggil di `kmem_free_checked` | Host test fragmentasi: `free_count == 1` setelah free semua |
| KH-I7 | `kmem_free_checked(NULL)` adalah no-op sukses | Cek `ptr == NULL` pertama | Host test implicit via calloc test |
| KH-I8 | Double free ditolak dengan kode negatif | Cek `block->free` sebelum set free | Host test `free_checked(p) < 0` setelah free pertama |
| KH-I9 | Pointer di luar arena ditolak | `kmem_ptr_in_heap()` memeriksa batas | `kmem_free_checked` mengembalikan -1 untuk pointer asing |
| KH-I10 | Tidak ada dependency libc dari object kernel | Kompilasi dengan `-ffreestanding -fno-builtin` | `nm -u build/m8/kmem.freestanding.o` kosong |
| KH-I11 | `kmem_validate()` lulus setelah init, alloc, dan free | Dipanggil di akhir `kmem_init` dan `kmem_free_checked` | Semua host test menjalankan `kmem_validate()` secara implisit |
| KH-I12 | Tidak digunakan dari interrupt context | Documented constraint; tidak ada reentrancy guard | Catatan di komentar `kmem.h` dan laporan |

### 9.6 Keputusan Desain

| Keputusan | Alternatif | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Arena bootstrap statik `.bss` 64 KiB | Page-backed heap dari PMM/VMM | Lebih aman untuk M8 awal; arena `.bss` sudah terpetakan bootloader; tidak bergantung pada VMM yang masih baru | Ukuran heap tetap; page-backed growth menjadi pengayaan |
| First-fit, bukan best-fit atau buddy | Best-fit, buddy system | Lebih sederhana untuk O(n) linear scan; correctness lebih mudah diaudit; cukup untuk kernel pendidikan M8 | Fragmentasi lebih tinggi dari best-fit; tidak ada worst-case bound seperti buddy |
| `KMEM_MIN_SPLIT = 32` | Nilai lebih kecil atau besar | Mencegah split menghasilkan blok terlalu kecil yang tidak berguna; 32 byte cukup untuk blok minimum meaningful | Sisa blok < header + 32 tidak dipecah; sedikit waste tapi lebih bersih |
| `kmem_validate()` dipanggil setiap `free` | Hanya saat debug | Correctness lebih penting dari performa pada kernel pendidikan; membantu mendeteksi bug lebih awal | O(n) overhead setiap free; tidak cocok untuk production allocator |
| Magic number `0x4d43534f53484541` | Nilai arbitrer | Dapat dibaca sebagai ASCII "MCSOSHA" (MCSOS Heap); mudah dikenali di memory dump atau log | Magic bukan mekanisme keamanan; dapat disiapkan oleh attacker, tapi cukup untuk debug |
| `kmem_memset` internal | Libc `memset` | Memastikan tidak ada dependency libc dari object kernel | Sedikit lebih lambat dari implementasi SIMD libc; tidak relevan untuk kernel pendidikan |

### 9.7 Security Boundary M8

| Risiko | Level | Mitigasi pada M8 | Status |
|---|---|---|---|
| Double free | Sedang | `block->free` dicek sebelum dibebaskan; kode -4 dikembalikan | Terverifikasi host test |
| Out-of-arena pointer | Sedang | `kmem_ptr_in_heap()` memeriksa `base <= ptr < end` | Terverifikasi kode review |
| Integer overflow pada `calloc` | Sedang | `count != 0 && bytes > SIZE_MAX/count` sebelum perkalian | Terverifikasi host test `calloc(SIZE_MAX, 2) == NULL` |
| Metadata corruption | Tinggi | Magic check di `kmem_validate`; return kode negatif jika korup | Terverifikasi kode review; tidak ada uji adversarial |
| Allocator dari IRQ context | Tinggi | Documented constraint; tidak ada lock | Mitigasi documented; runtime enforcement belum ada |
| Use-after-free | Tinggi | Tidak ada; pointer yang dikembalikan tidak di-zero atau di-poison | Known limitation M8; pengayaan red-zone atau poison pattern |
| Heap overrun (menulis melewati batas payload) | Tinggi | Tidak ada guard/canary | Known limitation M8; mitigasi dengan red-zone di pengayaan |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Buat Branch M8 dan Direktori

```bash
git switch -c praktikum-m8-kernel-heap
mkdir -p include/mcsos kernel/mm build/m8
git branch --show-current
```

Output: `praktikum-m8-kernel-heap`

### Langkah 2 — Buat Header `include/mcsos/kmem.h`

Mendefinisikan API publik: `KMEM_ALIGN=16`, `KMEM_MAGIC`, struct `kmem_stats_t`, dan deklarasi 6 fungsi publik.

```bash
cat > include/mcsos/kmem.h << 'EOF'
# [isi sesuai panduan M8 bagian 11.2]
EOF
```

Verifikasi: `cat include/mcsos/kmem.h` menampilkan 6 deklarasi fungsi.

### Langkah 3 — Buat Implementasi `kernel/mm/kmem.c`

Mengimplementasikan `kmem_block_t`, fungsi internal (align, memset, payload, split, coalesce), dan 6 fungsi publik. Helper `kmem_memset` menggantikan `memset` libc.

```bash
cat > kernel/mm/kmem.c << 'EOF'
# [isi sesuai panduan M8 bagian 11.3]
EOF
```

Verifikasi:

```text
grep -n "kmem_init|kmem_alloc|kmem_free_checked|kmem_validate|kmem_get_stats" kernel/mm/kmem.c
100:int kmem_init(...)
125:void *kmem_alloc(...)
144:void *kmem_calloc(...)  [memanggil kmem_alloc]
149:int kmem_free_checked(...)
164:void kmem_get_stats(...)
182:int kmem_validate(...)
```

### Langkah 4 — Buat Host Unit Test `tests/test_kmem.c`

4 skenario: basic alloc/free, calloc+overflow, double-free rejection, fragmentasi+coalesce.

```bash
cat > tests/test_kmem.c << 'EOF'
# [isi sesuai panduan M8 bagian 11.4]
EOF
```

### Langkah 5 — Tambahkan Target M8 ke Makefile

Menambahkan `CFLAGS_M8_COMMON`, `CFLAGS_M8_KERNEL`, `BUILD_M8 := build/m8`, dan 5 target: `m8-clean`, `m8-kmem-freestanding`, `m8-kmem-host-test`, `m8-audit`, `m8-all`.

```bash
cat >> Makefile << 'EOF'
# --- M8 Kernel Heap targets ---
...
EOF
```

### Langkah 6 — Jalankan `make m8-all`

```bash
make m8-all
```

Output:

```text
clang -std=c17 ... tests/test_kmem.c kernel/mm/kmem.c -o build/m8/test_kmem
M8 kmem host tests: PASS
clang ... --target=x86_64-unknown-none-elf ... -c kernel/mm/kmem.c -o build/m8/kmem.freestanding.o
nm -u build/m8/kmem.freestanding.o      [kosong]
test ! -s build/m8/nm_u.txt             [lulus]
readelf -h build/m8/kmem.freestanding.o → build/m8/readelf_h.txt
objdump -dr build/m8/kmem.freestanding.o → build/m8/kmem.objdump.txt
```

### Langkah 7 — Buat dan Jalankan Script Preflight

```bash
cat > scripts/check_m8_kmem.sh << 'EOF'
# [isi sesuai panduan M8 bagian 11.6]
EOF
chmod +x scripts/check_m8_kmem.sh
./scripts/check_m8_kmem.sh
```

Output: `[PASS] M8 preflight completed.`

### Langkah 8 — Integrasi Kernel di `kmain.c`

Menambahkan `#include "mcsos/kmem.h"`, arena `m8_boot_heap[65536]` di `.bss`, fungsi `m8_heap_bootstrap()`, dan pemanggilan setelah `m7_vmm_init()`. Patch dilakukan via Python script untuk menghindari konflik dengan kode M7 yang ada.

### Langkah 9 — Build Kernel

```bash
make clean && make build
```

Output: 13 file C + 1 file .S dikompilasi termasuk `kernel/mm/kmem.c` → `build/normal/kernel/mm/kmem.o`. Link berhasil.

### Langkah 10 — QEMU Smoke Test

```bash
tools/scripts/make_iso.sh 2>/dev/null
qemu-system-x86_64 -machine q35 -cpu max -m 256M \
    -cdrom build/mcsos.iso -boot d \
    -serial file:build/m8-qemu-serial.log \
    -display none -no-reboot -no-shutdown || true
```

Output serial (bagian M8):

```text
[M8] kmem initialized
kmem_total=0x000000000000fff0
kmem_free=0x000000000000ffc0
kmem_largest=0x000000000000ffc0
kmem_blocks=0x0000000000000001
[M8] heap probe alloc/free roundtrip ok
```

### Langkah 11 — Kumpulkan Evidence dan Commit

```bash
mkdir -p evidence/M8
make m8-all  # regenerasi karena make clean
cp build/m8/test_kmem.log build/m8/nm_u.txt \
   build/m8/readelf_h.txt build/m8/kmem.objdump.txt evidence/M8/
cp build/m8-qemu-serial.log evidence/M8/
# buat manifest.txt
git add include/mcsos/kmem.h kernel/mm/kmem.c tests/test_kmem.c \
        scripts/check_m8_kmem.sh Makefile kernel/core/kmain.c evidence/M8
git commit -m "M8: implement kernel heap allocator, host test, audit, and QEMU integration"
# → commit d9fadc3
```

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| C1 Header kmem | `grep KMEM_MAGIC include/mcsos/kmem.h` | Macro dan 6 deklarasi fungsi ada | `PASS` |
| C2 Host unit test | `make m8-kmem-host-test` | `M8 kmem host tests: PASS` | `PASS` |
| C3 Freestanding object | `make m8-kmem-freestanding` | `build/m8/kmem.freestanding.o` ada tanpa error | `PASS` |
| C4 Undefined symbol | `make m8-audit` (bagian nm) | `build/m8/nm_u.txt` kosong | `PASS` |
| C5 ELF64 audit | `cat build/m8/readelf_h.txt` | `ELF64`, `Advanced Micro Devices X86-64`, `REL` | `PASS` |
| C6 Symbol disassembly | `grep kmem_init build/m8/kmem.objdump.txt` | Semua 6 simbol allocator ada | `PASS` |
| C7 Preflight | `./scripts/check_m8_kmem.sh` | `[PASS] M8 preflight completed.` | `PASS` |
| C8 Kernel build | `make clean && make build` | `build/kernel.elf` berhasil, `kmem.o` masuk | `PASS` |
| C9 QEMU smoke test | QEMU command M8 | Serial log `[M8] kmem initialized` | `PASS` |
| C10 Evidence | `ls evidence/M8/` | 6 artefak + manifest ada | `PASS` |

---

## 12. Perintah Uji dan Validasi

### 12.1 Host Unit Test

```bash
make m8-kmem-host-test
```

Hasil:

```text
M8 kmem host tests: PASS
```

Status: `PASS`

### 12.2 Audit Freestanding Object

```bash
make m8-audit
cat build/m8/nm_u.txt        # harus kosong
cat build/m8/readelf_h.txt   # ELF64 x86-64
grep "kmem_init\|kmem_alloc\|kmem_free" build/m8/kmem.objdump.txt | head -10
```

Hasil:

```text
[nm_u.txt — kosong]

[readelf_h.txt ringkas]
Class: ELF64
Data: 2's complement, little endian
Type: REL (Relocatable file)
Machine: Advanced Micro Devices X86-64

[objdump — simbol allocator]
0000000000000000 <kmem_init>:
00000000000001c0 <kmem_validate>:
0000000000000390 <kmem_alloc>:
0000000000000670 <kmem_calloc>:
0000000000000750 <kmem_free_checked>:
00000000000009c0 <kmem_get_stats>:
```

Status: `PASS`

### 12.3 Build Kernel

```bash
make clean && make build
ls -lh build/kernel.elf
nm -n build/kernel.elf | grep "kmem_"
```

Hasil:

```text
build/kernel.elf ada

[nm output — kmem symbols di kernel.elf]
ffffffff... T kmem_init
ffffffff... T kmem_alloc
ffffffff... T kmem_calloc
ffffffff... T kmem_free_checked
ffffffff... T kmem_get_stats
ffffffff... T kmem_validate
```

Status: `PASS`

### 12.4 QEMU Smoke Test

```bash
cat build/m8-qemu-serial.log | grep -E "M8|M7|M6|M4|kernel entered"
```

Hasil:

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
[M4] IDT and exception dispatch path installed
```

Status: `PASS`

### 12.5 Preflight Lengkap

```bash
./scripts/check_m8_kmem.sh
```

Hasil: `[PASS] M8 preflight completed.`

Status: `PASS`

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan

| No. | Uji | Expected | Actual | Status | Evidence |
|---|---|---|---|---|---|
| 1 | `test_basic_alloc_free`: alloc 3 blok berbeda ukuran | Semua non-NULL, aligned 16 | Semua non-NULL, aligned | `PASS` | Host test output |
| 2 | `test_basic_alloc_free`: free semua dalam urutan berbeda | Semua `== 0`, validate lulus | Semua lulus | `PASS` | Host test output |
| 3 | `test_calloc_and_overflow`: calloc 64×4 = 256 byte | Non-NULL, semua byte = 0 | Non-NULL, zeroed | `PASS` | Host test assert |
| 4 | `test_calloc_and_overflow`: calloc `(SIZE_MAX, 2)` | `NULL` (overflow) | `NULL` | `PASS` | Host test assert |
| 5 | `test_double_free_rejected`: free pertama | `== 0` | `0` | `PASS` | Host test assert |
| 6 | `test_double_free_rejected`: free kedua | `< 0` | `-4` | `PASS` | Host test assert |
| 7 | `test_fragmentation_and_coalesce`: 16 alloc, free alternating | Semua lulus | Semua lulus | `PASS` | Host test assert |
| 8 | `test_fragmentation_and_coalesce`: setelah free semua | `free_count == 1`, `block_count == 1`, `largest_free > 4096` | Semua terpenuhi | `PASS` | Host test assert |
| 9 | `nm -u` freestanding object | Kosong | Kosong | `PASS` | `evidence/M8/nm_u.txt` |
| 10 | `readelf -h` freestanding object | ELF64 x86-64 REL | ELF64 x86-64 REL | `PASS` | `evidence/M8/readelf_h.txt` |
| 11 | Semua 6 simbol allocator di disassembly | Ada | Ada | `PASS` | `evidence/M8/kmem.objdump.txt` |
| 12 | Build kernel dengan `kmem.c` | Berhasil tanpa error | Berhasil | `PASS` | `make clean && make build` |
| 13 | QEMU: `[M8] kmem initialized` | Ada | Ada | `PASS` | `evidence/M8/m8-qemu-serial.log` |
| 14 | QEMU: `[M8] heap probe alloc/free roundtrip ok` | Ada | Ada | `PASS` | `evidence/M8/m8-qemu-serial.log` |
| 15 | Timer tick M5 masih berjalan setelah M8 | `ticks=...` muncul | Muncul | `PASS` | `evidence/M8/m8-qemu-serial.log` |
| 16 | Preflight script | `[PASS] M8 preflight completed.` | Lulus | `PASS` | Terminal output |

### 13.2 Log Serial QEMU Lengkap (Bagian Penting)

```text
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M4 kernel entered
kernel_start=0xffffffff80000000
kernel_end=0xffffffff80208040
rflags_before_idt=0x0000000000000082
idt_base=0xffffffff80005000
idt_limit=0x0000000000000fff
[M4] IDT loaded
[M4] selftest: IDT invariants passed
[M6] PMM initialized
pmm_frame_count=0x0000000000020000
pmm_free_frames=0x000000000001fd9e
pmm_used_frames=0x0000000000000262
pmm_sample_frame=0x0000000000001000
[M6] sample alloc/free roundtrip ok
[M7] VMM core initialized
vmm_root_paddr=0x0000000000001000
[M7] VMM map/query/unmap smoke test passed
[M7] ready for QEMU smoke test and GDB audit
[M8] kmem initialized
kmem_total=0x000000000000fff0
kmem_free=0x000000000000ffc0
kmem_largest=0x000000000000ffc0
kmem_blocks=0x0000000000000001
[M8] heap probe alloc/free roundtrip ok
[M4] IDT and exception dispatch path installed
[M5] ready for QEMU smoke test and GDB audit
ticks=0x0000000000000064
ticks=0x00000000000000c8
... [berlanjut]
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| `test_kmem.log` | `evidence/M8/test_kmem.log` | Output host unit test: `M8 kmem host tests: PASS` |
| `nm_u.txt` | `evidence/M8/nm_u.txt` | Bukti `nm -u` kosong (tidak ada undefined symbol) |
| `readelf_h.txt` | `evidence/M8/readelf_h.txt` | Bukti ELF64 x86-64 REL freestanding |
| `kmem.objdump.txt` | `evidence/M8/kmem.objdump.txt` | Disassembly dengan semua 6 simbol allocator |
| `m8-qemu-serial.log` | `evidence/M8/m8-qemu-serial.log` | Serial log boot QEMU penuh |
| `manifest.txt` | `evidence/M8/manifest.txt` | Manifest toolchain dan daftar file |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

```text
Host unit test berhasil karena implementasi kmem.c mengikuti prinsip
heap allocator deterministik: satu blok besar saat init, split saat
alloc, coalesce saat free. Skenario fragmentasi membuktikan bahwa setelah
16 alloc dan 16 free (dalam urutan berbeda), seluruh blok berhasil
dicoalesce menjadi satu blok bebas tunggal — ini membuktikan coalesce
forward dan backward keduanya bekerja.

Audit freestanding lulus (nm -u kosong) karena kmem.c sama sekali tidak
menggunakan fungsi libc. kmem_memset menggantikan memset; tidak ada
printf, malloc, atau fungsi runtime lain. Flag -ffreestanding -fno-builtin
memastikan compiler tidak menyisipkan panggilan runtime implisit.

Build kernel berhasil karena Makefile sudah dikonfigurasi di M6/M7 untuk
menemukan semua file .c di bawah kernel/ secara otomatis (find kernel src
-name '*.c'). Direktori kernel/mm/ yang baru langsung ter-include tanpa
perlu mengubah Makefile lebih lanjut.

QEMU smoke test berhasil karena arena bootstrap m8_boot_heap[65536] berada
di .bss kernel yang sudah di-zero dan terpetakan oleh bootloader Limine
sebelum kmain dipanggil. kmem_total = 0xfff0 (65520 byte, sedikit kurang
dari 64 KiB karena alignment) dan kmem_blocks = 1 (satu blok free besar
saat init) sesuai ekspektasi.

Timer tick M5 masih berjalan setelah M8 membuktikan integrasi heap tidak
merusak IDT, PIC, atau PIT yang sudah berjalan.
```

### 14.2 Analisis Kegagalan dan Bug yang Ditemukan

**Bug 1: `build/m8/` hilang setelah `make clean`**

```text
Gejala: cp gagal "No such file or directory" saat menyalin ke evidence/M8/
Penyebab: make clean menghapus seluruh build/, termasuk build/m8/,
  sebelum evidence disalin. Urutan semula adalah make clean → make build →
  cp build/m8/*.* evidence/M8/, tetapi build/m8/ tidak dibuat oleh
  make build (yang hanya membuat build/normal/ dan build/kernel.elf).
Perbaikan: jalankan make m8-all terlebih dahulu setelah make clean && make build
  untuk meregenerasi build/m8/ dan semua artefak M8.
Deteksi: error cp eksplisit dengan pesan path.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai? | Penjelasan |
|---|---|---|---|
| First-fit free-list | Scan dari `g_head`, ambil blok free pertama yang cukup | Sesuai | Paling sederhana; O(n) scan; cukup untuk kernel pendidikan |
| Split blok | `kmem_split_if_useful` membagi blok jika `sisa >= header + KMEM_MIN_SPLIT` | Sesuai | Mencegah fragmen terlalu kecil; nilai 32 byte mencegah overhead berlebihan |
| Coalesce forward | `kmem_coalesce_forward` menggabungkan next free berulang sampai tidak ada lagi | Sesuai | Referensi Linux: coalescing mencegah fragmentasi eksternal |
| Coalesce backward | Saat free, jika `block->prev->free`, panggil `coalesce_forward(block->prev)` | Sesuai | Menggabungkan blok free prev dengan blok yang baru dibebaskan |
| Alignment 16 byte | `kmem_align_up_size(bytes, 16)` sebelum mencari blok | Sesuai | Memenuhi ABI x86_64 dan kebutuhan SIMD |
| Magic header | `KMEM_MAGIC = 0x4d43534f53484541` pada setiap blok | Sesuai | Membantu deteksi corruption; bukan mekanisme keamanan |
| Overflow guard calloc | `count != 0 && bytes > SIZE_MAX/count` | Sesuai | Standar implementasi defensive programming |

### 14.4 Analisis Statistik Heap dari QEMU

```text
kmem_total  = 0xfff0 = 65520 byte
  → Arena 65536 byte (64 KiB) dikurangi alignment overhead header pertama
kmem_free   = 0xffc0 = 65472 byte
  → 65520 - sizeof(kmem_block_t) yang dipakai header pertama = 65472 byte
kmem_largest = 0xffc0 = 65472 byte
  → Satu blok free besar (belum ada alokasi selain probe yang sudah dibebaskan)
kmem_blocks  = 1
  → Probe alloc 128 byte sudah dibebaskan dan dicoalesce kembali ke satu blok

Probe alloc/free roundtrip:
  alloc(128) → satu blok 128 byte digunakan, list punya 2 blok
  free_checked(probe) → blok dibebaskan, coalesce → kembali ke 1 blok
  Hasil: kmem_blocks=1 setelah m8_heap_bootstrap selesai
```

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Perbaikan |
|---|---|---|---|
| `build/m8/` hilang setelah `make clean` | `cp: No such file or directory` saat menyalin evidence | `make clean` menghapus semua `build/`; `build/m8/` tidak dibuat oleh `make build` | Jalankan `make m8-all` sebelum menyalin ke `evidence/M8/` |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Gejala | Penyebab | Mitigasi |
|---|---|---|---|
| Arena heap belum terpetakan | QEMU reset/hang saat `kmem_init` | Arena di virtual address yang tidak hadir di page table | Gunakan arena `.bss` yang sudah terpetakan bootloader; jangan pakai virtual address tinggi sebelum VMM stabil |
| `nm -u` tidak kosong | `memset`, `printf`, atau `malloc` muncul | Penggunaan fungsi libc langsung atau compiler builtin | Gunakan `-ffreestanding -fno-builtin`; gunakan `kmem_memset` internal |
| Double free tidak terdeteksi | Korupsi heap diam-diam | Flag `free` tidak dicek sebelum membebaskan | `kmem_free_checked` memeriksa `block->free`; test double-free lulus |
| Fragmentasi permanen | `kmem_alloc` gagal meski total free besar | Blok free tidak dicoalesce | `kmem_coalesce_forward` dipanggil dari `kmem_free_checked` |
| `sizeof(kmem_block_t)` bukan kelipatan 16 | Alignment payload gagal | Struct tidak memiliki padding yang cukup | Field `reserved` dan `reserved2` ditambahkan untuk padding |
| Allocator dari IRQ context | Korupsi heap setelah timer tick | Reentrancy tanpa lock | Documented constraint; tidak boleh dipanggil dari IRQ handler di M8 |
| Page fault saat `kmem_alloc` | QEMU triple fault | Metadata blok melintas batas page yang belum dipetakan | Gunakan arena `.bss` kecil sementara; map semua page arena sebelum init |

### 15.3 Triage yang Dilakukan

```text
Bug build/m8/ hilang terdeteksi dari pesan error cp yang eksplisit.
Diagnosis dilakukan dengan melihat urutan perintah: make clean menghapus
build/, tetapi make m8-all belum dijalankan ulang setelah itu. Perbaikan
langsung dengan menjalankan make m8-all terlebih dahulu.

Tidak ada bug logika allocator yang ditemukan selama praktikum. Semua
4 skenario host test lulus tanpa modifikasi source. Ini konsisten dengan
panduan yang menyatakan source M8 sudah divalidasi di lingkungan penyusunan.
```

### 15.4 Rancangan GDB untuk Debugging Heap

```text
Jika kmem_init gagal atau kernel hang saat heap init:

Terminal 1 (QEMU):
  qemu-system-x86_64 -machine q35 -m 512M -serial stdio \
    -no-reboot -no-shutdown -s -S -cdrom build/mcsos.iso

Terminal 2 (GDB):
  gdb build/kernel.elf
  (gdb) target remote localhost:1234
  (gdb) break m8_heap_bootstrap
  (gdb) break kmem_init
  (gdb) break kmem_alloc
  (gdb) continue
  (gdb) info registers
  (gdb) x/32gx &m8_boot_heap  # inspeksi arena
  (gdb) p sizeof(kmem_block_t)  # verifikasi ukuran header

Jika page fault, CR2 dari handler M4 menunjukkan alamat yang menyebabkan
fault. Jika CR2 berada di sekitar alamat m8_boot_heap, arena belum
terpetakan atau pointer header melampaui batas page.
```

---

## 16. Prosedur Rollback

| Skenario | Perintah | Status |
|---|---|---|
| Kembali ke baseline M7 | `git switch m7-vmm-core` | Belum diuji aktual; branch M7 masih ada |
| Nonaktifkan heap saja | Hapus pemanggilan `m8_heap_bootstrap()` dari `kmain.c` | Paling aman; `kmem.c` tetap ada untuk host test |
| Bersihkan artefak M8 | `make m8-clean` | Teruji — menghapus `build/m8/` saja |
| Build ulang M7 setelah rollback | `make clean && make build` | Belum diuji aktual |

Catatan rollback:

```text
Branch M8 (praktikum-m8-kernel-heap) terpisah dari branch M7 (m7-vmm-core).
Rollback ke M7 dapat dilakukan dengan git switch tanpa risiko kehilangan
data M8. Rollback belum diuji aktual karena M8 berhasil tanpa perlu rollback.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Mitigasi pada M8 | Known limitation |
|---|---|---|
| Double free | Flag `block->free` dicek; kode -4 dikembalikan | Tidak ada poison pattern setelah free |
| Out-of-arena pointer | `kmem_ptr_in_heap()` memeriksa batas | Tidak ada guard page di sekitar arena |
| Integer overflow calloc | `bytes > SIZE_MAX/count` sebelum perkalian | Terverifikasi host test |
| Metadata corruption | Magic check di `kmem_validate` | Magic dapat disiapkan oleh attacker; bukan mekanisme keamanan produksi |
| Use-after-free | Tidak ada mitigasi | Pointer yang dikembalikan tidak di-poison |
| Heap overrun | Tidak ada guard/canary | Red-zone adalah pengayaan |
| Allocator dari IRQ | Documented constraint | Tidak ada runtime enforcement lock |

### 17.2 Reliability

| Aspek | Status M8 | Catatan |
|---|---|---|
| Double-free protection | ✅ `kode -4` | Terverifikasi host test |
| Null pointer free | ✅ No-op sukses | `ptr == NULL → return 0` |
| Overflow guard calloc | ✅ Return NULL | Terverifikasi host test |
| Coalescing | ✅ Forward dan backward | Terverifikasi skenario fragmentasi |
| Magic validation | ✅ Di alloc, free, validate | Deteksi corruption dini |
| Freestanding | ✅ `nm -u` kosong | Tidak ada dependency libc |
| Build dengan `-Werror` | ✅ Lulus | Semua warning dianggap error |

---

## 18. Pembagian Kerja

Tidak berlaku — praktikum dikerjakan secara individu.

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Repository dapat dibangun dari clean checkout | `PASS` | `make clean && make build` berhasil |
| `make m8-kmem-host-test` lulus | `PASS` | `M8 kmem host tests: PASS` |
| `nm -u build/m8/kmem.freestanding.o` kosong | `PASS` | `evidence/M8/nm_u.txt` kosong |
| `readelf -h` menunjukkan ELF64 x86-64 | `PASS` | `evidence/M8/readelf_h.txt` |
| Semua 6 simbol allocator ada di disassembly | `PASS` | `evidence/M8/kmem.objdump.txt` |
| Preflight script lulus | `PASS` | `[PASS] M8 preflight completed.` |
| QEMU log `[M8] kmem initialized` | `PASS` | `evidence/M8/m8-qemu-serial.log` |
| Tidak ada warning pada build | `PASS` | `-Werror` aktif; build lulus |
| Perubahan Git terkomit | `PASS` | Commit `d9fadc3` |
| Laporan berisi log, test, audit evidence | `PASS` | Bagian 12, 13, Lampiran |

| Kriteria pengayaan | Status | Catatan |
|---|---|---|
| Page-backed heap growth | `NA` | Membutuhkan VMM stabil dan mapping rentang virtual heap |
| Red zone / canary | `NA` | Tidak diimplementasikan; disebutkan sebagai known limitation |
| Slab/cache allocator | `NA` | Konsep dijelaskan di teori; tidak diimplementasikan |
| GDB session heap | `NA` | Rancangan ada di bagian 15.4; tidak dijalankan |

---

## 20. Readiness Review

| Status | Definisi | Pilihan |
|---|---|---|
| Belum siap uji | Build/test belum stabil | `[ ]` |
| Siap uji QEMU | Build bersih, QEMU berjalan, log tersedia | `✓` |
| Siap demonstrasi praktikum | Siap ditunjukkan dengan bukti uji, failure mode, rollback | `[ ]` |
| Kandidat siap pakai terbatas | Setelah test, security review, dokumentasi lengkap | `[ ]` |

Alasan readiness:

```text
Build bersih untuk semua target (host test, freestanding object, kernel.elf).
Host unit test lulus 4 skenario mencakup basic alloc/free, calloc+overflow,
double-free rejection, dan fragmentasi+coalesce. nm -u kosong. ELF64 x86-64
terverifikasi. Semua 6 simbol allocator ada di disassembly. Build kernel
berhasil dengan kmem.c terintegrasi. QEMU smoke test menampilkan [M8] kmem
initialized dengan statistik heap yang benar (total=65520, satu blok besar,
probe roundtrip lulus). Timer tick M5 masih berjalan normal setelah heap init.

Status siap demonstrasi belum diklaim karena: (1) rollback belum diuji
aktual; (2) GDB session formal pada heap belum dilakukan; (3) use-after-free
dan heap overrun tidak terproteksi; (4) allocator dari IRQ context tidak
memiliki runtime enforcement; (5) arena masih statik, belum page-backed.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | Arena heap statik 64 KiB | Kapasitas terbatas; tidak tumbuh otomatis | Cukup untuk kernel early; pengayaan page-backed heap | M8 pengayaan atau M9+ |
| 2 | Tidak ada proteksi use-after-free | Pointer yang dibebaskan dapat diakses kembali tanpa deteksi | Documented constraint | Pengayaan: pointer poison pattern |
| 3 | Tidak ada proteksi heap overrun | Menulis melewati batas payload merusak header blok berikutnya | `kmem_validate()` mendeteksi setelah fakta | Pengayaan: red zone/canary |
| 4 | Allocator tidak reentrant | Pemanggilan dari IRQ handler korupsi heap | Documented; larang panggilan dari IRQ | M9+ (lock/spinlock) |
| 5 | `kmem_validate()` O(n) setiap free | Overhead performa di production | Acceptable untuk kernel pendidikan | Production: validate hanya saat debug |
| 6 | Rollback belum diuji aktual | Risiko M7 tidak berjalan setelah rollback | Branch terpisah memungkinkan rollback via git switch | Sebelum demonstrasi M8 |

Keputusan akhir:

```text
Berdasarkan bukti make m8-all lulus, nm -u kosong, ELF64 x86-64 terverifikasi,
semua 6 simbol allocator ada di disassembly, QEMU log [M8] kmem initialized
dengan statistik heap benar, dan heap probe alloc/free roundtrip lulus, hasil
praktikum M8 ini layak disebut siap uji QEMU untuk kernel heap awal. M8 tidak
mengklaim siap demonstrasi praktikum karena rollback belum diuji, GDB session
heap belum dilakukan, dan beberapa known limitations keamanan belum dimitigasi.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---:|---:|---|---:|
| Kebenaran fungsional | 30 | API bekerja, host test lulus 4 skenario, split/coalesce/validate benar, heap init di QEMU | `28` |
| Kualitas desain dan invariants | 20 | 12 invariant terdokumentasi, kontrak eksplisit, pemisahan lapisan PMM/VMM/heap jelas | `18` |
| Pengujian dan bukti | 20 | Host test, nm -u, readelf, objdump, QEMU log, evidence manifest lengkap | `19` |
| Debugging dan failure analysis | 10 | Bug ditemukan dan diperbaiki, failure modes diantisipasi, GDB rancangan ada | `9` |
| Keamanan dan robustness | 10 | Double-free, overflow calloc, out-of-arena ditangani; known limitations documented | `8` |
| Dokumentasi dan laporan | 10 | Laporan rapi, reproducible, referensi IEEE, commit hash, evidence lengkap | `9` |
| **Total** | **100** | | `91` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

```text
Seluruh komponen wajib M8 berhasil:
- include/mcsos/kmem.h: API lengkap dengan KMEM_ALIGN=16, KMEM_MAGIC,
  struct kmem_stats_t, dan 6 deklarasi fungsi.
- kernel/mm/kmem.c: first-fit free-list allocator lengkap dengan split,
  coalesce forward/backward, magic validation, double-free rejection,
  out-of-arena rejection, overflow guard calloc, dan validate O(n).
- tests/test_kmem.c: 4 skenario host test lulus semua assertion.
- make m8-all: host test PASS, nm -u kosong, ELF64 x86-64, disassembly
  semua 6 simbol ada.
- scripts/check_m8_kmem.sh: [PASS] M8 preflight completed.
- Build kernel: kernel/mm/kmem.c masuk ke kernel.elf tanpa error.
- QEMU smoke test: [M8] kmem initialized, statistik benar (total=65520,
  blocks=1), probe roundtrip ok, timer M5 tetap berjalan.
- 1 bug operasional ditemukan dan diperbaiki (build/m8/ hilang setelah
  make clean).
- 1 commit bersih d9fadc3 di branch praktikum-m8-kernel-heap.
- 6 artefak evidence + manifest terkumpul di evidence/M8.
```

### 22.2 Yang Belum Berhasil

```text
- GDB session formal pada heap (break m8_heap_bootstrap, x/32gx &m8_boot_heap)
  tidak dilakukan karena tidak ada failure yang memerlukan debugging lebih lanjut.
- Rollback belum diuji aktual.
- Use-after-free, heap overrun, dan IRQ reentrancy tidak terproteksi;
  ini adalah known limitations yang didokumentasikan, bukan kegagalan wajib M8.
- Page-backed heap growth tidak diimplementasikan; ini pengayaan.
```

### 22.3 Rencana Perbaikan

```text
Sebelum demonstrasi M8:
1. Uji rollback aktual ke commit 59ebb47 (M7) dan verifikasi M7 masih
   berjalan, kemudian kembali ke M8.
2. Jalankan GDB session formal: break m8_heap_bootstrap, inspeksi
   m8_boot_heap, verifikasi header kmem_block_t di memori.

Untuk M9+:
1. Implementasikan page-backed heap growth: alokasi frame PMM, pemetaan
   VMM, lalu kmem_init pada rentang virtual yang sudah dipetakan.
2. Tambahkan spinlock/mutex untuk reentrancy guard saat SMP diimplementasikan.
3. Pertimbangkan pointer poison pattern setelah free untuk mendeteksi
   use-after-free.
4. Pertimbangkan red zone/canary untuk mendeteksi heap overrun lebih awal.
```

---

## 23. Lampiran

### Lampiran A — Commit Log

```text
d9fadc3 (HEAD -> praktikum-m8-kernel-heap) M8: implement kernel heap allocator, host test, audit, and QEMU integration
59ebb47 (m7-vmm-core) M7: add evidence manifest, QEMU log, and audit artifacts
382e235 M7: integrate VMM into kernel, QEMU smoke test passed
0161a40 M7: implement VMM core, host unit test, and audit scripts
be7a196 (praktikum/m6-pmm) M6: implement bitmap PMM, host unit test, and kernel integration
```

### Lampiran B — Diff Ringkas

```text
File baru pada commit M8 (d9fadc3):
  include/mcsos/kmem.h     — 34 baris header API
  kernel/mm/kmem.c         — 202 baris implementasi allocator
  tests/test_kmem.c        — 76 baris host unit test
  scripts/check_m8_kmem.sh — 45 baris script preflight
  evidence/M8/             — 6 file artefak + manifest

File yang diubah:
  kernel/core/kmain.c — +27 baris: include kmem.h, arena 64 KiB,
                        m8_heap_bootstrap(), pemanggilan setelah m7_vmm_init()
  Makefile            — +28 baris: variabel CFLAGS_M8_*, BUILD_M8,
                        target m8-clean/freestanding/host-test/audit/all
```

### Lampiran C — Log Build (Ringkas)

```text
[make clean && make build — 13 file C + 1 file .S]
clang ... -c kernel/arch/x86_64/idt.c
clang ... -c kernel/arch/x86_64/pic.c
clang ... -c kernel/arch/x86_64/pit.c
clang ... -c kernel/core/kmain.c
clang ... -c kernel/core/log.c
clang ... -c kernel/core/panic.c
clang ... -c kernel/core/serial.c
clang ... -c kernel/core/trap.c
clang ... -c kernel/lib/memory.c
clang ... -c kernel/mm/kmem.c        ← baru M8
clang ... -c src/pmm.c
clang ... -c src/vmm.c
clang ... -c kernel/arch/x86_64/isr.S
ld.lld -nostdlib -static ... -o build/kernel.elf [13 object files]
[Tidak ada error/warning]
```

### Lampiran D — Log QEMU Penuh (Bagian M8)

```text
[M8] kmem initialized
kmem_total=0x000000000000fff0
kmem_free=0x000000000000ffc0
kmem_largest=0x000000000000ffc0
kmem_blocks=0x0000000000000001
[M8] heap probe alloc/free roundtrip ok
```

### Lampiran E — Output Host Unit Test

```text
M8 kmem host tests: PASS
```

### Lampiran F — Output nm, readelf, objdump (Ringkas)

```text
[nm -u build/m8/kmem.freestanding.o — kosong]

[readelf -h build/m8/kmem.freestanding.o — ringkas]
Class:   ELF64
Data:    2's complement, little endian
Type:    REL (Relocatable file)
Machine: Advanced Micro Devices X86-64

[objdump — simbol utama]
0000000000000000 <kmem_init>:
00000000000001c0 <kmem_validate>:
0000000000000390 <kmem_alloc>:
0000000000000670 <kmem_calloc>:
0000000000000750 <kmem_free_checked>:
00000000000009c0 <kmem_get_stats>:
```

### Lampiran G — Isi `evidence/M8/manifest.txt`

```text
MCSOS M8 evidence manifest
timestamp_utc=2026-06-21T06:29:35Z
commit=59ebb474782155ced259d90f0bd1b17c81f2c0fd
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
lld=Ubuntu LLD 21.1.8 (compatible with GNU linkers)
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
kmem.objdump.txt
m8-qemu-serial.log
manifest.txt
nm_u.txt
readelf_h.txt
test_kmem.log
```

### Lampiran H — Jawaban Pertanyaan Analisis

**1. Perbedaan PMM, VMM, dan kernel heap?**
PMM mengelola frame fisik 4 KiB — unit terkecil yang dapat dialokasikan adalah satu frame. VMM mengelola pemetaan virtual ke physical melalui page table — unit terkecil adalah satu halaman 4 KiB virtual. Kernel heap mengelola objek berukuran byte di atas arena virtual yang sudah dipetakan — dapat mengalokasikan objek sekecil satu byte (meski selalu di-align ke 16). Ketiganya berjenjang: PMM menyediakan frame untuk VMM; VMM memetakan frame menjadi range virtual; heap memanfaatkan range virtual tersebut untuk manajemen objek.

**2. Mengapa kernel memerlukan allocator dinamis setelah boot awal?**
Variabel statik dibuat saat kompilasi dengan ukuran tetap. Struktur runtime seperti deskriptor proses, metadata file, buffer I/O, node timer, dan packet buffer memiliki jumlah yang tidak diketahui saat kompilasi dan berubah selama runtime. Tanpa allocator dinamis, kernel harus menggunakan array statis dengan ukuran maksimum yang sering terbuang sia-sia atau terlalu kecil.

**3. Mengapa `kmem_free_checked` mengembalikan kode error, bukan void?**
Karena pembebasan dapat gagal dengan beberapa cara: pointer NULL (bukan error, tapi harus dibedakan), pointer di luar arena, pointer tidak aligned, magic korup (corruption), atau double free. Jika free tidak mengembalikan kode, caller tidak dapat membedakan free sukses dari double free atau corruption, menyulitkan diagnosis bug. Kode negatif memungkinkan caller (atau host test) memeriksa hasil setiap operasi.

**4. Mengapa `kmem_validate()` dipanggil setiap `free`, bukan hanya saat debug?**
Pada kernel pendidikan M8, correctness lebih penting dari performa. Memanggil validate setiap free memastikan linked list selalu konsisten setelah modifikasi; jika tidak, validate mengembalikan kode negatif yang dapat memicu panic sebelum korupsi menyebar lebih jauh. Pada production allocator, validate dipindahkan ke mode debug saja karena overhead O(n) tidak dapat diterima.

**5. Risiko menggunakan allocator dari interrupt handler?**
Interrupt handler dapat mempreempt kode kernel mana pun, termasuk kode yang sedang memodifikasi linked list heap. Jika interrupt handler juga memanggil kmem_alloc atau kmem_free, ia dapat mengakses linked list dalam keadaan tidak konsisten (misalnya saat blok sedang di-split atau prev/next sedang diperbarui). Ini menyebabkan korupsi heap yang sulit dideteksi. Solusi pada SMP adalah spinlock yang me-disable interrupt selama operasi heap.

**6. Mengapa arena bootstrap menggunakan `.bss`, bukan virtual address tinggi?**
`.bss` sudah di-zero dan dipetakan oleh bootloader sebelum kmain dipanggil. Virtual address tinggi memerlukan VMM M7 yang stabil dan mapping eksplisit sebelum dapat diakses. Menggunakan arena `.bss` terlebih dahulu memisahkan risiko: bug allocator tidak langsung menjadi triple fault akibat page not present. Setelah allocator terbukti benar, page-backed heap dapat ditambahkan sebagai pengayaan.

**7. Mengapa `sizeof(kmem_block_t)` harus kelipatan `KMEM_ALIGN`?**
Payload block dimulai tepat setelah header: `payload = (char*)block + sizeof(kmem_block_t)`. Jika `sizeof(kmem_block_t)` bukan kelipatan 16, dan blok berada di alamat yang aligned 16, maka payload tidak aligned 16. Field `reserved` dan `reserved2` ditambahkan ke struct untuk memastikan ukuran header adalah kelipatan 16 sehingga payload selalu aligned 16 selama blok itu sendiri aligned 16.

**8. Apa yang terjadi jika magic header ditimpa oleh heap overrun?**
`kmem_validate()` akan mengembalikan kode -6 (`magic != KMEM_MAGIC`) saat menelusuri linked list dan menemukan blok dengan magic yang korup. Ini memungkinkan deteksi corruption lebih awal daripada crash acak. Namun, jika validate tidak dipanggil segera setelah overrun, blok berikutnya mungkin sudah di-allocated ke caller lain, menyebabkan dua caller menulis ke area fisik yang sama.

---

## 24. Daftar Referensi

```text
[1] Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's
    Manual, Volume 3: System Programming Guide," Intel Developer Documentation,
    2026. [Online]. Available:
    https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
    Accessed: Jun. 21, 2026.

[2] Advanced Micro Devices, "AMD64 Architecture Programmer's Manual Volume 2:
    System Programming," AMD, Rev. 3.44, Mar. 2026. [Online]. Available:
    https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2
    Accessed: Jun. 21, 2026.

[3] The Linux Kernel Organization, "Memory Allocation Guide," Linux Kernel
    Documentation, 2026. [Online]. Available:
    https://www.kernel.org/doc/html/latest/core-api/memory-allocation.html
    Accessed: Jun. 21, 2026.

[4] The Linux Kernel Organization, "The Slab Allocator: An Object-Caching
    Kernel Memory Allocator," Linux Kernel Documentation, 2026. [Online].
    Available:
    https://www.kernel.org/doc/html/latest/core-api/mm-api.html
    Accessed: Jun. 21, 2026.

[5] Free Software Foundation, "GNU Make Manual," GNU Project, 2026.
    [Online]. Available:
    https://www.gnu.org/software/make/manual/make.html
    Accessed: Jun. 21, 2026.

[6] Free Software Foundation, "Using LD, the GNU linker — Scripts," GNU
    Binutils Documentation, 2026. [Online]. Available:
    https://sourceware.org/binutils/docs/ld/Scripts.html
    Accessed: Jun. 21, 2026.

[7] LLVM Project, "Clang command line argument reference," LLVM Documentation,
    2026. [Online]. Available:
    https://clang.llvm.org/docs/ClangCommandLineReference.html
    Accessed: Jun. 21, 2026.

[8] QEMU Project, "QEMU System Emulation — Invocation," QEMU Documentation,
    2026. [Online]. Available:
    https://www.qemu.org/docs/master/system/invocation.html
    Accessed: Jun. 21, 2026.

[9] QEMU Project, "GDB usage — QEMU documentation," QEMU System Emulation
    Documentation, 2026. [Online]. Available:
    https://www.qemu.org/docs/master/system/gdb.html
    Accessed: Jun. 21, 2026.
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
| Artefak penting tersedia di `evidence/M8` | `Ya` |
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
d9fadc3
```

Status akhir yang diklaim:

```text
Siap uji QEMU untuk kernel heap awal — bukan siap produksi, dengan known
issues pada bagian 20 yang harus ditindaklanjuti (arena statik belum
page-backed, use-after-free tidak terproteksi, heap overrun tidak ada guard,
allocator tidak reentrant, rollback belum diuji aktual, GDB session heap
formal belum dilakukan).
```

Ringkasan satu paragraf:

```text
Praktikum M8 MCSOS 260502 telah diselesaikan oleh Agung Nurjaman
(25832073010) secara individu, melanjutkan dari gate M7 yang solid (commit
59ebb47). Kernel heap awal berbasis first-fit free-list allocator berhasil
dibangun lengkap: kmem_block_t dengan magic header, split blok dengan
KMEM_MIN_SPLIT=32, coalesce forward dan backward, alignment payload 16 byte,
double-free rejection (kode -4), out-of-arena rejection (kode -1), overflow
guard calloc, validate O(n) yang dipanggil setiap free, dan helper
kmem_memset internal tanpa dependency libc. Host unit test lulus 4 skenario
mencakup basic alloc/free alignment, calloc zeroing dan overflow, double-free
rejection, serta fragmentasi dan coalescing penuh (free_count=1 setelah
semua blok dibebaskan). Audit freestanding membuktikan nm -u kosong, ELF64
x86-64 REL, dan semua 6 simbol allocator ada di disassembly. Build kernel
berhasil dengan kernel/mm/kmem.c ter-include otomatis oleh SRC_C. QEMU smoke
test membuktikan [M8] kmem initialized dengan statistik heap yang benar
(total=65520, blocks=1 setelah probe roundtrip), dan timer tick M5 tetap
berjalan normal setelahnya. Satu bug operasional ditemukan dan diperbaiki
(build/m8/ hilang setelah make clean). Commit M8 (d9fadc3) tersimpan bersih
di branch praktikum-m8-kernel-heap dengan 6 artefak evidence dan manifest
toolchain. Status readiness yang diklaim adalah siap uji QEMU untuk kernel
heap awal, secara eksplisit bukan siap produksi, dengan enam known issues
yang didokumentasikan untuk ditindaklanjuti sebelum demonstrasi dan milestone
berikutnya.
```
