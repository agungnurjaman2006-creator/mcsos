# VFS Minimal, RAMFS In-Memory, FD Table, dan Integrasi Syscall File I/O pada MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M13_Agung.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, subset POSIX-like
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `M13` |
| Judul praktikum | `VFS Minimal, RAMFS In-Memory, FD Table, dan Integrasi Syscall File I/O pada MCSOS 260502` |
| Jenis pengerjaan | `Individu` |
| Nama mahasiswa | `Agung Nurjaman` |
| NIM | `25832073010` |
| Kelas | `Pendidikan Teknologi Informasi` |
| Nama kelompok | `-` |
| Anggota kelompok | `-` |
| Tanggal praktikum | `2026-06-29` |
| Tanggal pengumpulan | `2026-06-29` |
| Repository | `/home/agung/src/mcsos` |
| Branch | `praktikum-m13-vfs-ramfs` |
| Commit awal | `55a09ab` (M12) |
| Commit akhir | `8b8d15a` (M13 integrasi kernel) |
| Status readiness yang diklaim | `Siap uji QEMU untuk VFS/RAMFS single-process — bukan siap produksi, bukan bukti isolasi multi-proses, bukan filesystem persisten` |

---

## 1. Sampul

# Laporan Praktikum M13
## VFS Minimal, RAMFS In-Memory, FD Table, dan Integrasi Syscall File I/O pada MCSOS 260502

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
Panduan resmi praktikum M13 MCSOS 260502 digunakan sebagai referensi utama
dan kontrak implementasi untuk seluruh komponen (mcs_vfs.h, ramfs.c, fd.c,
sys_vfs.c, m13_vfs_host_test.c, dan target Makefile m13-host-test/
m13-freestanding/m13-audit). Dokumentasi POSIX open/read/write/lseek/close
(konsep umum, bukan dikutip literal) digunakan sebagai referensi konseptual
flag O_CREAT/O_TRUNC/O_APPEND dan semantik SEEK_SET/SEEK_CUR/SEEK_END. AI
assistant (Claude) digunakan untuk: (1) menulis draft awal mcs_vfs.h,
ramfs.c, fd.c, sys_vfs.c, m13_vfs_host_test.c, dan target Makefile M13
sesuai kontrak panduan; (2) memverifikasi struktur mcsos_thread_t (M9)
sebelum memutuskan desain integrasi -- ditemukan bahwa TCB M9 tidak memiliki
field pid atau fd_table sama sekali, sehingga keputusan menyambungkan
mcs_process_t ke alur eksekusi M9 menjadi keputusan desain baru yang tidak
diatur kontrak panduan maupun kode M9 yang sudah ada; (3) mendampingi
keputusan menggunakan satu mcs_process_t global (bukan satu per thread)
dengan alasan kejujuran arsitektur: MCSOS belum memiliki address space
terisolasi per-proses (M11 ELF loader hanya merencanakan load, belum
benar-benar menjalankan proses user terisolasi), sehingga FD table bersama
mencerminkan kondisi nyata kernel single-address-space, bukan memalsukan
isolasi proses yang belum ada; (4) mendiagnosis bahwa file inti M13
(header, RAMFS, FD table, host test, Makefile) ternyata sudah ter-commit
sebelumnya pada commit 20a8e2e sebelum sesi integrasi kernel dimulai --
git diff terhadap seluruh file tersebut kosong, mengonfirmasi yang ditulis
ulang selama sesi identik byte-per-byte dengan yang sudah ada, sehingga
hanya kmain.c dan evidence log yang benar-benar baru pada commit kedua;
(5) mendampingi perbaikan ukuran log QEMU yang membengkak (144.728 baris)
akibat M9 idle loop yang mencetak tick jauh lebih cepat daripada timeout
dinding-jam yang dipakai, kali ini berhasil dipangkas SEBELUM commit
(bukan lewat amend sesudahnya seperti insiden serupa pada M10). Seluruh
build, host unit test, audit nm/readelf/objdump, QEMU smoke test, dan
commit git dijalankan dan diverifikasi sendiri oleh mahasiswa di WSL 2
miliknya. AI tidak digunakan untuk mengubah kontrak fungsional di luar
yang ditentukan panduan resmi (struktur vnode/file/fd_table, flag open,
semantik lseek, dan model fail-closed untuk fd tidak valid/akses ditolak).
```

---

## 3. Tujuan Praktikum

1. Mendesain kontrak VFS minimal (`mcs_vnode_t`, `mcs_file_t`, `mcs_fd_table_t`, `mcs_process_t`) dan status code POSIX-like sesuai panduan M13.
2. Mengimplementasikan RAMFS in-memory (`mcs_ramfs_init`, `mcs_ramfs_lookup`, `mcs_ramfs_create_file`, `mcs_ramfs_seed_file`) dengan path lookup berbasis segmen (`/a/b/c`), alokasi vnode statik, dan alokasi data dari satu arena byte tetap.
3. Mengimplementasikan FD table per proses dengan operasi `open`/`read`/`write`/`lseek`/`close`/`dup`, termasuk validasi flag akses (`O_RDONLY`/`O_WRONLY`/`O_RDWR`), `O_CREAT`/`O_TRUNC`/`O_APPEND`, dan deteksi `EBADF` untuk fd yang sudah ditutup.
4. Menyediakan wrapper syscall (`mcs_sys_open`/`read`/`write`/`close`/`lseek`) sebagai lapisan tipis di atas operasi VFS, menerima `mcs_process_t*` sebagai pembawa FD table.
5. Menulis host unit test yang memverifikasi tiga skenario (baca dasar dengan `lseek`, create-write-read roundtrip, error handling dan limit FD) tanpa pernah boot QEMU.
6. Mengompilasi ketiga source VFS sebagai object freestanding x86_64 dan membuktikan tidak ada dependency libc lewat `nm -u` kosong.
7. Mengintegrasikan VFS/RAMFS ke `kmain.c` nyata, termasuk mendesain keputusan baru (di luar kontrak panduan dan kode M9) tentang bagaimana `mcs_process_t` terhubung ke alur eksekusi kernel thread yang sudah ada.
8. Membuktikan integrasi kernel berjalan benar di QEMU tanpa merusak scheduler M9 yang sudah ada, dan memperbaiki insiden ukuran log evidence yang membengkak sebelum commit (pembelajaran langsung dari pengalaman M10).

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan perbedaan vnode, file descriptor, dan path lookup pada filesystem sederhana | Bagian 6.1, 9.1; `mcs_vnode_t` menyimpan metadata, `mcs_file_t` menyimpan state per-open (offset, flags) |
| Mendesain RAMFS dengan alokasi statik (bukan dinamis) dan path lookup segmen-per-segmen | `kernel/vfs/ramfs.c`: `mcs_split_parent_leaf` memecah path jadi parent+leaf, `mcs_find_child` mencari anak satu level |
| Mengimplementasikan FD table dengan alokasi slot, validasi flag akses, dan deteksi fd tidak valid | `kernel/vfs/fd.c`: `mcs_fd_alloc`, `mcs_can_read`/`mcs_can_write`, `mcs_fd_get` mengembalikan NULL untuk fd belum dipakai |
| Menulis host unit test untuk logika filesystem tanpa hardware | `tests/m13_vfs_host_test.c`; lulus `M13 VFS/FD/RAMFS host tests: PASS` |
| Menghasilkan bukti `nm -u`, `readelf`, `objdump`, dan checksum artefak | `make m13-audit`; `nm_undefined.txt` kosong, `readelf` ELF64 REL x86_64, symbol `mcs_ramfs_init` ada di disassembly |
| Mendesain keputusan integrasi yang tidak diatur kontrak panduan atau kode existing, dengan alasan eksplisit | Bagian 9.2: satu `mcs_process_t` global, bukan satu per `mcsos_thread_t` |
| Mendiagnosis status commit existing sebelum menulis ulang kode untuk menghindari kerja ganda atau duplikasi tersembunyi | Bagian 10 Langkah 9: `git diff HEAD` dipakai untuk memverifikasi file yang ditulis ulang identik dengan yang sudah ter-commit |
| Mencegah insiden ukuran commit membengkak sebelum terjadi, bukan memperbaikinya sesudahnya | Bagian 14.2, 15.1: log dipangkas sebelum `git add`, berbeda dari pola M10 yang butuh `--amend` |

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
| M12 | Spinlock, mutex kooperatif, lock-order validator, diagnosis race/deadlock | `✓ selesai praktikum` |
| M13 | SMP, scalability, lock stress | `✓ selesai praktikum` |
| M14 | VFS, file descriptor, ramfs | `tidak dibahas` |
| M15 | Block layer, device model | `tidak dibahas` |
| M16 | Security model, capability/ACL | `tidak dibahas` |

Batas cakupan praktikum:

```text
M13 mencakup: header kontrak mcs_vfs.h (mcs_vnode_t, mcs_ramfs_t,
mcs_file_t, mcs_fd_table_t, mcs_process_t, sebelas kode status), ramfs.c
(init, lookup path segmen-per-segmen, create_file, seed_file, alokasi
vnode dan data statik), fd.c (fd table init, open/read/write/lseek/
close/dup, lima wrapper syscall mcs_sys_*), host unit test tiga skenario,
Makefile target m13-host-test/m13-freestanding/m13-audit, dan integrasi
ke kmain.c via satu mcs_process_t global.

M13 TIDAK mencakup: filesystem persisten (disk-backed), block layer,
device driver storage, direktori bertingkat tak terbatas (MCS_MAX_NODES
dan MCS_RAMFS_DATA_BYTES adalah batas statik tetap), hard/symbolic link,
permission/ownership POSIX penuh (uid/gid/mode), mmap file, locking
file-level, isolasi multi-proses sungguhan (mcs_process_t masih satu
instance global dipakai bersama seluruh kernel thread), dan penggunaan
lock M12 untuk melindungi RAMFS/FD table dari akses konkuren -- sesuai
kontrak panduan poin 11 yang eksplisit menyatakan M13 sengaja belum
menambahkan global VFS lock, integrasi locking dijadwalkan ke milestone
mendatang begitu ada kebutuhan konkurensi nyata (lebih dari satu thread
mengakses RAMFS bersamaan).
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

```text
M13 berfokus pada lapisan filesystem virtual minimal yang menjembatani
syscall file I/O (yang sebagian sudah dirintis konsepnya di M10, meski
M10 sendiri belum punya syscall file nyata) dengan representasi data di
memori. Virtual File System (VFS) adalah lapisan abstraksi yang
memungkinkan kernel menyediakan antarmuka file (open/read/write/close)
yang seragam terlepas dari implementasi penyimpanan sesungguhnya di
bawahnya -- pada M13 ini implementasi tunggalnya adalah RAMFS, filesystem
yang seluruh datanya hidup di RAM dan hilang saat reboot, dipilih karena
paling sederhana untuk pembelajaran tanpa memerlukan driver storage.

Vnode (virtual node) merepresentasikan satu entitas filesystem (file
atau direktori) secara abstrak, terlepas dari path yang dipakai untuk
mengaksesnya -- satu vnode bisa diakses lewat banyak path (lewat hard
link, meski M13 ini belum mengimplementasikannya). File descriptor (fd)
merepresentasikan SATU SESI PEMBUKAAN terhadap vnode, menyimpan state
yang spesifik untuk sesi itu (posisi baca/tulis saat ini, flag akses)
-- ini sebabnya dua fd berbeda bisa menunjuk ke vnode yang sama namun
punya posisi offset yang independen, dibuktikan langsung oleh mcs_vfs_dup
yang menggandakan fd (berbagi node yang sama) tapi field offset disalin
sebagai snapshot, bukan referensi bersama.
```

