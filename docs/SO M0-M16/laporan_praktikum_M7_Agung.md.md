# Virtual Memory Manager Awal, Page Table x86_64, dan Page Fault Diagnostics pada MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M7_Agung.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, subset POSIX-like
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `M7` |
| Judul praktikum | `Virtual Memory Manager Awal, Page Table x86_64, dan Page Fault Diagnostics pada MCSOS 260502` |
| Jenis pengerjaan | `Individu` |
| Nama mahasiswa | `Agung Nurjaman` |
| NIM | `25832073010` |
| Kelas | `Pendidikan Teknologi Informasi` |
| Nama kelompok | `-` |
| Anggota kelompok | `-` |
| Tanggal praktikum | `2026-06-20` |
| Tanggal pengumpulan | `2026-06-21` |
| Repository | `/home/agung/src/mcsos` |
| Branch | `m7-vmm-core` |
| Commit awal | `be7a196` (M6) |
| Commit akhir | `59ebb47` (M7) |
| Status readiness yang diklaim | `Siap uji QEMU untuk VMM awal — bukan siap produksi` |

---

## 1. Sampul

# Laporan Praktikum M7
## Virtual Memory Manager Awal, Page Table x86_64, dan Page Fault Diagnostics pada MCSOS 260502

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
Panduan resmi praktikum M7 MCSOS 260502 digunakan sebagai referensi utama
dan kontrak fungsi untuk seluruh komponen (vmm.h, vmm.c, test_vmm_host.c,
m7_preflight.sh, grade_m7.sh, patch Makefile dan kmain.c). Intel SDM Vol.3
digunakan sebagai referensi teknis untuk format page table 4-level x86_64,
canonical address 48-bit, error code page fault, dan instruksi invlpg/CR3.
AMD64 Architecture Programmer's Manual Vol.2 digunakan sebagai referensi
silang untuk long mode paging. AI assistant (Claude) digunakan untuk:
(1) menulis draft awal vmm.h, vmm.c, test_vmm_host.c, m7_preflight.sh,
grade_m7.sh, dan patch kmain.c sesuai kontrak panduan M7; (2) membantu
mendiagnosis masalah link error duplicate symbol pmm.o yang muncul karena
SRC_C Makefile awalnya hanya mencari di kernel/ sehingga src/vmm.c tidak
ter-include, kemudian setelah diperluas ke find kernel src muncul pmm.o
duplikat karena Makefile M6 sudah menambahkan pmm.o secara eksplisit di
OBJ; keduanya diperbaiki dengan memperluas SRC_C ke find kernel src dan
menghapus baris eksplisit pmm.o dari OBJ; (3) membantu mengarahkan urutan
eksekusi grade_m7.sh agar mkdir -p build/evidence dilakukan sebelum make
clean. AI tidak digunakan untuk mengubah kontrak fungsional di luar yang
ditentukan panduan resmi. Seluruh build, host unit test, audit nm/objdump,
QEMU smoke test, dan commit git dijalankan dan diverifikasi sendiri oleh
mahasiswa di WSL 2 miliknya.
```

---

## 3. Tujuan Praktikum

1. Membangun Virtual Memory Manager (VMM) awal berbasis page table 4-level x86_64 (PML4 → PDPT → PD → PT) yang dapat memetakan, menanyakan, dan membatalkan pemetaan halaman 4 KiB secara deterministik.
2. Mengimplementasikan validasi alamat canonical 48-bit dan validasi alignment 4 KiB untuk virtual address dan physical address, agar permintaan tidak valid ditolak secara eksplisit.
3. Mencegah remap diam-diam terhadap leaf PTE yang sudah present, dengan mengembalikan `VMM_ERR_EXISTS` secara eksplisit.
4. Menyediakan primitive arsitektural `invlpg`, `vmm_read_cr2`, `vmm_read_cr3`, dan `vmm_write_cr3` sebagai inline assembly freestanding, dengan no-op otomatis saat dikompilasi sebagai host unit test.
5. Menyediakan adapter `phys_to_virt` yang eksplisit agar tidak ada asumsi diam-diam bahwa physical address dapat di-dereference langsung sebagai pointer kernel.
6. Menyediakan host unit test deterministik yang membuktikan logika table walk, penolakan noncanonical, penolakan unaligned, deteksi duplicate map, dan unmap benar tanpa boot QEMU.
7. Mengintegrasikan VMM ke kernel MCSOS dengan adapter PMM M6, smoke test map/query/unmap di kmain, dan membuktikan log `[M7] VMM core initialized` muncul di QEMU.
8. Mengaudit object VMM freestanding agar bebas dependency host (`nm -u` kosong) dan membuktikan `invlpg` serta akses `cr3` ada di disassembly.
9. Mengumpulkan evidence reproducible ke `evidence/M7` beserta manifest toolchain.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Menjelaskan translasi virtual address x86_64 melalui PML4, PDPT, PD, dan PT | Bagian 6.1; fungsi `idx_pml4/idx_pdpt/idx_pd/idx_pt` di `vmm.c`; diagram page-table walk bagian 9.3 |
| Menjelaskan peran CR3 sebagai basis fisik page-table hierarchy | Disassembly menunjukkan `mov %cr3,%rax` dan `mov %rax,%cr3`; bagian 14.2 |
| Mengimplementasikan validasi canonical 48-bit | `vmm_is_canonical()` di `vmm.c`; host test `assert(!vmm_is_canonical(0x0000800000000000ULL))` lulus |
| Mengimplementasikan map, query, dan unmap halaman 4 KiB | Host test seluruh jalur lulus; QEMU smoke test map/query/unmap di `m7_vmm_init()` lulus |
| Mencegah remap diam-diam | Host test `vmm_map_page(duplikat) == VMM_ERR_EXISTS` lulus |
| Memanggil `invlpg` setelah unmap | `vmm_unmap_page()` memanggil `vmm_invalidate_page()`; disassembly membuktikan instruksi `invlpg` ada |
| Menggunakan adapter `phys_to_virt` eksplisit | `table_from_phys()` selalu melalui `space->phys_to_virt`; tidak ada cast fisik langsung |
| Mengaudit object freestanding tanpa undefined symbol | `nm -u build/vmm.o` kosong; dibuktikan di `evidence/M7/m7_vmm_nm_undefined.txt` |
| Mengintegrasikan VMM dengan PMM M6 di kernel nyata | Log QEMU menunjukkan `[M7] VMM core initialized` dan `smoke test passed` |
| Menjelaskan error code page fault minimal | Bagian 11; `vmm_read_cr2()` tersedia untuk handler vector 14 |

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
| M7 | VMM awal, page table 4-level, page fault diagnostics | `✓ selesai praktikum` |
| M8 | Thread, scheduler, synchronization | `[ ] tidak dibahas` |
| M9 | Syscall ABI dan user program loader | `[ ] tidak dibahas` |
| M10 | VFS, file descriptor, ramfs | `[ ] tidak dibahas` |
| M11 | Block layer dan device model | `[ ] tidak dibahas` |
| M12 | Persistent filesystem, recovery | `[ ] tidak dibahas` |
| M13 | Networking stack | `[ ] tidak dibahas` |
| M14 | Security model, capability/ACL, hardening | `[ ] tidak dibahas` |
| M15 | SMP, scalability, lock stress | `[ ] tidak dibahas` |
| M16 | Observability, release image, readiness review | `[ ] tidak dibahas` |

Batas cakupan praktikum:

```text
M7 mencakup: VMM core (vmm_space_init, vmm_map_page, vmm_query_page,
vmm_unmap_page), validasi canonical/alignment, pencegahan remap diam-diam,
primitive x86_64 (invlpg, read_cr2, read_cr3, write_cr3), host unit test
deterministik, audit freestanding (nm -u, objdump invlpg/cr3), integrasi
kernel dengan adapter PMM M6, QEMU smoke test, dan evidence manifest.

M7 TIDAK mencakup: aktivasi CR3 baru (write_cr3 tidak dipanggil pada
tugas wajib), user/kernel isolation, demand paging, copy-on-write,
swapping, huge page, PCID, 5-level paging, heap kmalloc, NXE/SMEP/SMAP
enforcement, KASLR/ASLR, page fault recovery otomatis, dan hardware
bring-up di luar QEMU.
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

