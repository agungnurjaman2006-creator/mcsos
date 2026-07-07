# Block Device Layer, RAM Block Driver, Buffer Cache Minimal, dan Jalur Persiapan Filesystem Persistent pada MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M14_Agung.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, subset POSIX-like
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `M14` |
| Judul praktikum | `Block Device Layer, RAM Block Driver, Buffer Cache Minimal, dan Jalur Persiapan Filesystem Persistent pada MCSOS 260502` |
| Jenis pengerjaan | `Individu` |
| Nama mahasiswa | `Agung Nurjaman` |
| NIM | `25832073010` |
| Kelas | `Pendidikan Teknologi Informasi` |
| Nama kelompok | `-` |
| Anggota kelompok | `-` |
| Tanggal praktikum | `2026-06-30` |
| Tanggal pengumpulan | `2026-06-30` |
| Repository | `/home/agung/src/mcsos` |
| Branch | `praktikum-m14-block-device` |
| Commit awal | `8b8d15a` (M13) |
| Commit akhir | `45893d1` (M14) |
| Status readiness yang diklaim | `Siap uji QEMU untuk block device layer, RAM block driver, dan buffer cache minimal — bukan bukti storage persistent aman terhadap power-loss, bukan bukti driver hardware siap, bukan bukti filesystem crash-consistent` |

---

## 1. Sampul