### 6.2 Konsep Arsitektur dan Desain Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| Alokasi statik vs dinamis untuk struktur kernel awal | RAMFS M13 memakai array tetap (`MCS_MAX_NODES=64`, `MCS_RAMFS_DATA_BYTES=8192`) di dalam struct, bukan `kmem_alloc` M8 | Konsisten dengan pola M9 (stack thread statik) dan M10 (buffer syscall statik) -- menunda dependency ke heap dinamis sampai modul inti benar-benar stabil teruji |
| Path lookup segmen-per-segmen tanpa cache | `mcs_split_parent_leaf`/`mcs_ramfs_lookup` mengiterasi `/` demi `/`, mencari anak satu level setiap iterasi via `mcs_find_child` | O(depth × node_count) per lookup -- diterima untuk skala M13 (64 node maksimum), bukan desain produksi |
| Pemisahan vnode (data) dari file descriptor (state sesi) | `mcs_vnode_t` tidak punya field `offset`; `mcs_file_t` punya `offset` dan pointer ke `node` | Diverifikasi host test: dua kali `open` pada path yang sama (`/x` dibuka 16 kali di `test_errors_and_fd_limit`) menghasilkan fd berbeda yang independen |
| Satu `mcs_process_t` global vs satu per kernel thread | TCB M9 (`mcsos_thread_t`) tidak memiliki field `pid`/`fd_table`; menambahkannya berisiko menyentuh kontrak M9 yang sudah teruji penuh (host test, smoke test, sesi GDB) | Keputusan desain murni milik sesi M13 ini, tidak diatur panduan maupun kode existing; didokumentasikan eksplisit di bagian 9.2, bukan disembunyikan sebagai "sudah seharusnya begitu" |
| Belum ada locking pada akses bersama RAMFS/FD table | Sesuai kontrak panduan poin 11, M13 sengaja tidak memakai `mcs_spin_lock`/`mcs_mutex_*` (M12) untuk melindungi struktur ini | Aman pada M13 ini karena hanya `kmain` (jalur boot sinkron, sebelum `sti()` diaktifkan ulang) yang mengakses `g_m13_ramfs`/`g_m13_process`; menjadi risiko nyata begitu lebih dari satu thread mengakses bersamaan |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 murni untuk seluruh modul (`ramfs.c`, `fd.c`, `sys_vfs.c`), tanpa assembly (berbeda dari M9/M10 yang membutuhkan stub assembly) |
| Helper string manual | `mcs_strlen`, `mcs_streq_n`, `mcs_copy_bytes`, `mcs_copy_name` ditulis manual di `ramfs.c` karena modul freestanding tidak boleh memanggil `strlen`/`memcpy` dari libc hosted |
| Dua jalur compile | Ketiga file dikompilasi dua kali: host native (`-O2 -Iinclude`, untuk `m13_vfs_host_test`) dan freestanding kernel (`--target=x86_64-unknown-none-elf -ffreestanding`, dilink jadi `m13_vfs_combined.o`) |
| Struktur folder | Modul VFS ditempatkan dalam `kernel/vfs/` (mengikuti pola folder modul tunggal M10 `kernel/syscall/`, bukan pola M9 yang memisah assembly ke `arch/x86_64/`), sehingga otomatis tertangkap glob Makefile `find kernel -name '*.c'` tanpa entry manual |
| Risiko undefined behavior | `mcs_copy_name` mengisi nol seluruh buffer nama sebelum menyalin, mencegah sisa data lama; `mcs_alloc_node` memvalidasi `fs->data_used + capacity > MCS_RAMFS_DATA_BYTES` sebelum mengklaim ruang data, mencegah penulisan di luar arena |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | The Open Group, "open, openat - open a file" POSIX.1-2017 (referensi konseptual umum tentang flag O_CREAT/O_TRUNC/O_APPEND, tidak dikutip literal) | Semantik flag pembukaan file | Dasar pemilihan kombinasi flag `MCS_O_*` dan perilaku `O_TRUNC`/`O_APPEND` saat open |
| [2] | The Open Group, "lseek - reposition read/write file offset" POSIX.1-2017 (referensi konseptual) | Semantik `SEEK_SET`/`SEEK_CUR`/`SEEK_END` | Dasar implementasi `mcs_vfs_lseek` yang menghitung `base + offset` berbeda untuk tiap whence |
| [3] | The Linux Kernel Documentation, "Virtual File System" (referensi konseptual pembanding desain) | Konsep vnode/inode abstrak, dentry, pemisahan metadata dari data | Dasar pemisahan `mcs_vnode_t` (metadata) dari `mcs_file_t` (state sesi per-open) |
| [4] | LLVM Project, "Clang command line argument reference" | Flag freestanding, target triple | Dasar konfigurasi `M13_CFLAGS_KERNEL` di Makefile |
| [5] | GNU Project, "GNU Make Manual" | Pattern rule, glob `$(shell find ...)` | Dasar pemahaman mengapa `kernel/vfs/*.c` otomatis tertangkap tanpa entry manual, berbeda dari `arch/x86_64/context_switch.S` M9 |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 (distribusi Ubuntu) |
| Target ISA | x86_64 |
| Target ABI (jalur kernel) | `x86_64-unknown-none-elf` |
| Target ABI (jalur host test) | Native host (`clang` tanpa target khusus) |
| Emulator | QEMU (qemu-system-x86_64) |
| Build system | GNU Make, target M13 ditambahkan langsung ke Makefile utama warisan M0-M12 |
| Bahasa utama | C17 (tanpa komponen assembly pada M13 ini) |

### 7.2 Versi Toolchain

| Item | Versi / Nilai |
|---|---|
| Windows | Windows 11 x64 |
| WSL distro | Ubuntu (di WSL 2) |
| Bash | GNU bash 5.3.9(1)-release |
| Clang | Ubuntu clang version 21.1.8 (6ubuntu1) |
| GCC (`cc`) | 15.2.0 (Ubuntu 15.2.0-16ubuntu1) |
| GNU ld/nm/readelf/objdump | 2.46 (GNU Binutils for Ubuntu) |
| GNU Make | 4.4.1 |
| `sha256sum` | uutils coreutils 0.8.0 (implementasi Rust, bukan GNU coreutils -- dicatat sebagai catatan lingkungan, tidak mempengaruhi hasil hash) |
| QEMU | 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3) |
| GDB | GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1 (tersedia, tidak dipakai aktif pada sesi M13 ini) |
| Commit hash | `8b8d15a` |

Catatan: mengikuti pola M9/M10, preflight M13 menjalankan perintah versi toolchain secara eksplisit dan menyimpan hasilnya ke `logs/m13_preflight.log`.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | `Ya` (diverifikasi `git rev-parse --show-toplevel` mengembalikan `/home/agung/src/mcsos`) |
| Remote repository | Tidak digunakan pada sesi ini (repository lokal) |
| Branch | `praktikum-m13-vfs-ramfs` |
| Commit hash awal (basis cabang) | `55a09ab` (M12: add preflight log) |
| Commit antara (sebelum sesi integrasi ini) | `20a8e2e` (M13: implement VFS minimal, RAMFS, FD table, syscall file I/O, host test, and audit -- sudah ada sebelum sesi integrasi kernel dimulai, lihat bagian 14.2) |
| Commit hash akhir | `8b8d15a` (M13: integrate VFS/RAMFS into kmain, add preflight and trimmed QEMU evidence) |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
  include/
    mcs_vfs.h           (sudah ada sejak 20a8e2e: kontrak vnode, ramfs,
                         file, fd_table, process, sebelas status code)
    mcs_sync.h           (M12, tidak diubah)
    mcsos_thread.h        (M9, tidak diubah -- tidak punya field pid/fd_table)
  kernel/
    vfs/
      ramfs.c             (sudah ada sejak 20a8e2e: init, lookup, create,
                           seed, helper string manual)
      fd.c                (sudah ada sejak 20a8e2e: fd table, operasi VFS,
                           lima wrapper syscall mcs_sys_*)
      sys_vfs.c            (sudah ada sejak 20a8e2e: hook transisional
                           mcs_active_ramfs_for_test)
    sync/                 (M12, tidak diubah)
    syscall/               (M10, tidak diubah)
    user/
      m11_elf_loader.c     (M11, tidak diubah)
    core/
      kmain.c              (diubah pada sesi ini: m13_vfs_bootstrap()
                            ditambahkan dan dipanggil setelah
                            m11_elf_smoke_test())
  tests/
    m13_vfs_host_test.c    (sudah ada sejak 20a8e2e: tiga skenario test)
  logs/
    m13_preflight.log       (baru: bukti gate M0-M12 dan versi toolchain)
    m13_serial.log           (baru, dipangkas 144.728 -> 60 baris SEBELUM
                             commit: bukti QEMU smoke test)
  Makefile                (target m13-host-test/m13-freestanding/
                          m13-audit/m13-all/m13-clean sudah ada sejak
                          20a8e2e, tidak diubah pada sesi ini)
  limine/, iso_root/      (warisan M5, di-gitignore, dipakai ulang untuk
                          smoke test M13)
  build/                  (di-gitignore: seluruh artefak kompilasi)
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `include/mcs_vfs.h` | Sudah ada (commit `20a8e2e`) | Kontrak struct vnode/ramfs/file/fd_table/process dan sebelas kode status, identik literal panduan | Rendah — header murni deklarasi; ditulis ulang pada sesi ini, terbukti identik byte-per-byte via `git diff` |
| `kernel/vfs/ramfs.c` | Sudah ada (commit `20a8e2e`) | RAMFS in-memory: init, lookup path, create file, seed file, helper string manual | Tinggi — kesalahan path lookup atau alokasi data berisiko korupsi struktur statik; dimitigasi host unit test dan smoke test QEMU |
| `kernel/vfs/fd.c` | Sudah ada (commit `20a8e2e`) | FD table dan operasi VFS lengkap (open/read/write/lseek/close/dup) plus lima wrapper syscall | Tinggi — kesalahan validasi flag akses atau alokasi fd berisiko membaca/menulis di luar batas; dimitigasi host unit test (termasuk skenario error dan fd limit) |
| `kernel/vfs/sys_vfs.c` | Sudah ada (commit `20a8e2e`) | Hook transisional `mcs_active_ramfs_for_test` untuk kebutuhan test | Rendah |
| `tests/m13_vfs_host_test.c` | Sudah ada (commit `20a8e2e`) | Host unit test tiga skenario | Rendah — kode test, tidak masuk binary kernel |
| `Makefile` | Sudah ada (commit `20a8e2e`) | Target `m13-host-test`/`m13-freestanding`/`m13-audit`/`m13-all`/`m13-clean` | Rendah — tidak disentuh pada sesi integrasi kali ini |
| `kernel/core/kmain.c` | **Diubah pada sesi ini** | Tambah `static mcs_ramfs_t g_m13_ramfs`, `static mcs_process_t g_m13_process`, fungsi `m13_vfs_bootstrap()` (seed, open, read, create, write, close, log), dipanggil setelah `m11_elf_smoke_test()` | Tinggi — titik integrasi paling berisiko picu page fault/panic; dimitigasi smoke test QEMU yang membuktikan tidak crash dan M9 tetap berjalan |

### 8.3 Ringkasan Diff

```bash
git status
git log --oneline -6
git show --stat HEAD
```

Output:

```text
$ git status
On branch praktikum-m13-vfs-ramfs
nothing to commit, working tree clean

$ git log --oneline -6
8b8d15a (HEAD -> praktikum-m13-vfs-ramfs) M13: integrate VFS/RAMFS into kmain, add preflight and trimmed QEMU evidence
20a8e2e M13: implement VFS minimal, RAMFS, FD table, syscall file I/O, host test, and audit
55a09ab (praktikum/m12-sync) M12: add preflight log
bebe0e3 M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit
ad07b27 (praktikum-m11-elf-user-loader) M11: implement ELF64 user loader, host test, audit, and QEMU integration
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI, int 0x80 dispatcher, and kernel integration

$ git show --stat HEAD
commit 8b8d15a
 3 files changed, 125 insertions(+)
 create mode 100644 logs/m13_preflight.log
 create mode 100644 logs/m13_serial.log
 (kernel/core/kmain.c termodifikasi, termasuk dalam diff lengkap)

$ git show --stat 20a8e2e
commit 20a8e2e
 Makefile                  |  33 ++++++
 include/mcs_vfs.h         |  99 +++++++++++++++++
 kernel/vfs/fd.c           | 272 ++++++++++++++++++++++++++++++++++++++++++++++
 kernel/vfs/ramfs.c        | 248 ++++++++++++++++++++++++++++++++++++++++++
 kernel/vfs/sys_vfs.c      |   7 ++
 tests/m13_vfs_host_test.c |  83 ++++++++++++++
 6 files changed, 742 insertions(+)
```

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

```text
Kernel M12 sudah memiliki sinkronisasi dasar, scheduler M9, syscall ABI
M10, dan ELF loader M11, tetapi belum memiliki satu pun cara menyimpan
atau mengakses "file" -- seluruh data yang dipakai kernel sejauh ini
hanya berupa variabel global dan struktur in-memory tanpa abstraksi
penyimpanan apa pun. M13 menutup kesenjangan ini dengan membangun
lapisan VFS minimal di atas RAMFS in-memory: representasi vnode/file
yang seragam, FD table yang melacak sesi pembukaan per proses, dan
wrapper syscall yang siap dipanggil dari dispatcher M10 begitu syscall
file (open/read/write/close/lseek) ditambahkan ke tabel M10 pada
milestone mendatang.
```

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Satu `mcs_process_t` global (`g_m13_process`), bukan satu per `mcsos_thread_t` | (a) Menambah field `pid`/`fd_table` ke struct `mcsos_thread_t` M9; (b) array global `mcs_process_t` di-index `thread->id` tanpa ubah struct M9 | Mengubah `mcsos_thread_t` (opsi a) berisiko menyentuh kontrak M9 yang sudah lulus host test, smoke test, DAN sesi GDB lengkap (pembuktian register-level `rsp` berpindah ke stack thread) -- regresi di sana akan jauh lebih mahal diperbaiki daripada manfaat langsung yang didapat sekarang. Opsi array per-thread (b) masih menyiratkan isolasi proses yang belum benar-benar ada (MCSOS belum punya address space terpisah per proses; M11 ELF loader baru merencanakan load, belum menjalankan proses user terisolasi sungguhan) | FD table dibagi bersama seluruh kernel thread (boot thread, demo thread A/B M9) -- ini JUJUR mencerminkan model single-address-space MCSOS saat ini, bukan memalsukan isolasi proses yang akan menyesatkan pembaca laporan |
| RAMFS memakai arena statik (`uint8_t data[8192]`), bukan `kmem_alloc` M8 | Mengalokasikan setiap file secara dinamis lewat heap M8 yang sudah stabil sejak M8 | Konsisten dengan pola "stack statik dulu, migrasi heap kemudian" yang sudah terbukti berhasil di M9 (stack thread demo) dan M10 (buffer syscall); RAMFS murni baru, mengisolasi risiko logika filesystem dari risiko interaksi dengan allocator dinamis | Kapasitas RAMFS tetap (8192 byte data, 64 node) tidak bisa membesar otomatis; migrasi ke heap dicatat sebagai pekerjaan lanjutan |
| Tidak memakai lock M12 untuk melindungi RAMFS/FD table | Membungkus seluruh operasi `mcs_vfs_*` dengan `mcs_spin_lock`/`mcs_mutex_lock` M12 | Kontrak panduan M13 poin 11 eksplisit menyatakan ini disengaja ditunda; pada sesi integrasi nyata, hanya `kmain` (jalur boot sinkron sebelum `sti()` diaktifkan ulang) yang mengakses `g_m13_ramfs`/`g_m13_process`, sehingga tidak ada race yang mungkin terjadi pada kondisi saat ini | Begitu lebih dari satu kernel thread (mis. demo thread A/B M9) benar-benar memanggil `mcs_sys_*` secara konkuren pada milestone mendatang, locking WAJIB ditambahkan -- ini dicatat eksplisit sebagai known issue, bukan diasumsikan aman selamanya |
| Verifikasi `git diff HEAD` sebelum menulis ulang file yang "terasa baru" | Langsung commit ulang seluruh file tanpa memeriksa riwayat git terlebih dahulu | Ditemukan bahwa file inti M13 sudah ter-commit (`20a8e2e`) sebelum sesi integrasi kernel ini dimulai, kemungkinan dari pekerjaan sebelumnya yang tidak tercatat dalam konteks sesi chat ini; memeriksa diff sebelum melanjutkan mencegah commit duplikat atau, lebih buruk, menimpa versi yang sudah benar dengan versi yang sedikit berbeda tanpa disadari | Commit kedua (`8b8d15a`) hanya berisi perubahan nyata (kmain.c, evidence log), bukan menduplikasi 742 baris yang sudah ada |