```text
M7 membangun fondasi Virtual Memory Manager pada kernel pendidikan
MCSOS 260502. Lima konsep utama:

1. Paging 4-level x86_64: CPU dalam long mode menggunakan empat level
   page table (PML4 → PDPT → PD → PT) untuk mentranslasikan virtual
   address ke physical address. CR3 menyimpan alamat fisik basis PML4.
   Setiap level berisi 512 entry 8-byte (64-bit). Translasi mengambil
   9 bit dari virtual address per level: bit 47–39 untuk PML4, bit 38–30
   untuk PDPT, bit 29–21 untuk PD, bit 20–12 untuk PT, dan bit 11–0
   sebagai page offset.

2. Canonical Address 48-bit: pada x86_64, virtual address yang valid
   (canonical) harus memiliki bit 47 direplikasi ke bit 48–63. Alamat
   0x0000_0000_0000_0000 hingga 0x0000_7FFF_FFFF_FFFF (bit 47 = 0,
   upper 16 bit = 0x0000) dan 0xFFFF_8000_0000_0000 hingga
   0xFFFF_FFFF_FFFF_FFFF (bit 47 = 1, upper 16 bit = 0xFFFF) adalah
   canonical. Alamat di antaranya menyebabkan #GP.

3. HHDM/Direct Map sebagai Akses Fisik: page table berada di memori
   fisik yang dialokasikan PMM. Untuk menulis entry page table, kernel
   membutuhkan virtual address yang terpetakan ke physical frame tersebut.
   Bootloader Limine menyediakan Higher Half Direct Map (HHDM) yang
   memetakan seluruh memori fisik ke region virtual tertentu. Pada M7,
   adapter phys_to_virt mengonversi physical address ke virtual address
   melalui HHDM offset. Tanpa ini, kernel tidak dapat memodifikasi page
   table yang berada di memori fisik.

4. TLB dan invlpg: Translation Lookaside Buffer (TLB) adalah cache
   hardware yang menyimpan translasi virtual-physical terbaru. Ketika
   entry page table dihapus (unmap), TLB mungkin masih menyimpan translasi
   lama sehingga akses ke virtual address tersebut masih berhasil (stale
   TLB). Instruksi invlpg mengusir satu entry TLB untuk virtual address
   tertentu agar translasi lama tidak dipakai lagi setelah unmap.

5. Page Fault Error Code: exception vector 14 (#PF) membawa error code
   berisi bit: P (0 = non-present, 1 = protection violation), W/R
   (0 = read, 1 = write), U/S (0 = supervisor, 1 = user), RSVD (reserved
   bit di PTE aktif), I/D (1 = instruction fetch). CR2 menyimpan linear
   address yang menyebabkan fault. Kombinasi error code + CR2 + RIP
   memungkinkan diagnosis penyebab fault tanpa harus menebak.
```

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| PML4/PDPT/PD/PT 4-level | Struktur utama VMM M7; setiap level mengambil 9 bit dari virtual address | Fungsi `idx_pml4/pdpt/pd/pt` di `vmm.c`; host test map lulus |
| CR3 | Register basis physical PML4; dibaca/ditulis dengan assembly | Disassembly `vmm.o` menunjukkan `mov %cr3,%rax` dan `mov %rax,%cr3` |
| `invlpg` | Instruksi invalidasi TLB per-halaman; dipanggil setelah unmap | Disassembly menunjukkan `invlpg (%rax)`; `m7_vmm_nm_undefined.txt` kosong |
| Canonical address 48-bit | Virtual address tidak canonical menyebabkan #GP | `vmm_is_canonical()` menolak `0x0000_8000_0000_0000`; host test lulus |
| Page fault vector 14 | Exception ketika translasi gagal atau hak akses dilanggar | `vmm_read_cr2()` tersedia; handler dispatcher M4 terhubung |
| CR2 | Menyimpan linear address penyebab page fault | `vmm_read_cr2()` menggunakan `mov %%cr2, %0` via inline assembly |
| `-mno-red-zone` | Kernel harus kompilasi tanpa red-zone agar interrupt handler aman | Flag aktif di seluruh build; `make check` dan `make build` lulus |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding; tidak ada stdlib host kecuali pada host unit test |
| Runtime | Tanpa hosted libc; adapter allocator dan phys_to_virt disuplai dari luar |
| ABI | `x86_64-unknown-none-elf`, `-mcmodel=kernel`, `-mno-red-zone` |
| Compiler flags kritis | `-ffreestanding`, `-fno-builtin`, `-nostdlib`, `-mno-red-zone`, `-fno-pic -fno-pie` |
| Dual-compile pattern | `#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)` memisahkan primitive hardware dari no-op host |
| Risiko undefined behavior | Cast `(uint64_t *)space->phys_to_virt(paddr)` hanya valid jika adapter mengembalikan pointer aligned; alignment divalidasi sebelum cast |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Intel SDM Vol.3 | Chapter 4 (Paging), Table 4-19, Fig 4-8 | Format PTE 64-bit, bit present/writable/user/huge/NX, canonical address |
| [2] | AMD64 APM Vol.2 | Chapter 5 (Page Translation and Protection) | Referensi silang translasi long mode, CR3, TLB |
| [3] | QEMU Documentation | GDB usage, `-s -S`, memory inspection | GDB workflow untuk inspect CR3/page table |
| [4] | Limine Documentation | HhdmRequest, MemoryMapRequest | Dasar konsep HHDM offset dan memory map handoff |
| [5] | GNU Binutils — LD | SECTIONS, MEMORY | Linker script layout kernel M7 |
| [6] | LLVM/Clang Reference | `-ffreestanding`, `-mno-red-zone` | Flags kompilasi freestanding kernel |
| [7] | LLVM LLD | `-nostdlib`, `-static` | Linking kernel ELF64 tanpa libc |

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
| Boot path | Limine bootloader (third_party/limine), dilanjutkan dari M2–M6 |
| Debugger | GNU gdb (Ubuntu 17.1-2ubuntu1) 17.1 |
| Build system | GNU Make |
| Bahasa utama | C17 freestanding + GAS assembly |
| Compiler | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Linker | Ubuntu LLD 21.1.8 (compatible with GNU linkers) |
| Host compiler (unit test) | clang (via HOSTCC) |

### 7.2 Versi Toolchain

```text
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
lld=Ubuntu LLD 21.1.8 (compatible with GNU linkers)
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
```

Diambil dari `evidence/M7/manifest.txt`, commit `59ebb47`.

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Apakah berada di filesystem Linux WSL, bukan `/mnt/c` | `Ya` |
| Remote repository | `-` (lokal) |
| Branch | `m7-vmm-core` |
| Commit hash awal | `be7a196` (M6 selesai) |
| Commit hash akhir | `59ebb47` (M7 evidence) |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
├── include/
│   ├── types.h           (M6, dipakai kembali)
│   ├── pmm.h             (M6, dipakai kembali)
│   └── vmm.h             (baru M7)
├── src/
│   ├── pmm.c             (M6, dipakai kembali)
│   └── vmm.c             (baru M7)
├── tests/
│   ├── test_pmm_host.c   (M6)
│   └── test_vmm_host.c   (baru M7)
├── scripts/
│   ├── check_m6_static.sh
│   ├── m7_preflight.sh   (baru M7)
│   └── grade_m7.sh       (baru M7)
├── kernel/
│   └── core/
│       └── kmain.c       (diperbarui M7)
├── evidence/
│   └── M7/               (baru M7)
│       ├── m7-qemu-serial.log
│       ├── m7_make_check.log
│       ├── m7_vmm_nm_undefined.txt
│       ├── m7_vmm_objdump.txt
│       ├── m7_vmm_readelf_header.txt
│       ├── m7_vmm_readelf_sections.txt
│       └── manifest.txt
├── Makefile              (diperbarui M7)
└── linker.ld
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis | Alasan | Risiko |
|---|---|---|---|
| `include/vmm.h` | Baru | Definisi API VMM, flags PTE, struct vmm_space/vmm_mapping, deklarasi fungsi | Sedang — urutan field struct dan nilai flag harus identik dengan yang dipakai vmm.c |
| `src/vmm.c` | Baru | Implementasi VMM core: canonical check, alignment check, table walk 4-level, alloc intermediate table, map/query/unmap, primitive assembly | Tinggi — kesalahan indeks bit virtual address menyebabkan table walk ke entry salah |
| `tests/test_vmm_host.c` | Baru | Host unit test: mock allocator 64 frame, uji canonical, alignment, map, query, unmap, duplicate, noncanonical | Rendah — hanya dipakai di host test, tidak masuk kernel |
| `scripts/m7_preflight.sh` | Baru | Validasi kesiapan M0–M6 dan toolchain sebelum M7 | Rendah |
| `scripts/grade_m7.sh` | Baru | Grading mekanis: make check, nm -u, objdump invlpg/cr3, kumpulkan evidence | Rendah |
| `kernel/core/kmain.c` | Ubah | Tambah `#include "vmm.h"`, `g_vmm`, `g_hhdm_offset`, adapter `m7_vmm_alloc/free/phys_to_virt`, fungsi `m7_vmm_init`, dan pemanggilan setelah PMM init | Tinggi — adapter phys_to_virt yang salah (HHDM offset salah) menyebabkan kernel menulis ke page table yang salah |
| `Makefile` | Ubah | Perluas `SRC_C` dari `find kernel` ke `find kernel src`, hapus baris eksplisit `pmm.o` dari OBJ, tambahkan target `check`, `vmm.o`, `test_vmm_host`, variabel `HOSTCC/HOST_CFLAGS/VMM_CFLAGS` | Sedang — perubahan SRC_C berisiko menarik file yang tidak diinginkan; penghapusan eksplisit pmm.o dari OBJ harus tidak menghapus pmm.o dari target check-m6 |

### 8.3 Ringkasan Diff

```text
git log --oneline m7-vmm-core
59ebb47 M7: add evidence manifest, QEMU log, and audit artifacts
382e235 M7: integrate VMM into kernel, QEMU smoke test passed
0161a40 M7: implement VMM core, host unit test, and audit scripts
be7a196 (praktikum/m6-pmm) M6: implement bitmap PMM, host unit test, and kernel integration
```

File baru pada commit M7:
- `include/vmm.h`, `src/vmm.c`, `tests/test_vmm_host.c`
- `scripts/m7_preflight.sh`, `scripts/grade_m7.sh`
- `evidence/M7/*`

File yang diubah pada commit M7:
- `kernel/core/kmain.c` — tambah integrasi VMM (76 baris baru)
- `Makefile` — perluas SRC_C, tambah target check/vmm/test_vmm_host

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

```text
Kernel M6 memiliki PMM yang dapat mengalokasikan frame fisik 4 KiB, tetapi
tidak memiliki mekanisme pemetaan virtual address ke physical address. Tanpa
VMM, kernel tidak dapat mengisolasi region memori, tidak dapat membuat
address space baru, dan setiap akses memori bergantung sepenuhnya pada page
table yang dibuat bootloader (yang tidak dapat dimodifikasi kernel secara
terkontrol). M7 menyelesaikan masalah ini dengan membangun library VMM yang
dapat membuat page table baru dari nol menggunakan frame dari PMM M6,
memetakan halaman 4 KiB dengan flags yang dikontrol kernel, dan membatalkan
pemetaan dengan invalidasi TLB yang benar. Dengan M7, kernel memiliki primitif
dasar untuk membangun address space yang dapat dikontrol, sebagai fondasi untuk
VMM penuh, heap, dan user mode di milestone berikutnya.
```