# Laporan Praktikum M14
## Block Device Layer, RAM Block Driver, Buffer Cache Minimal, dan Jalur Persiapan Filesystem Persistent pada MCSOS 260502

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
Panduan resmi praktikum M14 MCSOS 260502 digunakan sebagai referensi utama
dan kontrak implementasi untuk seluruh komponen (block.h, block.c,
ramblk.c, bcache.c, test_m14_block.c, Makefile.m14, scripts/m14_preflight.sh,
block_demo.c, dan pola integrasi kmain.c). Linux Kernel Documentation
(block layer dan blk-mq) serta dokumentasi null_blk digunakan sebagai
referensi konsep pemisahan request block I/O dari driver fisik dan nilai
perangkat blok sintetis untuk menguji lapisan block I/O tanpa media fisik.
Dokumentasi QEMU digunakan untuk opsi mesin, drive, format=raw, dan gdbstub.
Dokumentasi Clang freestanding dan GNU Binutils digunakan untuk audit
ELF object. AI assistant (Claude) digunakan untuk: (1) menulis draft awal
block.h (status code, struct device, operation table, RAM block metadata,
buffer cache metadata), block.c (registry, validasi range LBA, wrapper
read/write/flush), ramblk.c (driver RAM-backed tanpa dynamic allocation),
bcache.c (buffer cache write-back dengan clock-hand eviction dan dirty
flag), serta test_m14_block.c (host unit test) sesuai kontrak panduan
resmi; (2) menulis Makefile.m14 dengan target host-test/freestanding/audit,
mengikuti pola Makefile.m11 dan Makefile.m12 yang sudah ada di repository
(file Makefile terpisah dengan tab sebagai RECIPEPREFIX standar, bukan
'>' seperti Makefile utama); (3) menulis block_demo.c sebagai initializer
kernel yang memanggil panic path M3 jika init gagal, sesuai kontrak
panduan; (4) menulis Python patch untuk menyisipkan forward declaration
dan pemanggilan m14_block_demo_init() ke kmain.c setelah
m13_vfs_bootstrap() tanpa mengganggu kode M9-M13 yang sudah ada.
Seluruh build, host unit test, audit nm/readelf/objdump/sha256sum, QEMU
smoke test, dan commit git dijalankan dan diverifikasi sendiri oleh
mahasiswa di WSL 2 miliknya. AI tidak digunakan untuk mengubah kontrak
fungsional di luar yang ditentukan panduan resmi (invariant block device,
invariant RAM block driver, dan invariant buffer cache di bagian 11
panduan).
```

---

## 3. Tujuan Praktikum

1. Mendesain kontrak block device yang memisahkan registry, operasi driver (`read`/`write`/`flush`), validasi range LBA, dan kode error eksplisit (`MCSOS_BLK_OK`, `EINVAL`, `ERANGE`, `EFULL`, `EIO`, `ENODEV`).
2. Mengimplementasikan RAM block driver (`mcsos_ramblk_*`) yang deterministik, tidak bergantung pada dynamic allocation, dan dapat diuji di host.
3. Mengimplementasikan buffer cache minimal (`mcsos_bcache_*`) dengan field `valid`, `dirty`, `lba`, `dev`, eviction berbasis clock-hand, dan flush eksplisit.
4. Membuktikan dengan host unit test bahwa operasi read/write/flush dan validasi boundary (LBA out-of-range, count berlebih, count nol, buffer null) berjalan sesuai kontrak.
5. Menghasilkan object freestanding x86_64 (`block.o`, `ramblk.o`, `bcache.o`) tanpa undefined symbol setelah dilink menjadi satu relocatable object (`m14_block_layer.o`).
6. Menyusun bukti audit menggunakan `nm -u`, `readelf -h`, `objdump -dr`, `sha256sum`, log QEMU, dan laporan readiness.
7. Mengintegrasikan block layer ke kernel MCSOS setelah VFS/RAMFS M13 stabil, dengan initializer (`m14_block_demo_init`) yang mendaftarkan device `ram0` dan menjalankan smoke test write/read roundtrip.
8. Mengidentifikasi failure mode storage awal: out-of-range LBA, dirty buffer tidak di-flush, stale cache, block size mismatch, dan ketidakjelasan ownership buffer.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan perbedaan file-level I/O (VFS M13) dan block-level I/O (M14) | Bagian 6.1, 9.1; tabel perbandingan lapisan |
| Mendesain kontrak block device dengan registry, operation table, dan error code | `include/mcsos/block.h`: `mcsos_blk_device_t`, `mcsos_blk_ops_t`, `mcsos_blk_status_t` |
| Mengimplementasikan RAM block driver deterministik tanpa dynamic allocation | `kernel/block/ramblk.c`; storage dimiliki caller, tidak ada `malloc` |
| Mengimplementasikan buffer cache dengan valid/dirty/lba/dev dan flush eksplisit | `kernel/block/bcache.c`: `mcsos_bcache_entry_t`, `mcsos_bcache_flush_all` |
| Membuktikan read/write/flush dan boundary validation dengan host test | `tests/host/test_m14_block.c`; `M14 host tests PASS` |
| Menghasilkan object freestanding x86_64 tanpa undefined symbol | `make -f Makefile.m14 audit`; `nm -u` kosong pada `m14_block_layer.o` |
| Menyusun bukti audit nm/readelf/objdump/sha256sum/QEMU/laporan | Bagian 12, 13, Lampiran; `artifacts/m14/*` |
| Mengidentifikasi failure mode storage awal | Bagian 15; tabel failure modes |

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
| M11 | ELF64 loader, process image plan | `✓ selesai praktikum` |
| M12 | Spinlock, mutex, lock-order validator | `✓ selesai praktikum` |
| M13 | VFS minimal, RAMFS, FD table, syscall file I/O | `✓ selesai praktikum` |
| M14 | Block device layer, RAM block driver, buffer cache minimal | `✓ selesai praktikum` |
| M15 | Filesystem persistent berbasis blok | `[ ] tidak dibahas (target integrasi M14 ke depan)` |
| M16 | Networking stack | `[ ] tidak dibahas` |

Batas cakupan praktikum:

```text
M14 mencakup: block device registry (mcsos_blk_register/get/count/read/write/
flush), validasi LBA dan count terhadap block_count, RAM block driver
volatil (mcsos_ramblk_init dengan storage milik caller), buffer cache
minimal write-back dengan clock-hand eviction dan dirty flag
(mcsos_bcache_init/read/write/flush_all), host unit test, audit object
freestanding (nm -u, readelf, objdump, sha256sum), integrasi kernel
(block_demo.c) dengan fail-closed via KERNEL_PANIC, dan QEMU smoke test.

M14 TIDAK mencakup: driver disk hardware nyata (SATA/AHCI/NVMe/virtio-blk),
DMA, MSI/MSI-X, interrupt completion, virtqueue, filesystem persistent,
journal, fsck, crash consistency, POSIX storage ABI publik, security
boundary untuk pengguna, SMP-safe buffer cache (concurrency masih
single-core educational baseline), dan klaim produksi.
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

```text
M14 membangun lapisan block device di bawah VFS M13. Lima konsep utama:

1. File-level I/O vs Block-level I/O:
   VFS M13 bekerja pada abstraksi file: open, read, write, close dengan
   file descriptor. Block layer M14 bekerja pada abstraksi blok fisik:
   logical block address (LBA) dengan ukuran tetap (minimum 512 byte).
   File-level I/O pada akhirnya harus diterjemahkan ke block-level I/O
   saat filesystem persistent dibangun di M15+; M14 menyiapkan
   penerjemah tersebut tanpa langsung mengintegrasikannya ke VFS.

2. Block Device Registry dan Operation Table:
   Registry M14 menyimpan pointer ke device (bukan menyalin struct),
   sehingga lifetime device menjadi tanggung jawab pemilik. Operation
   table (mcsos_blk_ops_t) memisahkan caller dari implementasi driver:
   block layer memanggil dev->ops->read tanpa tahu apakah di baliknya
   adalah RAM, disk, atau perangkat masa depan. Pola ini identik dengan
   prinsip pemisahan block subsystem dari driver pada Linux modern.

3. RAM Block Driver sebagai Perangkat Sintetis:
   ramblk meniru block device menggunakan array memori statis milik
   caller. Tidak ada dynamic allocation di driver. Pendekatan ini
   sejalan dengan filosofi null_blk pada Linux: perangkat blok sintetis
   memungkinkan pengujian lapisan block I/O tanpa bergantung pada media
   fisik, PCIe, atau kompleksitas driver hardware.

4. Buffer Cache Write-Back:
   Cache M14 menyimpan satu blok per entry dengan flag dirty. Penulisan
   ke cache tidak langsung menulis ke device (write-back), melainkan
   menunggu flush eksplisit atau eviction. Ini lebih cepat dari
   write-through tetapi berisiko kehilangan data jika sistem crash
   sebelum flush — risiko ini harus dicatat secara eksplisit karena
   M14 belum memodelkan crash consistency.

5. Clock-Hand Eviction:
   Saat semua entry cache valid dan entry baru dibutuhkan, algoritma
   memilih korban secara round-robin (clock_hand) mulai dari posisi
   terakhir. Jika korban dirty, flush dijalankan terlebih dahulu sebelum
   entry dipakai ulang. Ini memastikan dirty entry tidak pernah hilang
   tanpa flush sukses — invariant kunci buffer cache M14.
```

### 6.2 Konsep Arsitektur dan Implementasi yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| LBA (Logical Block Address) | Setiap operasi block harus divalidasi `lba < block_count` | `mcsos_blk_validate_range`; host test LBA 32 (out of range untuk 32 blok) ditolak |
| Block size power-of-two, minimum 512 | Memastikan alignment dan indeks sederhana | `mcsos_is_power_of_two_u32`; host test pakai 512 byte |
| Overflow count vs block_count - lba | `count > block_count - lba` mendeteksi overflow tanpa wraparound | `(uint64_t)count > dev->block_count - lba` di `mcsos_blk_validate_range` |
| Driver operation table (function pointer) | Memisahkan caller dari implementasi driver | `mcsos_blk_ops_t { read, write, flush }` |
| Clock-hand (round-robin) eviction | Algoritma sederhana untuk memilih korban cache | `mcsos_bcache_select_victim` dengan `clock_hand % entry_count` |
| Freestanding C tanpa libc | Driver dan cache tidak boleh memakai `malloc`/`memcpy` libc | `mcsos_memcpy_u8` internal di setiap file; `nm -u` kosong |
| `-mno-red-zone` | Kernel freestanding tidak bergantung red-zone | Flag aktif di `CFLAGS_FREESTANDING` Makefile.m14 |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk block layer kernel; C17 hosted untuk host unit test |
| Runtime | Tanpa hosted libc di kernel; `mcsos_memcpy_u8` internal menggantikan `memcpy` |
| ABI | `x86_64-elf` (panduan M14) untuk audit standalone; `x86_64-unknown-none-elf` saat terintegrasi ke kernel via Makefile utama |
| Compiler flags kritis | `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, `-fno-pic`, `-mno-red-zone` |
| Tidak ada dynamic allocation | Storage RAM block driver dan data pool buffer cache disuplai caller, bukan dialokasikan driver/cache |
| Risiko UB | Cast `(mcsos_ramblk_t *)dev->driver_data` valid hanya jika `driver_data` memang menunjuk `mcsos_ramblk_t`; dijamin oleh `mcsos_ramblk_init` yang satu-satunya pengisi field tersebut |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Linux Kernel Documentation — Block Layer | Block I/O dan request handling | Prinsip pemisahan request block I/O dari perangkat fisik |
| [2] | Linux Kernel Documentation — blk-mq | Multi-queue block layer | Pembanding konseptual model single-queue M14 |
| [3] | Linux Kernel Documentation — null_blk | Block device sintetis | Dasar filosofi ramblk sebagai device tanpa media fisik |
| [4] | QEMU Documentation — System Emulation | `-drive`, `format=raw`, opsi mesin | Konfigurasi QEMU smoke test M14 |
| [5] | QEMU Documentation — GDB usage | `-s -S`, port 1234 | Rancangan GDB session M14 |
| [6] | LLVM/Clang Reference | `-ffreestanding` | Dasar kompilasi freestanding driver |
| [7] | GNU Binutils Documentation | `nm`, `readelf`, `objdump` | Audit object ELF M14 |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 Ubuntu 26.04 LTS (resolute) |
| Kernel host WSL | Linux 6.6.87.2-microsoft-standard-WSL2 |
| Target ISA | x86_64 |
| Target ABI standalone audit | `x86_64-elf` |
| Target ABI kernel | `x86_64-unknown-none-elf` |
| Emulator | QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3) |
| Boot path | Limine bootloader (third_party/limine), dilanjutkan dari M2–M13 |
| Build system | GNU Make 4.4.1 |
| Bahasa utama | C17 freestanding |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker (audit standalone) | GNU ld (GNU Binutils for Ubuntu) 2.46 |
| Linker (kernel) | Ubuntu LLD 21.1.8 (compatible with GNU linkers) |
| Binutils | GNU nm/readelf/objdump 2.46; sha256sum (uutils coreutils) 0.8.0 |

### 7.2 Versi Toolchain

```text
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
ld=GNU ld (GNU Binutils for Ubuntu) 2.46
nm=GNU nm (GNU Binutils for Ubuntu) 2.46
readelf=GNU readelf (GNU Binutils for Ubuntu) 2.46
objdump=GNU objdump (GNU Binutils for Ubuntu) 2.46
make=GNU Make 4.4.1
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
```

Diambil dari `artifacts/m14/tool_versions.txt`, commit `45893d1`.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | `Ya` |
| Remote repository | `-` (lokal) |
| Branch | `praktikum-m14-block-device` |
| Commit hash awal | `8b8d15a` (M13 selesai) |
| Commit hash akhir | `45893d1` (M14 selesai) |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── include/
│   └── mcsos/
│       ├── kmem.h            (M8)
│       ├── syscall.h         (M10)
│       ├── block.h           (baru M14)
│       └── user/
│           └── m11_elf_loader.h  (M11)
├── kernel/
│   ├── block/                (baru M14)
│   │   ├── block.c
│   │   ├── ramblk.c
│   │   ├── bcache.c
│   │   └── block_demo.c
│   └── core/
│       └── kmain.c           (diperbarui M14)
├── tests/
│   └── host/
│       └── test_m14_block.c  (baru M14)
├── scripts/
│   └── m14_preflight.sh      (baru M14)
├── Makefile.m14               (baru M14)
└── artifacts/
    └── m14/                   (baru M14)
        ├── host_info.txt
        ├── tool_versions.txt
        ├── preflight.log
        ├── git_status_before_m14.txt
        ├── git_status_after_m14.txt
        ├── m14_make_all.log
        └── qemu_m14.log
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis | Alasan | Risiko |
|---|---|---|---|
| `include/mcsos/block.h` | Baru | Kontrak publik internal: status code, `mcsos_blk_device_t`, `mcsos_blk_ops_t`, `mcsos_ramblk_t`, `mcsos_bcache_entry_t`, `mcsos_bcache_t`, deklarasi 12 fungsi | Rendah — header-only, tanpa logika |
| `kernel/block/block.c` | Baru | Registry, wrapper validasi range, `mcsos_blk_register/get/count/read/write/flush` | Tinggi — kesalahan validasi range menyebabkan driver menerima LBA/count tidak aman |
| `kernel/block/ramblk.c` | Baru | RAM block driver: `mcsos_ramblk_init`, `mcsos_ramblk_rw/read/write/flush` | Sedang — kesalahan kalkulasi `byte_offset`/`byte_count` menyebabkan akses di luar storage |
| `kernel/block/bcache.c` | Baru | Buffer cache: `mcsos_bcache_init/read/write/flush_all`, clock-hand eviction | Tinggi — kesalahan logika eviction dapat menghilangkan dirty entry tanpa flush |
| `kernel/block/block_demo.c` | Baru | Initializer kernel: registrasi `ram0`, smoke test write/read roundtrip, fail-closed via `KERNEL_PANIC` | Sedang — gagal init menyebabkan kernel panic terkontrol (by design) |
| `tests/host/test_m14_block.c` | Baru | Host unit test: registrasi, read/write/range/boundary, buffer cache write-back, flush | Rendah — hanya dipakai di host test |
| `Makefile.m14` | Baru | Target `host-test`, `freestanding`, `audit`, `all`, `clean` | Rendah — terisolasi dari Makefile utama, mengikuti pola Makefile.m11/m12 |
| `scripts/m14_preflight.sh` | Baru | Validasi tool, direktori, status Git sebelum M14 | Rendah |
| `kernel/core/kmain.c` | Ubah | Tambah forward declaration `m14_block_demo_init`, pemanggilan setelah `m13_vfs_bootstrap()` | Sedang — gagal init block layer memicu `KERNEL_PANIC` sebelum idle loop |

### 8.3 Ringkasan Diff

```text
git log --oneline praktikum-m14-block-device
45893d1 M14: implement block device layer, RAM block driver, buffer cache, host test, audit, and QEMU integration
8b8d15a (praktikum-m13-vfs-ramfs) M13: integrate VFS/RAMFS into kmain, add preflight and trimmed QEMU evidence
20a8e2e M13: implement VFS minimal, RAMFS, FD table, syscall file I/O, host test, and audit
```

File baru pada commit M14 (45893d1):
- `include/mcsos/block.h`
- `kernel/block/block.c`, `kernel/block/ramblk.c`, `kernel/block/bcache.c`, `kernel/block/block_demo.c`
- `tests/host/test_m14_block.c`
- `Makefile.m14`
- `scripts/m14_preflight.sh`
- `artifacts/m14/*` (7 file evidence)

File yang diubah:
- `kernel/core/kmain.c` — tambah forward declaration dan pemanggilan `m14_block_demo_init()`

---

## 9. Desain Teknis

### 9.1 Hubungan VFS, Block Layer, dan Driver

| Lapisan | Unit kerja | API utama | Tanggung jawab | Tidak boleh dilakukan |
|---|---|---|---|---|
| VFS/RAMFS M13 | File, file descriptor | `mcs_sys_open/read/write/close` | Abstraksi file untuk user/kernel program | Mengakses storage fisik langsung |
| Block Layer M14 | Blok logis (LBA) | `mcsos_blk_read/write/flush` | Validasi range, dispatch ke operation table | Mengetahui struktur file atau filesystem |
| RAM Block Driver M14 | Byte array | `mcsos_ramblk_rw` | Implementasi fisik perangkat sintetis | Memvalidasi ulang range (sudah divalidasi block layer) |
| Buffer Cache M14 | Satu blok per entry | `mcsos_bcache_read/write/flush_all` | Caching blok dengan write-back | Mengetahui isi/struktur blok (opaque byte array) |

### 9.2 Masalah yang Diselesaikan

```text
Kernel M13 memiliki VFS minimal dan RAMFS, tetapi seluruh data berada di
memori tanpa abstraksi block. Tanpa block layer, filesystem berikutnya
tidak memiliki cara terukur untuk berinteraksi dengan media penyimpanan
berbasis blok (baik sintetis maupun nyata). M14 menyelesaikan masalah ini
dengan tiga komponen kecil: registry block device yang memvalidasi setiap
operasi sebelum mencapai driver, RAM block driver sebagai perangkat
sintetis volatil, dan buffer cache write-back minimal dengan invariant
dirty/flush yang eksplisit. Desain ini sengaja menghindari kompleksitas
driver hardware nyata (PCIe, DMA, interrupt completion, virtqueue) agar
invariant storage dapat diverifikasi terlebih dahulu sebelum modul
berikutnya menambah kompleksitas hardware.
```

### 9.3 Keputusan Desain

| Keputusan | Alternatif | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Registry menyimpan pointer device, bukan menyalin struct | Copy struct ke registry | Lifetime device menjadi tanggung jawab pemilik; menghindari duplikasi data | Caller wajib menjamin device hidup selama terdaftar di registry |
| Validasi range dilakukan di block layer (`mcsos_blk_validate_range`), bukan di driver | Setiap driver memvalidasi sendiri | Driver hardware berikutnya tidak boleh menerima LBA/count tidak aman; satu titik validasi lebih mudah diaudit | Driver yang dipanggil langsung tanpa lewat block layer (untuk uji internal) tidak ter-validasi |
| RAM block driver tanpa dynamic allocation | Driver mengalokasikan storage sendiri | Storage harus dapat diuji di host tanpa heap; lifetime storage jelas milik caller | Caller (test atau kernel) wajib menyediakan array statik/global yang cukup besar |
| Buffer cache write-back, bukan write-through | Write-through (langsung tulis ke device) | Memperkenalkan trade-off performa vs konsistensi yang relevan untuk teori OS; menyiapkan jalur eksperimen ke depan | Risiko kehilangan data dirty jika crash sebelum flush — harus dicatat eksplisit sebagai known limitation |
| Clock-hand eviction (round-robin), bukan LRU | LRU (least recently used) | Lebih sederhana untuk diimplementasi dan diaudit pada cache kecil (2 entry pada host test) | Tidak optimal untuk workload locality tinggi; cukup untuk pembuktian invariant pada M14 |
| `flush` driver RAM adalah no-op sukses | Driver tetap menyalin data (no-op semantik tapi ada operasi) | Data RAM block driver sudah berada di backing memory volatil; flush fisik tidak diperlukan | Konsisten dengan model "tidak ada write-back fisik" untuk perangkat RAM |
| Fail-closed di `block_demo.c`: `KERNEL_PANIC` jika init gagal | Lanjut dengan device invalid | Mencegah kernel berjalan dengan block layer yang tidak terdaftar dengan benar | Kernel berhenti (panic terkontrol) jika ada bug inisialisasi, bukan silent failure |

### 9.4 Diagram Alur Operasi

```text
mcsos_blk_read(dev, lba, count, buffer):
  1. validate_range(dev, lba, count, buffer):
     - dev/buffer NULL atau count==0 atau ops==NULL → EINVAL
     - lba >= block_count → ERANGE
     - count > block_count - lba → ERANGE  (overflow-safe)
  2. dev->ops->read == NULL → EINVAL
  3. dev->ops->read(dev, lba, count, buffer)  [dispatch ke driver]

mcsos_ramblk_rw(dev, lba, count, buffer, is_write):
  1. dev/driver_data/buffer NULL → EINVAL
  2. byte_offset = lba * block_size
     byte_count  = count * block_size
  3. byte_offset > storage_size atau byte_count > storage_size - byte_offset → ERANGE
  4. is_write: copy buffer → storage+offset
     !is_write: copy storage+offset → buffer
  5. return OK

mcsos_bcache_read(cache, dev, lba, buffer):
  1. cache/dev/buffer NULL atau block_size mismatch → EINVAL
  2. find(cache, dev, lba):
     - ditemukan: copy entry->data → buffer; return OK
     - tidak ditemukan:
       a. select_victim(cache):
          - cari entry !valid (cepat)
          - tidak ada: pakai clock_hand, flush jika dirty
       b. blk_read(dev, lba, 1, victim->data)
       c. set victim->dev/lba/valid=1/dirty=0
       d. copy victim->data → buffer
  3. return OK

mcsos_bcache_flush_all(cache):
  loop seluruh entries: jika valid && dirty → blk_write lalu dirty=0
```

### 9.5 Kontrak Antarmuka

| Fungsi | Precondition | Postcondition | Error path |
|---|---|---|---|
| `mcsos_blk_register` | `dev != NULL`, `ops` lengkap (`read`,`write` non-NULL), `name` non-kosong, `block_size` valid power-of-two ≥512, `block_count > 0` | Device terdaftar di registry, dapat diakses via `mcsos_blk_get` | `EINVAL` (kontrak gagal), `EFULL` (registry penuh, max 8 device) |
| `mcsos_blk_read`/`write` | `dev` terdaftar valid, `lba < block_count`, `count > 0`, `count <= block_count - lba`, `buffer != NULL` | Data disalin dari/ke device melalui operation table | `EINVAL`, `ERANGE` |
| `mcsos_blk_flush` | `dev != NULL`, `dev->ops != NULL` | Driver flush dipanggil; jika `flush == NULL`, dianggap sukses no-op | `EINVAL` |
| `mcsos_ramblk_init` | `dev/ram/storage/name != NULL`, `block_size` valid, `storage_size >= block_size` dan kelipatan `block_size` | `dev` terisi lengkap (block_size, block_count, ops, driver_data) siap register | `EINVAL` |
| `mcsos_bcache_init` | `cache/entries/data_pool != NULL`, `entry_count > 0`, `block_size > 0` | Cache siap pakai; semua entry `valid=0` | `EINVAL` |
| `mcsos_bcache_write` | `cache/dev/buffer != NULL`, `cache->block_size == dev->block_size` | Entry cache terisi/diupdate, ditandai dirty; belum tentu sampai device | `EINVAL` (dari validasi awal), propagasi error dari `select_victim`/flush korban |
| `mcsos_bcache_flush_all` | `cache != NULL`, `cache->entries != NULL` | Semua entry dirty di-flush ke device; jika sukses semua, `dirty=0` untuk semua | Propagasi error dari `mcsos_blk_write` pertama yang gagal (berhenti di entry tersebut) |

### 9.6 Invariants

| Kode | Invariant | Implementasi | Bukti |
|---|---|---|---|
| BD-I1 | `dev != NULL` untuk seluruh operasi publik | Cek NULL di awal setiap fungsi publik `block.c` | Host test `mcsos_blk_write(&dev, 0, 1, 0)` (buffer NULL) → `EINVAL` |
| BD-I2 | `dev->ops`, `ops->read`, `ops->write` non-NULL sebelum register | Cek di `mcsos_blk_register` | Review kode; tidak diuji negatif eksplisit di host test panduan |
| BD-I3 | `block_size >= 512` dan power-of-two | `mcsos_is_power_of_two_u32` + perbandingan `MCSOS_BLK_DEFAULT_SECTOR_SIZE` | Host test memakai 512 byte; `ramblk_init` menolak nilai tidak valid |
| BD-I4 | `block_count > 0` | Cek di `mcsos_blk_register` | `ramblk_init` menghitung `block_count = storage_size/block_size`, selalu >0 jika storage valid |
| BD-I5 | `lba < block_count` dan `count <= block_count - lba` | `mcsos_blk_validate_range`, overflow-safe (bukan `lba+count`) | Host test LBA=32 (out of range, block_count=32) dan LBA=31,count=2 (overflow ke 33) keduanya `ERANGE` |
| BD-I6 | `count == 0` ditolak `EINVAL` | Cek eksplisit di `mcsos_blk_validate_range` | Host test `mcsos_blk_write(&dev, 0, 0, tmp)` → `EINVAL` |
| BD-I7 | `storage` RAM block dimiliki caller, driver tidak alokasi | `mcsos_ramblk_init` menerima `storage` sebagai parameter, tidak ada `malloc` | Review kode; `nm -u` kosong membuktikan tidak ada call alokasi libc |
| BD-I8 | `flush` RAM block driver no-op sukses | `mcsos_ramblk_flush` langsung `return MCSOS_BLK_OK` | Review kode `ramblk.c` |
| BD-I9 | Setiap cache entry memuat tepat satu blok | `entries[i].capacity = block_size`; `data` menunjuk offset tetap di `data_pool` | Review `mcsos_bcache_init` |
| BD-I10 | Entry valid harus punya `(dev, lba)` terdefinisi | `mcsos_bcache_find` hanya mencocokkan entry `valid==1` | Review kode; konsisten di seluruh path read/write |
| BD-I11 | Entry dirty harus di-flush sebelum victim reuse | `mcsos_bcache_select_victim` memanggil `mcsos_bcache_flush_entry` sebelum entry valid lama dipakai ulang | Host test 2-entry cache: tulis LBA 4, lalu LBA 5 — tidak menghilangkan data dirty tanpa flush sukses (diverifikasi via `flush_all` + read ulang) |
| BD-I12 | `cache->block_size == dev->block_size` | Cek eksplisit di `mcsos_bcache_read`/`write` | Host test memakai 512 byte konsisten di device dan cache |
| BD-I13 | `bcache_write` menandai dirty; data belum wajib di device sampai flush | `e->dirty = 1` di `mcsos_bcache_write`, tidak memanggil `mcsos_blk_write` langsung | Host test: setelah `bcache_write`, `blk_read` langsung dari device **tidak** menunjukkan data baru (`memcmp != 0`), baru setelah `flush_all` data sama |

### 9.7 Security Boundary M14

| Risiko | Level | Mitigasi pada M14 | Status |
|---|---|---|---|
| LBA/count out-of-range diteruskan ke driver | Tinggi | `mcsos_blk_validate_range` sebagai satu-satunya gerbang sebelum dispatch ke `ops->read/write` | Terverifikasi host test ERANGE |
| Overflow `lba + count` | Sedang | Pembanding `count > block_count - lba`, bukan `lba+count > block_count` | Terverifikasi review kode; aman dari wraparound 64-bit |
| Buffer NULL diteruskan ke driver | Sedang | Cek `buffer == 0` di `mcsos_blk_validate_range` sebelum dispatch | Terverifikasi host test EINVAL |
| Dirty entry hilang tanpa flush | Tinggi | `select_victim` flush korban dirty sebelum reuse; `flush_all` iterasi seluruh entry | Terverifikasi host test roundtrip |
| Block size mismatch cache vs device | Sedang | Cek eksplisit `cache->block_size != dev->block_size` | Terverifikasi review kode |
| Dependency libc di driver/cache freestanding | Sedang | `mcsos_memcpy_u8` internal di setiap file, bukan `memcpy` libc | Terverifikasi `nm -u` kosong pada `m14_block_layer.o` |
| Concurrent access tanpa lock (SMP) | Tinggi (di luar scope) | Documented constraint; M14 belum SMP-safe | Dicatat sebagai known limitation, bukan ditangani |
| Power-loss / crash sebelum flush | Tinggi (di luar scope) | RAM block volatil sudah jelas tidak persistent; write-back cache memperbesar window risiko | Dicatat sebagai known limitation eksplisit di seluruh laporan |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Periksa Baseline M13 dan Buat Branch M14

```bash
git status --short
git switch -c praktikum-m14-block-device
mkdir -p include/mcsos kernel/block tests/host scripts artifacts/m14
```

Output: working tree bersih, branch `praktikum-m14-block-device` aktif.

### Langkah 2 — Kumpulkan Versi Toolchain dan Jalankan Preflight

```bash
{ uname -a; lsb_release -a 2>/dev/null || cat /etc/os-release; } | tee artifacts/m14/host_info.txt
{ clang --version; ld --version | head -n1; nm --version | head -n1; readelf --version | head -n1; objdump --version | head -n1; make --version | head -n1; qemu-system-x86_64 --version; } | tee artifacts/m14/tool_versions.txt
./scripts/m14_preflight.sh
```

Output: semua `OK_CMD` dan `OK_DIR` terisi; berakhir dengan `M14_PREFLIGHT_DONE`.

### Langkah 3 — Buat Header Block Layer

`include/mcsos/block.h`: status code, `mcsos_blk_device_t`, `mcsos_blk_ops_t`, `mcsos_ramblk_t`, `mcsos_bcache_entry_t`, `mcsos_bcache_t`, deklarasi 12 fungsi publik.

### Langkah 4 — Buat Registry dan Wrapper Validasi

`kernel/block/block.c`: registry statik 8 device, `mcsos_blk_register/get/count`, `mcsos_blk_validate_range` (overflow-safe), `mcsos_blk_read/write/flush`.

### Langkah 5 — Buat RAM Block Driver

`kernel/block/ramblk.c`: `mcsos_memcpy_u8` internal, `mcsos_ramblk_rw/read/write/flush`, `mcsos_ramblk_init` dengan validasi `storage_size` kelipatan `block_size`.

### Langkah 6 — Buat Buffer Cache Minimal

`kernel/block/bcache.c`: `mcsos_bcache_find`, `mcsos_bcache_flush_entry`, `mcsos_bcache_select_victim` (clock-hand), `mcsos_bcache_init/read/write/flush_all`.

### Langkah 7 — Buat Host Unit Test

`tests/host/test_m14_block.c`: registrasi device, write/read roundtrip, range validation (ERANGE/EINVAL), buffer cache write-back, flush, dan verifikasi data baru sampai ke device setelah flush.

### Langkah 8 — Buat Makefile.m14

Mengikuti pola `Makefile.m11`/`Makefile.m12` yang sudah ada: tab sebagai separator (bukan `>` seperti Makefile utama). Target: `host-test`, `freestanding`, `audit`, `all`, `clean`.

Verifikasi separator:

```bash
grep -n "mkdir\|host-test:" Makefile.m14 | cat -A
```

Output: baris recipe diawali `^I` (tab asli), bukan spasi.

### Langkah 9 — Jalankan Build dan Test M14

```bash
make -f Makefile.m14 clean 2>/dev/null || true
make -f Makefile.m14 all 2>&1 | tee artifacts/m14/m14_make_all.log
```

Output:

```text
M14 host tests PASS
nm -u build/m14_block_layer.o   [kosong]
test ! -s artifacts/m14_nm_undefined.txt   [lulus]
```

### Langkah 10 — Verifikasi Audit ELF dan Checksum

```bash
grep -E "Class:|Type:|Machine:" artifacts/m14_readelf_block.txt
cat artifacts/m14_sha256.txt
```

Output: `ELF64`, `REL (Relocatable file)`, `Advanced Micro Devices X86-64`; 5 checksum tersimpan.

### Langkah 11 — Integrasi ke Kernel Utama

Periksa bahwa `SRC_C` Makefile utama (`find kernel src -name '*.c'`) mencakup `kernel/block/` secara otomatis. Buat `kernel/block/block_demo.c` sebagai initializer dengan `KERNEL_PANIC` fail-closed jika init gagal.

### Langkah 12 — Patch `kmain.c`

Via Python: tambahkan forward declaration `void m14_block_demo_init(void);` dan pemanggilan setelah `m13_vfs_bootstrap();`.

### Langkah 13 — Build Kernel

```bash
make clean && make build
```

Output: 4 file `kernel/block/*.o` masuk linker command; link berhasil tanpa error.

### Langkah 14 — QEMU Smoke Test

```bash
tools/scripts/make_iso.sh 2>/dev/null
qemu-system-x86_64 -machine q35 -cpu max -m 256M \
    -cdrom build/mcsos.iso -boot d \
    -serial file:artifacts/m14/qemu_m14.log \
    -display none -no-reboot -no-shutdown || true
```

Output: `[M14] block layer initialized` dan `[M14] ram0 write/read roundtrip ok` di serial log.

### Langkah 15 — Kumpulkan Evidence dan Commit

```bash
git status --short | tee artifacts/m14/git_status_after_m14.txt
git add include/mcsos/block.h kernel/block/ tests/host/test_m14_block.c \
        Makefile.m14 scripts/m14_preflight.sh kernel/core/kmain.c artifacts/m14
git commit -m "M14: implement block device layer, RAM block driver, buffer cache, host test, audit, and QEMU integration"
# → commit 45893d1
```

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Bukti wajib | Status |
|---|---|---|---|
| CP14.1 Preflight | `./scripts/m14_preflight.sh` | `artifacts/m14/preflight.log` berakhir `M14_PREFLIGHT_DONE` | `PASS` |
| CP14.2 Host test | `make -f Makefile.m14 host-test` | `M14 host tests PASS` | `PASS` |
| CP14.3 Freestanding | `make -f Makefile.m14 freestanding` | `build/block.o`, `ramblk.o`, `bcache.o` ELF64 x86-64 | `PASS` |
| CP14.4 Audit | `make -f Makefile.m14 audit` | `nm -u` kosong, `readelf`/`objdump`/`sha256sum` tersimpan | `PASS` |
| CP14.5 Integrasi kernel | `make clean && make build` | `kernel.elf` berhasil dengan 4 object block masuk | `PASS` |
| CP14.6 QEMU smoke test | QEMU command M14 | `qemu_m14.log` menunjukkan `[M14] block layer initialized` | `PASS` |
| CP14.7 Git commit | `git log --oneline -n 3` | Commit `45893d1` ada | `PASS` |

---

## 12. Perintah Uji dan Validasi

### 12.1 Host Unit Test

```bash
make -f Makefile.m14 host-test
```

Hasil:

```text
cc -std=c17 -Wall -Wextra -Werror -Iinclude -O2 tests/host/test_m14_block.c \
   kernel/block/block.c kernel/block/ramblk.c kernel/block/bcache.c -o build/test_m14_block
./build/test_m14_block
M14 host tests PASS
```

Status: `PASS`

### 12.2 Audit Object Freestanding

```bash
make -f Makefile.m14 audit
cat artifacts/m14_nm_undefined.txt          # harus kosong
grep -E "Class:|Type:|Machine:" artifacts/m14_readelf_block.txt
cat artifacts/m14_sha256.txt
```

Hasil:

```text
[m14_nm_undefined.txt — kosong]

  Class:                             ELF64
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64

dc243ee2096ee9d74ac64df0a02f3a229482feb898cb2f7f51cf438866d1e423  build/block.o
f86848e89e6aadf6ca006f984c26af38b564397b2358e5e8abfbee84a935dc68  build/ramblk.o
02ef7db9aaf4cbe1310b17769af54df432ac82f001f414451f989ef111cb06fe  build/bcache.o
e2a3314a7e8d9ef38478f84ebc5d6b45bbe8b1fc4d1ce2163802e09ee089fe6e  build/m14_block_layer.o
abfae5e887a778bfe796c51c41317d0b0169c371b3f1493d7688de99ed79b4e7  build/test_m14_block
```

Status: `PASS`

### 12.3 Build Kernel

```bash
make clean && make build
```

Hasil: 27 file dikompilasi termasuk `kernel/block/block.c`, `ramblk.c`, `bcache.c`, `block_demo.c`. Link `ld.lld` berhasil tanpa error/warning.

Status: `PASS`

### 12.4 QEMU Smoke Test

```bash
cat artifacts/m14/qemu_m14.log | grep -E "M14|M13|kernel entered"
```

Hasil:

```text
MCSOS 260502 M4 kernel entered
[M13] ramfs+vfs smoke test passed
[M13] motd.txt content: mcsos-m13-ramfs
[M14] block layer initialized
[M14] ram0 write/read roundtrip ok
```

Status: `PASS`

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan

| No. | Uji | Expected | Actual | Status | Evidence |
|---|---|---|---|---|---|
| 1 | Registrasi device `ram0` via `mcsos_ramblk_init` + `mcsos_blk_register` | `MCSOS_BLK_OK`, `blk_count()==1` | Sesuai | `PASS` | Host test |
| 2 | `dev.block_count` untuk storage 32×512 byte | `32` | `32` | `PASS` | Host test |
| 3 | Write LBA 3 lalu read LBA 3, bandingkan data | Identik (`memcmp==0`) | Identik | `PASS` | Host test |
| 4 | Read LBA 32 (out of range untuk 32 blok) | `MCSOS_BLK_ERANGE` | `MCSOS_BLK_ERANGE` | `PASS` | Host test |
| 5 | Write LBA 31, count 2 (overflow ke 33) | `MCSOS_BLK_ERANGE` | `MCSOS_BLK_ERANGE` | `PASS` | Host test |
| 6 | Write count 0 | `MCSOS_BLK_EINVAL` | `MCSOS_BLK_EINVAL` | `PASS` | Host test |
| 7 | Write buffer NULL | `MCSOS_BLK_EINVAL` | `MCSOS_BLK_EINVAL` | `PASS` | Host test |
| 8 | `bcache_write` LBA 4, lalu `bcache_read` LBA 4 | Data identik dengan yang ditulis | Identik | `PASS` | Host test |
| 9 | `blk_read` langsung dari device setelah `bcache_write` (sebelum flush) | Data **berbeda** (belum di-flush) | Berbeda (`memcmp != 0`) | `PASS` | Host test |
| 10 | `bcache_flush_all` lalu `blk_read` dari device | Data identik (sudah ter-flush) | Identik | `PASS` | Host test |
| 11 | `bcache_write` LBA 5 (entry kedua dari cache 2-entry), flush, read | Data identik | Identik | `PASS` | Host test |
| 12 | `nm -u` linked relocatable object | Kosong | Kosong | `PASS` | `m14_nm_undefined.txt` |
| 13 | `readelf -h` linked object | ELF64 x86-64 REL | ELF64 x86-64 REL | `PASS` | `m14_readelf_block.txt` |
| 14 | `objdump` linked object | Disassembly tersimpan | Tersimpan | `PASS` | `m14_objdump_block.txt` |
| 15 | Checksum 5 artefak | Tersimpan | 5 hash tersimpan | `PASS` | `m14_sha256.txt` |
| 16 | Build kernel dengan block layer | Berhasil tanpa error | Berhasil | `PASS` | `make clean && make build` |
| 17 | QEMU: `[M14] block layer initialized` | Ada | Ada | `PASS` | `qemu_m14.log` |
| 18 | QEMU: `[M14] ram0 write/read roundtrip ok` | Ada | Ada | `PASS` | `qemu_m14.log` |
| 19 | M13 ramfs/vfs masih berjalan setelah M14 | `[M13]` log muncul sebelum `[M14]` | Muncul, tidak regresi | `PASS` | `qemu_m14.log` |
| 20 | Tidak ada triple fault / reboot loop | QEMU berjalan stabil sampai timeout manual | Stabil | `PASS` | `qemu_m14.log` |

### 13.2 Log Serial QEMU Penuh (Bagian Penting)

```text
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M4 kernel entered
[M4] IDT loaded
...
[M13] ramfs+vfs smoke test passed
[M13] motd.txt content: mcsos-m13-ramfs
[M14] block layer initialized
blk_count=0x0000000000000001
ram0_block_size=0x0000000000000200
ram0_block_count=0x0000000000000040
[M14] ram0 write/read roundtrip ok
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| `host_info.txt` | `artifacts/m14/host_info.txt` | Identitas host (uname, OS release) |
| `tool_versions.txt` | `artifacts/m14/tool_versions.txt` | Versi clang, ld, nm, readelf, objdump, make, qemu |
| `preflight.log` | `artifacts/m14/preflight.log` | Output preflight, berakhir `M14_PREFLIGHT_DONE` |
| `m14_make_all.log` | `artifacts/m14/m14_make_all.log` | Output lengkap `make -f Makefile.m14 all` |
| `m14_nm_undefined.txt` | `artifacts/m14_nm_undefined.txt` | Bukti `nm -u` kosong pada `m14_block_layer.o` |
| `m14_readelf_block.txt` | `artifacts/m14_readelf_block.txt` | Bukti ELF64 x86-64 REL |
| `m14_objdump_block.txt` | `artifacts/m14_objdump_block.txt` | Disassembly linked relocatable object |
| `m14_sha256.txt` | `artifacts/m14_sha256.txt` | Checksum 5 artefak build |
| `qemu_m14.log` | `artifacts/m14/qemu_m14.log` | Serial log QEMU penuh M4→M14 |
| `git_status_before_m14.txt` | `artifacts/m14/git_status_before_m14.txt` | Status git sebelum M14 (working tree bersih) |
| `git_status_after_m14.txt` | `artifacts/m14/git_status_after_m14.txt` | Status git setelah implementasi, sebelum commit |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

```text
Host unit test berhasil karena implementasi mengikuti kontrak panduan
secara tepat: validasi range dilakukan dengan overflow-safe comparison
(count > block_count - lba, bukan lba + count > block_count) sehingga
tidak ada celah wraparound pada uint64_t. Kasus LBA=31, count=2 pada
device 32 blok berhasil ditolak karena block_count - lba = 1, sedangkan
count = 2 > 1.

Buffer cache write-back terbukti bekerja sesuai model: setelah
bcache_write, data yang dibaca langsung dari device (blk_read, bypass
cache) berbeda dari data yang baru ditulis — ini membuktikan bahwa data
memang belum sampai ke device sampai flush dipanggil. Setelah
bcache_flush_all, data di device identik dengan yang ditulis ke cache.
Ini adalah bukti konkret bahwa invariant BD-I13 (data belum wajib di
device sampai flush) terpenuhi, bukan klaim tanpa bukti.

Audit freestanding lulus (nm -u kosong) karena ketiga file (block.c,
ramblk.c, bcache.c) memiliki fungsi mcsos_memcpy_u8 internal masing-masing
(meski nama fungsi diberi suffix berbeda di ramblk.c dan bcache.c untuk
menghindari duplicate symbol saat linked) sehingga tidak ada panggilan ke
memcpy libc. Flag -ffreestanding -fno-builtin memastikan compiler tidak
menyisipkan builtin call tersembunyi.

Build kernel berhasil karena SRC_C := $(shell find kernel src -name '*.c')
sudah mencakup kernel/block/ secara otomatis tanpa perubahan Makefile
utama — pola yang sama seperti integrasi kernel/user/ pada M11.

QEMU smoke test berhasil karena storage RAM block (g_m14_ramdisk_storage,
32 KiB) berada di .bss kernel yang sudah di-zero dan terpetakan oleh
bootloader sebelum kmain dipanggil. Smoke test write/read roundtrip di
block_demo.c berhasil membuktikan jalur block layer berfungsi end-to-end
pada runtime kernel nyata, bukan hanya host test. M13 ramfs/vfs masih
berjalan normal setelah M14, membuktikan tidak ada regresi.
```

### 14.2 Analisis Kegagalan atau Tantangan

```text
Tidak ada bug logika yang ditemukan selama implementasi M14. Satu hal yang
perlu kehati-hatian adalah penamaan fungsi helper memcpy internal di tiga
file berbeda (block.c tidak memerlukan memcpy karena hanya wrapper, ramblk.c
memakai mcsos_memcpy_u8, bcache.c memakai mcsos_memcpy_u8_bcache) — nama
yang berbeda diperlukan agar tidak terjadi duplicate symbol saat ketiga
object di-link menjadi satu relocatable object m14_block_layer.o dengan
ld -r. Hal ini konsisten dengan pengalaman pada milestone sebelumnya (M7)
di mana duplicate symbol pernah terjadi karena dua file berbeda mendefinisikan
fungsi dengan nama sama.

Tantangan lain adalah memastikan Makefile.m14 mengikuti konvensi tab
sebagai RECIPEPREFIX, bukan '>' seperti Makefile utama, mengingat
Makefile.m11 sebelumnya pernah bermasalah dengan separator. Verifikasi
dengan cat -A dilakukan sejak awal untuk memastikan tab asli terpakai,
sehingga masalah serupa M11 tidak terulang pada M14.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai? | Penjelasan |
|---|---|---|---|
| Pemisahan request block I/O dari driver (Linux block subsystem) | `mcsos_blk_ops_t` sebagai operation table; caller tidak tahu implementasi driver | Sesuai | Dibuktikan oleh `block.c` yang hanya memanggil `dev->ops->read/write/flush` tanpa tahu itu RAM atau perangkat lain |
| `null_blk` sebagai perangkat sintetis tanpa media fisik | `mcsos_ramblk_*` meniru block device dengan array memori | Sesuai | RAM block driver memungkinkan pengujian block layer tanpa hardware nyata |
| LBA harus divalidasi terhadap `block_count` | `mcsos_blk_validate_range` | Sesuai | Terverifikasi host test LBA out-of-range |
| Buffer cache write-back vs write-through trade-off | M14 memilih write-back; dirty flag eksplisit | Sesuai | Trade-off (performa vs risiko data loss) dicatat di teori dan laporan |
| Dirty entry tidak boleh hilang tanpa flush | `select_victim` flush korban dirty sebelum reuse | Sesuai | Terverifikasi host test 2-entry cache roundtrip |
| Power-of-two block size untuk alignment sederhana | `mcsos_is_power_of_two_u32` | Sesuai | Cek di `block.c` dan `ramblk.c` |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Diidentifikasi (Sesuai Panduan)

| Failure mode | Gejala | Penyebab | Mitigasi M14 |
|---|---|---|---|
| Out-of-range LBA | Driver menerima LBA di luar `block_count` | Caller tidak memvalidasi sebelum memanggil driver langsung | `mcsos_blk_validate_range` adalah gerbang wajib sebelum dispatch ke `ops->read/write` |
| Dirty buffer tidak di-flush | Data hilang saat shutdown/crash | Write-back tanpa flush eksplisit sebelum shutdown | `mcsos_bcache_flush_all` harus dipanggil sebelum shutdown; `select_victim` flush otomatis saat eviction |
| Stale cache | Cache menyimpan data lama setelah device berubah dari luar cache | Tidak ada invalidasi cache saat device dimodifikasi di luar jalur cache | M14 tidak menangani ini secara eksplisit; documented sebagai known limitation (concurrency M14 single-writer melalui cache saja) |
| Block size mismatch | Data terbaca salah jika cache dan device punya `block_size` berbeda | Konfigurasi salah saat `bcache_init` | Cek eksplisit `cache->block_size != dev->block_size` di `bcache_read`/`write` |
| Ketidakjelasan ownership buffer | Use-after-free atau data race pada buffer storage | Tidak ada dokumentasi siapa pemilik memori | Dicatat eksplisit di invariant BD-I7: storage RAM block dimiliki caller, bukan driver |

### 15.2 Failure Modes yang Diantisipasi (Tambahan)

| Failure mode | Gejala | Penyebab | Mitigasi |
|---|---|---|---|
| Registry penuh | `mcsos_blk_register` mengembalikan `EFULL` | Lebih dari `MCSOS_BLK_MAX_DEVICES` (8) device didaftarkan | Caller harus memeriksa return code; tidak ada eviction otomatis registry |
| Overflow `byte_offset`/`byte_count` di RAM block driver | Akses di luar storage tanpa terdeteksi | Kalkulasi `lba * block_size` melebihi `uint64_t` pada device sangat besar | Pada skala M14 (storage kecil, demo 32 KiB) risiko ini sangat rendah; untuk device besar perlu overflow check tambahan di `ramblk_rw` (catatan pengayaan) |
| `block_demo.c` panic saat boot | Kernel tidak mencapai idle loop | `mcsos_ramblk_init` atau `mcsos_blk_register` gagal karena storage tidak valid | `KERNEL_PANIC` fail-closed mencegah kernel berjalan dengan block layer rusak; log panic membantu diagnosis |
| Concurrent access buffer cache (SMP) | Korupsi state cache jika dua thread memanggil bersamaan | M14 belum SMP-safe, tidak ada lock internal | Documented constraint; caller wajib serialisasi akses dengan lock eksternal jika SMP diaktifkan |

### 15.3 Triage yang Dilakukan

```text
Tidak ada kegagalan runtime yang memerlukan triage mendalam pada M14.
Verifikasi proaktif dilakukan pada dua titik: (1) memastikan separator
Makefile.m14 memakai tab via cat -A sebelum menjalankan make, mencegah
error "missing separator" seperti yang pernah terjadi pada M11; (2)
memastikan penamaan fungsi memcpy internal berbeda di setiap file
(mcsos_memcpy_u8 vs mcsos_memcpy_u8_bcache) untuk mencegah duplicate
symbol error seperti yang pernah terjadi pada M7 dengan pmm.o.
```

### 15.4 Rancangan GDB untuk Debugging Block Layer

```text
Jika m14_block_demo_init gagal atau KERNEL_PANIC dipicu:

Terminal 1 (QEMU):
  qemu-system-x86_64 -machine q35 -m 256M -serial stdio \
    -no-reboot -no-shutdown -S -s -cdrom build/mcsos.iso

Terminal 2 (GDB):
  gdb build/kernel.elf \
    -ex 'target remote :1234' \
    -ex 'break mcsos_blk_register' \
    -ex 'break mcsos_blk_read' \
    -ex 'break mcsos_blk_write' \
    -ex 'continue'

Bukti yang disimpan:
  gdb build/kernel.elf \
    -ex 'target remote :1234' \
    -ex 'info breakpoints' \
    -ex 'info registers' \
    -ex 'quit' \
    | tee artifacts/m14/gdb_m14_session.txt
```

Catatan: sesi GDB interaktif M14 belum dijalankan pada praktikum ini karena tidak ada failure runtime yang memerlukan investigasi lebih lanjut; rancangan di atas disiapkan sebagai prosedur baku jika failure muncul pada demonstrasi.

---

## 16. Prosedur Rollback

| Skenario | Perintah | Status |
|---|---|---|
| Kembali ke baseline M13 | `git switch praktikum-m13-vfs-ramfs` | Belum diuji aktual; branch M13 masih ada di `8b8d15a` |
| Nonaktifkan block demo saja | Hapus pemanggilan `m14_block_demo_init()` dari `kmain.c` | Paling aman; block layer tetap ada untuk host test |
| Hapus semua file M14 | `git restore kernel/core/kmain.c; git rm -r kernel/block/ include/mcsos/block.h tests/host/ Makefile.m14 scripts/m14_preflight.sh` | Belum diuji aktual |
| Bersihkan artefak build M14 | `make -f Makefile.m14 clean` | Teruji — menghapus `build/` dan `artifacts/m14_*.txt` |

Catatan rollback:

```text
Branch M14 (praktikum-m14-block-device) terpisah dari branch M13
(praktikum-m13-vfs-ramfs). Rollback ke M13 dapat dilakukan dengan git
switch tanpa risiko kehilangan data M14. Rollback belum diuji aktual
karena M14 berhasil tanpa perlu rollback.
```

---

## 17. Keamanan dan Reliability

### 17.1 Keamanan

| Risiko | Mitigasi pada M14 | Known limitation |
|---|---|---|
| LBA/count out-of-range diteruskan ke driver | `mcsos_blk_validate_range` sebagai gerbang wajib | Tidak ada |
| Overflow kalkulasi range | Pembanding overflow-safe (`count > block_count - lba`) | Tidak ada |
| Buffer NULL ke driver | Cek eksplisit di validasi | Tidak ada |
| Dirty entry hilang tanpa flush | Flush otomatis saat eviction; `flush_all` tersedia | Caller masih wajib memanggil `flush_all` sebelum shutdown — tidak otomatis |
| Block size mismatch cache/device | Cek eksplisit | Tidak ada |
| Dependency libc | `nm -u` kosong | Tidak ada |
| Concurrent access (SMP) | Tidak ada lock internal | M14 belum SMP-safe; documented constraint |
| Power-loss/crash sebelum flush | Tidak ada mitigasi | RAM block volatil; write-back cache memperbesar window risiko; eksplisit non-goal M14 |
| Device isolation / DMA protection | Tidak ada | Belum relevan karena belum ada driver hardware nyata |

### 17.2 Reliability

| Aspek | Status M14 | Catatan |
|---|---|---|
| Validasi range fail-closed | ✅ `ERANGE`/`EINVAL` eksplisit | Terverifikasi host test |
| Overflow safety | ✅ `count > block_count - lba` | Terverifikasi host test (LBA 31, count 2) |
| Dirty flush sebelum eviction | ✅ `select_victim` | Terverifikasi host test roundtrip |
| Freestanding | ✅ `nm -u` kosong | Terverifikasi `m14_nm_undefined.txt` |
| Build `-Werror` | ✅ Lulus | Semua warning dianggap error |
| M13 tidak regresi | ✅ Ramfs/VFS masih jalan | Terverifikasi QEMU log |
| Fail-closed init kernel | ✅ `KERNEL_PANIC` jika init gagal | Konsisten dengan panic path M3 |

---

## 18. Pembagian Kerja

Tidak berlaku — praktikum dikerjakan secara individu.

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Repository dapat dibangun dari clean checkout | `PASS` | `make clean && make build` berhasil |
| Hasil M0–M13 sudah diperiksa (preflight) | `PASS` | `artifacts/m14/preflight.log` berakhir `M14_PREFLIGHT_DONE` |
| Header, registry, driver, cache, host test, Makefile tersedia | `PASS` | 7 file baru terkonfirmasi |
| Host unit test M14 lulus | `PASS` | `M14 host tests PASS` |
| Source dikompilasi sebagai object freestanding x86_64 | `PASS` | `build/block.o`, `ramblk.o`, `bcache.o` ada |
| Linked relocatable object tanpa undefined symbol | `PASS` | `nm -u build/m14_block_layer.o` kosong |
| `readelf -h` menunjukkan ELF64 relocatable x86-64 | `PASS` | `artifacts/m14_readelf_block.txt` |
| `objdump` dapat diaudit | `PASS` | `artifacts/m14_objdump_block.txt` |
| Checksum artefak tersimpan | `PASS` | `artifacts/m14_sha256.txt` |
| Integrasi kernel tidak merusak VFS/RAMFS M13 | `PASS` | M13 log masih muncul normal sebelum M14 |
| QEMU smoke test dijalankan | `PASS` | `[M14] ram0 write/read roundtrip ok` di log |
| Semua perubahan dikomit | `PASS` | Commit `45893d1` |
| Laporan menyertakan bukti lengkap | `PASS` | Laporan ini |

| Kriteria pengayaan | Status | Catatan |
|---|---|---|
| Counter statistik read/write/flush/hit/miss/eviction | `NA` | Belum diimplementasikan |
| Mode write-through opsional pada cache | `NA` | Belum diimplementasikan |
| Negative test `block_size` tidak power-of-two | `NA` | Implisit melalui validasi `mcsos_is_power_of_two_u32`, belum diuji eksplisit |
| `mcsos_blk_dump_devices()` | `NA` | Belum diimplementasikan |

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
Build bersih untuk semua target (host test, freestanding object, linked
relocatable object, kernel.elf). Host unit test lulus mencakup happy path
dan boundary cases (ERANGE, EINVAL). nm -u kosong pada m14_block_layer.o.
ELF64 x86-64 REL terverifikasi via readelf. Disassembly tersimpan via
objdump. Checksum 5 artefak tersimpan via sha256sum. Build kernel berhasil
dengan block layer terintegrasi. QEMU smoke test menampilkan [M14] block
layer initialized dan [M14] ram0 write/read roundtrip ok. M13 VFS/RAMFS
tidak regresi setelah M14.

Status siap demonstrasi belum diklaim karena: (1) rollback belum diuji
aktual; (2) sesi GDB interaktif belum dijalankan (hanya rancangan); (3)
tugas pengayaan (counter statistik, write-through opsional, dump devices)
belum diimplementasikan. M14 secara eksplisit BUKAN bukti storage persistent
aman terhadap power-loss, BUKAN bukti driver hardware siap, dan BUKAN bukti
filesystem crash-consistent.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | RAM block driver volatil, bukan persistent | Data hilang saat reboot QEMU | Sesuai scope M14: persistence baru dikaji M15+ | M15 |
| 2 | Buffer cache belum SMP-safe | Korupsi state jika diakses concurrent | Single-core educational baseline; lock eksternal wajib jika SMP diaktifkan | Setelah lock discipline M12 matang untuk block layer |
| 3 | Tidak ada device isolation, DMA protection, capability check | Tidak relevan untuk RAM block sintetis, tapi wajib sebelum driver hardware nyata | Tidak ada — sengaja ditunda | M15+ saat driver hardware diperkenalkan |
| 4 | Rollback belum diuji aktual | Risiko M13 tidak berjalan setelah rollback | Branch terpisah memungkinkan rollback manual | Sebelum demonstrasi |
| 5 | GDB session formal belum dilakukan | Tidak ada bukti GDB M14 | Rancangan ada di bagian 15.4 | Sebelum demonstrasi atau saat failure muncul |
| 6 | Caller wajib memanggil `flush_all` manual sebelum shutdown | Risiko data dirty hilang jika lupa flush | Tidak ada flush otomatis saat shutdown kernel | Pengayaan: hook flush otomatis di shutdown path |

Keputusan akhir:

```text
Berdasarkan bukti host test PASS (mencakup happy path dan boundary case),
nm -u kosong, ELF64 x86-64 REL terverifikasi, checksum tersimpan, build
kernel berhasil, dan QEMU log [M14] block layer initialized serta
[M14] ram0 write/read roundtrip ok, hasil praktikum M14 layak disebut
siap uji QEMU untuk block device layer, RAM block driver, dan buffer
cache minimal. M14 tidak mengklaim siap demonstrasi praktikum karena
rollback belum diuji aktual dan GDB session formal belum dilakukan. M14
secara eksplisit tidak mengklaim storage persistent aman terhadap
power-loss, driver hardware siap, atau filesystem crash-consistent —
ketiga klaim tersebut adalah non-goals M14 yang ditunda ke milestone
berikutnya.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---:|---:|---|---:|
| Kebenaran fungsional | 30 | Registry, RAM block driver, buffer cache bekerja sesuai kontrak; semua host test lulus termasuk boundary case dan write-back roundtrip | `29` |
| Kualitas desain dan invariants | 20 | 13 invariant terdokumentasi (BD-I1–BD-I13), pemisahan registry/driver/cache jelas, fail-closed di integrasi kernel | `19` |
| Pengujian dan bukti | 20 | Host test, nm-u, readelf, objdump, sha256sum, QEMU smoke test, preflight, evidence lengkap | `19` |
| Debugging dan failure analysis | 10 | 5 failure mode panduan dijelaskan dengan mitigasi konkret, rancangan GDB ada | `9` |
| Keamanan dan robustness | 10 | Overflow check, fail-closed, freestanding tanpa libc, SMP constraint documented | `9` |
| Dokumentasi dan laporan | 10 | Laporan rapi, reproducible, referensi IEEE, commit hash, evidence lengkap | `9` |
| **Total** | **100** | | `94` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

```text
Seluruh komponen wajib M14 berhasil:
- include/mcsos/block.h: kontrak publik lengkap dengan status code,
  device struct, operation table, RAM block dan buffer cache metadata.
- kernel/block/block.c: registry 8 device, validasi range overflow-safe,
  wrapper read/write/flush.
- kernel/block/ramblk.c: RAM block driver deterministik tanpa dynamic
  allocation, storage milik caller.
- kernel/block/bcache.c: buffer cache write-back dengan clock-hand
  eviction, dirty flag, flush_all.
- tests/host/test_m14_block.c: host test lulus mencakup happy path,
  boundary case (ERANGE/EINVAL), dan write-back roundtrip yang
  membuktikan data belum sampai device sampai flush.
- make -f Makefile.m14 all: host test PASS, freestanding compile berhasil,
  nm-u kosong, ELF64 REL terverifikasi, checksum tersimpan.
- kernel/block/block_demo.c: initializer fail-closed via KERNEL_PANIC.
- Build kernel: 4 object block layer masuk ke kernel.elf tanpa error.
- QEMU smoke test: [M14] block layer initialized, ram0 write/read
  roundtrip ok, M13 tidak regresi.
- 1 commit bersih 45893d1 di branch praktikum-m14-block-device.
- 11 artefak evidence di artifacts/m14/ dan artifacts/.
```

### 22.2 Yang Belum Berhasil

```text
- Tugas pengayaan (counter statistik, write-through opsional, negative
  test block_size non-power-of-two, mcsos_blk_dump_devices) belum
  diimplementasikan — sesuai scope, ini bukan kewajiban M14.
- Sesi GDB interaktif formal belum dijalankan; hanya rancangan tersedia.
- Rollback belum diuji aktual.
- Buffer cache belum SMP-safe — sesuai scope, ditunda sampai lock
  discipline M12 matang untuk konteks block layer.
```

### 22.3 Rencana Perbaikan

```text
Sebelum demonstrasi M14:
1. Uji rollback aktual ke commit 8b8d15a (M13) dan verifikasi M13 masih jalan.
2. Jalankan GDB session: break mcsos_blk_register/read/write, verifikasi
   call stack dan parameter saat block_demo_init dipanggil.

Untuk M15+:
1. Integrasikan block layer M14 dengan VFS M13: buat block-backed RAMFS
   atau filesystem persistent sederhana yang menggunakan mcsos_bcache_*
   sebagai perantara ke mcsos_blk_*.
2. Tambahkan counter statistik (read/write/flush/hit/miss/eviction) untuk
   observability buffer cache.
3. Pertimbangkan mode write-through opsional sebagai trade-off konsistensi.
4. Mulai kajian crash consistency dan journal sebelum filesystem persistent
   nyata dibangun di atas block layer ini.
5. Setelah lock discipline M12 matang untuk konteks block layer, jadikan
   buffer cache SMP-safe dengan lock eksternal yang terdokumentasi.
```

---

## 23. Lampiran

### Lampiran A — Commit Log

```text
45893d1 (HEAD -> praktikum-m14-block-device) M14: implement block device layer, RAM block driver, buffer cache, host test, audit, and QEMU integration
8b8d15a (praktikum-m13-vfs-ramfs) M13: integrate VFS/RAMFS into kmain, add preflight and trimmed QEMU evidence
20a8e2e M13: implement VFS minimal, RAMFS, FD table, syscall file I/O, host test, and audit
55a09ab (praktikum/m12-sync) M12: add preflight log
bebe0e3 M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit
```

### Lampiran B — Diff Ringkas

```text
File baru pada commit M14 (45893d1):
  include/mcsos/block.h         — header API block layer
  kernel/block/block.c          — registry dan wrapper validasi
  kernel/block/ramblk.c         — RAM block driver
  kernel/block/bcache.c         — buffer cache minimal
  kernel/block/block_demo.c     — initializer kernel fail-closed
  tests/host/test_m14_block.c   — host unit test
  Makefile.m14                  — target host-test/freestanding/audit
  scripts/m14_preflight.sh      — preflight script
  artifacts/m14/                — 7 file evidence

File yang diubah:
  kernel/core/kmain.c — +2 baris: forward declaration m14_block_demo_init,
                        pemanggilan setelah m13_vfs_bootstrap()

Total: 16 file changed, 2400421 insertions (mayoritas dari artefak QEMU
log dan binary checksum besar; source code inti jauh lebih kecil)
```

### Lampiran C — Log Build (Ringkas)

```text
[make clean && make build — 27 file dikompilasi]
clang ... -c kernel/arch/x86_64/idt.c
clang ... -c kernel/arch/x86_64/pic.c
clang ... -c kernel/arch/x86_64/pit.c
clang ... -c kernel/block/bcache.c        ← baru M14
clang ... -c kernel/block/block.c         ← baru M14
clang ... -c kernel/block/block_demo.c    ← baru M14
clang ... -c kernel/block/ramblk.c        ← baru M14
clang ... -c kernel/core/kmain.c
clang ... -c kernel/core/log.c
clang ... -c kernel/core/panic.c
clang ... -c kernel/core/serial.c
clang ... -c kernel/core/trap.c
clang ... -c kernel/lib/memory.c
clang ... -c kernel/mcsos_thread.c
clang ... -c kernel/mm/kmem.c
clang ... -c kernel/sync/lockdep.c
clang ... -c kernel/sync/mutex.c
clang ... -c kernel/sync/spinlock.c
clang ... -c kernel/syscall/syscall.c
clang ... -c kernel/user/m11_elf_loader.c
clang ... -c kernel/vfs/fd.c
clang ... -c kernel/vfs/ramfs.c
clang ... -c kernel/vfs/sys_vfs.c
clang ... -c src/pmm.c
clang ... -c src/vmm.c
clang ... -c kernel/arch/x86_64/isr.S
clang ... -c kernel/syscall/syscall_entry.S
clang ... -c arch/x86_64/context_switch.S
ld.lld -nostdlib -static ... -o build/kernel.elf [29 object files]
[Tidak ada error/warning]
```

### Lampiran D — Log QEMU Penuh (Bagian M14)

```text
[M13] ramfs+vfs smoke test passed
[M13] motd.txt content: mcsos-m13-ramfs
[M14] block layer initialized
blk_count=0x0000000000000001
ram0_block_size=0x0000000000000200
ram0_block_count=0x0000000000000040
[M14] ram0 write/read roundtrip ok
```

### Lampiran E — Output Host Unit Test

```text
M14 host tests PASS
```

### Lampiran F — Output nm, readelf, objdump (Ringkas)

```text
[nm -u build/m14_block_layer.o — kosong]

[readelf -h build/m14_block_layer.o — ringkas]
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Number of section headers:         12
  Section header string table index: 11

[objdump — disassembly tersimpan di artifacts/m14_objdump_block.txt,
 mencakup mcsos_blk_register, mcsos_blk_read, mcsos_blk_write,
 mcsos_ramblk_init, mcsos_bcache_read, mcsos_bcache_write, dll.]
```

### Lampiran G — Checksum Artefak

```text
dc243ee2096ee9d74ac64df0a02f3a229482feb898cb2f7f51cf438866d1e423  build/block.o
f86848e89e6aadf6ca006f984c26af38b564397b2358e5e8abfbee84a935dc68  build/ramblk.o
02ef7db9aaf4cbe1310b17769af54df432ac82f001f414451f989ef111cb06fe  build/bcache.o
e2a3314a7e8d9ef38478f84ebc5d6b45bbe8b1fc4d1ce2163802e09ee089fe6e  build/m14_block_layer.o
abfae5e887a778bfe796c51c41317d0b0169c371b3f1493d7688de99ed79b4e7  build/test_m14_block
```

### Lampiran H — Jawaban Pertanyaan Analisis

**1. Mengapa block layer perlu memvalidasi range sebelum memanggil driver, padahal driver bisa memvalidasi sendiri?**
Jika setiap driver harus memvalidasi sendiri, validasi akan terduplikasi di setiap implementasi driver (RAM, SATA, NVMe, virtio-blk di masa depan), meningkatkan risiko satu driver lupa memvalidasi dan menerima LBA/count berbahaya. Dengan satu titik validasi di block layer (`mcsos_blk_validate_range`), driver dapat berasumsi input sudah aman, dan audit keamanan cukup fokus ke satu fungsi.

**2. Mengapa kalkulasi range menggunakan `count > block_count - lba`, bukan `lba + count > block_count`?**
Pada tipe `uint64_t`, jika `lba` mendekati nilai maksimum, `lba + count` dapat overflow dan wraparound ke nilai kecil, lolos dari pemeriksaan `> block_count` padahal sebenarnya tidak valid. Karena `lba < block_count` sudah dipastikan terlebih dahulu, `block_count - lba` selalu aman dari underflow, sehingga perbandingan `count > block_count - lba` bebas dari risiko overflow.

**3. Apa perbedaan mendasar antara write-back dan write-through, dan mengapa M14 memilih write-back?**
Write-through menulis langsung ke device setiap kali cache ditulis — sederhana dan konsisten tetapi lambat karena setiap operasi menunggu I/O fisik. Write-back menunda penulisan ke device sampai flush eksplisit atau eviction — lebih cepat untuk akses berulang tetapi berisiko kehilangan data jika sistem crash sebelum flush. M14 memilih write-back untuk memperkenalkan trade-off ini sebagai materi pembelajaran, dengan risiko power-loss dicatat secara eksplisit sebagai non-goal yang belum dimitigasi.

**4. Mengapa entry dirty harus di-flush sebelum dipakai ulang sebagai victim cache?**
Jika entry dirty langsung ditimpa tanpa flush, data yang sudah ditulis ke cache tetapi belum sampai device akan hilang permanen — padahal caller mengira data sudah "tersimpan" setelah `bcache_write` berhasil. `mcsos_bcache_select_victim` memanggil `mcsos_bcache_flush_entry` terlebih dahulu untuk memastikan data dirty benar-benar sampai device sebelum slot cache dipakai untuk blok lain.

**5. Mengapa RAM block driver tidak melakukan dynamic allocation?**
Driver freestanding kernel tidak memiliki heap yang andal sebelum subsistem memori (PMM/VMM/kmem) sepenuhnya siap, dan driver hardware nyata sering beroperasi pada tahap boot awal sebelum heap kompleks tersedia. Dengan menerima `storage` sebagai parameter dari caller, driver dapat diuji di host (tanpa kernel sama sekali) dan lifetime memori menjadi eksplisit milik caller, bukan tersembunyi di dalam driver.

**6. Apa risiko jika `flush` pada RAM block driver bukan no-op, melainkan benar-benar menyalin data ke "media" lain?**
Jika RAM block driver memiliki semantik flush yang menyalin data ke lokasi lain (misalnya untuk simulasi persistence), maka model mental "data sudah di device setelah flush" menjadi tidak konsisten dengan kenyataan bahwa RAM block tetaplah volatil. M14 sengaja membuat `flush` no-op sukses karena data RAM block driver sudah berada di backing memory sejak `write` dipanggil — tidak ada lapisan fisik tambahan yang perlu disinkronkan.

**7. Mengapa buffer cache M14 belum SMP-safe, dan apa konsekuensinya?**
M14 dibangun di atas concurrency model single-core educational baseline (M12 baru memperkenalkan lock dasar). Tanpa lock internal, dua thread yang memanggil `mcsos_bcache_read`/`write` secara bersamaan dapat menyebabkan race condition pada `clock_hand`, `entries[]`, atau `dirty` flag — misalnya dua thread memilih entry victim yang sama, atau satu thread membaca entry yang sedang dimodifikasi thread lain. Konsekuensinya, caller wajib menyerialisasi akses dengan lock eksternal (dari M12) jika SMP diaktifkan; M14 sendiri tidak menyediakan proteksi ini.

**8. Mengapa M14 secara eksplisit bukan bukti "storage persistent aman terhadap power-loss"?**
RAM block driver bersifat volatil by design — data hilang sepenuhnya saat QEMU di-reset atau host dimatikan, karena tidak ada penulisan ke media fisik. Bahkan bila driver fisik nyata digunakan, buffer cache write-back M14 tidak memiliki mekanisme crash consistency (seperti journal atau write-ahead log) yang menjamin data dirty tetap konsisten setelah crash mendadak. Klaim "aman terhadap power-loss" membutuhkan jaminan tambahan (atomic write, journal, atau ordering guarantee) yang sama sekali belum dibangun di M14 — ini secara eksplisit didokumentasikan sebagai non-goal.

**9. Apa hubungan desain M14 dengan model `blk-mq` (multi-queue) pada Linux?**
M14 menggunakan model single-queue konseptual: setiap operasi block langsung dipanggil secara sinkron melalui `mcsos_blk_read/write/flush` tanpa antrian atau penjadwalan request. `blk-mq` pada Linux memperkenalkan multiple hardware queue dan software queue untuk memanfaatkan paralelisme perangkat modern (NVMe dengan banyak queue), dengan request scheduling, merging, dan batching. M14 sengaja tidak mengadopsi model ini karena kompleksitasnya jauh melebihi kebutuhan perangkat sintetis RAM block — model single-queue cukup untuk membuktikan invariant dasar block layer sebelum performa menjadi pertimbangan.

**10. Bagaimana block layer M14 dapat diperluas untuk mendukung driver virtio-blk di masa depan tanpa mengubah VFS?**
Kontrak `mcsos_blk_ops_t` (operation table dengan `read`/`write`/`flush`) sudah memisahkan caller dari implementasi driver. Untuk menambahkan virtio-blk, cukup membuat file driver baru (misalnya `virtioblk.c`) yang mengimplementasikan fungsi sesuai signature `mcsos_blk_rw_fn`, lalu mendaftarkan device melalui `mcsos_blk_register` — persis seperti `ramblk.c`. Block layer (`block.c`) dan kontrak VFS di atasnya tidak perlu diubah sama sekali; hanya bagian inisialisasi kernel yang memilih device mana yang didaftarkan (RAM untuk testing, virtio-blk untuk hardware nyata).

---

## 24. Daftar Referensi

```text
[1] The Linux Kernel Organization, "Block Layer," Linux Kernel
    Documentation, 2026. [Online]. Available:
    https://www.kernel.org/doc/html/latest/block/index.html
    Accessed: Jun. 30, 2026.

[2] The Linux Kernel Organization, "Multi-Queue Block IO Queueing
    Mechanism (blk-mq)," Linux Kernel Documentation, 2026. [Online].
    Available: https://www.kernel.org/doc/html/latest/block/blk-mq.html
    Accessed: Jun. 30, 2026.

[3] The Linux Kernel Organization, "Null block device driver," Linux
    Kernel Documentation, 2026. [Online]. Available:
    https://www.kernel.org/doc/html/latest/block/null_blk.html
    Accessed: Jun. 30, 2026.

[4] QEMU Project, "QEMU System Emulation — Invocation," QEMU
    Documentation, 2026. [Online]. Available:
    https://www.qemu.org/docs/master/system/invocation.html
    Accessed: Jun. 30, 2026.

[5] QEMU Project, "GDB usage — QEMU documentation," QEMU System Emulation
    Documentation, 2026. [Online]. Available:
    https://www.qemu.org/docs/master/system/gdb.html
    Accessed: Jun. 30, 2026.

[6] LLVM Project, "Clang command line argument reference," LLVM
    Documentation, 2026. [Online]. Available:
    https://clang.llvm.org/docs/ClangCommandLineReference.html
    Accessed: Jun. 30, 2026.

[7] Free Software Foundation, "GNU Binutils: nm, readelf, objdump," GNU
    Binutils Documentation, 2026. [Online]. Available:
    https://sourceware.org/binutils/docs/
    Accessed: Jun. 30, 2026.
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
| Artefak penting tersedia di `artifacts/m14` | `Ya` |
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
45893d1
```

Status akhir yang diklaim:

```text
Siap uji QEMU untuk block device layer, RAM block driver, dan buffer
cache minimal — bukan bukti storage persistent aman terhadap power-loss,
bukan bukti driver hardware siap, dan bukan bukti filesystem
crash-consistent, dengan known issues pada bagian 20 (rollback belum
diuji aktual, GDB session formal belum dilakukan, tugas pengayaan belum
diimplementasikan, buffer cache belum SMP-safe).
```

Ringkasan satu paragraf:

```text
Praktikum M14 MCSOS 260502 telah diselesaikan oleh Agung Nurjaman
(25832073010) secara individu, melanjutkan dari gate M13 yang solid
(commit 8b8d15a). Block device layer awal berhasil dibangun lengkap:
registry 8 device dengan validasi overflow-safe (mcsos_blk_register/get/
count/read/write/flush), RAM block driver deterministik tanpa dynamic
allocation (mcsos_ramblk_init/rw), dan buffer cache minimal write-back
dengan clock-hand eviction serta dirty flag (mcsos_bcache_init/read/write/
flush_all). Host unit test lulus mencakup happy path, boundary case
(ERANGE untuk LBA out-of-range dan overflow count, EINVAL untuk count nol
dan buffer NULL), serta pembuktian eksplisit bahwa data write-back belum
sampai device sebelum flush — invariant kunci buffer cache. Audit
freestanding membuktikan nm -u kosong pada linked relocatable object
m14_block_layer.o, ELF64 x86-64 REL terverifikasi via readelf, disassembly
tersimpan via objdump, dan checksum 5 artefak tersimpan via sha256sum.
Build kernel berhasil dengan 4 object kernel/block/* terintegrasi otomatis
melalui SRC_C Makefile utama. Initializer kernel (block_demo.c) mendaftarkan
device ram0 dengan fail-closed via KERNEL_PANIC jika init gagal. QEMU smoke
test membuktikan [M14] block layer initialized dan [M14] ram0 write/read
roundtrip ok, dengan M13 VFS/RAMFS tidak mengalami regresi. Commit M14
(45893d1) tersimpan bersih di branch praktikum-m14-block-device dengan 11
artefak evidence. Status readiness yang diklaim adalah siap uji QEMU untuk
block device layer, RAM block driver, dan buffer cache minimal, secara
eksplisit bukan bukti storage persistent aman terhadap power-loss, bukan
bukti driver hardware siap, dan bukan bukti filesystem crash-consistent —
ketiganya adalah non-goals M14 yang akan ditindaklanjuti pada milestone
filesystem persistent berikutnya.
```