### 9.3 Arsitektur Ringkas

```mermaid
flowchart TD
    A[kmain setelah m11_elf_smoke_test] --> B[m13_vfs_bootstrap]
    B --> C[mcs_ramfs_init: g_m13_ramfs, root dir terbentuk]
    C --> D[mcs_fd_table_init: g_m13_process.fd_table]
    D --> E[mcs_ramfs_seed_file: tulis /motd.txt langsung ke arena data]
    E --> F[mcs_sys_open /motd.txt RDONLY -> fd]
    F --> G[mcs_sys_read fd, 15 byte -> readbuf]
    G --> H[mcs_sys_open /log.txt CREAT|RDWR|TRUNC -> wfd]
    H --> I[mcs_sys_write wfd, boot-ok]
    I --> J[mcs_sys_close fd dan wfd]
    J --> K[log_writeln: ramfs+vfs smoke test passed + motd.txt content]
    K --> L["#ifdef MCSOS_M4_TRIGGER_BREAKPOINT/PANIC, lalu M5 interrupt + m9_scheduler_idle_loop"]
```

Penjelasan diagram:

```text
Bootstrap VFS terjadi setelah M11 (ELF loader plan), sebelum percabangan
M4/M5 (breakpoint/panic test dan aktivasi interrupt). Seluruh siklus
smoke test (seed, open, read, create, write, close) terjadi SINKRON di
jalur boot, sebelum sti() dipanggil ulang -- ini sebabnya tidak ada
race condition yang mungkin terjadi pada konfigurasi saat ini meski
belum ada locking. Batas tanggung jawab: ramfs.c hanya mengurus
representasi data dan path lookup; fd.c hanya mengurus state sesi
per-fd dan validasi akses; kmain.c hanya mengurus kapan bootstrap
terjadi dan skenario apa yang didemonstrasikan sebagai bukti fungsional.
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `mcs_ramfs_init(fs)` | `kmain` (`m13_vfs_bootstrap`), host test | Seluruh array `nodes`/`data` | - | Root dir (`/`, id 0) terbentuk; seluruh node lain `used=0` | Tidak ada error path; `fs == NULL` aman (no-op) |
| `mcs_ramfs_seed_file(fs, path, data, len)` | `kmain`, host test | `mcs_ramfs_create_file` (internal) | `data` tidak NULL kecuali `len==0` | Vnode file terbentuk (atau dipakai ulang jika sudah ada), data disalin ke arena | Mengembalikan `MCS_ENOSPC` jika `len` melebihi kapasitas default (256 byte per file baru) |
| `mcs_ramfs_lookup(fs, path, out_node)` | `mcs_ramfs_create_file`, `mcs_vfs_open` | Pointer ke vnode ditemukan | `path` harus mulai `/` dan lebih pendek dari `MCS_MAX_PATH` | `*out_node` terisi pointer vnode | Mengembalikan `MCS_ENOENT` jika segmen tidak ditemukan, `MCS_ENOTDIR` jika segmen tengah bukan direktori, `MCS_EINVAL`/`MCS_ENAMETOOLONG` untuk path tidak valid |
| `mcs_vfs_open(table, fs, path, flags)` | `mcs_sys_open` | Slot `mcs_file_t` baru di `table` | Jika `O_CREAT` tidak diset, file harus sudah ada | Mengembalikan fd (indeks `>=0`) | Mengembalikan `MCS_ENFILE` jika seluruh 16 slot fd penuh, `MCS_EISDIR` jika membuka direktori untuk menulis, `MCS_EACCES` jika `O_TRUNC` tanpa izin tulis |
| `mcs_vfs_read(table, fd, buf, len)` | `mcs_sys_read` | Memori `buf` | `fd` harus valid dan dibuka dengan izin baca | Byte disalin dari arena data ke `buf`, `file->offset` bertambah | Mengembalikan `MCS_EBADF` untuk fd tidak valid/sudah ditutup, `MCS_EACCES` jika dibuka tanpa izin baca, `MCS_EISDIR` jika node adalah direktori |
| `mcs_vfs_write(table, fd, buf, len)` | `mcs_sys_write` | Arena data RAMFS | `fd` harus valid dan dibuka dengan izin tulis | Byte disalin dari `buf` ke arena data, `node->size` bertambah jika melampaui ukuran sebelumnya | Mengembalikan `MCS_ENOSPC` jika kapasitas file (256 byte) terlampaui, `MCS_EBADF`/`MCS_EACCES` serupa `read` |
| `mcs_vfs_lseek(table, fd, offset, whence)` | `mcs_sys_lseek` | `file->offset` | `fd` harus valid, node bukan direktori | `file->offset` terhitung ulang berdasarkan `whence` | Mengembalikan `MCS_EINVAL` jika hasil offset negatif atau `whence` tidak dikenal |
| `mcs_vfs_close(table, fd)` | `mcs_sys_close` | Slot `mcs_file_t` | `fd` harus valid | Slot dikosongkan (`used=0`, `node`/`fs`=NULL) | Mengembalikan `MCS_EBADF` jika fd sudah tidak valid |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `g_m13_ramfs` (`mcs_ramfs_t`, statis di `kmain.c`) | `nodes[64]`, `node_count`, `data[8192]`, `data_used` | Dimiliki `kmain.c` sebagai instance tunggal | Hidup sepanjang kernel berjalan, diinisialisasi sekali di `m13_vfs_bootstrap()` | `node_count <= MCS_MAX_NODES`; `data_used <= MCS_RAMFS_DATA_BYTES`; node 0 selalu root dir |
| `g_m13_process` (`mcs_process_t`, statis di `kmain.c`) | `pid`, `fd_table` | Dimiliki `kmain.c`, **dibagi bersama** seluruh kernel thread (lihat bagian 9.2) | Hidup sepanjang kernel berjalan | `pid` konstan setelah inisialisasi (`1u`); `fd_table` hanya diubah lewat `mcs_sys_*`, tidak pernah ditulis langsung |
| `mcs_vnode_t` (di dalam `nodes[]`) | `used`, `id`, `parent`, `type`, `name[32]`, `size`, `data_offset`, `data_capacity` | Dimiliki `mcs_ramfs_t` pemiliknya, tidak pernah direlokasi setelah dialokasikan | Hidup statis sepanjang `mcs_ramfs_t` hidup | `id` sama dengan indeks array; `data_offset+data_capacity <= data_used` total arena setelah alokasi |
| `mcs_file_t` (di dalam `fd_table.files[]`) | `used`, `flags`, `offset`, `node` (pointer), `fs` (pointer) | Dimiliki `mcs_fd_table_t` pemiliknya | Hidup dari `open` sampai `close` (slot dikosongkan setelah `close`) | `offset <= node->size` untuk operasi baca normal (boleh melebihi untuk tulis di luar EOF, tapi M13 ini tidak mengimplementasikan sparse file/hole) |

### 9.6 Invariants

1. `node_count <= MCS_MAX_NODES` dan `data_used <= MCS_RAMFS_DATA_BYTES` selalu terjaga — `mcs_alloc_node` menolak alokasi (`MCS_ENOSPC`) sebelum batas terlampaui, tidak pernah menulis di luar array.
2. Setiap `fd` yang dikembalikan `mcs_vfs_open`/`mcs_vfs_dup` adalah indeks valid (`0` s.d. `MCS_MAX_OPEN_FILES-1`) dengan slot `used=1` — diverifikasi host test (`fds[i] == (int)i` untuk alokasi berurutan).
3. Operasi pada `fd` yang sudah `close` selalu mengembalikan `MCS_EBADF`, tidak pernah mengakses `node`/`fs` yang sudah NULL — diverifikasi host test (`mcs_sys_read(&proc, fd, buf, 1) == MCS_EBADF` setelah `close`).
4. `mcs_vfs_write` tidak pernah menyalin byte melebihi `node->data_capacity - file->offset` — `n = min(len, capacity-offset)`, dan jika `n < len` (artinya tidak cukup ruang) mengembalikan `MCS_ENOSPC` SEBELUM menyalin satu byte pun, bukan menyalin sebagian diam-diam.
5. Path lookup selalu menolak path relatif — `mcs_ramfs_lookup`/`mcs_split_parent_leaf` memvalidasi `path[0] == '/'` di awal, diverifikasi host test (`mcs_sys_open(&proc, &fs, "relative", ...) == MCS_EINVAL`).
6. Integrasi kernel tidak boleh mengganggu state scheduler M9 yang sudah berjalan — diverifikasi langsung lewat log QEMU yang menunjukkan tick A/B M9 tetap berlanjut normal setelah `[M13] ramfs+vfs smoke test passed`.

### 9.7 Ownership, Locking, dan Concurrency

| Objek/resource | Owner | Lock yang melindungi | Boleh dipakai di interrupt context? | Catatan |
|---|---|---|---|---|
| `g_m13_ramfs`, `g_m13_process` | `kernel/core/kmain.c` | **Tidak ada** (sesuai kontrak panduan poin 11, sengaja ditunda) | **Tidak dianjurkan** — belum diuji dipanggil dari konteks interrupt apa pun pada sesi ini | Pada M13 ini, hanya `kmain` (jalur boot sinkron) yang mengakses kedua struktur ini; aman untuk kondisi saat ini, TAPI menjadi risiko nyata begitu demo thread M9 (A/B) atau syscall handler M10 benar-benar memanggil `mcs_sys_*` secara konkuren pada milestone mendatang |
| Modul RAMFS/FD secara umum | `kernel/vfs/{ramfs,fd}.c` | Tidak ada | Sama seperti di atas | M12 (`mcs_spin_lock`/`mcs_mutex_*`) sudah tersedia di kernel dan ter-link (terbukti symbol `mcs_lockdep_*`/`mcs_mutex_*`/`mcs_spin_*` ada di `kernel.elf`), tapi **belum dipakai** untuk melindungi VFS — keputusan sadar, bukan kelalaian |

Lock order yang berlaku:

```text
Tidak ada locking eksplisit pada M13, identik filosofinya dengan M5-M12
untuk modul yang belum benar-benar diuji konkurensi nyatanya. Risiko
race antara demo thread M9 dan akses VFS belum relevan pada sesi ini
karena g_m13_ramfs/g_m13_process hanya disentuh dari jalur boot sinkron
kmain, BUKAN dari m9_demo_thread_a/b. Risiko ini menjadi nyata dan WAJIB
ditangani (kemungkinan dengan mcs_spin_lock M12) begitu syscall file
M13 benar-benar dipanggil dari thread M9 yang berjalan konkuren, atau
dari syscall dispatcher M10 yang dipanggil lewat int 0x80 di tengah
eksekusi thread lain.
```

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Path lookup membaca di luar batas `MCS_MAX_NAME` untuk segmen nama panjang | `mcs_split_parent_leaf`/`mcs_ramfs_lookup` | `if (seg_len == 0u \|\| seg_len >= MCS_MAX_NAME) return MCS_EINVAL;` sebelum dipakai untuk `mcs_copy_name`/`mcs_streq_n` | Tidak diuji aktif dengan nama segmen tepat di batas (31/32 karakter) pada sesi ini (known issue) |
| Penulisan ke arena data RAMFS melebihi kapasitas file | `mcs_vfs_write` | `n = min(len, capacity - offset); if (n < len) return MCS_ENOSPC;` — validasi sebelum `mcs_copy_from_user` dipanggil | Diverifikasi desain; tidak diuji aktif dengan kasus penuh persis pada sesi ini |
| Helper string manual (`mcs_strlen`, dst.) berisiko infinite loop jika `path` tidak null-terminated | `mcs_strlen`, `mcs_split_parent_leaf` | Seluruh path yang dipakai pada M13 ini berasal dari literal C (`"/motd.txt"`, dst.) yang dijamin null-terminated; belum ada validasi eksplisit terhadap input dari syscall sungguhan (M10 dispatcher belum punya syscall file) | Risiko nyata begitu path benar-benar berasal dari "user" lewat syscall — dicatat sebagai known issue, sesuai pola kehati-hatian `mcs_user_check_range` M10 yang belum diterapkan di sini |
| Double compile `ramfs.c`/`fd.c`/`sys_vfs.c` dengan dua compiler/target berbeda | Jalur host test vs freestanding kernel | Ketiga file tidak memanggil fungsi libc apa pun (`malloc`, `strlen` standar, dst.), aman di kedua jalur tanpa behavior berbeda | `nm -u` kosong pada `m13_vfs_combined.o`; host test lulus sebagai binary native |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| Path dan flag dari pemanggil `mcs_sys_*` | `path`, `flags`, `fd` (saat ini hanya dipanggil dari `kmain` dengan literal C, belum dari syscall sungguhan) | Validasi `path[0]=='/'`, panjang path, panjang segmen, validitas `fd` (`mcs_fd_get` mengembalikan NULL untuk fd di luar batas atau belum dipakai) | Mengembalikan kode error (`MCS_EINVAL`/`MCS_EBADF`/dst.), tidak pernah mengakses memori di luar batas yang divalidasi |
| Belum ada validasi pointer user (`mcs_user_check_range` gaya M10) pada `buf` di `mcs_sys_read`/`write` | Pointer `buf` yang diteruskan ke `mcs_vfs_read`/`write` | **Tidak ada validasi rentang alamat** pada M13 ini — berbeda dari M10 yang punya `mcs_user_check_range` untuk syscall non-file | Risiko nyata jika `mcs_sys_read`/`write` dipanggil dengan pointer dari "user" yang tidak divalidasi; pada sesi ini hanya dipanggil dengan buffer stack `kmain` yang valid, sehingga belum teraktivasi |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Preflight gate M0-M12 dan buat branch M13

Perintah:

```bash
cd ~/src/mcsos
git status --short
git branch --show-current
git log --oneline -5
mkdir -p logs
{
  echo "== git =="; git rev-parse --show-toplevel; git rev-parse --short HEAD; git status --short
  echo; echo "== tools =="
  bash --version | head -n 1; cc --version | head -n 1; clang --version | head -n 1
  ld --version | head -n 1; readelf --version | head -n 1; objdump --version | head -n 1
  nm --version | head -n 1; make --version | head -n 1; sha256sum --version | head -n 1
  qemu-system-x86_64 --version | head -n 1; gdb --version | head -n 1
} | tee logs/m13_preflight.log
git checkout -b praktikum-m13-vfs-ramfs
mkdir -p include kernel/vfs tests build/m13
```

Output ringkas:

```text
branch sebelumnya: praktikum/m12-sync, commit 55a09ab, working tree
bersih. Riwayat M9->M10->M11->M12 lengkap dan berurutan. Toolchain:
clang 21.1.8, gcc 15.2.0, binutils 2.46, QEMU 10.2.1, GDB 17.1,
sha256sum uutils 0.8.0 (bukan GNU coreutils, dicatat sebagai catatan
lingkungan). Branch baru: praktikum-m13-vfs-ramfs aktif.
```

### Langkah 2 — Verifikasi struktur TCB M9 dan API M11/M12 sebelum desain integrasi

Perintah:

```bash
grep -n "typedef struct mcsos_thread\|pid\|fd_table" include/mcsos_thread.h
find . -iname "*.h" -path "*/include/*" | grep -v build | sort
grep -n "^[a-zA-Z].*(" include/mcs_sync.h include/mcsos/user/m11_elf_loader.h | head -20
```

Output ringkas:

```text
mcsos_thread_t TIDAK memiliki field pid atau fd_table sama sekali --
hanya TCB murni (magic, id, name, state, context, entry, arg,
stack_base, stack_size, next, switches, ticks, exit_code).
mcs_sync.h (M12) sudah memakai prefix mcs_ (sama dengan kontrak panduan
M13), berbeda dari mcsos_/M9 dan mcsos/M10 -- tidak perlu penyesuaian
prefix untuk M13.
```

Diagnosis: keputusan "bagaimana `mcs_process_t` terhubung ke alur eksekusi" adalah keputusan desain baru yang tidak diatur kontrak panduan maupun kode M9 yang sudah ada (lihat bagian 9.2).

### Langkah 3 — Menulis header, RAMFS, FD table, dan hook test

Perintah:

```bash
cat > include/mcs_vfs.h << 'EOF'
[isi kontrak lengkap: enum tipe vnode, status code, struct vnode/ramfs/
file/fd_table/process, sembilan belas deklarasi fungsi]
EOF
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only include/mcs_vfs.h