### 9.2 Keputusan Desain

| Keputusan | Alternatif | Alasan memilih | Konsekuensi |
|---|---|---|---|
| `struct vmm_space` dengan function pointer adapter | Global state hardcoded | Memungkinkan host unit test tanpa hardware; adapter dipakai ulang untuk kernel integration | Function pointer call overhead kecil; tidak relevan pada kernel pendidikan |
| `phys_to_virt` sebagai adapter eksplisit | Cast langsung `(void *)paddr` | Mencegah asumsi diam-diam bahwa physical address == virtual address; hanya valid pada sistem dengan identity mapping | Setiap akses page table harus melalui adapter; kode lebih verbose tapi lebih aman |
| Dual-compile: `#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)` | File terpisah | Satu file source untuk freestanding dan host test; tidak ada duplikasi logika | Harus menjaga konsistensi kondisi preprocessor antara host dan target build |
| `VMM_ERR_EXISTS` untuk duplicate map | Silent overwrite | Mencegah bug mapping yang tidak terdeteksi; caller harus eksplisit menangani kasus remap | Caller harus memeriksa return code; lebih ketat tetapi lebih aman |
| Intermediate table baru di-zero sebelum present | Zero on demand | Mencegah stale data dari frame PMM (yang mungkin berisi data lama) menjadi entry PTE yang diinterpretasi CPU | `vmm_zero_page()` dipanggil sebelum `table[index] = ...` di `get_or_alloc_next_table()` |
| HHDM offset = 0 (identity map) pada M7 | Offset Limine asli | Bootstrap Limine HHDM request belum diimplementasikan; identity mapping valid selama bootloader masih menyediakan mapping tersebut | Tidak portabel ke sistem dengan HHDM offset berbeda dari 0; dicatat sebagai known issue |
| Tidak mengaktifkan CR3 baru | Aktifkan write_cr3 setelah init | Mapping kernel/stack/IDT/HHDM belum diverifikasi lengkap; write_cr3 prematur akan menyebabkan triple fault | VMM M7 hanya sebagai library; aktivasi CR3 adalah pengayaan yang membutuhkan mapping lengkap |

### 9.3 Diagram Page-Table Walk

```text
Virtual Address (64-bit):
 63        48 47      39 38      30 29      21 20      12 11       0
 [sign extend][  PML4i  ][  PDPTi  ][   PDi   ][   PTi   ][ offset ]
              [  9 bit  ][  9 bit  ][  9 bit  ][  9 bit  ][ 12 bit ]

Translasi:
CR3 (physical) → PML4[PML4i] → PDPT[PDPTi] → PD[PDi] → PT[PTi] → frame + offset

Flowchart vmm_map_page:
1. Validasi: canonical(vaddr)? aligned(vaddr)? aligned(paddr)?
2. PML4 = table_from_phys(space, root_paddr)
3. get_or_alloc_next_table(PML4, idx_pml4) → PDPT
4. get_or_alloc_next_table(PDPT, idx_pdpt) → PD
5. get_or_alloc_next_table(PD, idx_pd) → PT
6. PT[idx_pt]: jika present → return VMM_ERR_EXISTS
7. PT[idx_pt] = (paddr & ADDR_MASK) | PRESENT | (flags & allowed)
8. return VMM_MAP_OK

get_or_alloc_next_table (per level):
  - Jika entry present & bukan huge: kembalikan pointer ke next table
  - Jika entry present & huge: return VMM_ERR_EXISTS (M7 tidak mengelola huge)
  - Jika entry tidak present: alloc_frame → zero → pasang entry PRESENT|WRITABLE
```

### 9.4 Kontrak Antarmuka

| Fungsi | Precondition | Postcondition | Error path |
|---|---|---|---|
| `vmm_space_init` | `space != NULL`, `phys_to_virt != NULL`, `root_paddr` aligned 4 KiB | Space terisi, siap dipakai oleh map/query/unmap | `VMM_ERR_INVAL` jika precondition gagal |
| `vmm_map_page` | Space valid, vaddr canonical & aligned, paddr aligned | Leaf PTE terisi dengan paddr dan flags; intermediate table dibuat otomatis jika belum ada | `VMM_ERR_INVAL` (canonical/alignment), `VMM_ERR_EXISTS` (leaf sudah present), `VMM_ERR_NOMEM` (alloc_frame gagal) |
| `vmm_query_page` | Space valid, vaddr canonical & aligned, out != NULL | `out->vaddr`, `out->paddr`, `out->flags` terisi sesuai PTE | `VMM_ERR_INVAL`, `VMM_ERR_NOT_FOUND` (tidak ada mapping di salah satu level) |
| `vmm_unmap_page` | Space valid, vaddr canonical & aligned | Leaf PTE dizeroi; `vmm_invalidate_page(vaddr)` dipanggil | `VMM_ERR_INVAL`, `VMM_ERR_NOT_FOUND` |
| `vmm_invalidate_page` | vaddr valid (tidak divalidasi di dalam fungsi) | TLB entry untuk vaddr diusir; no-op pada host test | Tidak ada error path |
| `vmm_read_cr2` | Dipanggil dari exception handler (misalnya page fault vector 14) | Nilai CR2 (fault address) dikembalikan | No-op pada host test, mengembalikan 0 |

### 9.5 Invariants

| Kode | Invariant | Implementasi | Bukti |
|---|---|---|---|
| VMM-I1 | `root_paddr` selalu aligned 4 KiB | `vmm_space_init` menolak unaligned root | Host test `vmm_space_init` lulus |
| VMM-I2 | Virtual address harus canonical 48-bit | `vmm_is_canonical()` di semua entry point | Host test noncanonical `0x0000800000000000` gagal dengan `VMM_ERR_INVAL` |
| VMM-I3 | `vaddr` dan `paddr` harus aligned 4 KiB | `vmm_is_aligned_4k()` di semua entry point | Host test `paddr = 0x400001` (unaligned) gagal dengan `VMM_ERR_INVAL` |
| VMM-I4 | Intermediate table baru selalu di-zero sebelum present | `vmm_zero_page()` dipanggil sebelum memasang entry | Review `get_or_alloc_next_table`; host test tidak ada stale entry |
| VMM-I5 | Remap leaf present tidak boleh overwrite diam-diam | Return `VMM_ERR_EXISTS` jika `pt[pti] & VMM_PTE_PRESENT` | Host test duplicate map lulus |
| VMM-I6 | Unmap memanggil `vmm_invalidate_page` | `vmm_unmap_page` selalu memanggil `vmm_invalidate_page(vaddr)` sebelum return | Disassembly `invlpg` ada; host test unmap + query = NOT_FOUND |
| VMM-I7 | Huge page tidak ditangani | Entry dengan bit huge di intermediate → `VMM_ERR_EXISTS`/`NOT_FOUND` | Review kode `get_or_alloc_next_table` dan `vmm_query_page` |
| VMM-I8 | Akses fisik selalu melalui adapter `phys_to_virt` | `table_from_phys()` selalu memanggil `space->phys_to_virt` | Review `table_from_phys`; tidak ada cast `(uint64_t *)paddr` langsung |
| VMM-I9 | Object freestanding tanpa dependency libc | Dikompilasi dengan `-ffreestanding -fno-builtin` | `nm -u build/vmm.o` kosong; `evidence/M7/m7_vmm_nm_undefined.txt` kosong |

### 9.6 Security Boundary M7

| Boundary | Risiko | Mitigasi | Catatan |
|---|---|---|---|
| Flag `VMM_PTE_USER` | Halaman kernel terekspos ke user mode jika bit user tidak di-clear | Caller mengontrol flags; VMM M7 hanya menerima flags dari `allowed` mask | M7 belum ada user mode; risiko aktual baru ada di M9+ |
| Flag `VMM_PTE_NO_EXECUTE` | Halaman data dapat dieksekusi jika NX tidak dipasang | Caller dapat menyertakan `VMM_PTE_NO_EXECUTE`; VMM tidak memaksa W^X | W^X enforcement adalah pengayaan M7; dicatat sebagai known issue |
| HHDM offset = 0 | Jika paddr tidak valid, `phys_to_virt` mengembalikan pointer ke virtual address yang mungkin berisi data kernel | `vmm_is_aligned_4k(paddr)` dan `frame < TEST_FRAMES` (host) membatasi jangkauan | HHDM asli Limine membatasi range lebih ketat; M7 memakai identity map sementara |
| Aktivasi CR3 baru | Triple fault jika mapping kernel/stack/IDT belum lengkap | `vmm_write_cr3` tidak dipanggil pada tugas wajib M7 | Pengayaan hanya setelah mapping lengkap dan rollback jelas |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Buat Branch M7

```bash
git switch -c m7-vmm-core
git branch --show-current
```

Output: `m7-vmm-core`

### Langkah 2 — Buat Header `include/vmm.h`

Mendefinisikan kontrak API VMM: macro flags PTE, kode error, typedef function pointer, struct `vmm_space` dan `vmm_mapping`, deklarasi seluruh fungsi publik.

```bash
cat > include/vmm.h << 'EOF'
# [isi sesuai panduan M7 Langkah 1]
EOF
```

Verifikasi:

```text
grep -n "vmm_map_page|vmm_query_page|vmm_unmap_page|vmm_read_cr2|struct vmm_space" include/vmm.h
32:struct vmm_space {
48:int  vmm_space_init(...)
54:int  vmm_map_page(...)
55:int  vmm_unmap_page(...)
56:int  vmm_query_page(...)
58:uint64_t vmm_read_cr3(void);
60:uint64_t vmm_read_cr2(void);
```

### Langkah 3 — Buat Implementasi `src/vmm.c`

Mengimplementasikan table walk 4-level, canonical check, alignment check, `get_or_alloc_next_table`, `vmm_map_page`, `vmm_query_page`, `vmm_unmap_page`, dan primitive assembly x86_64 dengan dual-compile pattern.

```bash
cat > src/vmm.c << 'EOF'
# [isi sesuai panduan M7 Langkah 2]
EOF
```

Verifikasi:

```text
grep -n "vmm_zero_page|vmm_is_canonical|idx_pml4|invlpg|MCSOS_HOST_TEST" src/vmm.c
3:static void vmm_zero_page(uint64_t *page) {
13:bool vmm_is_canonical(uint64_t vaddr) {
19:static unsigned idx_pml4(uint64_t vaddr) { ... }
163:#if defined(__x86_64__) && !defined(MCSOS_HOST_TEST)
165:    __asm__ volatile("invlpg (%0)" ...
```

### Langkah 4 — Buat Host Unit Test `tests/test_vmm_host.c`

Membuat 64 frame fisik palsu sebagai array statis, mock allocator mulai frame 2, root pada frame 1, lalu menguji seluruh path: canonical, unaligned, map, query, duplicate, unmap, map ulang setelah unmap.

```bash
cat > tests/test_vmm_host.c << 'EOF'
# [isi sesuai panduan M7 Langkah 3]
EOF
```

Verifikasi:

```text
grep -n "vmm_map_page|vmm_query_page|vmm_unmap_page|PASS" tests/test_vmm_host.c
50: assert(vmm_map_page(...) == VMM_MAP_OK);
54: assert(vmm_query_page(...) == VMM_MAP_OK);
61: assert(vmm_map_page(duplikat) == VMM_ERR_EXISTS);
62: assert(vmm_map_page(unaligned) == VMM_ERR_INVAL);
63: assert(vmm_map_page(noncanonical) == VMM_ERR_INVAL);
65: assert(vmm_unmap_page(...) == VMM_MAP_OK);
66: assert(vmm_query_page(setelah unmap) == VMM_ERR_NOT_FOUND);
74: puts("M7 VMM host tests PASS");
```

### Langkah 5 — Update Makefile

Menambahkan `HOSTCC`, `HOST_CFLAGS`, `VMM_CFLAGS`, target `check`, `$(BUILD_DIR)/vmm.o`, dan `$(BUILD_DIR)/test_vmm_host`. Memperluas `SRC_C` dari `find kernel` ke `find kernel src`. Menghapus baris eksplisit `pmm.o` dari `OBJ` yang menjadi duplikat setelah `SRC_C` diperluas.

```text
Masalah yang ditemukan: setelah SRC_C diperluas ke "find kernel src",
pmm.o muncul dua kali di linker command karena baris eksplisit pmm.o
di OBJ dari Makefile M6 masih ada.
Solusi: hapus baris eksplisit pmm.o dari OBJ, BP_OBJ, PANIC_OBJ.
```

Perintah perbaikan:

```bash
python3 -c "
content = open('Makefile').read()
content = content.replace(
    'SRC_C := \$(shell find kernel -name \'*.c\' | LC_ALL=C sort)',
    'SRC_C := \$(shell find kernel src -name \'*.c\' | LC_ALL=C sort)'
)
open('Makefile', 'w').write(content)
"
# Hapus baris eksplisit pmm.o dari OBJ
```

### Langkah 6 — Jalankan `make check`

```bash
make check
```

Output:

```text
clang --target=x86_64-unknown-none-elf ... -c src/vmm.c -o build/vmm.o
clang -std=c17 ... -DMCSOS_HOST_TEST ... src/vmm.c tests/test_vmm_host.c -o build/test_vmm_host
build/test_vmm_host
M7 VMM host tests PASS
nm -u build/vmm.o
objdump -dr build/vmm.o > build/vmm.objdump.txt
grep -q "invlpg" build/vmm.objdump.txt
grep -q "cr3" build/vmm.objdump.txt
[M7][PASS] make check lulus
```

### Langkah 7 — Buat Script Preflight dan Grading

```bash
cat > scripts/m7_preflight.sh << 'EOF' ...
cat > scripts/grade_m7.sh << 'EOF' ...
chmod +x scripts/m7_preflight.sh scripts/grade_m7.sh
./scripts/m7_preflight.sh
```

Output preflight:

```text
[OK] git, make, clang, ld.lld, readelf, objdump, nm, qemu-system-x86_64
[OK] semua file M6 ada (pmm.h, pmm.c, vmm.h, vmm.c, test_vmm_host.c)
[OK] dispatcher trap M4 terdeteksi
M7 VMM host tests PASS
[PASS] M7 preflight selesai.
```

### Langkah 8 — Commit Pertama M7

```bash
git add include/vmm.h src/vmm.c tests/test_vmm_host.c \
        scripts/m7_preflight.sh scripts/grade_m7.sh Makefile
git commit -m "M7: implement VMM core, host unit test, and audit scripts"
# → commit 0161a40
```

### Langkah 9 — Integrasi Kernel di `kmain.c`

Menambahkan `#include "vmm.h"`, variabel global `g_vmm` dan `g_hhdm_offset`, tiga adapter (alloc/free/phys_to_virt), fungsi `m7_vmm_init()` yang mengalokasikan root frame dari PMM, menginisialisasi VMM space, dan menjalankan smoke test map/query/unmap. Pemanggilan `m7_vmm_init()` ditambahkan setelah `m6_pmm_init_dummy()`.

### Langkah 10 — Build Kernel

```bash
make clean && make build
```

Output: 12 file dikompilasi (termasuk `src/vmm.c` dan `src/pmm.c`), link berhasil tanpa error.

### Langkah 11 — QEMU Smoke Test

```bash
tools/scripts/make_iso.sh 2>/dev/null
qemu-system-x86_64 -machine q35 -cpu max -m 256M \
    -cdrom build/mcsos.iso -boot d \
    -serial file:build/m7-qemu-serial.log \
    -display none -no-reboot -no-shutdown \
    -d int,cpu_reset,guest_errors \
    -D build/qemu-m7.log || true
cat build/m7-qemu-serial.log
```

Output serial (bagian M7):

```text
[M7] VMM core initialized
vmm_root_paddr=0x0000000000001000
[M7] VMM map/query/unmap smoke test passed
[M7] ready for QEMU smoke test and GDB audit
```

### Langkah 12 — Commit Integrasi dan Evidence

```bash
git add kernel/core/kmain.c Makefile
git commit -m "M7: integrate VMM into kernel, QEMU smoke test passed"
# → commit 382e235

mkdir -p evidence/M7
cp build/m7-qemu-serial.log evidence/M7/
# + copy dari build/evidence/
git add evidence/M7
git commit -m "M7: add evidence manifest, QEMU log, and audit artifacts"
# → commit 59ebb47
```

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| C1 Header VMM | `grep struct include/vmm.h` | `struct vmm_space`, `struct vmm_mapping` ada | `PASS` |
| C2 Compile object | `make build/vmm.o` | `build/vmm.o` ada tanpa error | `PASS` |
| C3 Host unit test | `make check` | `M7 VMM host tests PASS` | `PASS` |
| C4 Undefined symbol | `nm -u build/vmm.o` | Output kosong | `PASS` |
| C5 Disassembly audit | `objdump -dr build/vmm.o` | `invlpg` dan `cr3` ditemukan | `PASS` |
| C6 Kernel integration | `make clean && make build` | `build/kernel.elf` berhasil tanpa error | `PASS` |
| C7 QEMU smoke test | QEMU command M7 | Serial log `[M7] VMM core initialized` | `PASS` |
| C8 Evidence | `ls evidence/M7/` | Semua artefak ada termasuk manifest | `PASS` |

---

## 12. Perintah Uji dan Validasi

### 12.1 Host Unit Test

```bash
make check
```

Hasil:

```text
M7 VMM host tests PASS
nm -u build/vmm.o           # kosong
grep -q "invlpg" build/vmm.objdump.txt   # lulus
grep -q "cr3"   build/vmm.objdump.txt   # lulus
[M7][PASS] make check lulus
```

Status: `PASS`

### 12.2 Audit Object Freestanding

```bash
nm -u build/vmm.o
objdump -dr build/vmm.o | grep -n "invlpg\|cr3" | head -10
```

Hasil:

```text
[nm -u — kosong]

704: 9cd:  0f 01 38   invlpg (%rax)
711: 00000000000009e0 <vmm_read_cr3>:
715: 9e5:  0f 20 d8   mov %cr3,%rax
724: 00000000000000a00 <vmm_write_cr3>:
730: a0d:  0f 22 d8   mov %rax,%cr3
```

Status: `PASS`

### 12.3 Build Kernel dengan VMM

```bash
make clean && make build
```

Hasil: 12 file C + 1 file .S dikompilasi. `src/vmm.c` masuk ke `build/normal/src/vmm.o`. Link berhasil.

Status: `PASS`

### 12.4 QEMU Smoke Test

```bash
tools/scripts/make_iso.sh 2>/dev/null
qemu-system-x86_64 -machine q35 -cpu max -m 256M \
    -cdrom build/mcsos.iso -boot d \
    -serial file:build/m7-qemu-serial.log \
    -display none -no-reboot -no-shutdown || true
```

Hasil (serial log bagian M7):

```text
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
```

Status: `PASS`

### 12.5 Grading Lokal

```bash
./scripts/grade_m7.sh
```

Hasil:

```text
M7 VMM host tests PASS
[PASS] static grade M7 selesai
```