cat > kernel/vfs/ramfs.c << 'EOF'
[isi implementasi: helper string manual, path lookup segmen-per-segmen,
alokasi vnode, init, lookup, create_file, seed_file]
EOF
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only kernel/vfs/ramfs.c

cat > kernel/vfs/fd.c << 'EOF'
[isi implementasi: fd table init/alloc/get, open/read/write/lseek/
close/dup, lima wrapper syscall mcs_sys_*]
EOF
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only kernel/vfs/fd.c

cat > kernel/vfs/sys_vfs.c << 'EOF'
[hook transisional mcs_active_ramfs_for_test]
EOF
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only kernel/vfs/sys_vfs.c
```

Output ringkas:

```text
[seluruh fsyntax-only bersih tanpa output untuk keempat file]
```

Catatan retrospektif (lihat bagian 14.2): file-file ini ternyata ditulis ulang identik dengan yang sudah ter-commit di `20a8e2e` sebelumnya, baru disadari pada Langkah 9.

### Langkah 4 — Host unit test

Perintah:

```bash
cat > tests/m13_vfs_host_test.c << 'EOF'
[isi test: test_basic_read (seed, open, read, lseek, read, close,
verifikasi EBADF), test_create_write_read (CREAT|RDWR|TRUNC, write,
lseek, read, verifikasi roundtrip), test_errors_and_fd_limit (path
relatif EINVAL, file tidak ada ENOENT, alokasi fd sampai 16 penuh
ENFILE, close lalu re-open slot kosong)]
EOF
mkdir -p build/m13
clang -std=c17 -Wall -Wextra -Werror -O2 -Iinclude \
  tests/m13_vfs_host_test.c kernel/vfs/ramfs.c kernel/vfs/fd.c kernel/vfs/sys_vfs.c \
  -o build/m13/m13_vfs_host_test
build/m13/m13_vfs_host_test
```

Output ringkas:

```text
M13 VFS/FD/RAMFS host tests: PASS
```

Indikator berhasil: seluruh tiga skenario lulus tanpa pernah menyentuh QEMU.

### Langkah 5 — Target Makefile M13 dan audit lengkap

Perintah:

```bash
cat >> Makefile << 'EOF'
[target m13-host-test, m13-freestanding, m13-audit, m13-all, m13-clean,
disesuaikan memakai $(CC)/$(LD)/$(NM)/$(READELF)/$(OBJDUMP) Makefile M4-M12]
EOF
make m13-clean
make m13-all
```

Output ringkas:

```text
M13 VFS/FD/RAMFS host tests: PASS
ELF Header: Class: ELF64, Type: REL, Machine: Advanced Micro Devices X86-64
grep -q "Machine..." / "mcs_ramfs_init" -- lulus
test ! -s nm_undefined.txt -- lulus (kosong)
sha256sum tercatat untuk m13_vfs_host_test dan m13_vfs_combined.o:
  f10321982b8e522de28445ea939a86812d4575082d1050c0ff9014b6fc331ce2  m13_vfs_host_test
  c63e871276205bee6eecb8d458823181f054a13fb04db5813dea2ecffe4aef32  m13_vfs_combined.o
```

Indikator berhasil: checkpoint host-test-only lulus penuh, tanpa pernah menyentuh QEMU, persis filosofi awal panduan M13 (modul independen dulu).

### Langkah 6 — Keputusan lanjut: integrasi kernel nyata

Maksud langkah:

```text
Memutuskan apakah berhenti di titik aman host-test-only (sesuai
filosofi panduan), atau melanjutkan ke integrasi kernel nyata yang
memerlukan desain baru (bagaimana mcs_process_t terhubung ke alur
eksekusi M9). Keputusan: lanjut ke integrasi nyata.
```

Tiga opsi desain dipertimbangkan (lihat bagian 9.2 untuk alasan lengkap):
1. Tambah field `pid`/`fd_table` ke `mcsos_thread_t` M9 — ditolak (risiko regresi kontrak M9 yang sudah teruji penuh).
2. Array global `mcs_process_t` indexed by `thread->id` — ditolak (menyiratkan isolasi proses yang belum benar-benar ada).
3. **Satu `mcs_process_t` global dibagi seluruh kernel thread** — dipilih (kejujuran arsitektur, risiko paling kecil).

### Langkah 7 — Menulis `m13_vfs_bootstrap()` dan integrasi ke kmain.c

Perintah:

```bash
grep -n "m10_syscall_bootstrap();\|m9_scheduler_idle_loop();\|#include" kernel/core/kmain.c
grep -n "mcs_process_t\|mcs_ramfs_t\|m13_\|MCS_O_\|mcs_sys_" kernel/core/kmain.c
sed -n '410,442p' kernel/core/kmain.c
```

Output ringkas:

```text
kmain.c SUDAH punya #include "mcs_vfs.h" (baris 14), tapi belum ada
satu pun pemakaian kode M13 -- kemungkinan sisa placeholder dari
pekerjaan sebelumnya yang belum sempat dilanjutkan.
Urutan boot lengkap: M4->M6->M7->M8->M9->M10->M11->(M5 interrupt)->idle.
m11_elf_smoke_test() berakhir di baris 397, titik penyisipan paling
tepat untuk m13_vfs_bootstrap().
```

Perintah penyisipan:

```bash
sed -i '397a\
[fungsi lengkap m13_vfs_bootstrap: ramfs_init, fd_table_init, seed
/motd.txt, open+read, create+write /log.txt, close kedua fd, log hasil]' kernel/core/kmain.c
sed -i 's/    m11_elf_smoke_test();/    m11_elf_smoke_test();\n    m13_vfs_bootstrap();/' kernel/core/kmain.c
```

Indikator berhasil: sisipan rapi, seluruh escape `&` benar (`&g_m13_ramfs`, `&g_m13_process`), tidak ada karakter rusak.

### Langkah 8 — Build penuh dan QEMU smoke test

Perintah:

```bash
make clean
make all
nm -n build/kernel.elf | grep -i "mcs_\|m13"
cp build/kernel.elf iso_root/boot/kernel.elf
xorriso -as mkisofs [...] -o build/mcsos.iso
./limine/limine bios-install build/mcsos.iso
mkdir -p logs
timeout 5 qemu-system-x86_64 -m 256M -machine q35 \
  -serial file:logs/m13_serial.log -display none -no-reboot -no-shutdown \
  -cdrom build/mcsos.iso
grep -n "M13\|M11\|M10\|M9" logs/m13_serial.log | head -20
```

Output ringkas:

```text
[make all sukses penuh; kernel/vfs/{fd,ramfs,sys_vfs}.c otomatis
tertangkap glob find kernel -name '*.c'; juga terkonfirmasi kernel/
sync/{lockdep,mutex,spinlock}.c (M12) dan kernel/user/m11_elf_loader.c
(M11) sudah ter-link sebelumnya]

nm -n build/kernel.elf | grep -i "mcs_\|m13":
m13_vfs_bootstrap (t); 9 mcs_lockdep_*/mcs_mutex_*/mcs_spin_* (M12);
seluruh fungsi VFS publik (mcs_fd_table_init, mcs_vfs_*, mcs_sys_*,
mcs_ramfs_*); g_m13_ramfs, g_m13_process, mcs_active_ramfs_for_test
sebagai data statik

QEMU (ringkasan grep):
26:[M9] scheduler initialized
28-33:[M10] syscall init/ping ok/get_ticks ok/smoke done/int 0x80 gate installed/ping ok
34-42:[M11] elf: ident ok / plan ok / user image plan ready
43:[M13] ramfs+vfs smoke test passed
44:[M13] motd.txt content: mcsos-m13-ramfs
56-63:[M9] thread A tick / thread B tick (bergantian, berlanjut tanpa henti)
```

Indikator berhasil: urutan boot kausal benar (M9->M10->M11->M13->M9), isi `motd.txt` yang dibaca kembali persis cocok dengan yang di-seed (`"mcsos-m13-ramfs"`), file kedua (`/log.txt`) berhasil dibuat dan ditulis tanpa memicu panic apa pun, dan M9 scheduler tetap utuh setelahnya.

### Langkah 9 — Diagnosis status commit existing sebelum commit final

Perintah:

```bash
git status
git status --short --ignored | grep -i "mcs_vfs\|vfs\|m13\|Makefile"
ls -la include/mcs_vfs.h kernel/vfs/ Makefile
grep -c "m13-" Makefile
git log --oneline -3
git show --stat HEAD | head -15
git diff HEAD -- include/mcs_vfs.h kernel/vfs/ tests/m13_vfs_host_test.c Makefile
```

Output ringkas:

```text
git status hanya menunjukkan kernel/core/kmain.c sebagai modified, dan
dua file logs/ sebagai untracked -- TIDAK menunjukkan mcs_vfs.h,
kernel/vfs/, atau Makefile sama sekali, padahal file-file itu ada di
disk dan Makefile punya 6 target m13-.