Status: `PASS`

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan

| No. | Uji | Expected | Actual | Status | Evidence |
|---|---|---|---|---|---|
| 1 | `vmm_is_canonical(0xFFFF800000200000)` | `true` | `true` | `PASS` | Host test assert lulus |
| 2 | `vmm_is_canonical(0x0000800000000000)` | `false` | `false` | `PASS` | Host test assert lulus |
| 3 | `vmm_map_page` halaman canonical + aligned | `VMM_MAP_OK` | `VMM_MAP_OK` | `PASS` | Host test assert lulus |
| 4 | `vmm_query_page` setelah map | paddr & flags benar | paddr == 0x300000, flags ada PRESENT+WRITABLE+NX | `PASS` | Host test assert lulus |
| 5 | `vmm_map_page` duplicate | `VMM_ERR_EXISTS` | `VMM_ERR_EXISTS` | `PASS` | Host test assert lulus |
| 6 | `vmm_map_page` paddr unaligned (0x400001) | `VMM_ERR_INVAL` | `VMM_ERR_INVAL` | `PASS` | Host test assert lulus |
| 7 | `vmm_map_page` vaddr noncanonical | `VMM_ERR_INVAL` | `VMM_ERR_INVAL` | `PASS` | Host test assert lulus |
| 8 | `vmm_unmap_page` setelah map | `VMM_MAP_OK` | `VMM_MAP_OK` | `PASS` | Host test assert lulus |
| 9 | `vmm_query_page` setelah unmap | `VMM_ERR_NOT_FOUND` | `VMM_ERR_NOT_FOUND` | `PASS` | Host test assert lulus |
| 10 | `vmm_unmap_page` pada yang sudah di-unmap | `VMM_ERR_NOT_FOUND` | `VMM_ERR_NOT_FOUND` | `PASS` | Host test assert lulus |
| 11 | `nm -u build/vmm.o` | kosong | kosong | `PASS` | `evidence/M7/m7_vmm_nm_undefined.txt` kosong |
| 12 | `invlpg` di disassembly | ada | ada di offset 0x9cd | `PASS` | `evidence/M7/m7_vmm_objdump.txt` baris 704 |
| 13 | `cr3` di disassembly | ada | ada di vmm_read_cr3 dan vmm_write_cr3 | `PASS` | `evidence/M7/m7_vmm_objdump.txt` baris 711–730 |
| 14 | Build kernel dengan vmm.c | berhasil tanpa error | berhasil | `PASS` | `make clean && make build` output |
| 15 | QEMU log `[M7] VMM core initialized` | ada | ada | `PASS` | `evidence/M7/m7-qemu-serial.log` |
| 16 | QEMU log `VMM map/query/unmap smoke test passed` | ada | ada | `PASS` | `evidence/M7/m7-qemu-serial.log` |
| 17 | Timer tick M5 masih berjalan setelah M7 | `ticks=...` muncul | muncul, bertahap dari 0x64 | `PASS` | `evidence/M7/m7-qemu-serial.log` |
| 18 | `grade_m7.sh` | `[PASS] static grade M7 selesai` | lulus | `PASS` | Terminal output |

### 13.2 Log Serial QEMU Lengkap (Ringkas)

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
[M5] boot: external interrupt bring-up start
[M5] PIC remapped
[M5] selftest: PIC mask invariants passed
[M5] PIT configured
[M5] sti: enabling interrupts
[M4] IDT and exception dispatch path installed
[M5] ready for QEMU smoke test and GDB audit
ticks=0x0000000000000064
ticks=0x00000000000000c8
... [berlanjut]
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| `m7-qemu-serial.log` | `evidence/M7/m7-qemu-serial.log` | Serial log boot QEMU penuh |
| `m7_make_check.log` | `evidence/M7/m7_make_check.log` | Output `make check` lengkap |
| `m7_vmm_nm_undefined.txt` | `evidence/M7/m7_vmm_nm_undefined.txt` | Bukti `nm -u` kosong |
| `m7_vmm_objdump.txt` | `evidence/M7/m7_vmm_objdump.txt` | Disassembly dengan `invlpg` dan `cr3` |
| `m7_vmm_readelf_header.txt` | `evidence/M7/m7_vmm_readelf_header.txt` | Header ELF vmm.o |
| `m7_vmm_readelf_sections.txt` | `evidence/M7/m7_vmm_readelf_sections.txt` | Section table vmm.o |
| `manifest.txt` | `evidence/M7/manifest.txt` | Manifest toolchain dan daftar file |

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

```text
Host unit test berhasil karena struktur vmm_space dengan function pointer
adapter memungkinkan mock physical memory (array statis 64 frame) dipakai
sebagai pengganti PMM hardware. Pendekatan ini membuktikan logika table walk
secara independen dari hardware sebelum menyentuh kernel sama sekali.

Audit freestanding lulus (nm -u kosong) karena vmm.c tidak memanggil
fungsi apapun dari libc. vmm_zero_page menggunakan loop manual, bukan
memset. Tidak ada printf, malloc, atau fungsi runtime lain.

Build kernel berhasil setelah dua bug Makefile diperbaiki: (1) SRC_C
harus mencakup src/ agar vmm.c ter-compile; (2) baris eksplisit pmm.o
di OBJ harus dihapus setelah SRC_C diperluas, agar pmm.o tidak muncul
dua kali di linker command.

QEMU smoke test berhasil karena adapter phys_to_virt menggunakan
g_hhdm_offset = 0 (identity map) yang valid selama bootloader Limine
masih menyediakan direct mapping. vmm_root_paddr = 0x1000 benar karena
PMM M6 mengalokasikan frame pertama yang tersedia (frame 1 = 0x1000,
karena frame 0 selalu di-reserved). Smoke test map/query/unmap di kmain
berhasil membuktikan bahwa page table yang dibuat VMM dapat di-walk
balik dengan benar.

Timer tick M5 masih berjalan setelah M7 membuktikan bahwa integrasi
VMM tidak merusak interrupt path, IDT, atau PIT yang sudah berjalan
di M5.
```

### 14.2 Analisis Kegagalan dan Bug yang Ditemukan

**Bug 1: `SRC_C` tidak mencakup `src/`**

```text
Gejala: link error "undefined symbol: vmm_space_init"
Penyebab: SRC_C := $(shell find kernel -name '*.c') hanya mencari di
  kernel/, tidak mencakup src/vmm.c
Perbaikan: SRC_C := $(shell find kernel src -name '*.c')
Deteksi: link error eksplisit dari ld.lld
```

**Bug 2: Duplicate symbol `pmm_*` saat link**

```text
Gejala: ld.lld error "duplicate symbol: pmm_zero_state" (dan 8 simbol
  lain dari pmm.c)
Penyebab: setelah SRC_C diperluas ke "find kernel src", pmm.c ter-include
  otomatis ke OBJ. Namun Makefile M6 sudah menambahkan pmm.o secara
  eksplisit di OBJ (baris 43, 46, 49), sehingga pmm.o muncul dua kali
  di linker command.
Perbaikan: hapus baris eksplisit pmm.o dari OBJ, BP_OBJ, PANIC_OBJ via
  Python string replace karena sed tidak dapat menangani karakter khusus
  Makefile.
Deteksi: link error eksplisit dari ld.lld
```

**Bug 3: `grade_m7.sh` tidak dapat menemukan `build/evidence/`**

```text
Gejala: "m7_make_check.log: No such file or directory"
Penyebab: urutan semula adalah mkdir -p build/evidence, lalu make clean
  (yang menghapus build/), lalu make check. Setelah make clean, folder
  build/evidence hilang sebelum tee dapat menulis ke dalamnya.
Perbaikan: pindahkan make clean ke baris pertama, mkdir -p build/evidence
  ke baris kedua, baru make check.
Deteksi: error saat cp artefak dari build/evidence yang sudah terhapus.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai? | Penjelasan |
|---|---|---|---|
| 4-level paging: 9-9-9-9-12 bit | `idx_pml4(vaddr >> 39)`, `idx_pdpt(vaddr >> 30)`, `idx_pd(vaddr >> 21)`, `idx_pt(vaddr >> 12)` semua `& 0x1FF` | Sesuai | Intel SDM Vol.3 Fig 4-8; dibuktikan lewat host test yang mengalokasikan intermediate table otomatis |
| Canonical 48-bit: bit 47 direplikasi ke 48–63 | `sign = (vaddr >> 47) & 1; upper = vaddr >> 48; sign ? upper == 0xFFFF : upper == 0` | Sesuai | Host test `0x0000800000000000` noncanonical (bit 47 = 0 tapi upper = 0x0001) ditolak |
| PTE bit 0 = Present, bit 1 = Writable, bit 63 = NX | Macro `VMM_PTE_PRESENT`, `VMM_PTE_WRITABLE`, `VMM_PTE_NO_EXECUTE` | Sesuai | Host test membuktikan flags terbaca kembali dari `vmm_query_page` |
| `invlpg` untuk invalidasi TLB per-halaman | `__asm__ volatile("invlpg (%0)" ...)` di `vmm_invalidate_page` | Sesuai | Disassembly menunjukkan opcode `0f 01 38` (invlpg) |
| CR3 menyimpan physical address PML4 | `vmm_read_cr3` dan `vmm_write_cr3` menggunakan `mov cr3` | Sesuai | Disassembly menunjukkan `mov %cr3,%rax` dan `mov %rax,%cr3` |
| Intermediate table baru harus di-zero | `vmm_zero_page()` sebelum `table[index] = ...` | Sesuai | Mencegah stale entry dari frame PMM yang mungkin berisi data lama |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Perbaikan |
|---|---|---|---|
| `undefined symbol: vmm_space_init` | Link error saat `make build` | `SRC_C` tidak mencakup `src/` | Perluas ke `find kernel src` |
| `duplicate symbol: pmm_zero_state` | Link error, 9 simbol duplikat | `pmm.o` muncul dua kali di OBJ setelah SRC_C diperluas | Hapus baris eksplisit `pmm.o` dari OBJ |
| `grade_m7.sh: No such file or directory` | Script gagal copy artefak | Urutan `make clean` sebelum `mkdir -p build/evidence` | Pindahkan `make clean` ke baris pertama script |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Gejala | Penyebab | Mitigasi |
|---|---|---|---|
| Triple fault setelah `vmm_write_cr3` | QEMU reset tanpa log | Mapping kernel/stack/IDT belum lengkap saat CR3 diaktifkan | Jangan panggil `vmm_write_cr3` pada tugas wajib M7 |
| QEMU reset setelah `vmm_map_page` di kernel | Triple fault atau #PF | Frame yang dialokasikan PMM menimpa region aktif (kernel, stack, IDT) | PMM M7 memakai dummy map yang tidak sepenuhnya tahu lokasi fisik kernel; risiko overlap ada |
| Stale TLB setelah unmap | Akses ke vaddr yang sudah di-unmap masih berhasil | `invlpg` tidak dipanggil atau dipanggil dengan alamat salah | Selalu panggil `vmm_invalidate_page(vaddr)` setelah zeroing PTE |
| Huge bit di intermediate table | `VMM_ERR_EXISTS` atau `VMM_ERR_NOT_FOUND` tidak terduga | Bootloader atau page table lama menggunakan huge page di level PD/PDPT | M7 hanya mengelola table buatan sendiri; jangan parse bootloader mapping sebagai 4 KiB table |
| Null adapter `phys_to_virt` | Segfault atau #PF di host test | Lupa mengisi field di `vmm_space_init` | `vmm_space_init` menolak `phys_to_virt == 0` |

### 15.3 Triage yang Dilakukan

```text
Bug link error "undefined symbol" dan "duplicate symbol" keduanya
terdeteksi langsung dari output ld.lld yang eksplisit menyebutkan simbol
dan file sumber. Triage dilakukan dengan membaca linker command yang
dicetak Makefile: ditemukan vmm.o tidak ada (karena src/ tidak di-scan)
dan pmm.o muncul dua kali (karena baris eksplisit + auto-scan).
```

### 15.4 Page Fault Diagnostics (Rancangan Integrasi)

```text
Handler page fault (#PF = vector 14) di dispatcher M4 (kernel/core/trap.c)
dapat menggunakan vmm_read_cr2() untuk membaca fault address. Error code
page fault berisi bit:
  bit 0 (P):    0 = page not present, 1 = protection violation
  bit 1 (W/R):  0 = read, 1 = write
  bit 2 (U/S):  0 = supervisor, 1 = user mode
  bit 3 (RSVD): reserved bit di PTE aktif
  bit 4 (I/D):  1 = instruction fetch

Rancangan log page fault minimal:
  void page_fault_handler(x86_64_trap_frame_t *frame) {
      uint64_t cr2 = vmm_read_cr2();
      log_key_value_hex64("pf_cr2",   cr2);
      log_key_value_hex64("pf_error", frame->error_code);
      log_key_value_hex64("pf_rip",   frame->rip);
      KERNEL_PANIC("page fault", cr2);
  }

Integrasi ini belum dilakukan pada M7 karena tidak ada uji page fault
terkendali yang dijadwalkan. vmm_read_cr2() sudah tersedia dan dapat
dipanggil dari dispatcher saat vector == 14.
```

---

## 16. Prosedur Rollback

| Skenario | Perintah | Status |
|---|---|---|
| Kembali ke baseline M6 | `git switch praktikum/m6-pmm` | Belum diuji aktual; branch M6 masih ada di `be7a196` |
| Hapus file M7 saja | `git restore include/vmm.h src/vmm.c tests/test_vmm_host.c scripts/m7_preflight.sh scripts/grade_m7.sh Makefile kernel/core/kmain.c` | Belum diuji aktual |
| Bersihkan artefak build | `make clean` | Teruji |
| Build ulang M6 setelah rollback | `make clean && make build && tools/scripts/make_iso.sh` | Belum diuji aktual setelah rollback |

Catatan rollback:

```text
Branch M7 (m7-vmm-core) terpisah dari branch M6 (praktikum/m6-pmm).
Rollback ke M6 dapat dilakukan dengan git switch ke branch atau commit
be7a196 tanpa risiko kehilangan data M7 yang sudah dikomit. Rollback
belum diuji aktual karena M7 berhasil tanpa perlu rollback.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Level | Mitigasi pada M7 | Status |
|---|---|---|---|
| `VMM_PTE_USER` tidak di-clear untuk halaman kernel | Sedang | Caller mengontrol flags; VMM hanya menerima flags dari `allowed` mask | Mitigasi sebagian; W^X/NX enforcement belum ada |
| Tidak ada `VMM_PTE_NO_EXECUTE` enforcement | Sedang | Caller dapat menyertakan NX; VMM tidak memaksa | Pengayaan; documented known issue |
| HHDM offset = 0 diasumsikan tanpa validasi | Tinggi | Hanya valid selama bootloader identity mapping aktif; tidak dipanggil `vmm_write_cr3` | Documented; tidak berisiko selama CR3 tidak diganti |
| Pointer kernel dicetak ke serial log (`vmm_root_paddr`) | Rendah (QEMU lab) | Tidak ada redaksi | Tidak relevan pada kernel pendidikan di QEMU |
| Tidak ada proteksi overlap frame | Sedang | PMM M6 mengelola frame yang sudah digunakan; VMM meminta frame baru via alloc_frame | Risiko overlap jika PMM map dummy tidak tepat melingkup region aktif |

### 17.2 Reliability

| Aspek | Status M7 | Catatan |
|---|---|---|
| Double-map protection | ✅ `VMM_ERR_EXISTS` | Terverifikasi host test |
| Null pointer check | ✅ `space == 0`, `out == 0`, `phys_to_virt == 0` | Terverifikasi review kode |
| Overflow physical address | ✅ `vmm_is_aligned_4k` membuang bit bawah | Frame selalu aligned |
| Stale TLB setelah unmap | ✅ `invlpg` dipanggil | Terverifikasi disassembly |
| Zero intermediate table | ✅ `vmm_zero_page` sebelum entry dipasang | Mencegah stale PTE |
| Build freestanding | ✅ `nm -u` kosong | Terverifikasi evidence |

---

## 18. Pembagian Kerja

Tidak berlaku — praktikum dikerjakan secara individu.

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Repository dapat dibangun dari clean checkout | `PASS` | `make clean && make build` berhasil |
| Perintah build dan test terdokumentasi | `PASS` | Bagian 10 dan 12 laporan |
| `make check` lulus | `PASS` | `evidence/M7/m7_make_check.log` |
| `tests/test_vmm_host.c` semua assertion lulus | `PASS` | `M7 VMM host tests PASS` |
| `nm -u build/vmm.o` kosong | `PASS` | `evidence/M7/m7_vmm_nm_undefined.txt` kosong |
| `objdump` menunjukkan `invlpg` dan akses CR3 | `PASS` | `evidence/M7/m7_vmm_objdump.txt` baris 704, 715, 730 |
| API VMM memiliki error path eksplisit | `PASS` | `VMM_ERR_INVAL`, `VMM_ERR_EXISTS`, `VMM_ERR_NOT_FOUND`, `VMM_ERR_NOMEM` |
| VMM menolak noncanonical dan unaligned | `PASS` | Host test lulus; kode diverifikasi |
| Kernel mencetak `M7 VMM core initialized` di QEMU | `PASS` | `evidence/M7/m7-qemu-serial.log` |
| Tidak ada warning kritis pada build | `PASS` | `-Werror` aktif; build lulus |
| Perubahan Git terkomit | `PASS` | Commit `59ebb47` |
| Laporan berisi log, test, disassembly evidence | `PASS` | Bagian 12, 13, Lampiran |

| Kriteria pengayaan | Status | Catatan |
|---|---|---|
| Aktivasi CR3 baru | `NA` | Tidak dilakukan; berisiko triple fault tanpa mapping lengkap |
| NX policy enforcement | `NA` | Flag NX tersedia di API; enforcement belum diimplementasikan |
| W^X policy | `NA` | Belum ada; disebutkan sebagai known issue |
| TLB shootdown design | `NA` | SMP belum ada di M7 |

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
Build bersih untuk semua target (vmm.o freestanding, test_vmm_host host,
kernel.elf kernel). Host unit test lulus semua 10 assertion. nm -u kosong.
Disassembly membuktikan invlpg dan akses cr3. Build kernel berhasil dengan
vmm.c terintegrasi. QEMU smoke test menampilkan [M7] VMM core initialized
dan map/query/unmap smoke test passed. Timer tick M5 masih berjalan normal
setelah integrasi VMM.

Status siap demonstrasi belum diklaim karena: (1) rollback belum diuji
aktual; (2) integrasi page fault diagnostics (vmm_read_cr2 di handler
vector 14) belum dilakukan; (3) HHDM offset masih placeholder 0 bukan
nilai Limine asli; (4) W^X enforcement belum ada.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | HHDM offset = 0, bukan nilai Limine asli | Tidak portabel ke sistem dengan HHDM offset berbeda | Hanya valid selama bootloader identity mapping aktif; jangan panggil `write_cr3` | M8+ saat Limine HhdmRequest diimplementasikan |
| 2 | Memory map PMM masih dummy, bukan Limine asli | Frame yang dialokasikan VMM mungkin overlap dengan region kernel aktif | Test smoke map/query/unmap pada alamat yang tidak dipakai kernel | M8+ bersama integrasi Limine penuh |
| 3 | Page fault diagnostics belum diintegrasikan ke dispatcher M4 | `vmm_read_cr2` tersedia tapi tidak dipanggil dari trap handler vector 14 | Handler vector 14 masih memanggil `KERNEL_PANIC` tanpa log CR2 | Sebelum demonstrasi M7 |
| 4 | Rollback belum diuji aktual | Risiko M6 tidak berjalan setelah rollback dari M7 | Branch terpisah memungkinkan rollback via `git switch` | Sebelum demonstrasi M7 |
| 5 | W^X dan NX enforcement tidak ada | Semua halaman dapat executable dan writable jika caller tidak menyetel flag NX | API tersedia; caller dapat menyetel `VMM_PTE_NO_EXECUTE` secara manual | M8+ (security hardening milestone) |
| 6 | `vmm_write_cr3` tidak pernah dipanggil | Page table baru tidak aktif; mapping VMM tidak digunakan oleh CPU | Hanya untuk demonstrasi logika VMM; aktivasi CR3 membutuhkan mapping lengkap | Pengayaan, bukan wajib |

Keputusan akhir:

```text
Berdasarkan bukti build (make check lulus, nm -u kosong, invlpg dan cr3
di disassembly), QEMU serial log yang menunjukkan [M7] VMM core initialized
dan map/query/unmap smoke test passed, serta seluruh 8 checkpoint terpenuhi,
hasil praktikum M7 ini layak disebut siap uji QEMU untuk VMM awal.
M7 tidak mengklaim siap demonstrasi praktikum karena rollback belum diuji,
page fault diagnostics belum diintegrasikan, HHDM offset masih placeholder,
dan W^X enforcement belum ada.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---:|---:|---|---:|
| Kebenaran fungsional | 30 | API VMM bekerja, host test lulus, map/query/unmap benar, canonical/alignment ditolak benar | `28` |
| Kualitas desain dan invariants | 20 | Kontrak VMM jelas, ownership frame jelas, adapter phys_to_virt eksplisit, invariant I1–I9 terdokumentasi | `18` |
| Pengujian dan bukti | 20 | `make check`, `nm -u`, `objdump`, QEMU log, evidence manifest lengkap | `18` |
| Debugging dan failure analysis | 10 | Failure modes ditemukan dan diperbaiki, triage dijelaskan, page fault path dirancang | `9` |
| Keamanan dan robustness | 10 | W^X/NX dibahas, user bit dijelaskan, reserved bit dimask, CR3 activation risk dicatat | `8` |
| Dokumentasi dan laporan | 10 | Laporan rapi, reproducible, referensi IEEE, commit hash, evidence lengkap | `9` |
| **Total** | **100** | | `90` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Kesimpulan

### 22.1 Yang Berhasil

```text
Seluruh komponen wajib M7 berhasil:
- include/vmm.h: API lengkap dengan flags PTE, struct vmm_space dan
  vmm_mapping, deklarasi seluruh fungsi, kode error eksplisit.
- src/vmm.c: implementasi table walk 4-level, canonical check,
  alignment check, get_or_alloc_next_table, map/query/unmap, primitive
  assembly dengan dual-compile pattern.
- tests/test_vmm_host.c: 10 assertion mencakup semua jalur penting lulus.
- Audit freestanding: nm -u kosong, invlpg dan cr3 ada di disassembly.
- Build kernel: src/vmm.c berhasil diintegrasikan ke kernel ELF64.
- QEMU smoke test: [M7] VMM core initialized, vmm_root_paddr=0x1000,
  map/query/unmap smoke test passed, timer M5 tetap berjalan.
- Evidence: 7 file + manifest terkumpul di evidence/M7.
- 3 commit bersih di branch m7-vmm-core.
- 2 bug Makefile dan 1 bug urutan script ditemukan dan diperbaiki.
```

### 22.2 Yang Belum Berhasil

```text
- Page fault diagnostics (vmm_read_cr2 di handler vector 14) belum
  diintegrasikan ke dispatcher trap.c — hanya tersedia di API.
- HHDM offset masih placeholder 0; belum menggunakan nilai Limine asli
  dari HhdmRequest.
- Aktivasi CR3 baru (vmm_write_cr3) tidak dilakukan — sesuai panduan,
  ini membutuhkan mapping kernel/stack/IDT/serial lengkap terlebih dulu.
- W^X enforcement tidak ada; semua halaman yang dipetakan berpotensi
  executable jika caller tidak menyetel NX.
- Rollback belum diuji aktual.
```

### 22.3 Rencana Perbaikan

```text
Sebelum demonstrasi M7:
1. Integrasikan vmm_read_cr2() ke dispatcher vector 14 di trap.c,
   tambahkan log CR2/error_code/RIP/RSP untuk page fault.
2. Uji rollback aktual ke commit be7a196 dan verifikasi M6 masih jalan.

Untuk M8+:
1. Implementasikan Limine HhdmRequest untuk mendapatkan HHDM offset asli,
   agar phys_to_virt tidak bergantung pada identity mapping bootloader.
2. Implementasikan Limine MemoryMapRequest untuk mengganti dummy PMM map
   dengan data firmware asli.
3. Setelah HHDM dan PMM asli tersedia, lengkapi mapping kernel/stack/IDT/
   serial, lalu uji aktivasi CR3 baru dengan rollback yang jelas.
4. Pertimbangkan NX enforcement untuk region data kernel pada milestone
   security hardening.
```

---

## 23. Lampiran

### Lampiran A — Commit Log

```text
59ebb47 (HEAD -> m7-vmm-core) M7: add evidence manifest, QEMU log, and audit artifacts
382e235 M7: integrate VMM into kernel, QEMU smoke test passed
0161a40 M7: implement VMM core, host unit test, and audit scripts
be7a196 (praktikum/m6-pmm) M6: implement bitmap PMM, host unit test, and kernel integration
08e1d4f (praktikum/m5-timer-irq) M5: implement PIC remap, PIT 100Hz timer, and IRQ dispatch
```

### Lampiran B — Diff Ringkas

```text
File baru pada commit M7 (0161a40, 382e235, 59ebb47):
  include/vmm.h               — 65 baris header API VMM
  src/vmm.c                   — 188 baris implementasi VMM core
  tests/test_vmm_host.c       — 78 baris host unit test
  scripts/m7_preflight.sh     — 50 baris script preflight
  scripts/grade_m7.sh         — 22 baris script grading
  evidence/M7/                — 7 file artefak + manifest

File yang diubah:
  kernel/core/kmain.c         — +76 baris: include vmm.h, g_vmm, g_hhdm_offset,
                                adapter alloc/free/phys_to_virt, m7_vmm_init()
  Makefile                    — SRC_C diperluas ke "find kernel src",
                                hapus eksplisit pmm.o, tambah target check/vmm/test_vmm_host
```

### Lampiran C — Log Build Lengkap (Ringkas)

```text
[make clean && make build]
clang --target=x86_64-unknown-none-elf ... -c kernel/arch/x86_64/idt.c
clang --target=x86_64-unknown-none-elf ... -c kernel/arch/x86_64/pic.c
clang --target=x86_64-unknown-none-elf ... -c kernel/arch/x86_64/pit.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/kmain.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/log.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/panic.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/serial.c
clang --target=x86_64-unknown-none-elf ... -c kernel/core/trap.c
clang --target=x86_64-unknown-none-elf ... -c kernel/lib/memory.c
clang --target=x86_64-unknown-none-elf ... -c src/pmm.c
clang --target=x86_64-unknown-none-elf ... -c src/vmm.c
clang --target=x86_64-unknown-none-elf ... -c kernel/arch/x86_64/isr.S
ld.lld -nostdlib -static ... -o build/kernel.elf [12 object files]
[Tidak ada error/warning]
```

### Lampiran D — Log QEMU Penuh (Bagian M7)

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
[M5] boot: external interrupt bring-up start
pic_master_offset=0x0000000000000020
pic_slave_offset=0x0000000000000028
[M5] PIC remapped
[M5] selftest: PIC mask invariants passed
pit_hz=0x0000000000000064
pit_divisor=0x0000000000002e9b
[M5] PIT configured
[M5] sti: enabling interrupts
[M4] IDT and exception dispatch path installed
[M5] ready for QEMU smoke test and GDB audit
ticks=0x0000000000000064
ticks=0x00000000000000c8
ticks=0x000000000000012c
ticks=0x0000000000000190
ticks=0x00000000000001f4
... [berlanjut]
```

### Lampiran E — Output nm dan Disassembly Penting

```text
[nm -u build/vmm.o — kosong]

[objdump -dr build/vmm.o | grep -n "invlpg|cr3"]
704: 9cd:       0f 01 38                invlpg (%rax)
711:00000000000009e0 <vmm_read_cr3>:
715: 9e5:       0f 20 d8                mov    %cr3,%rax
724:0000000000000a00 <vmm_write_cr3>:
730: a0d:       0f 22 d8                mov    %rax,%cr3
```

### Lampiran F — Isi `evidence/M7/manifest.txt`

```text
MCSOS M7 evidence manifest
timestamp_utc=2026-06-20T21:18:14Z
commit=382e235ace5a1266477fbc032d780ab2d9eecb08
clang=Ubuntu clang version 21.1.8 (6ubuntu1)
lld=Ubuntu LLD 21.1.8 (compatible with GNU linkers)
qemu=QEMU emulator version 10.2.1 (Debian 1:10.2.1+ds-1ubuntu3)
m7-qemu-serial.log
m7_make_check.log
m7_vmm_nm_undefined.txt
m7_vmm_objdump.txt
m7_vmm_readelf_header.txt
m7_vmm_readelf_sections.txt
manifest.txt
```

### Lampiran G — Jawaban Pertanyaan Analisis

**1. Mengapa `root_paddr` harus berupa alamat fisik, bukan virtual?**
CR3 menyimpan physical address basis PML4. Ketika CPU melakukan page-table walk, ia membaca PML4 menggunakan physical address dari CR3 sebelum paging dapat digunakan untuk mentranslasikan alamat. Jika root_paddr adalah virtual address, CPU tidak dapat menemukan PML4 karena belum ada translasi yang aktif untuk alamat tersebut.

**2. Mengapa kernel membutuhkan HHDM/direct map untuk mengedit page table?**
Page table dialokasikan sebagai frame fisik oleh PMM. CPU mengakses memori melalui virtual address, bukan physical address. Untuk menulis entry ke frame fisik yang berisi page table, kernel membutuhkan virtual address yang terpetakan ke frame fisik tersebut. HHDM menyediakan mapping tersebut: `virtual = hhdm_offset + physical`. Tanpa HHDM, kernel tidak memiliki virtual address yang valid untuk mengakses frame page table.

**3. Apa risiko jika `vmm_map_page()` mengizinkan remap diam-diam?**
Remap diam-diam akan menimpa PTE yang ada tanpa memberi tahu caller. Ini dapat menyebabkan: (1) frame fisik yang sebelumnya terpetakan menjadi "bocor" (tidak bisa dibebaskan karena caller lama tidak tahu frame sudah tidak terpetakan); (2) dua virtual address berbeda tiba-tiba menunjuk ke frame fisik yang sama; (3) bug pemetaan sulit dideteksi. `VMM_ERR_EXISTS` memaksa caller untuk eksplisit menangani kasus remap.

**4. Mengapa `invlpg` dipanggil setelah unmap?**
TLB menyimpan cache translasi virtual-physical. Setelah PTE di-zero (unmap), TLB mungkin masih menyimpan translasi lama. Tanpa `invlpg`, akses ke virtual address yang sudah di-unmap dapat berhasil karena CPU menggunakan translasi dari TLB, bukan dari page table yang sudah diperbarui. `invlpg` mengusir entry TLB untuk alamat tersebut agar CPU dipaksa membaca ulang dari page table.

**5. Mengapa huge page tidak dipakai pada tugas wajib M7?**
Huge page (2 MiB di PD, 1 GiB di PDPT) menggunakan bit 7 (PS) di entry intermediate table. Jika kernel salah menginterpretasi huge page sebagai pointer ke next-level table, ia akan menulis entry page table ke offset dalam halaman data yang besar tersebut, merusak data. M7 hanya membangun fondasi paging; huge page membutuhkan penanganan khusus dan deteksi bit PS yang belum diimplementasikan.

**6. Perbedaan page fault non-present vs protection violation?**
Non-present: bit P = 0 di PTE; page belum dipetakan sama sekali. Biasanya terjadi karena akses ke region yang belum di-map atau sudah di-unmap. Protection violation: bit P = 1 di PTE; page ada tapi hak akses dilanggar (misalnya write ke halaman read-only, atau user mode mengakses halaman supervisor-only). Error code bit 0 (P) membedakan keduanya.

**7. Mengapa `write_cr3()` berisiko jika mapping kernel stack belum lengkap?**
Saat `write_cr3()` dipanggil, CPU mulai menggunakan page table baru untuk semua translasi virtual address. Jika stack kernel tidak dipetakan di page table baru, instruksi berikutnya yang menyebabkan stack access (seperti `push`, `call`, atau exception handler) akan memicu #SS (stack fault) atau #PF. Karena exception handler sendiri membutuhkan stack yang valid, ini menyebabkan double fault, lalu triple fault, dan CPU reset.

**8. Konsekuensi security jika semua halaman kernel writable dan executable?**
W+X (writable dan executable secara bersamaan) memungkinkan attacker yang dapat menulis ke memori kernel (misalnya via bug buffer overflow) untuk menyisipkan shellcode ke region data kernel lalu mengeksekusinya. W^X policy memisahkan: region text hanya executable (tidak writable), region data hanya writable (tidak executable), sehingga menyisipkan kode ke data tidak membantu karena data tidak dapat dieksekusi.

**9. Perubahan desain M7 untuk SMP dan TLB shootdown?**
Pada SMP, setiap CPU memiliki TLB sendiri. Ketika satu CPU memodifikasi atau menghapus mapping, CPU lain mungkin masih menyimpan translasi lama di TLB mereka. TLB shootdown adalah mekanisme untuk memberitahu semua CPU agar mengusir entry TLB yang sudah tidak valid, biasanya via Inter-Processor Interrupt (IPI). Desain M7 harus ditambahkan: (1) lock/mutex untuk VMM space agar tidak ada race condition saat modifikasi page table; (2) fungsi TLB shootdown yang mengirim IPI ke semua CPU aktif setelah unmap.

**10. Mengapa host unit test tidak cukup membuktikan paging hardware benar?**
Host unit test menggunakan mock memory (array statis) dan mock allocator yang berjalan di atas Linux host dengan virtual memory Linux. Test tidak menyentuh hardware page table walk x86_64 sama sekali; semua pointer dalam test adalah virtual address Linux yang sudah valid. Paging hardware nyata melibatkan CR3, CPU cache (TLB), timing hardware, interaksi dengan bootloader page table, dan edge case seperti huge page dari firmware yang tidak muncul dalam mock. Hanya QEMU/hardware yang dapat membuktikan page table berjalan benar di CPU x86_64 nyata.

---

## 24. Daftar Referensi

```text
[1] Intel Corporation, "Intel 64 and IA-32 Architectures Software Developer's
    Manual, Volume 3: System Programming Guide," Intel Developer Documentation,
    2026. [Online]. Available:
    https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
    Accessed: Jun. 20, 2026.

[2] Advanced Micro Devices, "AMD64 Architecture Programmer's Manual Volume 2:
    System Programming," AMD, Rev. 3.44, Mar. 2026. [Online]. Available:
    https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2
    Accessed: Jun. 20, 2026.

[3] QEMU Project, "GDB usage — QEMU documentation," QEMU System Emulation
    Documentation, 2026. [Online]. Available:
    https://qemu.eu/doc/6.0/system/gdb.html
    Accessed: Jun. 20, 2026.

[4] Limine Bootloader Organization, "Limine," GitHub repository and bootloader
    documentation, 2026. [Online]. Available:
    https://github.com/limine-bootloader
    Accessed: Jun. 20, 2026.

[5] limine crate documentation, "MemoryMapRequest," docs.rs, 2026. [Online].
    Available:
    https://docs.rs/limine/latest/limine/request/struct.MemoryMapRequest.html
    Accessed: Jun. 20, 2026.

[6] limine-protocol crate documentation, "HHDMRequest," docs.rs, 2026.
    [Online]. Available:
    https://docs.rs/limine-protocol/latest/limine_protocol/struct.HHDMRequest.html
    Accessed: Jun. 20, 2026.

[7] Free Software Foundation, "Using LD, the GNU linker — Scripts," GNU
    Binutils Documentation, 2026. [Online]. Available:
    https://ftp.gnu.org/old-gnu/Manuals/ld/html_node/ld_6.html
    Accessed: Jun. 20, 2026.

[8] LLVM Project, "Linker Script implementation notes and policy — LLD
    documentation," LLVM, 2026. [Online]. Available:
    https://lld.llvm.org/ELF/linker_script.html
    Accessed: Jun. 20, 2026.

[9] LLVM Project, "Clang command line argument reference," LLVM, 2026.
    [Online]. Available:
    https://clang.llvm.org/docs/ClangCommandLineReference.html
    Accessed: Jun. 20, 2026.
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
| Artefak penting tersedia di `evidence/M7` | `Ya` |
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
59ebb47
```

Status akhir yang diklaim:

```text
Siap uji QEMU untuk VMM awal — bukan siap produksi, dengan known issues
pada bagian 20 yang harus ditindaklanjuti (HHDM offset masih placeholder,
PMM map dummy, page fault diagnostics belum diintegrasikan ke dispatcher,
rollback belum diuji aktual, CR3 activation tidak dilakukan, W^X
enforcement tidak ada).
```

Ringkasan satu paragraf:

```text
Praktikum M7 MCSOS 260502 telah diselesaikan oleh Agung Nurjaman
(25832073010) secara individu, melanjutkan dari gate M6 yang solid (commit
be7a196). Virtual Memory Manager berbasis page table 4-level x86_64 berhasil
dibangun lengkap: validasi canonical 48-bit, validasi alignment 4 KiB,
table walk PML4→PDPT→PD→PT, alokasi intermediate table otomatis dari PMM,
pencegahan remap diam-diam (VMM_ERR_EXISTS), TLB invalidation via invlpg,
dan adapter phys_to_virt yang eksplisit, seluruhnya diverifikasi lewat host
unit test yang lulus 10 assertion tanpa QEMU. Audit freestanding membuktikan
nm -u kosong dan disassembly memuat invlpg serta akses CR3. Integrasi kernel
berhasil setelah memperbaiki dua bug Makefile (SRC_C tidak mencakup src/ dan
duplicate pmm.o di OBJ), dengan log QEMU menampilkan [M7] VMM core
initialized, vmm_root_paddr=0x1000, dan VMM map/query/unmap smoke test
passed, sementara timer tick M5 tetap berjalan normal setelahnya. Tiga
commit bersih (0161a40, 382e235, 59ebb47) tersimpan di branch m7-vmm-core
dengan 7 artefak evidence termasuk manifest toolchain. Status readiness yang
diklaim adalah siap uji QEMU untuk VMM awal, bukan siap produksi, dengan
enam known issues yang didokumentasikan secara eksplisit sebagai batasan yang
harus ditindaklanjuti sebelum milestone VMM penuh dan aktivasi CR3 baru.
```