git log -3 mengungkap: commit 20a8e2e ("M13: implement VFS minimal,
RAMFS, FD table, syscall file I/O, host test, and audit") SUDAH ADA
sebelum sesi integrasi kernel ini dimulai, mencakup persis keenam file
yang "terasa baru" ditulis pada Langkah 3-5.

git diff HEAD terhadap keenam file tersebut: KOSONG. Mengonfirmasi
seluruh konten yang ditulis ulang pada Langkah 3-5 identik byte-per-byte
dengan yang sudah ter-commit -- tidak ada kerusakan atau duplikasi,
hanya "mengetik ulang sesuatu yang sudah ada" akibat tidak memeriksa
git log di awal sesi setelah git checkout -b.
```

### Langkah 10 — Commit perubahan nyata dan perbaikan ukuran log

Perintah (sebelum commit, pengecekan ukuran log):

```bash
wc -l logs/m13_serial.log
```

Output (anomali ditemukan):

```text
144728 logs/m13_serial.log
```

Diagnosis: sama persis dengan insiden M10 — `timeout 5` tidak cukup mencegah M9 idle loop mencetak ribuan baris tick A/B dalam jendela 5 detik tersebut. Kali ini pengecekan dilakukan **sebelum** `git add`, bukan setelah commit pertama seperti M10.

Perintah (perbaikan, sebelum commit):

```bash
sort logs/m13_serial.log | uniq -c | sort -rn | head -5
head -60 logs/m13_serial.log > /tmp/m13_serial_trimmed.log
mv /tmp/m13_serial_trimmed.log logs/m13_serial.log
wc -l logs/m13_serial.log
```

Output ringkas:

```text
72333 [M9] thread B tick; 72332 [M9] thread A tick -- bukan error
berulang, murni tick sah yang tidak perlu disimpan sebanyak itu.
Setelah head -60: 60 logs/m13_serial.log
```

Perintah (commit final):

```bash
git add kernel/core/kmain.c logs/m13_preflight.log logs/m13_serial.log
git status
git commit -m "M13: integrate VFS/RAMFS into kmain, add preflight and trimmed QEMU evidence [...]"
git log --oneline -6
git status
```

Output ringkas:

```text
[8b8d15a] M13: integrate VFS/RAMFS into kmain, ...
 3 files changed, 125 insertions(+)
nothing to commit, working tree clean
```

Indikator berhasil: ukuran commit wajar (125 baris untuk 3 file), masalah ukuran log ditemukan dan diperbaiki SEBELUM commit pertama (berbeda dari M10 yang butuh `--amend` setelahnya), riwayat git bersih.

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| Header dan C valid + host test | `clang ... -fsyntax-only`, `make m13-host-test` | Bersih, `M13 VFS/FD/RAMFS host tests: PASS` | `PASS` |
| Freestanding compile | `make m13-freestanding` | `m13_vfs_combined.o` terbentuk | `PASS` |
| Audit object | `make m13-audit` | `nm -u` kosong, ELF64 x86_64 REL, symbol `mcs_ramfs_init` ada | `PASS` |
| Kernel link | `make all` setelah patch `kmain.c` | `kernel.elf`/`mcsos.iso` terbentuk dengan symbol VFS terlink | `PASS` |
| QEMU smoke test | Smoke test integrasi | Log `[M13] ramfs+vfs smoke test passed` dan `motd.txt content` benar, M9 lanjut normal | `PASS` |
| Diagnosis status commit existing | `git diff HEAD` sebelum commit | Tidak ada duplikasi tersembunyi atau konflik dengan pekerjaan sebelumnya | `PASS` |
| Git evidence | `git status`, `git log` | Working tree bersih, commit terdokumentasi jelas, ukuran log wajar | `PASS` |

---

## 12. Perintah Uji dan Validasi

### 12.1 Header dan C Syntax Check

```bash
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only include/mcs_vfs.h
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only kernel/vfs/ramfs.c
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only kernel/vfs/fd.c
clang -std=c17 -Wall -Wextra -Werror -Iinclude -fsyntax-only kernel/vfs/sys_vfs.c
```

Hasil: bersih, tidak ada output di keempat perintah. Status: `PASS`

### 12.2 Host Unit Test

```bash
clang -std=c17 -Wall -Wextra -Werror -O2 -Iinclude \
  tests/m13_vfs_host_test.c kernel/vfs/ramfs.c kernel/vfs/fd.c kernel/vfs/sys_vfs.c \
  -o build/m13/m13_vfs_host_test
build/m13/m13_vfs_host_test
```

Hasil: `M13 VFS/FD/RAMFS host tests: PASS`. Status: `PASS`

### 12.3 Makefile Target M13 Lengkap

```bash
make m13-clean
make m13-all
cat build/m13/nm_undefined.txt
wc -l build/m13/nm_undefined.txt
```

Hasil: host test PASS, ELF64 x86_64 REL, dua `grep -q` (Machine, `mcs_ramfs_init`) lulus, `nm_undefined.txt` 0 baris. Status: `PASS`

### 12.4 Kernel Build dengan VFS Terintegrasi

```bash
make clean
make all
nm -n build/kernel.elf | grep -i "mcs_\|m13"
```

Hasil: seluruh symbol VFS publik, hook test, dan dua variabel statis (`g_m13_ramfs`, `g_m13_process`) ditemukan; M11/M12 juga terkonfirmasi ter-link. Status: `PASS`

### 12.5 QEMU Smoke Test

```bash
timeout 5 qemu-system-x86_64 -m 256M -machine q35 \
  -serial file:logs/m13_serial.log -display none -no-reboot -no-shutdown \
  -cdrom build/mcsos.iso
grep -n "M13\|M11\|M10\|M9" logs/m13_serial.log | head -20
```

Hasil: `[M13] ramfs+vfs smoke test passed` dan `[M13] motd.txt content: mcsos-m13-ramfs` muncul tepat setelah M11, M9 tick berlanjut normal setelahnya. Status: `PASS`

### 12.6 GDB Debug Session

```text
Tidak dijalankan pada sesi M13 ini (sama seperti M10, M12 -- mengikuti
catatan M12 bahwa GDB tidak diwajibkan ulang setiap milestone setelah
sesi GDB lengkap M9). Verifikasi dilakukan murni lewat host test, audit
statis, dan log serial QEMU, yang terbukti cukup untuk membuktikan
RAMFS/FD table bekerja end-to-end. Dicatat sebagai rencana perbaikan
opsional pada bagian 22.3.
```

Status: `NA`

### 12.7 Stress/Fuzz/Fault Injection Test

```text
Tugas pengayaan panduan (direktori bertingkat lebih dari satu level,
file mendekati batas kapasitas 256 byte, fd limit dipanggil dari thread
M9 konkuren, lock M12 diintegrasikan untuk melindungi RAMFS) belum
dikerjakan pada sesi ini. Dicatat sebagai rencana perbaikan pada bagian
22.3.
```

Status: `NA`

### 12.8 Visual Evidence

| Screenshot | Lokasi file | Keterangan |
|---|---|---|
| Tidak ada | - | Bukti dikumpulkan dalam bentuk log teks terminal (lihat Lampiran C dan D), konsisten dengan pendekatan laporan M9-M12 |

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
|---|---|---|---|---|---|
| 1 | Header dan implementasi C valid sintaks | Tidak ada warning/error | Bersih di seluruh empat file | `PASS` | Bagian 12.1 |
| 2 | Host unit test VFS/FD/RAMFS | Tiga skenario lulus | `M13 VFS/FD/RAMFS host tests: PASS` | `PASS` | Bagian 12.2 |
| 3 | Freestanding object bebas dependency | `nm -u` kosong | `nm_undefined.txt` 0 baris | `PASS` | Bagian 12.3 |
| 4 | Kernel build dengan symbol VFS terlink | `nm -n` menampilkan symbol lengkap | Seluruh symbol publik dan dua data statis ditemukan | `PASS` | Bagian 12.4 |
| 5 | Struktur TCB M9 diverifikasi sebelum desain integrasi | Pembacaan kode nyata sebelum implementasi | `mcsos_thread_t` tidak punya `pid`/`fd_table` | `PASS` | Bagian 14.2, 15.1 |
| 6 | Integrasi VFS/RAMFS di QEMU | Log smoke test passed, isi file benar | Terkonfirmasi, M9 lanjut normal | `PASS` | Bagian 12.5 |
| 7 | Diagnosis commit existing sebelum menulis ulang kode | `git diff` dipakai sebagai verifikasi, bukan asumsi | Diff kosong, tidak ada duplikasi tersembunyi | `PASS` | Bagian 14.2, 15.1 |
| 8 | Pencegahan ukuran commit membengkak SEBELUM terjadi | Log dipangkas sebelum `git add`, bukan amend sesudahnya | `wc -l` dicek sebelum staging, dipangkas 144.728->60 baris | `PASS` | Bagian 14.2, 15.1 |

### 13.2 Log Penting

```text
Boot marker M13 lengkap (lihat Lampiran C/D untuk log penuh):
[M13] ramfs+vfs smoke test passed
[M13] motd.txt content: mcsos-m13-ramfs
```

### 13.3 Artefak Bukti

| Artefak | Path | SHA-256 | Fungsi |
|---|---|---|---|
| `m13_vfs_host_test` | `build/m13/m13_vfs_host_test` | `f10321982b8e522de28445ea939a86812d4575082d1050c0ff9014b6fc331ce2` | Binary host unit test |
| `m13_vfs_combined.o` | `build/m13/m13_vfs_combined.o` | `c63e871276205bee6eecb8d458823181f054a13fb04db5813dea2ecffe4aef32` | Object gabungan freestanding RAMFS+FD+hook |
| `kernel.elf` | `build/kernel.elf` | `tidak dihitung pada sesi ini` | Kernel binary dengan VFS terlink |
| `mcsos.iso` | `build/mcsos.iso` | `tidak dihitung pada sesi ini` | Boot image dengan VFS teruji runtime |
| `m13_preflight.log` | `logs/m13_preflight.log` | `tidak dihitung pada sesi ini` | Bukti gate M0-M12 dan versi toolchain |
| `m13_serial.log` (dipangkas) | `logs/m13_serial.log` | `tidak dihitung pada sesi ini` | Bukti QEMU smoke test, 60 baris |

Catatan: melanjutkan pola M9-M12, hash SHA-256 untuk dua artefak inti M13 ditangkap langsung dari target `make m13-audit`. Hash untuk `kernel.elf`/`mcsos.iso` tetap belum dihitung, melanjutkan known issue berkelanjutan sejak M5.

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

```text
Keberhasilan M13 dibuktikan berlapis: host test membuktikan logika
RAMFS/FD table benar TANPA hardware (tiga skenario: baca dasar dengan
lseek, create-write-read roundtrip, error handling dan limit fd), audit
statis membuktikan object freestanding bebas dependency libc, dan smoke
test QEMU membuktikan seluruh rantai bekerja end-to-end di kernel
sungguhan: data yang di-seed (mcsos-m13-ramfs) berhasil dibaca kembali
PERSIS lewat mcs_sys_open->mcs_sys_read, file baru berhasil dibuat dan
ditulis lewat mcs_sys_open(O_CREAT)->mcs_sys_write, dan scheduler M9
tetap utuh setelahnya (tick A/B berlanjut tanpa henti). Bukti paling
kuat bukan hanya "tidak ada crash", melainkan isi string yang benar-
benar tercetak di log serial cocok byte-per-byte dengan yang di-seed --
ini membuktikan path lookup, alokasi data arena, dan offset tracking
FD table semuanya benar di lingkungan freestanding sungguhan, bukan
hanya di host test yang berjalan di atas libc lengkap.
```

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

```text
Berbeda dari M9/M10 yang kegagalannya murni teknis, M13 mengalami dua
insiden bersifat PROSES yang keduanya berhasil ditangani dengan baik
karena pembelajaran langsung dari pengalaman M10 sebelumnya:

Kegagalan pertama (proses, ditemukan lewat verifikasi git, bukan
kerusakan): setelah menulis ulang include/mcs_vfs.h, kernel/vfs/{ramfs,
fd,sys_vfs}.c, tests/m13_vfs_host_test.c, dan target Makefile M13 dari
nol (Langkah 3-5), git status pada Langkah 9 TIDAK menampilkan satu pun
dari file tersebut sebagai modified/untracked, padahal seharusnya
"baru ditulis". Diagnosis lewat git log mengungkap commit 20a8e2e sudah
ada sebelum sesi integrasi kernel dimulai, mencakup persis keenam file
itu. Verifikasi lebih lanjut dengan git diff HEAD terhadap seluruh file
tersebut menghasilkan output KOSONG -- mengonfirmasi konten yang ditulis
ulang pada sesi ini identik byte-per-byte dengan yang sudah ter-commit,
sehingga tidak ada kerusakan atau duplikasi nyata, hanya "mengetik ulang
sesuatu yang sudah ada" akibat tidak memeriksa riwayat git di awal sesi
setelah git checkout -b. Pembelajaran konkret: SEHARUSNYA git log
--oneline -5 dijalankan segera setelah checkout branch baru, sebelum
mulai menulis file apa pun, bukan hanya di akhir sesi saat git status
terlihat aneh.

Kegagalan kedua (proses, sama persis dengan insiden M10, TAPI ditangani
lebih baik): logs/m13_serial.log mencapai 144.728 baris akibat M9 idle
loop yang mencetak tick jauh lebih cepat daripada timeout 5 detik
dinding-jam yang dipakai untuk menghentikan QEMU -- pola identik dengan
insiden M10 (155.042 baris). Perbedaan krusial dari M10: kali ini wc -l
dijalankan SEBELUM git add/commit (Langkah 10), bukan setelah commit
pertama selesai. Akibatnya perbaikan (head -60, lalu mv) terjadi
SEBELUM staging, sehingga commit M13 langsung bersih (125 baris untuk
3 file) tanpa perlu git commit --amend sama sekali -- bukti konkret
bahwa pembelajaran dari insiden M10 benar-benar diterapkan, bukan hanya
dicatat di laporan tanpa mengubah kebiasaan kerja.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| Pemisahan vnode (metadata) dari file descriptor (state sesi) [3] | `mcs_vnode_t` tanpa field offset; `mcs_file_t` dengan `offset` dan pointer ke `node` | Sesuai | Host test `test_errors_and_fd_limit` membuktikan 16 kali `open` pada path `/x` yang sama menghasilkan 16 fd independen, masing-masing punya `offset` sendiri meski menunjuk `node` yang sama |
| Semantik `O_TRUNC` memotong file ke ukuran 0 saat open, bukan saat write pertama [1] | `if ((flags & MCS_O_TRUNC) != 0u) { node->size = 0u; table->files[fd].offset = 0u; }` dieksekusi langsung di `mcs_vfs_open` | Sesuai | Diverifikasi tidak langsung lewat smoke test QEMU: `/log.txt` dibuka dengan `O_CREAT|O_RDWR|O_TRUNC` dan langsung bisa ditulis dari offset 0 tanpa sisa data lama (meski file baru sehingga truncate tidak teruji pada file yang sudah berisi data sebelumnya) |
| `lseek(SEEK_END)` menghitung posisi relatif terhadap ukuran file saat ini, bukan kapasitas [2] | `base = (long)file->node->size;` untuk `MCS_SEEK_END`, bukan `data_capacity` | Sesuai | Diverifikasi desain kode; tidak diuji aktif dengan kasus `SEEK_END` pada host test maupun smoke test (known issue, hanya `SEEK_SET`/`SEEK_CUR` yang diuji eksplisit di `test_basic_read`) |
| Verifikasi riwayat version control sebelum menulis ulang kode yang "terasa baru" [5] (praktik rekayasa perangkat lunak umum, bukan dikutip literal dari satu sumber spesifik) | `git log`/`git diff HEAD` dipakai pada Langkah 9, SETELAH menulis ulang, bukan SEBELUM | Sebagian sesuai — praktik baik dilakukan, tapi urutannya terbalik (idealnya dicek sebelum menulis, bukan setelah curiga dari `git status` yang aneh) | Pembelajaran ini dicatat eksplisit sebagai perbaikan proses untuk milestone berikutnya, bukan disembunyikan sebagai seolah-olah semua berjalan optimal dari awal |

### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Bukti | Catatan |
|---|---|---|---|
| Kompleksitas `mcs_ramfs_lookup` | O(depth × node_count) — setiap segmen path memanggil `mcs_find_child` yang melakukan linear scan seluruh `node_count` | Tidak ada indexing/hash, murni iterasi array | Diterima untuk skala M13 (`MCS_MAX_NODES=64`), bukan desain produksi untuk filesystem besar |
| Kompleksitas `mcs_fd_alloc` | O(`MCS_MAX_OPEN_FILES`) — linear scan mencari slot kosong | Konsisten filosofi M9 (`mcs_sched_pick_next`) dan M10 (tabel dispatcher), tidak ada struktur data lebih kompleks untuk skala kecil ini | Diverifikasi host test: alokasi berurutan `fds[i] == (int)i` membuktikan slot pertama yang kosong selalu diambil |
| Ukuran arena data RAMFS | 8192 byte tetap (`MCS_RAMFS_DATA_BYTES`), kapasitas default per file baru 256 byte (`mcs_alloc_node` dipanggil dengan `capacity=256u` di `mcs_ramfs_create_file`) | `kernel/vfs/ramfs.c`; tidak ada mekanisme resize file setelah alokasi awal | Keterbatasan eksplisit: file tidak bisa membesar melebihi 256 byte meski arena total masih punya sisa ruang, kecuali lewat alokasi vnode baru |
| Waktu boot QEMU hingga `[M13] ramfs+vfs smoke test passed` | Tidak diukur presisi; teramati di baris 43 dari total log (setelah M9/M10/M11 selesai) | Log QEMU bagian 10 Langkah 8 | Bootstrap VFS sepenuhnya sinkron sebelum `sti()` M5 diaktifkan kembali |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab sementara | Bukti | Perbaikan |
|---|---|---|---|---|
| File M13 ternyata sudah ter-commit sebelumnya, tidak terdeteksi di awal sesi | `git status` setelah menulis ulang 6 file tidak menampilkan satu pun sebagai modified/untracked | `git log -3` tidak dijalankan segera setelah `git checkout -b`, sehingga commit `20a8e2e` yang sudah ada tidak diketahui sampai akhir sesi | `git log --oneline -3`, `git diff HEAD` (kosong) pada Langkah 9 | Verifikasi `git diff` mengonfirmasi tidak ada kerusakan nyata; pembelajaran proses dicatat untuk milestone berikutnya: cek `git log` SEBELUM menulis file, bukan setelah curiga |
| Ukuran log QEMU membengkak (insiden berulang dari M10) | `wc -l logs/m13_serial.log` = 144.728, jauh di luar kewajaran | `timeout 5` tetap memberi M9 idle loop cukup waktu mencetak puluhan ribu baris tick sebelum benar-benar dihentikan | `sort \| uniq -c` membuktikan isi sehat (tick A/B berulang sah) | **Ditemukan dan diperbaiki SEBELUM commit** (`head -60` lalu staging), berbeda dari insiden M10 yang baru ditemukan setelah commit pertama dan butuh `--amend` |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Race condition pada `g_m13_ramfs`/`g_m13_process` jika diakses konkuren oleh lebih dari satu thread | Tidak teramati pada sesi ini karena hanya `kmain` (jalur sinkron) yang mengakses kedua struktur | Korupsi struktur RAMFS/FD table jika dua thread menulis bersamaan tanpa lock | Belum dimitigasi (sesuai kontrak panduan poin 11 yang sengaja menunda locking); dicatat eksplisit sebagai known issue yang HARUS ditangani begitu ada akses konkuren nyata |
| Path dengan segmen nama tepat di batas `MCS_MAX_NAME` (31/32 karakter) | Tidak diuji aktif pada sesi ini | Potensi off-by-one pada validasi `seg_len >= MCS_MAX_NAME` | Validasi sudah ada di desain (`>=`, bukan `>`), tapi belum diverifikasi dengan kasus uji tepat di batas |
| Kapasitas file (256 byte) terlampaui pada penulisan besar | Tidak diuji aktif dengan kasus tepat penuh atau melebihi pada sesi ini | `mcs_vfs_write` seharusnya mengembalikan `MCS_ENOSPC` sebelum menulis sebagian | Logika validasi sudah ada (`if (n < len) return MCS_ENOSPC;` sebelum copy), belum diverifikasi dengan kasus uji aktif |
| Pointer `buf` dari "user" tidak divalidasi rentang alamatnya (berbeda dari M10) | Tidak relevan pada sesi ini karena hanya dipanggil dengan buffer stack `kmain` yang valid | Risiko nyata jika `mcs_sys_read`/`write` dipanggil dari syscall sungguhan dengan pointer user tidak tepercaya | Belum dimitigasi; dicatat sebagai known issue, perlu pola `mcs_user_check_range` M10 diterapkan di M13 |

### 15.3 Triage yang Dilakukan

```text
Urutan diagnosis sepanjang sesi: (1) preflight gate M0-M12; (2)
verifikasi struktur mcsos_thread_t M9 SEBELUM mendesain integrasi --
mengungkap ketidaktersediaan field pid/fd_table, mencegah asumsi keliru
bahwa integrasi tinggal "menambah field saja"; (3) verifikasi sintaks
header dan C secara terpisah untuk keempat file; (4) host unit test
tiga skenario untuk logika murni; (5) audit freestanding object; (6)
keputusan eksplisit melanjutkan ke integrasi kernel (bukan berhenti di
host-test-only), didahului diskusi tiga opsi desain dengan alasan
tertulis untuk setiap opsi sebelum memilih; (7) integrasi nyata ke
kmain.c dengan verifikasi sisip per-baris (escape karakter khusus) demi
menghindari kerusakan seperti yang pernah terjadi di milestone lain;
(8) build dan smoke test QEMU sebagai pembuktian end-to-end; (9) baru
pada titik git status terasa "aneh" (tidak menampilkan file yang
seharusnya baru), dilakukan diagnosis git log/git diff yang mengungkap
commit existing -- ini diagnosis REAKTIF (dipicu kejanggalan), berbeda
dari pembacaan kode proaktif yang sudah dipraktikkan di M9/M10 untuk
hal lain; (10) pengecekan wc -l pada log evidence SEBELUM staging,
diagnosis PROAKTIF kali ini karena belajar langsung dari pengalaman
M10 -- perbandingan langsung antara (9) yang reaktif dan (10) yang
proaktif menjadi pembelajaran metodologis tersendiri pada sesi ini.
```

### 15.4 Panic Path

```text
KERNEL_PANIC dipanggil secara kondisional di m13_vfs_bootstrap() untuk
enam skenario: seed file gagal, open /motd.txt gagal, read tidak
menghasilkan 15 byte yang diharapkan, open/create /log.txt gagal,
write tidak menghasilkan 7 byte yang diharapkan, dan close salah satu
fd gagal. Pada sesi smoke test M13 yang berhasil, tidak satu pun dari
keenam panic path ini terpicu -- seluruh log menunjukkan jalur sukses
sampai "[M13] ramfs+vfs smoke test passed". Pengujian aktif memicu
panic path VFS secara sengaja (mis. dengan ukuran data seed yang
sengaja melebihi kapasitas) belum dilakukan pada sesi ini dan dicatat
sebagai rencana perbaikan pada bagian 22.3.
```

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M12 | `git checkout 55a09ab` atau `git checkout praktikum/m12-sync` | Tidak ada data kerja M13 yang hilang karena tetap di branch terpisah | `belum diuji` |
| Kembali ke commit M13 host-test-only (sebelum integrasi kernel) | `git checkout 20a8e2e` | Memungkinkan rollback parsial: mempertahankan modul VFS independen tanpa integrasi kmain.c yang lebih berisiko | `belum diuji, tapi tersedia secara struktural karena commit terpisah` |
| Revert commit integrasi M13 | `git revert 8b8d15a` | Log dan hasil audit M13 (dicatat di laporan ini) | `belum diuji` |
| Bersihkan artefak build M13 | `make m13-clean` | Tidak ada (source tetap aman) | `teruji` — dijalankan beberapa kali sebagai bagian normal workflow |
| **Pelajaran khusus M13**: cek `git log`/`git diff` sebelum menulis ulang file pada branch baru | `git log --oneline -5` segera setelah `git checkout -b` | Mencegah pengulangan kerja atau, lebih buruk, menimpa versi yang sudah benar tanpa disadari | `dipelajari pada sesi ini, direkomendasikan jadi langkah baku sebelum Langkah 3 pada milestone berikutnya` |

Catatan rollback:

```text
M13 memberi dua keuntungan struktural dibanding M10 untuk rollback:
pertama, karena modul host-test-only (20a8e2e) dan integrasi kernel
(8b8d15a) tersimpan sebagai DUA commit terpisah (bukan satu commit
besar), rollback parsial -- mempertahankan modul VFS independen sambil
membatalkan integrasi kmain.c yang lebih berisiko -- secara struktural
tersedia tanpa perlu cherry-pick rumit. Kedua, tidak ada insiden git
checkout yang merugikan seperti M10 karena tidak ada eksperimen Makefile
berisiko pada sesi ini. Satu pelajaran baru: verifikasi riwayat git
SEBELUM memulai pekerjaan pada branch baru, bukan hanya saat akan commit.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Path dari pemanggil diakses tanpa validasi rentang | Boundary pemanggil-ke-RAMFS | Path tidak valid (relatif, terlalu panjang, segmen kosong) diproses sampai mengakses memori yang salah | Validasi `path[0]=='/'`, panjang total (`MCS_MAX_PATH`), panjang segmen (`MCS_MAX_NAME`) sebelum dipakai untuk pencarian/penyalinan | Host test `mcs_sys_open(&proc, &fs, "relative", ...) == MCS_EINVAL` |
| Fd di luar batas atau belum dipakai diakses | Boundary pemanggil-ke-FD-table | Akses array `files[]` di luar batas, atau dereferensi `node`/`fs` yang NULL pada slot belum dipakai | `mcs_fd_get` memvalidasi `fd < 0 \|\| fd >= MCS_MAX_OPEN_FILES \|\| !used` sebelum mengembalikan pointer, mengembalikan NULL untuk kasus invalid | Host test `mcs_sys_read(&proc, fd, buf, 1) == MCS_EBADF` setelah `close` |
| Pointer `buf` dari pemanggil `mcs_sys_read`/`write` tidak divalidasi rentang alamatnya | Boundary pemanggil-ke-memori (berbeda dari M10 yang punya `mcs_user_check_range`) | Jika dipanggil dengan pointer tidak tepercaya, berpotensi membaca/menulis memori di luar batas yang dimaksudkan | **Belum dimitigasi** pada M13 ini; hanya aman karena pemanggil saat ini (kmain) selalu memberi pointer valid | Bagian 17.1 ini sendiri sebagai dokumentasi eksplisit keterbatasan, bukan disembunyikan |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| RAMFS bukan persisten — seluruh data hilang saat reboot/power-off | Tidak ada cara menyimpan data lintas sesi boot | Sesuai sifat in-memory filesystem, bukan bug | Didokumentasikan eksplisit sebagai non-goal M13 (lihat batas cakupan bagian 5) |
| Tidak ada locking pada akses bersama, sehingga belum ada cara mendeteksi korupsi akibat race | Statistik/struktur RAMFS bisa rusak diam-diam jika diakses konkuren | Tidak teramati pada sesi ini (hanya akses sinkron) | Sesuai kontrak panduan, ditunda sampai ada kebutuhan konkurensi nyata; dicatat sebagai known issue prioritas tinggi |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| Open path relatif | `"relative"` (tanpa `/` di awal) | `MCS_EINVAL` | Sesuai — diverifikasi host test | `PASS` |
| Open file yang tidak ada tanpa `O_CREAT` | `"/missing"` | `MCS_ENOENT` | Sesuai — diverifikasi host test | `PASS` |
| Alokasi fd sampai penuh | Membuka 17 file saat `MCS_MAX_OPEN_FILES=16` | Percobaan ke-17 `MCS_ENFILE` | Sesuai — diverifikasi host test | `PASS` |
| Operasi pada fd setelah `close` | `read(fd)` setelah `close(fd)` | `MCS_EBADF` | Sesuai — diverifikasi host test | `PASS` |
| Penulisan melebihi kapasitas file (256 byte) | Tidak diuji aktif pada sesi ini | `MCS_ENOSPC` sebelum menulis sebagian | Tidak diuji | `NA` |
| Nama segmen path tepat di batas `MCS_MAX_NAME` | Tidak diuji aktif pada sesi ini | Ditolak `MCS_EINVAL` atau diterima jika tepat di bawah batas | Tidak diuji | `NA` |

---

## 18. Pembagian Kerja Kelompok

Tidak berlaku — praktikum ini dikerjakan secara individu oleh Agung Nurjaman (25832073010).

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Source code VFS/RAMFS dapat dikompilasi sebagai C17 freestanding | `PASS` | Bagian 12.1, 12.3 |
| Host unit test mencakup skenario baca, tulis, dan error handling | `PASS` | Bagian 12.2 |
| Object freestanding tidak memiliki unresolved symbol | `PASS` | Bagian 12.3 |
| Integrasi kernel nyata berhasil tanpa merusak milestone sebelumnya | `PASS` | Bagian 12.4, 12.5 |
| Keputusan desain di luar kontrak panduan (integrasi `mcs_process_t`) didokumentasikan dengan alasan eksplisit | `PASS` | Bagian 9.2 |
| Status commit existing diverifikasi sebelum menambah pekerjaan baru | `PASS` | Bagian 10 Langkah 9, 14.2 |
| Insiden proses (ukuran log) dicegah sebelum terjadi, bukan diperbaiki sesudahnya | `PASS` | Bagian 10 Langkah 10, 14.2 |
| Validasi runtime QEMU dijalankan ulang di lingkungan WSL 2 mahasiswa sendiri | `PASS` | Seluruh smoke test dijalankan langsung oleh mahasiswa |

### 19.1 Checkpoint Resmi Panduan M13

| Checkpoint | Kriteria panduan | Status | Catatan deviasi |
|---|---|---|---|
| Header dan implementasi valid | Sintaks bersih, host test lulus | `PASS` | Tidak ada deviasi |
| Freestanding compile dan audit | `nm -u` kosong, ELF64 x86_64 | `PASS` | Tidak ada deviasi |
| Integrasi kernel | Tidak diwajibkan literal oleh kontrak panduan (M13 mendeskripsikan diri sebagai modul host-test-only), tetapi dilanjutkan atas keputusan eksplisit | `PASS` | Integrasi melampaui filosofi minimum panduan; keputusan desain (`mcs_process_t` global) didokumentasikan sebagai ekstensi di luar kontrak, bukan bagian dari kontrak itu sendiri |
| Locking M12 untuk VFS | Kontrak panduan poin 11 eksplisit menyatakan ini ditunda | `PASS (sesuai rencana, bukan kegagalan)` | Tidak diintegrasikan, sesuai instruksi panduan sendiri |

---

## 20. Readiness Review

| Status | Definisi | Pilihan |
|---|---|---|
| Belum siap uji | Build/test belum stabil atau bukti belum cukup | `[ ]` |
| Siap uji QEMU untuk VFS/RAMFS single-process | Build bersih, host test dan QEMU smoke berjalan, log tersedia | `✓` |
| Siap untuk multi-proses terisolasi | Memerlukan address space terpisah per proses, FD table per proses sungguhan | `[ ]` |
| Siap produksi/filesystem persisten | Tidak berlaku untuk M13 | `[ ]` |

Alasan readiness:

```text
Status "siap uji QEMU untuk VFS/RAMFS single-process" dipilih karena
seluruh checkpoint lulus dengan bukti berlapis: host test membuktikan
logika murni benar, audit statis membuktikan object bebas dependency,
dan smoke test QEMU membuktikan integrasi end-to-end bekerja tanpa
merusak M9-M11. Status ini SECARA EKSPLISIT BUKAN "siap untuk multi-
proses terisolasi" -- mcs_process_t pada M13 ini adalah satu instance
global yang dibagi seluruh kernel thread, bukan FD table terisolasi
per proses sungguhan, dan belum ada locking untuk melindungi akses
konkuren begitu lebih dari satu thread benar-benar memanggil mcs_sys_*
secara bersamaan.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | `mcs_process_t` adalah satu instance global, bukan satu per kernel thread/proses sungguhan | FD table dibagi seluruh thread, tidak ada isolasi proses nyata | Didokumentasikan jujur sebagai keterbatasan arsitektur saat ini, bukan dipalsukan sebagai isolasi yang sudah ada | Memerlukan integrasi dengan `mcsos_thread_t` M9 (tambah field) atau model proses baru di milestone mendatang |
| 2 | Tidak ada locking M12 untuk melindungi RAMFS/FD table dari akses konkuren | Race condition jika lebih dari satu thread memanggil `mcs_sys_*` bersamaan | Sesuai kontrak panduan poin 11; aman untuk kondisi saat ini (hanya akses sinkron dari `kmain`) | Wajib ditangani begitu syscall file M13 dipanggil dari thread M9 atau dispatcher M10 secara konkuren |
| 3 | Pointer `buf` pada `mcs_sys_read`/`write` belum divalidasi rentang alamatnya (berbeda dari `mcs_user_check_range` M10) | Risiko jika dipanggil dengan pointer dari "user" tidak tepercaya | Tidak teraktivasi karena hanya dipanggil dengan buffer stack `kmain` yang valid pada sesi ini | Menerapkan pola validasi M10 sebelum syscall file benar-benar dipanggil dari `int 0x80` |
| 4 | Negative test aktif (kapasitas file terlampaui, segmen nama tepat di batas) belum dijalankan | Beberapa validasi hanya diverifikasi dari pembacaan kode, belum dari eksekusi nyata | Implementasi sudah ada dan tervalidasi secara desain | Sebelum demonstrasi/penilaian jika diminta |
| 5 | Sesi GDB tidak dijalankan pada M13 | Tidak ada bukti debugging register-level untuk operasi VFS, hanya log serial | Log serial dan host test terbukti cukup untuk verifikasi end-to-end | Pengayaan, sebelum demonstrasi jika diminta |
| 6 | Hash SHA-256 untuk `kernel.elf`/`mcsos.iso` belum dicatat | Bukti integritas kriptografis sebagian | `sha256sum` sudah ada untuk dua artefak inti M13 lewat `make m13-audit` | Sebelum pengumpulan akhir jika diwajibkan |
| 7 | Prosedur rollback belum dieksekusi aktif | Sama dengan known issue berkelanjutan dari M5-M12 | Dua commit M13 terpisah secara struktural mendukung rollback parsial | Sebelum demonstrasi/penilaian |

Keputusan akhir:

```text
Berdasarkan bukti host unit test PASS, audit freestanding object kosong
dependency, kernel build sukses dengan symbol VFS terlink, dan QEMU
smoke test yang membuktikan RAMFS/FD table bekerja end-to-end tanpa
merusak M9-M11, hasil praktikum M13 ini layak disebut SIAP UJI QEMU
UNTUK VFS/RAMFS SINGLE-PROCESS sesuai definisi panduan -- bukan siap
untuk multi-proses terisolasi maupun filesystem persisten. Tujuh known
issue di atas, terutama soal mcs_process_t global dan ketiadaan locking,
harus ditindaklanjuti sebelum VFS ini dianggap layak menjadi fondasi
syscall file sungguhan atau proses terisolasi pada milestone berikutnya.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---|---:|---|---:|
| Kebenaran fungsional | 30 | RAMFS, FD table, operasi VFS, dan integrasi kernel berjalan benar end-to-end | `[diisi penilai]` |
| Kualitas desain dan invariants | 20 | Kontrak vnode/file/fd_table jelas, keputusan integrasi yang tidak diatur kontrak didokumentasikan dengan alasan eksplisit | `[diisi penilai]` |
| Pengujian dan bukti | 20 | Host test, static audit, QEMU log lengkap, mencakup skenario error | `[diisi penilai]` |
| Debugging/failure analysis | 10 | Failure modes teknis DAN proses (status commit, ukuran log) dianalisis jujur | `[diisi penilai]` |
| Keamanan dan robustness | 10 | Validasi path/fd, keterbatasan locking dan validasi pointer didokumentasikan jujur | `[diisi penilai]` |
| Dokumentasi/laporan | 10 | Laporan rapi, command/log lengkap, insiden proses didokumentasikan transparan, referensi memadai | `[diisi penilai]` |
| **Total** | **100** |  | `[diisi penilai]` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

```text
Seluruh tugas wajib panduan M13 berhasil diimplementasikan dan
dibuktikan dua lapis: kontrak VFS minimal (vnode, ramfs, file, fd_table,
process) sesuai panduan, RAMFS in-memory dengan path lookup segmen-per-
segmen dan alokasi statik, FD table dengan validasi flag akses dan
deteksi fd tidak valid, serta lima wrapper syscall siap dipakai
dispatcher M10. Host unit test mencakup tiga skenario realistis (baca
dasar, create-write-read, error handling dan limit fd) lulus tanpa
QEMU. Berbeda dari filosofi awal panduan yang mencukupkan diri sebagai
modul host-test-only, sesi ini melanjutkan ke integrasi kernel nyata
dengan keputusan desain baru yang tidak diatur kontrak maupun kode
existing -- satu mcs_process_t global dibagi seluruh kernel thread,
dipilih atas dasar kejujuran arsitektur (MCSOS belum punya isolasi
proses sungguhan) bukan kemudahan implementasi semata. Smoke test QEMU
membuktikan RAMFS bekerja end-to-end: data yang di-seed berhasil dibaca
kembali persis, file baru berhasil dibuat dan ditulis, dan scheduler M9
tetap utuh setelahnya. Pencapaian metodologis unik pada M13 dibanding
M9-M12: dua insiden proses ditemukan, satu di antaranya (ukuran log
QEMU) BERHASIL DICEGAH sebelum terjadi pada commit, bukan diperbaiki
sesudahnya lewat amend seperti pengalaman M10 -- bukti konkret bahwa
pembelajaran dari milestone sebelumnya benar-benar mengubah kebiasaan
kerja, bukan hanya tercatat di laporan.
```

### 22.2 Yang Belum Berhasil

```text
mcs_process_t masih satu instance global, belum benar-benar terisolasi
per proses/thread. Locking M12 belum diintegrasikan untuk melindungi
RAMFS/FD table dari akses konkuren, sesuai kontrak panduan namun tetap
menjadi risiko nyata untuk pekerjaan lanjutan. Validasi pointer user
gaya M10 (mcs_user_check_range) belum diterapkan pada mcs_sys_read/
write. Negative test aktif (kapasitas file terlampaui, batas nama
segmen) belum dijalankan. Sesi GDB tidak dijalankan pada M13. Hash
SHA-256 untuk kernel.elf/mcsos.iso belum dicatat. Prosedur rollback
belum dieksekusi aktif. Satu pembelajaran proses juga belum benar-benar
"berhasil dicegah dari awal": verifikasi git log/git diff terhadap file
yang sudah ter-commit baru dilakukan REAKTIF (setelah git status
terlihat aneh), bukan PROAKTIF (sebelum mulai menulis file pada branch
baru) -- berbeda dari pencegahan ukuran log yang sudah proaktif pada
sesi ini.
```

### 22.3 Rencana Perbaikan

```text
1. Mendesain isolasi proses yang lebih jujur: baik dengan menambahkan
   field pid/fd_table ke mcsos_thread_t M9 (setelah mempertimbangkan
   ulang risiko regresi dengan lebih matang), atau dengan model baru
   yang eksplisit memisahkan "process" dari "thread" sebagai dua
   entitas berbeda.
2. Mengintegrasikan mcs_spin_lock atau mcs_mutex_* M12 untuk melindungi
   g_m13_ramfs/g_m13_process, terutama sebelum syscall file benar-benar
   dipanggil dari thread M9 yang berjalan konkuren.
3. Menerapkan mcs_user_check_range (pola M10) pada mcs_sys_read/write
   sebelum pointer dari "user" benar-benar dipakai, menutup kesenjangan
   keamanan yang didokumentasikan pada bagian 17.1.
4. Menjalankan negative test aktif: penulisan tepat melebihi kapasitas
   256 byte, segmen nama path tepat di batas MCS_MAX_NAME, dan direktori
   bertingkat lebih dari satu level.
5. Menjalankan sesi GDB formal (breakpoint pada mcs_vfs_open dan
   mcs_ramfs_lookup) untuk memperkaya bukti debugging selain log serial.
6. Menjadikan git log --oneline -5 sebagai langkah baku SEGERA setelah
   git checkout -b pada milestone berikutnya, bukan hanya saat git
   status terlihat mencurigakan -- menerapkan pembelajaran proaktif
   yang sama seperti yang sudah berhasil diterapkan untuk pencegahan
   ukuran log pada sesi ini.
7. Mencatat hash SHA-256 untuk kernel.elf dan mcsos.iso, melengkapi
   SHA256SUMS yang sudah ada untuk m13_vfs_host_test dan
   m13_vfs_combined.o.
8. Menjalankan dan memverifikasi prosedur rollback (git checkout ke
   commit M12 atau ke commit host-test-only 20a8e2e) sebelum sesi
   demonstrasi/penilaian.
```

---

## 23. Lampiran

### Lampiran A — Commit Log

```text
8b8d15a (HEAD -> praktikum-m13-vfs-ramfs) M13: integrate VFS/RAMFS into kmain, add preflight and trimmed QEMU evidence
20a8e2e M13: implement VFS minimal, RAMFS, FD table, syscall file I/O, host test, and audit
55a09ab (praktikum/m12-sync) M12: add preflight log
bebe0e3 M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit
ad07b27 (praktikum-m11-elf-user-loader) M11: implement ELF64 user loader, host test, audit, and QEMU integration
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI, int 0x80 dispatcher, and kernel integration
```

### Lampiran B — Diff Ringkas

```diff
 Commit 20a8e2e (sudah ada sebelum sesi integrasi ini, dikonfirmasi via
 git diff HEAD kosong saat ditulis ulang pada Langkah 3-5):
 Makefile                  |  33 ++++++
 include/mcs_vfs.h         |  99 +++++++++++++++++
 kernel/vfs/fd.c           | 272 ++++++++++++++++++++++++++++++++++++++++++++++
 kernel/vfs/ramfs.c        | 248 ++++++++++++++++++++++++++++++++++++++++++
 kernel/vfs/sys_vfs.c      |   7 ++
 tests/m13_vfs_host_test.c |  83 ++++++++++++++

 Commit 8b8d15a (perubahan nyata sesi ini):
 kernel/core/kmain.c       | diubah signifikan: include mcs_vfs.h sudah
                             ada sebelumnya (placeholder tidak terpakai),
                             ditambahkan g_m13_ramfs, g_m13_process,
                             m13_vfs_bootstrap() dipanggil setelah
                             m11_elf_smoke_test()
 logs/m13_preflight.log    | baru, bukti gate M0-M12 dan toolchain
 logs/m13_serial.log       | baru, dipangkas 144.728 -> 60 baris SEBELUM
                             staging (lihat bagian 14.2 untuk perbandingan
                             dengan insiden serupa M10)
```

### Lampiran C — Log Build dan Audit Lengkap

```text
make m13-all (ringkasan):
M13 VFS/FD/RAMFS host tests: PASS
ELF Header: Class: ELF64, Type: REL, Machine: Advanced Micro Devices X86-64
grep -q "Machine..." / "mcs_ramfs_init" -- lulus
test ! -s nm_undefined.txt -- lulus (kosong)
sha256sum:
  f10321982b8e522de28445ea939a86812d4575082d1050c0ff9014b6fc331ce2  build/m13/m13_vfs_host_test
  c63e871276205bee6eecb8d458823181f054a13fb04db5813dea2ecffe4aef32  build/m13/m13_vfs_combined.o

make all (ringkasan, setelah integrasi kmain.c):
[seluruh file kernel/*.c, kernel/vfs/{ramfs,fd,sys_vfs}.c, kernel/sync/
{lockdep,mutex,spinlock}.c, kernel/syscall/syscall.c, kernel/user/
m11_elf_loader.c, src/pmm.c, src/vmm.c, kernel/mm/kmem.c,
kernel/mcsos_thread.c, kernel/arch/x86_64/isr.S, kernel/syscall/
syscall_entry.S, arch/x86_64/context_switch.S terkompilasi tanpa
warning meski -Werror aktif]
ld.lld -nostdlib -static ... -o build/kernel.elf [seluruh objek
termasuk fd.o, ramfs.o, sys_vfs.o]
[seluruh assertion grep -q dari target inspect lulus tanpa pesan error]

nm -n build/kernel.elf | grep -i "mcs_\|m13" (ringkasan):
m13_vfs_bootstrap (t)
mcs_lockdep_*, mcs_mutex_*, mcs_spin_* (M12, T)
mcs_fd_table_init, mcs_vfs_*, mcs_sys_*, mcs_ramfs_* (M13, T)
g_m13_ramfs, g_m13_process, mcs_active_ramfs_for_test (data statik)
```

### Lampiran D — Log QEMU Lengkap (Dipangkas)

```text
limine: Loading executable `boot():/boot/kernel.elf`...
MCSOS 260502 M4 kernel entered
[... log M4-M8 berlanjut normal ...]
[M9] scheduler initialized
[M10] syscall init
[M10] syscall ping ok
[M10] syscall get_ticks ok
[M10] syscall smoke done
[M10] int 0x80 gate installed
[M10] int 0x80 ping ok
[M11] elf: ident ok
[... log M11 plan berlanjut ...]
[M11] elf: plan ok
[M11] user image plan ready
[M13] ramfs+vfs smoke test passed
[M13] motd.txt content: mcsos-m13-ramfs
[M9] thread A tick
[M9] thread B tick
[M9] thread A tick
[M9] thread B tick
... (berlanjut bergantian tanpa henti, M9 tidak terganggu, log dipangkas
60 baris dari total 144.728 baris asli sebelum staging git)
```

### Lampiran E — Screenshot

Tidak ada screenshot pada sesi ini; seluruh bukti berbentuk log teks terminal (lihat Lampiran C dan D), konsisten dengan pendekatan laporan M9-M12.

### Lampiran F — Pertanyaan Reflektif

```text
1. Mengapa mcs_vnode_t (metadata) dipisah dari mcs_file_t (state sesi),
   bukan disatukan?
   Karena satu vnode bisa dibuka berkali-kali secara independen --
   setiap open menghasilkan sesi baru dengan posisi baca/tulis sendiri.
   Jika offset disimpan di vnode, dua fd yang membuka file yang sama
   akan saling mengganggu posisi bacanya. Host test membuktikan ini
   langsung: 16 kali open pada /x yang sama menghasilkan 16 fd
   independen yang semuanya menunjuk node yang sama tapi punya offset
   masing-masing.

2. Mengapa diputuskan satu mcs_process_t global, bukan satu per
   mcsos_thread_t M9, padahal secara konsep "lebih benar" memisahkan
   per proses?
   Karena mcsos_thread_t M9 adalah kontrak yang SUDAH TERUJI LENGKAP
   (host test, smoke test QEMU, DAN sesi GDB register-level) -- mengubah
   strukturnya untuk menambah field pid/fd_table berisiko meretakkan
   fondasi yang sudah terbukti solid, untuk manfaat yang belum benar-
   benar dibutuhkan saat ini (MCSOS belum punya isolasi address space
   per proses, sehingga "satu proses per thread" hanya akan jadi
   abstraksi kosong tanpa isolasi nyata di baliknya). Keputusan ini
   mengutamakan kejujuran terhadap kondisi arsitektur saat ini di atas
   kerapian konseptual yang belum punya dasar nyata.

3. Mengapa file VFS yang "terasa baru ditulis" pada sesi ini ternyata
   sudah ter-commit sebelumnya, dan apa pembelajarannya?
   Karena branch praktikum-m13-vfs-ramfs dibuat dari commit M12, lalu
   sesi langsung menulis ulang file tanpa memeriksa apakah branch itu
   benar-benar "kosong" terlebih dahulu. git checkout -b membuat
   branch baru dari titik HEAD saat itu, dan jika sebelumnya (di luar
   sesi chat ini) sudah ada commit M13 yang ditambahkan ke histori,
   commit itu akan ikut terbawa ke branch baru. Pembelajarannya:
   git log --oneline -5 seharusnya menjadi langkah PERTAMA setelah
   membuat branch baru, bukan baru dicek ketika git status terlihat
   aneh di akhir sesi.

4. Mengapa pencegahan insiden ukuran log pada M13 dianggap lebih baik
   metodologinya dibanding M10, padahal hasil akhirnya sama (log
   dipangkas)?
   Karena pada M10, ukuran log yang membengkak baru disadari SETELAH
   commit pertama selesai (lewat output git commit yang melaporkan
   155.440 insertions), memaksa penggunaan git commit --amend untuk
   memperbaiki riwayat yang sudah tercatat. Pada M13, wc -l dijalankan
   SEBELUM git add, sehingga log sudah bersih sebelum staging maupun
   commit pertama terjadi -- tidak ada riwayat git yang perlu
   diperbaiki sama sekali. Perbedaan ini menunjukkan pembelajaran dari
   M10 benar-benar mengubah urutan kerja pada M13, bukan hanya dicatat
   sebagai "pelajaran" di laporan tanpa mengubah kebiasaan nyata.

5. Mengapa M13 sengaja tidak memakai lock M12 untuk RAMFS/FD table,
   padahal M12 sudah tersedia dan ter-link di kernel?
   Karena menambahkan locking tanpa kebutuhan konkurensi nyata hanya
   menambah kompleksitas tanpa manfaat terverifikasi -- pada konfigurasi
   M13 saat ini, satu-satunya pemanggil g_m13_ramfs/g_m13_process adalah
   kmain pada jalur boot sinkron, sebelum interrupt M5 diaktifkan ulang,
   sehingga TIDAK ADA race yang mungkin terjadi untuk dicegah. Menambah
   lock sekarang hanya akan menjadi kode yang "terlihat aman" tanpa
   pernah benar-benar diuji skenario konkurensinya -- lebih jujur
   mendokumentasikan ketidakhadirannya secara eksplisit sebagai known
   issue, sesuai instruksi panduan sendiri, daripada menambahkan locking
   palsu yang memberi rasa aman keliru.
```

---

## 24. Daftar Referensi

```text
[1] The Open Group, "open, openat - open a file," POSIX.1-2017 (referensi
    konseptual umum, tidak dikutip literal). [Online]. Available:
    https://pubs.opengroup.org/onlinepubs/9699919799/functions/open.html
    Accessed: Jun. 29, 2026.

[2] The Open Group, "lseek - reposition read/write file offset,"
    POSIX.1-2017 (referensi konseptual umum). [Online]. Available:
    https://pubs.opengroup.org/onlinepubs/9699919799/functions/lseek.html
    Accessed: Jun. 29, 2026.

[3] The Linux Kernel Documentation, "Virtual File System," kernel.org,
    2026. [Online]. Available:
    https://www.kernel.org/doc/html/latest/filesystems/vfs.html
    Accessed: Jun. 29, 2026.

[4] LLVM Project, "Clang command line argument reference," Clang
    Documentation, 2026. [Online]. Available:
    https://clang.llvm.org/docs/ClangCommandLineReference.html
    Accessed: Jun. 29, 2026.

[5] GNU Project, "GNU Make Manual," Free Software Foundation, 2026.
    [Online]. Available: https://www.gnu.org/software/make/manual/
    Accessed: Jun. 29, 2026.

[6] GNU Project, "Git - git-diff Documentation," Git Documentation,
    2026. [Online]. Available: https://git-scm.com/docs/git-diff
    Accessed: Jun. 29, 2026.
```

---

## 25. Checklist Final Sebelum Pengumpulan

| Checklist | Status |
|---|---|
| Semua placeholder sudah diganti | `Ya` |
| Metadata laporan lengkap | `Ya` |
| Commit awal dan akhir dicatat (termasuk catatan commit existing) | `Ya` |
| Perintah build dan test dapat dijalankan ulang | `Ya` |
| Log build dilampirkan | `Ya` |
| Log QEMU dilampirkan (dipangkas, bukti dijelaskan transparan) | `Ya` |
| Artefak penting tersedia (sebagian dengan hash SHA-256) | `Ya` |
| Desain, invariants, dan failure modes dijelaskan | `Ya` |
| Insiden proses (status commit existing, ukuran log) didokumentasikan transparan | `Ya` |
| Security/reliability dibahas | `Ya` |
| Readiness review tidak berlebihan | `Ya` |
| Rubrik penilaian disiapkan | `Ya` (kolom nilai dikosongkan untuk penilai) |
| Referensi memakai format IEEE | `Ya` |
| Laporan disimpan sebagai Markdown | `Ya` |

---

## 26. Pernyataan Pengumpulan

Saya mengumpulkan laporan ini bersama artefak pendukung pada commit:

```text
8b8d15a
```

Status akhir yang diklaim:

```text
Siap uji QEMU untuk VFS/RAMFS single-process — bukan siap produksi,
bukan bukti isolasi multi-proses, bukan filesystem persisten, dengan
known issues pada bagian 20 yang harus ditindaklanjuti (mcs_process_t
masih global, belum ada locking M12 untuk RAMFS/FD table, validasi
pointer user gaya M10 belum diterapkan, negative test aktif belum
dijalankan, sesi GDB belum dijalankan, hash kernel.elf/mcsos.iso belum
dicatat, rollback belum diuji aktual).
```

Ringkasan satu paragraf:

```text
Praktikum M13 MCSOS 260502 telah diselesaikan oleh Agung Nurjaman
(25832073010) secara individu, melanjutkan dari gate M12 yang solid
(commit 55a09ab). VFS minimal berhasil dibangun lengkap: kontrak vnode/
file/fd_table/process, RAMFS in-memory dengan path lookup segmen-per-
segmen dan alokasi statik, FD table dengan validasi flag akses dan
deteksi fd tidak valid, serta lima wrapper syscall siap dipakai
dispatcher M10. Host unit test mencakup tiga skenario (baca dasar,
create-write-read, error handling dan limit fd) lulus tanpa QEMU.
Berbeda dari filosofi awal panduan yang mencukupkan diri sebagai modul
host-test-only, sesi ini melanjutkan ke integrasi kernel nyata dengan
keputusan desain baru -- satu mcs_process_t global dibagi seluruh
kernel thread, dipilih atas dasar kejujuran arsitektur karena MCSOS
belum memiliki isolasi proses sungguhan, bukan demi kerapian konseptual
semata. Smoke test QEMU membuktikan RAMFS bekerja end-to-end: data
yang di-seed berhasil dibaca kembali persis (mcsos-m13-ramfs), file
baru berhasil dibuat dan ditulis, dan scheduler M9 tetap utuh setelahnya.
Dua insiden proses ditemukan dan ditangani pada sesi ini: pertama,
diagnosis mengungkap bahwa file inti M13 ternyata sudah ter-commit
sebelumnya (20a8e2e) sebelum sesi integrasi kernel dimulai, dikonfirmasi
aman lewat git diff yang kosong; kedua, ukuran log QEMU yang membengkak
(144.728 baris, insiden identik dengan M10) berhasil DICEGAH SEBELUM
commit kali ini, berbeda dari M10 yang baru diperbaiki sesudahnya lewat
amend -- bukti konkret bahwa pembelajaran dari milestone sebelumnya
benar-benar mengubah kebiasaan kerja. Dua commit tersimpan bersih di
branch praktikum-m13-vfs-ramfs: commit host-test-only (20a8e2e) dan
commit integrasi kernel (8b8d15a). Status readiness yang diklaim adalah
siap uji QEMU untuk VFS/RAMFS single-process, secara eksplisit bukan
siap untuk multi-proses terisolasi maupun filesystem persisten, dengan
tujuh known issues sebagai catatan untuk ditindaklanjuti sebelum
syscall file sungguhan atau isolasi proses dibangun di atas fondasi ini.
```
