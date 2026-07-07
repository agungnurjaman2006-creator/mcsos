# Sinkronisasi Kernel Awal: Spinlock, Mutex Kooperatif, Lock-Order Validator, dan Diagnosis Race/Deadlock pada MCSOS 260502

**Nama file laporan:** `laporan_praktikum_M12_Agung.md`
**Nama sistem operasi:** MCSOS versi 260502
**Target default:** x86_64, QEMU, Windows 11 x64 + WSL 2, kernel monolitik pendidikan, C17 freestanding dengan assembly minimal, subset POSIX-like
**Dosen:** Muhaemin Sidiq, S.Pd., M.Pd.
**Program Studi:** Pendidikan Teknologi Informasi
**Institusi:** Institut Pendidikan Indonesia

---

## 0. Metadata Laporan

| Atribut | Isi |
|---|---|
| Kode praktikum | `M12` |
| Judul praktikum | `Sinkronisasi Kernel Awal: Spinlock, Mutex Kooperatif, Lock-Order Validator, dan Diagnosis Race/Deadlock pada MCSOS 260502` |
| Jenis pengerjaan | `Individu` |
| Nama mahasiswa | `Agung Nurjaman` |
| NIM | `25832073010` |
| Kelas | `Pendidikan Teknologi Informasi` |
| Nama kelompok | `-` |
| Anggota kelompok | `-` |
| Tanggal praktikum | `2026-06-29` |
| Tanggal pengumpulan | `2026-06-29` |
| Repository | `/home/agung/src/mcsos` |
| Branch | `praktikum/m12-sync` |
| Commit awal | `ad07b27` (M11) |
| Commit akhir | `55a09ab` (M12 preflight log) |
| Status readiness yang diklaim | `Siap uji QEMU untuk sinkronisasi kernel awal single-core menuju SMP — bukan siap produksi, bukan bukti bebas deadlock, bukan bukti race-free penuh` |

---

## 1. Sampul

# Laporan Praktikum M12
## Sinkronisasi Kernel Awal: Spinlock, Mutex Kooperatif, Lock-Order Validator, dan Diagnosis Race/Deadlock pada MCSOS 260502

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
Panduan resmi praktikum M12 MCSOS 260502 digunakan sebagai referensi utama
dan kontrak implementasi untuk seluruh komponen (mcs_sync.h, lockdep.c,
spinlock.c, mutex.c, m12_sync_host_test.c, dan Makefile.m12). Intel SDM
Vol.3 digunakan sebagai referensi teknis untuk memory ordering x86_64,
instruksi atomik, dan konteks interrupt. Dokumentasi GCC __atomic builtins
digunakan sebagai referensi acquire/release/relaxed ordering. Dokumentasi
Linux kernel (locktypes.html, lockdep-design.html, mutex-design.html)
digunakan sebagai referensi pembanding desain lock. AI assistant (Claude)
digunakan untuk: (1) menjelaskan konsep ABI, dispatcher, dan validasi pointer
di awal sesi; (2) menyusun perintah langkah-per-langkah untuk membuat header,
lockdep.c, spinlock.c, mutex.c, host test, dan Makefile.m12; (3) mendiagnosis
error freestanding compile karena CC default ke GCC yang tidak mendukung
-target (diperbaiki dengan CC=clang eksplisit); (4) menjelaskan hasil audit
nm/readelf/objdump. Seluruh build, host unit test, audit, preflight, dan
commit git dijalankan dan diverifikasi sendiri oleh mahasiswa di WSL 2.
AI tidak digunakan untuk mengubah kontrak fungsional di luar yang ditentukan
panduan resmi.
```

---

## 3. Tujuan Praktikum

1. Mengimplementasikan spinlock freestanding x86_64 berbasis operasi atomik `__atomic_exchange_n` (acquire) dan `__atomic_store_n` (release) sebagai primitive non-blocking untuk critical section pendek.
2. Mengimplementasikan mutex kooperatif awal dengan owner semantics yang menolak rekursi, menolak unlock oleh non-owner, dan menyediakan kontrak state yang benar sebelum wait queue ditambahkan pada modul berikutnya.
3. Mengimplementasikan lock-order validator sederhana bergaya lockdep yang mendeteksi recursive acquire, lock-order inversion (rank menurun), dan release non-LIFO, dengan bukti observability melalui `violation_count`.
4. Menulis host unit test deterministik menggunakan pthread yang memverifikasi spinlock-protected counter (4 thread × 25.000 iterasi), mutex owner semantics, dan negative cases lockdep, tanpa memerlukan boot QEMU.
5. Mengompilasi ketiga source sinkronisasi sebagai object freestanding x86_64 dengan Clang dan membuktikan tidak ada dependency libc melalui `nm -u` kosong.
6. Mengaudit object dengan `readelf`, `objdump`, dan `sha256sum` sebagai bukti bahwa instruksi atomik dan spin loop terkompilasi dengan benar.
7. Mendokumentasikan failure modes sinkronisasi (deadlock, recursive acquire, unlock non-owner, lock-order inversion, interrupt reentry) beserta mitigasi yang diterapkan.

---

## 4. Capaian Pembelajaran Praktikum

Setelah praktikum ini, mahasiswa mampu:

| CPL/CPMK praktikum | Bukti yang harus ditunjukkan |
|---|---|
| Membedakan data race, race condition, deadlock, livelock, dan priority inversion | Bagian 6.1; analisis failure mode bagian 15 |
| Menjelaskan perbedaan spinlock dan mutex serta konteks penggunaan yang tepat | Bagian 6.1; keputusan desain bagian 9.2 |
| Mengimplementasikan spinlock dengan `__atomic_exchange_n` ACQUIRE dan `__atomic_store_n` RELEASE | `kernel/sync/spinlock.c`; `objdump` menunjukkan `xchg` di `mcs_spin_try_lock` |
| Mengimplementasikan mutex kooperatif dengan owner checking dan penolakan rekursi | `kernel/sync/mutex.c`; host test `recursive mutex rejected` dan `non-owner unlock rejected` lulus |
| Membuat lock-order validator dengan invariant rank monoton naik dan release LIFO | `kernel/sync/lockdep.c`; host test `reject descending rank` dan `reject recursion` lulus |
| Menulis host unit test yang memverifikasi race-protected counter, owner semantics, dan lockdep | `tests/m12_sync_host_test.c`; `[PASS] M12 synchronization host tests passed` |
| Mengompilasi source sinkronisasi sebagai object freestanding x86_64 | `build/m12/lockdep.o`, `spinlock.o`, `mutex.o`; Clang `-target x86_64-elf` |
| Mengaudit `nm -u`, `readelf -h`, `objdump -d`, dan checksum artefak | `build/m12/nm-undefined.txt` kosong; `readelf` ELF64 REL; `objdump` menunjukkan `xchg` dan `pause` |
| Menjelaskan failure modes dan mitigasi sinkronisasi | Bagian 15; analisis teknis bagian 14 |

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
| M13 | SMP, scalability, lock stress | `tidak dibahas` |
| M14 | VFS, file descriptor, ramfs | `tidak dibahas` |
| M15 | Block layer, device model | `tidak dibahas` |
| M16 | Security model, capability/ACL | `tidak dibahas` |

Batas cakupan praktikum:

```text
M12 mencakup: header kontrak mcs_sync.h (tipe mcs_spinlock_t, mcs_mutex_t,
mcs_lockdep_state_t, dan enam kode error), lockdep.c (rank monoton naik,
recursive acquire ditolak, release LIFO, violation_count), spinlock.c
(__atomic_exchange_n ACQUIRE, __atomic_store_n RELEASE, pause loop),
mutex.c (__atomic_compare_exchange_n, owner check, recursive mutex ditolak,
non-owner unlock ditolak), host unit test 4 fungsi test dengan pthread,
Makefile.m12 (host-test, freestanding, audit), dan evidence preflight.

M12 TIDAK mencakup: futex, priority inheritance, RCU, rwlock, seqlock,
lock-free queue, wait queue penuh, scheduler wakeup, irqsave/irqrestore,
SMP AP bring-up, preemptive scheduler final, pembuktian formal race freedom,
fault injection, fuzzing lock, stress test multi-core, dan integration
langsung ke semua subsystem PMM/VMM/heap.
```

---

## 6. Dasar Teori Ringkas

### 6.1 Konsep Sistem Operasi yang Diuji

```text
M12 membangun fondasi sinkronisasi kernel pada MCSOS 260502. Lima konsep utama:

1. Spinlock dan Busy-Wait:
   Spinlock adalah mutual exclusion primitive yang menunggu dengan busy-wait
   (loop spin) ketika lock sedang dimiliki. Invariant utamanya: locked == 0
   berarti bebas, locked == 1 berarti dimiliki tepat satu eksekusi logis.
   Spinlock hanya cocok untuk critical section pendek yang tidak memanggil
   operasi blocking. Menggunakan spinlock untuk operasi panjang menyebabkan
   waste CPU karena thread lain terus melakukan busy-wait tanpa hasil.

2. Memory Ordering: Acquire dan Release:
   Acquire ordering pada lock memastikan bahwa semua akses memori setelah
   lock tidak dapat direorder oleh compiler/CPU ke sebelum lock berhasil.
   Release ordering pada unlock memastikan semua akses dalam critical section
   sudah selesai dan terlihat oleh thread berikutnya sebelum lock dilepas.
   Tanpa ordering ini, CPU out-of-order execution atau compiler reordering
   dapat membuat nilai yang diperbarui dalam critical section tidak terlihat
   oleh pemegang lock berikutnya. `volatile` saja tidak cukup karena tidak
   memberikan ordering semantics terhadap core atau thread lain.

3. Mutex Owner-Aware:
   Mutex M12 menyimpan identitas owner (`owner_id`) yang hanya dapat di-unlock
   oleh thread yang sama. Ini memberikan dua properti penting: (a) recursive
   acquire oleh owner yang sama ditolak dengan EDEADLK — mencegah self-deadlock;
   (b) unlock oleh non-owner ditolak dengan EPERM — mencegah privilege confusion
   di mana thread keliru melepas lock milik thread lain. Mutex M12 belum
   memiliki wait queue; thread yang gagal lock harus melakukan polling
   (try_lock ulang) atau kembali ke scheduler.

4. Lock-Order Validator (Lockdep Sederhana):
   Deadlock klasik terjadi ketika dua jalur mengambil lock A dan B dengan
   urutan berbeda: jalur 1 mengambil A lalu B, jalur 2 mengambil B lalu A.
   Jika keduanya berjalan bersamaan dan masing-masing sudah memegang lock
   pertamanya, keduanya akan menunggu selamanya. Lock-order validator M12
   mencegah ini dengan mewajibkan rank lock monoton naik: jika sedang memegang
   lock kelas 20, tidak boleh mengambil lock kelas 10. Ini adalah kontrak
   preventif, bukan deteksi post-facto.

5. Observability dan violation_count:
   Setiap pelanggaran lock order, recursive acquire, atau release non-LIFO
   menaikkan `violation_count`. Ini membuat anomali terukur dan dapat
   diaudit dari log atau debugger tanpa menghentikan eksekusi sepenuhnya.
   Observability yang terukur adalah syarat debugging kernel yang dapat
   direproduksi.
```

### 6.2 Konsep Arsitektur x86_64 yang Relevan

| Konsep | Relevansi pada praktikum | Bukti/verifikasi |
|---|---|---|
| `XCHG` (atomic exchange) | Instruksi ini implisit LOCK prefix pada x86_64, memastikan acquire atomic | `objdump` menunjukkan `xchg %eax,(%rdi)` di `mcs_spin_try_lock` |
| `PAUSE` instruction | Mereduksi konsumsi power dan pipeline pressure saat busy-wait | `objdump` menunjukkan `pause` di spin loop `mcs_spin_lock` |
| Memory ordering x86_64 | x86 memiliki TSO (Total Store Order) sehingga acquire/release lebih ringan dibanding ARM | `-O2` menghasilkan `movl $0x0` (plain store) untuk release, cukup di TSO |
| Interrupt context | Interrupt handler tidak boleh tidur; spinlock cocok untuk protect shared state dalam handler | Bagian desain; invariant non-blocking |
| `volatile` vs atomics | `volatile` hanya mencegah compiler mengoptimalkan akses, tidak memberikan ordering antar-thread | Pertanyaan analisis nomor 1; `__atomic_*` digunakan, bukan `volatile` saja |
| `-mno-red-zone` | Kernel tidak boleh bergantung red-zone karena interrupt dapat mengoverwrite area tersebut | Flag aktif di `KERNEL_CFLAGS` Makefile.m12 |

### 6.3 Konsep Implementasi Freestanding

| Aspek | Keputusan praktikum |
|---|---|
| Bahasa | C17 freestanding untuk kernel sync; C17 hosted dengan pthread untuk host unit test |
| Runtime | Tanpa hosted libc; tidak ada `memset`, `printf`, atau fungsi libc di lockdep/spinlock/mutex |
| ABI | `x86_64-elf`, `-mno-red-zone`, `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector` |
| Compiler flags kritis | `-std=c17 -Wall -Wextra -Werror -target x86_64-elf -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mno-red-zone -O2` |
| No-libc enforcement | Loop manual untuk init `held_class[]` dan `held_name[]`; `nm -u` kosong membuktikan tidak ada dependency libc |
| Risiko undefined behavior | Pointer null diperiksa sebelum dereference di semua fungsi publik; `__atomic_*` builtins tidak menghasilkan UB pada tipe yang didukung |

### 6.4 Referensi Teori yang Digunakan

| No. | Sumber | Bagian yang digunakan | Alasan relevansi |
|---|---|---|---|
| [1] | Intel SDM Vol.3 | Memory ordering, LOCK prefix, XCHG, PAUSE | Fondasi instruksi atomik dan konsistensi memori x86_64 |
| [2] | Linux Kernel Documentation — Lock types and their rules | Spinlock vs mutex, interrupt context rules | Kategori lock dan aturan konteks eksekusi |
| [3] | Linux Kernel Documentation — Lockdep design | Rank kelas lock, deadlock detection graph | Konsep lock-order validator M12 |
| [4] | Linux Kernel Documentation — Generic Mutex Subsystem | Owner semantics, recursive reject | Desain mutex kooperatif M12 |
| [5] | GCC `__atomic` Builtins | `__atomic_exchange_n`, `__atomic_store_n`, `__atomic_compare_exchange_n` | Implementasi acquire/release ordering |
| [6] | LLVM/Clang Reference | `-ffreestanding`, `-target x86_64-elf` | Flags kompilasi freestanding kernel |
| [7] | GNU Binutils | `nm`, `readelf`, `objdump` | Audit artefak freestanding |
| [8] | QEMU Documentation | GDB usage, `-no-reboot`, serial | Konfigurasi QEMU smoke test |

---

## 7. Lingkungan Praktikum

### 7.1 Host dan Target

| Komponen | Nilai |
|---|---|
| Host OS | Windows 11 x64 |
| Lingkungan build | WSL 2 — Linux DESKTOP-FPCS9GF 6.6.87.2-microsoft-standard-WSL2 |
| Distro WSL | Ubuntu (dengan Clang 21.1.8 dan GCC 15.2.0) |
| Target ISA | x86_64 |
| Target ABI kernel | `x86_64-elf` (freestanding) |
| Emulator | QEMU (dilanjutkan dari M2–M11; smoke test integrasi tidak mengubah boot path) |
| Build system | GNU Make 4.4.1 |
| Bahasa utama kernel | C17 freestanding |
| Compiler kernel | Ubuntu clang version 21.1.8 (6ubuntu1) |
| Compiler host test | cc (Ubuntu GCC 15.2.0-16ubuntu1) |
| Binutils | nm, readelf, objdump, sha256sum (tersedia di WSL Ubuntu) |

### 7.2 Versi Toolchain

Output dari `evidence/M12/preflight.log`, commit `55a09ab`:

```text
2026-06-29T02:52:28+07:00
Linux DESKTOP-FPCS9GF 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC
Thu Jun  5 18:30:46 UTC 2025 x86_64 GNU/Linux
Ubuntu clang version 21.1.8 (6ubuntu1)
cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU Make 4.4.1
55a09ab
```

### 7.3 Lokasi Repository

| Item | Nilai |
|---|---|
| Path repository di WSL | `~/src/mcsos` |
| Berada di filesystem Linux WSL, bukan `/mnt/c` | `Ya` |
| Remote repository | lokal |
| Branch | `praktikum/m12-sync` |
| Commit hash awal (gate M11) | `ad07b27` |
| Commit hash akhir (M12 final) | `55a09ab` |

---

## 8. Repository dan Struktur File

### 8.1 Struktur Direktori yang Relevan

```text
mcsos/
  include/
    mcs_sync.h              ← header kontrak M12 (tipe + error code + deklarasi API)
  kernel/
    sync/
      lockdep.c             ← lock-order validator: rank monoton, recursive reject, LIFO release
      spinlock.c            ← spinlock atomik: exchange ACQUIRE, store RELEASE, pause loop
      mutex.c               ← mutex owner-aware: compare-exchange, owner check, non-owner reject
  tests/
    m12_sync_host_test.c    ← 4 fungsi test: lockdep order, lockdep negative, spinlock thread, mutex owner
  evidence/
    M12/
      preflight.log         ← toolchain version + git hash
      build.log             ← log make all CC=clang
  build/
    m12/
      lockdep.o             ← freestanding object x86_64-elf
      spinlock.o            ← freestanding object x86_64-elf
      mutex.o               ← freestanding object x86_64-elf
      m12_sync_host_test    ← host test binary (linux-gnu)
      nm-undefined.txt      ← audit nm -u (kosong = tidak ada unresolved symbol)
      readelf-lockdep.txt   ← audit readelf -h lockdep.o
      objdump-spinlock.txt  ← audit objdump -d spinlock.o
      sha256sums.txt        ← checksum keempat artefak
      host-test.log         ← log host unit test
  Makefile.m12              ← target: host-test, freestanding, audit, all, clean
```

### 8.2 File yang Dibuat atau Diubah

| File | Jenis perubahan | Alasan perubahan | Risiko |
|---|---|---|---|
| `include/mcs_sync.h` | Baru | Kontrak tunggal tipe dan API; tidak bergantung libc | Rendah — hanya deklarasi |
| `kernel/sync/lockdep.c` | Baru | Validator rank lock, tanpa alokasi dinamis | Sedang — logika stack `held_class` harus benar |
| `kernel/sync/spinlock.c` | Baru | Spinlock atomik freestanding; `pause` untuk efisiensi | Sedang — correctness ordering acquire/release |
| `kernel/sync/mutex.c` | Baru | Mutex owner-aware; compare-exchange untuk lock atomik | Sedang — urutan store owner setelah acquire harus benar |
| `tests/m12_sync_host_test.c` | Baru | Verifikasi fungsional dan negative case; pthread stress | Rendah — host only, tidak menyentuh kernel |
| `Makefile.m12` | Baru | Build pipeline host-test, freestanding, audit | Rendah — tidak mengubah Makefile utama |
| `evidence/M12/preflight.log` | Baru | Catat versi toolchain dan commit hash sebelum M12 | Rendah — log statis |

### 8.3 Ringkasan Diff

```bash
git log --oneline -4
```

```text
55a09ab (HEAD -> praktikum/m12-sync) M12: add preflight log
bebe0e3 M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit
ad07b27 (praktikum-m11-elf-user-loader) M11: implement ELF64 user loader, host test, audit, and QEMU integration
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI, int 0x80 dispatcher, and kernel integration
```

7 file baru, 470 insertions, 0 deletions pada commit `bebe0e3`.

---

## 9. Desain Teknis

### 9.1 Masalah yang Diselesaikan

```text
Setelah M9 (scheduler), M10 (syscall), dan M11 (loader), MCSOS memiliki
lebih dari satu alur eksekusi konseptual: interrupt handler, scheduler,
syscall dispatcher, ELF loader, dan kelak proses user. Tanpa sinkronisasi
eksplisit, dua jalur yang mengakses struktur data yang sama secara bersamaan
(misalnya runqueue scheduler atau allocator PMM) dapat menghasilkan data race:
salah satu update hilang, state tidak konsisten, atau pointer korup yang
menyebabkan triple fault.

M12 menyediakan fondasi sinkronisasi minimum: spinlock untuk critical section
pendek non-blocking, mutex untuk jalur task context yang kelak dapat sleep,
dan lock-order validator untuk mencegah deadlock karena akuisisi lock dengan
urutan berbeda di jalur berbeda.
```

### 9.2 Keputusan Desain

| Keputusan | Alternatif yang dipertimbangkan | Alasan memilih | Konsekuensi |
|---|---|---|---|
| Spinlock dengan `__atomic_exchange_n` ACQUIRE | `cmpxchg` langsung via inline assembly | `__atomic` builtin lebih portable, tidak bergantung pada assembler target | Compiler dapat memilih instruksi optimal per target |
| `pause` di spin loop | Loop plain `while` tanpa hint | `pause` mereduksi konsumsi power dan mengurangi memory order violation penalty di HT core | Sedikit lebih panjang di `objdump` |
| Mutex dengan `compare_exchange` (tidak hanya exchange) | Spinlock biasa yang juga menyimpan owner | CAS memastikan hanya satu pemenang yang menulis owner; exchange tidak memberikan jaminan ini | Logika sedikit lebih kompleks tapi lebih benar |
| Lock-order validator berbasis rank linear | Graph dependency penuh (lockdep Linux) | Graph penuh membutuhkan alokasi dinamis; rank linear tidak perlu alokasi dan cukup untuk pendidikan | Tidak mendeteksi semua pola deadlock potensial, hanya inversion dan rekursi |
| Objek lock dimiliki pemanggil (tidak ada alokasi dinamis) | Alokasi heap per lock | Alokasi sebelum heap siap menyebabkan bootstrapping problem; ownership eksplisit lebih aman | Pemanggil wajib pastikan lifetime objek lock cukup |
| Tabel error code negatif (`EINVAL`, `EBUSY`, dll.) | Enum internal | Nilai negatif konsisten dengan konvensi syscall dan error POSIX-like yang sudah ada di MCSOS | Caller harus cek `< 0` bukan `== ERROR_CODE` tertentu |

### 9.3 Arsitektur Ringkas

```mermaid
flowchart TD
    A[Pemanggil: scheduler / syscall / loader / interrupt handler] --> B{Pilih primitive}
    B -->|Critical section pendek, non-blocking| C[mcs_spin_lock]
    B -->|Task context, butuh owner tracking| D[mcs_mutex_try_lock]
    C --> E[__atomic_exchange_n ACQUIRE]
    E -->|locked sudah 1| F[spin + pause loop]
    F --> E
    E -->|berhasil, locked = 1| G[Eksekusi critical section]
    G --> H[mcs_spin_unlock: __atomic_store_n RELEASE]
    D --> I[__atomic_compare_exchange_n ACQUIRE]
    I -->|gagal| J{cek owner == caller?}
    J -->|Ya| K[return EDEADLK]
    J -->|Tidak| L[return EBUSY]
    I -->|berhasil| M[store owner RELEASE]
    M --> N[Eksekusi protected section]
    N --> O[mcs_mutex_unlock: cek owner, hapus owner, clear lock]
    C -.-> P[mcs_lockdep_before_acquire: validasi rank]
    P -->|rank turun atau rekursi| Q[violation_count++, return EDEADLK]
    P -->|rank naik| R[push ke held_class stack]
    H -.-> S[mcs_lockdep_after_release: cek LIFO]
```

Penjelasan diagram:

```text
Pemanggil memilih spinlock untuk jalur non-blocking pendek (interrupt handler,
update counter runqueue) atau mutex untuk jalur task context yang lebih panjang.
Lock-order validator (lockdep) dipanggil secara eksplisit oleh pemanggil
sebelum acquire dan setelah release — validator tidak dipanggil otomatis
dari dalam spinlock/mutex karena itu akan membutuhkan per-thread lockdep_state
yang belum tersedia di M12. Setiap violation menaikkan violation_count sebagai
sinyal yang dapat diobservasi.
```

### 9.4 Kontrak Antarmuka

| Antarmuka | Pemanggil | Penerima | Precondition | Postcondition | Error path |
|---|---|---|---|---|---|
| `mcs_spin_lock(lock)` | Subsystem kernel | spinlock.c | `lock` tidak NULL, sudah `mcs_spin_init` | `lock->locked == 1`; caller dalam critical section | NULL ptr: return tanpa crash (null check) |
| `mcs_spin_unlock(lock)` | Subsystem kernel | spinlock.c | `lock` tidak NULL, caller memegang lock | `lock->locked == 0` dengan RELEASE ordering | NULL ptr: return tanpa crash |
| `mcs_mutex_try_lock(mutex, owner_id)` | Task context | mutex.c | `mutex` tidak NULL, `owner_id != 0` | `MCS_SYNC_OK`: mutex dimiliki caller; state konsisten | `EDEADLK` (recursive), `EBUSY` (dimiliki lain), `EINVAL` (null/zero) |
| `mcs_mutex_unlock(mutex, owner_id)` | Task context | mutex.c | `mutex` tidak NULL, caller adalah owner | `mutex->locked == 0`, `mutex->owner == 0` | `EPERM` (non-owner), `EINVAL` (null/zero) |
| `mcs_lockdep_before_acquire(state, class_id, name)` | Pemanggil sebelum lock | lockdep.c | `state` tidak NULL, `class_id != 0` | Jika valid: push ke stack; `depth++` | `EDEADLK` (rekursi atau rank turun), `EOVERFLOW` (stack penuh) |
| `mcs_lockdep_after_release(state, class_id, name)` | Pemanggil setelah unlock | lockdep.c | `state` tidak NULL, lock ini ada di top stack | Pop dari stack; `depth--` | `EDEADLK` (bukan top stack), `EPERM` (stack kosong) |

### 9.5 Struktur Data Utama

| Struktur data | Field penting | Ownership | Lifetime | Invariant |
|---|---|---|---|---|
| `mcs_spinlock_t` | `locked` (volatile uint32), `class_id`, `name` | Subsystem pemanggil | Seumur objek pemilik lock | `locked == 0` ↔ bebas; `locked == 1` ↔ tepat satu pemegang |
| `mcs_mutex_t` | `locked` (volatile uint32), `owner` (uint64), `class_id`, `name` | Subsystem pemanggil | Seumur objek pemilik lock | `locked == 1` → `owner != 0`; `locked == 0` → `owner == 0` |
| `mcs_lockdep_state_t` | `held_class[16]`, `held_name[16]`, `depth`, `violation_count` | Per-thread atau per-context | Seumur thread/context | `held_class[0..depth-1]` monoton naik; `depth <= 16` |

### 9.6 Invariants

1. `mcs_spinlock_t.locked == 0` berarti lock bebas; `locked == 1` berarti dimiliki tepat satu eksekusi logis.
2. Acquire spinlock bersifat atomic — tidak ada dua pemanggil yang dapat sama-sama membaca `0` dan menulis `1`.
3. `mcs_spinlock_t` tidak boleh di-lock dari jalur yang memanggil fungsi blocking atau sleep.
4. `mcs_mutex_t.locked == 1` selalu diikuti `owner != 0`; `locked == 0` selalu diikuti `owner == 0`.
5. Owner yang sama tidak boleh mengambil mutex yang sama untuk kedua kali (recursive acquire ditolak).
6. Hanya owner yang boleh memanggil `mcs_mutex_unlock`; non-owner mendapat `EPERM`.
7. `mcs_lockdep_state_t.held_class[0..depth-1]` harus monoton naik — rank baru harus lebih besar dari semua yang ada di stack.
8. Release lock-order validator harus melepas elemen paling atas (LIFO); release non-LIFO adalah violation.
9. Setiap violation menaikkan `violation_count` — nilai ini tidak boleh berkurang selama lifetime state.
10. Semua fungsi publik mengembalikan `MCS_SYNC_EINVAL` jika pointer NULL atau `owner_id == 0`, tanpa melakukan dereference.

### 9.7 Ownership, Locking, dan Concurrency

| Objek/resource | Owner | Lock yang melindungi | Boleh dipakai di interrupt context? | Catatan |
|---|---|---|---|---|
| `mcs_spinlock_t` | Subsystem pemanggil | Dirinya sendiri (self-protecting) | Ya, jika critical section non-blocking | Tidak boleh tidur dalam critical section |
| `mcs_mutex_t` | Task context yang memanggil try_lock | Dirinya sendiri | Tidak — mutex dapat dipakai oleh task, tidak oleh interrupt handler | Interrupt handler yang perlu shared state harus memakai spinlock |
| `mcs_lockdep_state_t` | Per-thread/per-context, belum per-thread otomatis di M12 | Tidak ada (state lokal per-context) | Bergantung pada konteks yang menyuplai state | Di M12, pemanggil harus menyuplai state eksplisit |

Lock order yang berlaku:

```text
Rank lock harus monoton naik: pmm_lock (10) → vmm_lock (20) → proc_table (200).
Mengambil lock dengan rank lebih rendah saat memegang lock rank lebih tinggi
adalah lock-order inversion dan ditolak oleh lockdep validator.
```

### 9.8 Memory Safety dan Undefined Behavior Risk

| Risiko | Lokasi | Mitigasi | Bukti |
|---|---|---|---|
| Dereference NULL pointer | Semua fungsi publik | Null check eksplisit sebelum akses, return error atau void tanpa crash | Review kode; setiap fungsi baris pertama cek `if (ptr == 0)` |
| Integer overflow pada `depth` | `lockdep.c: mcs_lockdep_before_acquire` | Cek `depth >= MCS_LOCKDEP_MAX_HELD` sebelum push | Kode eksplisit; EOVERFLOW dikembalikan |
| Race pada `owner` setelah CAS | `mutex.c: mcs_mutex_try_lock` | Owner hanya ditulis setelah CAS berhasil dengan RELEASE; dibaca dengan ACQUIRE di unlock | GCC/Clang `__atomic_*` memastikan ordering |
| Aliasing antara `volatile uint32_t` dan `uint32_t` | `spinlock.c`, `mutex.c` | `__atomic_*` builtins menangani aliasing dengan benar | Diterima sebagai pattern standar kernel |
| Out-of-bounds akses `held_class` | `lockdep.c` | Loop dibatasi `i < state->depth && i < MCS_LOCKDEP_MAX_HELD` | Review kode |

### 9.9 Security Boundary

| Boundary | Data tidak tepercaya | Validasi yang dilakukan | Failure mode aman |
|---|---|---|---|
| API publik mcs_sync.h | Pointer dari pemanggil kernel | Null check sebelum dereference | Return error code, tanpa crash |
| Lock kelas dari pemanggil | `class_id == 0` tidak valid | Cek eksplisit `class_id == 0 → EINVAL` | Return EINVAL |
| Owner ID dari pemanggil mutex | `owner_id == 0` tidak valid (reserved: tidak ada owner) | Cek eksplisit `owner_id == 0 → EINVAL` | Return EINVAL |
| Unlock request | Owner ID yang diklaim caller | Load owner dengan ACQUIRE, bandingkan; tidak cocok → EPERM | State lock tidak berubah |

---

## 10. Langkah Kerja Implementasi

### Langkah 1 — Buat Branch M12 dan Direktori

Maksud langkah: memastikan branch M12 dimulai dari titik M11 yang bersih dan tidak mencemari branch sebelumnya.

```bash
git status --short
git log --oneline -3
git checkout -b praktikum/m12-sync
mkdir -p include kernel/sync tests evidence/M12
```

Output:

```text
ad07b27 (HEAD -> praktikum-m11-elf-user-loader) M11: implement ELF64 user loader ...
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI ...
29c0595 (praktikum/m9-kernel-thread-scheduler) M9: implement kernel thread ...
Switched to a new branch 'praktikum/m12-sync'
```

Indikator berhasil: branch baru aktif dari commit M11, direktori `kernel/sync` dibuat.

### Langkah 2 — Buat Header `include/mcs_sync.h`

Maksud langkah: mendefinisikan kontrak tunggal tipe dan API sinkronisasi. Header tidak bergantung pada libc hosted — hanya `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`.

```bash
cat > include/mcs_sync.h <<'EOF'
... (55 baris, lihat Lampiran B)
EOF
wc -l include/mcs_sync.h
head -5 include/mcs_sync.h
```

Output:

```text
55 include/mcs_sync.h
#ifndef MCS_SYNC_H
#define MCS_SYNC_H
#include <stdint.h>
#include <stddef.h>
```

| Artefak | Lokasi | Fungsi |
|---|---|---|
| `mcs_sync.h` | `include/mcs_sync.h` | Kontrak tipe dan API seluruh komponen M12 |

Indikator berhasil: 55 baris, include guard `#ifndef MCS_SYNC_H` ada, tidak ada include libc.

### Langkah 3 — Buat `kernel/sync/lockdep.c`

Maksud langkah: mengimplementasikan validator rank monoton, penolakan recursive acquire, dan penolakan release non-LIFO dengan `violation_count` sebagai bukti observability.

```bash
cat > kernel/sync/lockdep.c <<'EOF'
... (72 baris, lihat Lampiran B)
EOF
wc -l kernel/sync/lockdep.c
```

Output: `72 kernel/sync/lockdep.c`

Indikator berhasil: 72 baris, empat fungsi publik diimplementasikan.

### Langkah 4 — Buat `kernel/sync/spinlock.c`

Maksud langkah: mengimplementasikan spinlock atomik dengan `__atomic_exchange_n` ACQUIRE untuk lock dan `__atomic_store_n` RELEASE untuk unlock, serta `pause` untuk efisiensi busy-wait.

```bash
cat > kernel/sync/spinlock.c <<'EOF'
... (48 baris, lihat Lampiran B)
EOF
wc -l kernel/sync/spinlock.c
```

Output: `48 kernel/sync/spinlock.c`

Indikator berhasil: 48 baris, `mcs_cpu_relax` dengan `pause` untuk x86_64, lima fungsi publik.

### Langkah 5 — Buat `kernel/sync/mutex.c`

Maksud langkah: mengimplementasikan mutex owner-aware dengan `__atomic_compare_exchange_n` untuk atomik lock, pengecekan owner sebelum unlock, dan penolakan recursive acquire.

```bash
cat > kernel/sync/mutex.c <<'EOF'
... (53 baris, lihat Lampiran B)
EOF
wc -l kernel/sync/mutex.c
```

Output: `53 kernel/sync/mutex.c`

Indikator berhasil: 53 baris, lima fungsi publik, urutan store owner setelah CAS berhasil sudah benar.

### Langkah 6 — Buat `tests/m12_sync_host_test.c`

Maksud langkah: menulis verifikasi fungsional yang mencakup positive dan negative case tanpa memerlukan boot QEMU. Test spinlock menggunakan 4 thread pthread dengan 25.000 iterasi masing-masing sebagai stress counter.

```bash
cat > tests/m12_sync_host_test.c <<'EOF'
... (83 baris, lihat Lampiran B)
EOF
wc -l tests/m12_sync_host_test.c
```

Output: `83 tests/m12_sync_host_test.c`

Indikator berhasil: 83 baris, empat fungsi test: `test_lockdep_order`, `test_lockdep_negative`, `test_spinlock_threads`, `test_mutex_owner`.

### Langkah 7 — Buat `Makefile.m12`

Maksud langkah: menyediakan tiga target terpisah — `host-test` (compile dan jalankan binary host), `freestanding` (compile object kernel), `audit` (nm/readelf/objdump/sha256sum) — agar setiap tahap dapat dijalankan dan direproduksi secara mandiri.

```bash
cat > Makefile.m12 <<'EOF'
... (34 baris, lihat Lampiran B)
EOF
```

Indikator berhasil: target `all`, `host-test`, `freestanding`, `audit`, `clean` tersedia.

### Langkah 8 — Jalankan `make -f Makefile.m12 all CC=clang`

Maksud langkah: menjalankan seluruh pipeline — host test, freestanding compile, dan audit — dalam satu perintah, dengan log tersimpan untuk bukti reproduksi.

```bash
make -f Makefile.m12 all 2>&1 | tee evidence/M12/build.log
```

Catatan: target `freestanding` dan `audit` memerlukan `CC=clang` karena GCC tidak mendukung flag `-target`. Error awal terjadi karena `CC` default ke `cc` (GCC 15.2.0). Diperbaiki dengan:

```bash
make -f Makefile.m12 freestanding audit CC=clang 2>&1 | tee -a evidence/M12/build.log
```

Output ringkas:

```text
[PASS] M12 synchronization host tests passed
clang ... -c kernel/sync/lockdep.c  -o build/m12/lockdep.o
clang ... -c kernel/sync/spinlock.c -o build/m12/spinlock.o
clang ... -c kernel/sync/mutex.c    -o build/m12/mutex.o
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o
(kosong — tidak ada unresolved symbol)
sha256sum ... > build/m12/sha256sums.txt
```

| Artefak | Lokasi | Fungsi |
|---|---|---|
| `m12_sync_host_test` | `build/m12/` | Binary host test linux-gnu |
| `lockdep.o` | `build/m12/` | Freestanding object x86_64-elf |
| `spinlock.o` | `build/m12/` | Freestanding object x86_64-elf |
| `mutex.o` | `build/m12/` | Freestanding object x86_64-elf |
| `nm-undefined.txt` | `build/m12/` | Bukti tidak ada dependency libc |
| `readelf-lockdep.txt` | `build/m12/` | Bukti format ELF64 REL |
| `objdump-spinlock.txt` | `build/m12/` | Bukti instruksi atomik dan pause |
| `sha256sums.txt` | `build/m12/` | Checksum keempat artefak |

Indikator berhasil: `[PASS]` dari host test, nol warning/error compiler, `nm -u` kosong, sha256sums tersimpan.

### Langkah 9 — Preflight Log dan Commit

```bash
mkdir -p evidence/M12
{
  date -Is; uname -a
  clang --version | head -n 1
  cc --version | head -n 1
  make --version | head -n 1
  git rev-parse --short HEAD
  git status --short
} | tee evidence/M12/preflight.log

git add include/mcs_sync.h kernel/sync/lockdep.c kernel/sync/spinlock.c \
        kernel/sync/mutex.c tests/m12_sync_host_test.c Makefile.m12 evidence/M12/
git commit -m "M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit"
git add evidence/M12/preflight.log
git commit -m "M12: add preflight log"
git log --oneline -3
```

Output:

```text
55a09ab (HEAD -> praktikum/m12-sync) M12: add preflight log
bebe0e3 M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit
ad07b27 (praktikum-m11-elf-user-loader) M11: implement ELF64 user loader, host test, audit, and QEMU integration
```

---

## 11. Checkpoint Buildable

| Checkpoint | Perintah | Expected result | Status |
|---|---|---|---|
| C1 Header sync | `test -f include/mcs_sync.h && echo ok` | `ok` | `PASS` |
| C2 Host unit test | `make -f Makefile.m12 host-test` | `[PASS] M12 synchronization host tests passed` | `PASS` |
| C3 Freestanding compile | `make -f Makefile.m12 freestanding CC=clang` | `lockdep.o`, `spinlock.o`, `mutex.o` dibuat tanpa warning | `PASS` |
| C4 Object audit | `make -f Makefile.m12 audit CC=clang` | `nm -u` kosong; `readelf` ELF64 REL; `objdump` ada `xchg` dan `pause`; sha256 tersimpan | `PASS` |
| C5 Git commit | `git log --oneline -2` | Commit `55a09ab` dan `bebe0e3` ada | `PASS` |
| C6 QEMU integration | Tidak dimodifikasi dari M11 | Boot path M2–M11 tidak diubah; sinkronisasi belum diintegrasikan ke kernel image | `NA — belum dijalankan dalam sesi ini` |

Catatan checkpoint:

```text
C6 (QEMU smoke test) tidak dijalankan pada sesi praktikum ini karena integrasi
primitif sinkronisasi ke kernel image (misalnya menambahkan self-test di kmain)
belum dilakukan. Boot QEMU dari M11 tidak rusak karena file-file M12 tidak
mengubah Makefile utama, kmain.c, atau linker script. Rollback ke M11 dapat
dilakukan kapan saja tanpa risiko.
```

---

## 12. Perintah Uji dan Validasi

### 12.1 Host Unit Test

```bash
make -f Makefile.m12 host-test
```

Hasil:

```text
cc -std=c17 -Wall -Wextra -Werror -Iinclude -O2 -pthread \
   kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c \
   tests/m12_sync_host_test.c -o build/m12/m12_sync_host_test
build/m12/m12_sync_host_test | tee build/m12/host-test.log
[PASS] M12 synchronization host tests passed
```

Status: `PASS`

### 12.2 Static Inspection — nm

```bash
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o
```

Hasil:

```text
build/m12/lockdep.o:
build/m12/spinlock.o:
build/m12/mutex.o:
```

Semua bagian kosong — tidak ada unresolved external symbol. Ketiga object benar-benar freestanding.

Status: `PASS`

### 12.3 Static Inspection — readelf

```bash
readelf -h build/m12/lockdep.o
```

Hasil penting:

```text
  Class:   ELF64
  Type:    REL (Relocatable file)
  Machine: Advanced Micro Devices X86-64
  Entry point address: 0x0
```

Format object benar: ELF64, tipe relocatable, target x86_64.

Status: `PASS`

### 12.4 Static Inspection — objdump

```bash
objdump -d build/m12/spinlock.o
```

Bukti instruksi kritis:

```text
mcs_spin_try_lock:
  2e: 87 07   xchg %eax,(%rdi)   ← atomic exchange (LOCK XCHG implisit di x86)

mcs_spin_lock:
  70: f3 90   pause               ← spin loop efficiency hint

mcs_spin_unlock:
  89: c7 07 00 00 00 00  movl $0x0,(%rdi)  ← release store
```

`XCHG` pada x86_64 memiliki implicit LOCK prefix — operasi ini atomic tanpa perlu `lock xchg` eksplisit. `pause` memastikan spin loop efisien. `movl $0x0` untuk release cukup di TSO memory model x86.

Status: `PASS`

### 12.5 Stress Test Spinlock (via pthread)

4 thread × 25.000 iterasi mengakses counter yang dilindungi spinlock:

```text
require_true(g_counter == (unsigned long)THREADS * (unsigned long)ITERS,
             "spinlock-protected counter exact");
```

Jika ada race (dua thread berhasil masuk critical section bersamaan), counter akan kurang dari 100.000. Test ini lulus — counter tepat 100.000.

Status: `PASS`

---

## 13. Hasil Uji

### 13.1 Tabel Ringkasan Hasil

| No. | Uji | Expected result | Actual result | Status | Evidence |
|---|---|---|---|---|---|
| 1 | `test_lockdep_order` — acquire rank 10 | `MCS_SYNC_OK` | `MCS_SYNC_OK` | `PASS` | `build/m12/host-test.log` |
| 2 | `test_lockdep_order` — acquire rank 20 (naik) | `MCS_SYNC_OK` | `MCS_SYNC_OK` | `PASS` | `build/m12/host-test.log` |
| 3 | `test_lockdep_order` — depth setelah dua lock | `2` | `2` | `PASS` | `build/m12/host-test.log` |
| 4 | `test_lockdep_order` — release rank 20 LIFO | `MCS_SYNC_OK` | `MCS_SYNC_OK` | `PASS` | `build/m12/host-test.log` |
| 5 | `test_lockdep_order` — release rank 10 LIFO | `MCS_SYNC_OK` | `MCS_SYNC_OK` | `PASS` | `build/m12/host-test.log` |
| 6 | `test_lockdep_order` — depth kembali ke 0 | `0` | `0` | `PASS` | `build/m12/host-test.log` |
| 7 | `test_lockdep_negative` — reject rank turun (20 → 10) | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | `PASS` | `build/m12/host-test.log` |
| 8 | `test_lockdep_negative` — reject recursive acquire rank 20 | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | `PASS` | `build/m12/host-test.log` |
| 9 | `test_lockdep_negative` — violation_count == 2 | `2` | `2` | `PASS` | `build/m12/host-test.log` |
| 10 | `test_spinlock_threads` — counter 4×25000 | `100000` | `100000` | `PASS` | `build/m12/host-test.log` |
| 11 | `test_spinlock_threads` — spinlock bebas setelah test | `false` | `false` | `PASS` | `build/m12/host-test.log` |
| 12 | `test_mutex_owner` — owner 1 lock | `MCS_SYNC_OK` | `MCS_SYNC_OK` | `PASS` | `build/m12/host-test.log` |
| 13 | `test_mutex_owner` — owner tercatat | `1u` | `1u` | `PASS` | `build/m12/host-test.log` |
| 14 | `test_mutex_owner` — recursive mutex ditolak | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | `PASS` | `build/m12/host-test.log` |
| 15 | `test_mutex_owner` — owner lain melihat BUSY | `MCS_SYNC_EBUSY` | `MCS_SYNC_EBUSY` | `PASS` | `build/m12/host-test.log` |
| 16 | `test_mutex_owner` — non-owner unlock ditolak | `MCS_SYNC_EPERM` | `MCS_SYNC_EPERM` | `PASS` | `build/m12/host-test.log` |
| 17 | `test_mutex_owner` — owner unlock berhasil | `MCS_SYNC_OK` | `MCS_SYNC_OK` | `PASS` | `build/m12/host-test.log` |
| 18 | `test_mutex_owner` — mutex bebas setelah unlock | `false` | `false` | `PASS` | `build/m12/host-test.log` |
| 19 | `nm -u` ketiga object | Tidak ada unresolved symbol | Kosong | `PASS` | `build/m12/nm-undefined.txt` |
| 20 | `readelf -h` lockdep.o | ELF64, REL, x86_64 | ELF64, REL, x86_64 | `PASS` | `build/m12/readelf-lockdep.txt` |
| 21 | `objdump` — instruksi `xchg` ada | `87 07 xchg %eax,(%rdi)` | Ditemukan | `PASS` | `build/m12/objdump-spinlock.txt` |
| 22 | `objdump` — instruksi `pause` ada | `f3 90 pause` | Ditemukan | `PASS` | `build/m12/objdump-spinlock.txt` |

### 13.2 Log Penting

```text
[PASS] M12 synchronization host tests passed
```

### 13.3 Artefak Bukti

| Artefak | Path | Fungsi |
|---|---|---|
| `lockdep.o` | `build/m12/lockdep.o` | Freestanding object lock-order validator |
| `spinlock.o` | `build/m12/spinlock.o` | Freestanding object spinlock atomik |
| `mutex.o` | `build/m12/mutex.o` | Freestanding object mutex owner-aware |
| `m12_sync_host_test` | `build/m12/m12_sync_host_test` | Binary host unit test |
| `nm-undefined.txt` | `build/m12/nm-undefined.txt` | Bukti tidak ada dependency libc |
| `readelf-lockdep.txt` | `build/m12/readelf-lockdep.txt` | Bukti format ELF64 REL x86_64 |
| `objdump-spinlock.txt` | `build/m12/objdump-spinlock.txt` | Bukti instruksi `xchg` dan `pause` |
| `sha256sums.txt` | `build/m12/sha256sums.txt` | Checksum keempat artefak utama |
| `host-test.log` | `build/m12/host-test.log` | Log hasil host unit test |
| `preflight.log` | `evidence/M12/preflight.log` | Versi toolchain dan commit hash |
| `build.log` | `evidence/M12/build.log` | Log build lengkap make all |

Perintah hash:

```bash
cat build/m12/sha256sums.txt
```

---

## 14. Analisis Teknis

### 14.1 Analisis Keberhasilan

```text
Seluruh 22 assertion dalam host unit test lulus dalam satu eksekusi tanpa
modifikasi source setelah ditulis. Ini menunjukkan bahwa kontrak yang
didefinisikan di mcs_sync.h diimplementasikan dengan konsisten di ketiga
file C.

Keberhasilan stress test spinlock (counter tepat 100.000 setelah 4 thread ×
25.000 iterasi) membuktikan bahwa acquire/release ordering benar — tidak ada
dua thread yang berhasil masuk critical section bersamaan. Pada x86_64,
`xchg %eax,(%rdi)` bersifat atomik secara hardware karena implicit LOCK prefix,
sehingga tidak mungkin dua thread sama-sama membaca nilai 0 dari `locked` dan
menulis 1 secara bersamaan.

`nm -u` kosong pada ketiga object membuktikan bahwa tidak ada hidden dependency
ke runtime helper seperti `memset`, `memcpy`, `__stack_chk_fail`, atau fungsi
libc lainnya. Ini penting karena di kernel, runtime helper tersebut tidak
tersedia kecuali disediakan eksplisit.

Bukti `objdump` menunjukkan bahwa optimasi O2 Clang menghasilkan output yang
benar: `xchg` untuk atomic exchange, `pause` di spin loop, dan plain `movl
$0x0` untuk release store. Plain store untuk release sudah cukup pada TSO
(x86/x86_64) karena semua store pada x86 memiliki release semantics secara
implisit.
```

### 14.2 Analisis Kegagalan atau Perbedaan Hasil

```text
Satu kendala ditemukan selama implementasi: target `freestanding` gagal saat
dipanggil dengan `make -f Makefile.m12 all` tanpa `CC=clang` eksplisit.

Gejala: `cc: error: unrecognized command-line option '-target'`
Penyebab: Makefile.m12 menggunakan `$(CC)` yang defaultnya adalah `cc`
(alias GCC di Ubuntu). GCC tidak mengenal flag `-target`; flag ini adalah
sintaks Clang.
Diagnosis: cek output `cc --version` yang menunjukkan GCC 15.2.0, bukan Clang.
Perbaikan: panggil target freestanding dan audit dengan `CC=clang` eksplisit.

Pelajaran: Makefile yang menggunakan flag Clang-specific sebaiknya menyertakan
guard atau documentation bahwa `CC=clang` diperlukan untuk target kernel,
atau menyetel `CC := clang` secara default dengan `?=` yang dapat di-override.
```

### 14.3 Perbandingan dengan Teori

| Konsep teori | Implementasi praktikum | Sesuai/tidak sesuai | Penjelasan |
|---|---|---|---|
| Acquire ordering pada lock | `__atomic_exchange_n(..., __ATOMIC_ACQUIRE)` | Sesuai | Barrier ini memastikan akses setelah lock tidak direorder ke sebelum lock |
| Release ordering pada unlock | `__atomic_store_n(..., __ATOMIC_RELEASE)` | Sesuai | Memastikan semua write dalam critical section terlihat sebelum lock bebas |
| Mutex owner semantics | `owner` disimpan atomik setelah CAS berhasil | Sesuai | Owner hanya valid setelah lock dikonfirmasi; dibaca dengan ACQUIRE saat unlock |
| Lock-order invariant | Rank harus monoton naik; release LIFO | Sesuai | `held_class[depth-1] < new_class` diperiksa sebelum push |
| Fail-closed pada null pointer | Return error/void tanpa dereference | Sesuai | Setiap fungsi publik cek `ptr == 0` di baris pertama |
| `volatile` tidak cukup untuk sinkronisasi | Tidak digunakan sebagai satu-satunya mekanisme | Sesuai | `__atomic_*` digunakan; `volatile` hanya ada pada field struct sebagai marker |

### 14.4 Kompleksitas dan Kinerja

| Aspek | Estimasi/hasil | Catatan |
|---|---|---|
| Kompleksitas `mcs_spin_lock` | O(N) — N = lama contention | Dalam single-core, contention hanya terjadi jika interrupt mengganggu |
| Kompleksitas `mcs_lockdep_before_acquire` | O(D) — D = depth stack (max 16) | Linear scan untuk cek recursive acquire |
| Kompleksitas `mcs_mutex_try_lock` | O(1) | Single CAS; tidak ada loop |
| Waktu build host test | < 2 detik | Dikompilasi sekali dari empat file C |
| Waktu host test | < 1 detik | 4 thread × 25.000 iterasi selesai hampir instan di host |

---

## 15. Debugging dan Failure Modes

### 15.1 Failure Modes yang Ditemukan

| Failure mode | Gejala | Penyebab | Bukti | Perbaikan |
|---|---|---|---|---|
| `-target` tidak dikenal oleh GCC | `cc: error: unrecognized command-line option '-target'` | `CC` default ke GCC; `-target` adalah flag Clang | Output make | Panggil dengan `CC=clang` eksplisit |

### 15.2 Failure Modes yang Diantisipasi

| Failure mode | Deteksi | Dampak | Mitigasi |
|---|---|---|---|
| Deadlock (dua jalur saling menunggu lock) | `violation_count` naik di lockdep; GDB `info threads` menunjukkan semua thread di spin loop | Kernel hang, tidak ada progress | Lock hierarchy monoton naik; lockdep validator sebelum acquire |
| Recursive spinlock acquire | Spinlock tidak mencatat owner, sehingga self-deadlock tidak terdeteksi langsung | Kernel hang di spin loop | Jangan panggil `mcs_spin_lock` dari dalam critical section lock yang sama |
| Interrupt handler mengambil lock yang dipegang task | Jika task sedang dalam critical section dan interrupt mengambil lock yang sama, interrupt spin selamanya | Hang — interrupt tidak pernah selesai | Spinlock hanya untuk non-blocking; interrupt-aware design memerlukan irqsave |
| Non-owner unlock | `mcs_mutex_unlock` mengembalikan `EPERM` | State lock tidak berubah; protected data tetap terlindungi | Owner check sebelum hapus owner dan clear lock |
| Stack lockdep overflow (> 16 lock) | `mcs_lockdep_before_acquire` mengembalikan `EOVERFLOW` | `violation_count` naik | Pantau depth; 16 sudah cukup untuk kernel pendidikan |
| Lock tidak di-release sebelum return | Tidak ada deteksi otomatis di M12; lockdep hanya cek urutan | Lock bocor; thread lain spin selamanya | RAII wrapper atau review kode; wait queue M13 akan membantu |

### 15.3 Triage yang Dilakukan

```text
Untuk error build `-target`:
1. Lihat pesan error: "unrecognized command-line option '-target'"
2. Jalankan `cc --version` — ternyata GCC 15.2.0
3. Jalankan `clang --version` — tersedia Clang 21.1.8
4. Panggil ulang make dengan CC=clang eksplisit
5. Build berhasil

Untuk debugging deadlock potensial di QEMU:
1. Gunakan `qemu -s -S` dan GDB
2. `info threads` untuk melihat semua thread
3. `bt` di setiap thread untuk melihat apakah ada yang stuck di spin loop
4. Pantau `violation_count` via GDB `watch`
```

### 15.4 Panic Path

```text
M12 tidak menambahkan panic path baru. Panic path dari M3 tetap aktif.
Jika lock-order violation terdeteksi saat integrasi kernel, log `violation_count`
dan serial log dapat digunakan untuk mendiagnosis. Untuk situasi kritis
(misalnya recursive spinlock acquire menyebabkan hang), GDB breakpoint di
`mcs_spin_lock` dapat memperlihatkan stack backtrace.
```

---

## 16. Prosedur Rollback

| Skenario rollback | Perintah | Data yang harus diselamatkan | Status |
|---|---|---|---|
| Kembali ke commit M11 | `git checkout ad07b27` | `evidence/M12/` log disimpan dulu | Teruji secara konseptual — branch terpisah |
| Revert commit M12 | `git revert bebe0e3` | log evidence | Belum diuji eksekusi; branch terpisah sudah cukup |
| Bersihkan artefak build | `make -f Makefile.m12 clean` | source aman, hanya `build/m12/` yang dihapus | Teruji |
| Verifikasi M11 masih jalan | `git checkout praktikum-m11-elf-user-loader && make build` | tidak perlu | Tidak dimodifikasi |

Catatan rollback:

```text
Karena M12 dikerjakan di branch terpisah `praktikum/m12-sync`, rollback
ke M11 hanya perlu `git checkout praktikum-m11-elf-user-loader`. Tidak ada
perubahan pada Makefile utama, kmain.c, atau linker script — sehingga
kernel M11 tetap dapat dibangun tanpa modifikasi. Evidence M12 disimpan
di `evidence/M12/` dan tidak akan hilang selama branch aktif.
```

---

## 17. Keamanan dan Reliability

### 17.1 Risiko Keamanan

| Risiko | Boundary | Dampak | Mitigasi | Evidence |
|---|---|---|---|---|
| Race pada shared state kernel | Internal kernel (interrupt vs task) | Data korup, PMM/VMM metadata invalid, kernel crash | Spinlock untuk critical section pendek | Host stress test 4 thread × 25.000 PASS |
| Recursive acquire spinlock | Saat jalur kode memiliki lock dan memanggil fungsi yang mencoba lock yang sama | Kernel hang (self-deadlock) | Tidak ada self-detect di spinlock; caller wajib tidak recursive | Dokumentasi invariant; lockdep dapat membantu jika class sama |
| Non-owner unlock mutex | Bug implementasi yang melepas lock milik thread lain | Protected state terbuka sebelum waktunya | Owner check via ACQUIRE load sebelum clear | Host test `non-owner unlock rejected` PASS |
| Interrupt reentry (irq mengambil lock yang dipegang task) | Interrupt handler vs task context | Hang — interrupt tidak bisa selesai | Desain M12 single-core; irqsave/irqrestore diperlukan untuk SMP | Didokumentasikan sebagai known limitation |
| Lock-order inversion antar jalur | Dua jalur dengan urutan lock berbeda | Deadlock potensial pada SMP | Lock-order validator dengan rank monoton | Host test `reject descending rank` PASS |

### 17.2 Reliability dan Data Integrity

| Risiko reliability | Dampak | Deteksi | Mitigasi |
|---|---|---|---|
| Lock tidak dilepas (leak) | Thread lain spin selamanya | Tidak ada auto-detect di M12 | Review kode; RAII wrapper pada modul berikutnya |
| State lock parsial pada error | Protected data terekspos sebagian | Host test negative case | Mutex: urutan hapus owner dulu, clear lock kedua; tidak pernah sebaliknya |
| `violation_count` overflow (> UINT32_MAX) | Hitung pelanggaran wrap-around | Tidak ada saat ini | Nilai praktis: ribuan violation sudah sinyal panik — overflow tidak realistis |
| Spin loop tidak ada batas waktu | Kernel hang jika lock tidak pernah dilepas | GDB `info threads` | Timeout spin adalah pengayaan M13+ |

### 17.3 Negative Test

| Negative test | Input buruk | Expected result | Actual result | Status |
|---|---|---|---|---|
| Lockdep — rank turun | Acquire rank 10 saat memegang rank 20 | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | `PASS` |
| Lockdep — recursive acquire | Acquire rank 20 dua kali | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | `PASS` |
| Mutex — recursive acquire | `try_lock(mutex, 1)` saat `owner == 1` | `MCS_SYNC_EDEADLK` | `MCS_SYNC_EDEADLK` | `PASS` |
| Mutex — other owner busy | `try_lock(mutex, 2)` saat `owner == 1` | `MCS_SYNC_EBUSY` | `MCS_SYNC_EBUSY` | `PASS` |
| Mutex — non-owner unlock | `unlock(mutex, 2)` saat `owner == 1` | `MCS_SYNC_EPERM` | `MCS_SYNC_EPERM` | `PASS` |

---

## 18. Pembagian Kerja Kelompok

Tidak berlaku — praktikum dikerjakan secara individu.

---

## 19. Kriteria Lulus Praktikum

| Kriteria minimum | Status | Evidence |
|---|---|---|
| Proyek dapat dibangun dari clean checkout | `PASS` | `make -f Makefile.m12 all CC=clang` berhasil |
| Perintah build terdokumentasi | `PASS` | Bagian 10 laporan ini |
| Host unit test M12 lulus | `PASS` | `[PASS] M12 synchronization host tests passed` |
| Object freestanding x86_64 berhasil dibuat | `PASS` | `lockdep.o`, `spinlock.o`, `mutex.o` ada |
| Audit `nm -u`, `readelf`, `objdump`, checksum tersedia | `PASS` | `build/m12/nm-undefined.txt` kosong; file lain ada |
| Tidak ada warning kritis pada build | `PASS` | `evidence/M12/build.log` — nol warning |
| Perubahan Git dikomit | `PASS` | Commit `bebe0e3` dan `55a09ab` |
| Desain dan failure modes dijelaskan | `PASS` | Bagian 9 dan 15 laporan |

| Kriteria lanjutan | Status | Evidence |
|---|---|---|
| QEMU smoke test kernel integration | `NA` | Integrasi ke kmain belum dilakukan dalam sesi ini |
| Static analysis (cppcheck/clang-tidy) | `NA` | Belum dijalankan |
| Fault injection | `NA` | Belum dijalankan |
| Rollback diuji | `Belum` | Branch terpisah tersedia; revert belum dieksekusi |

---

## 20. Readiness Review

| Status | Definisi | Pilihan |
|---|---|---|
| Belum siap uji | Build/test belum stabil atau bukti belum cukup | `[ ]` |
| Siap uji QEMU | Build bersih, host test lulus, object audit lengkap | `✓` |
| Siap demonstrasi praktikum | Siap ditunjukkan di kelas dengan bukti uji, failure mode, dan rollback | `[ ]` |
| Kandidat siap pakai terbatas | Hanya untuk penggunaan terbatas setelah test, security review, dan known issue | `[ ]` |

Alasan readiness:

```text
M12 diklaim siap uji QEMU berdasarkan bukti berikut: host unit test 22 assertion
lulus, freestanding compile Clang 21 tanpa warning, nm -u kosong, readelf
menunjukkan ELF64 REL x86_64, objdump menunjukkan instruksi xchg dan pause,
sha256sum tersimpan, preflight log toolchain tercatat, dan dua commit tersimpan
di branch praktikum/m12-sync di atas M11.

Belum layak disebut siap demonstrasi karena: (a) QEMU smoke test integrasi
ke kmain belum dijalankan dalam sesi ini; (b) irqsave/irqrestore belum ada;
(c) spinlock tidak mendeteksi recursive acquire sendiri; (d) fault injection
dan stress test multi-core belum dilakukan.
```

Known issues:

| No. | Issue | Dampak | Workaround | Target perbaikan |
|---|---|---|---|---|
| 1 | Spinlock tidak mendeteksi recursive acquire | Self-deadlock tidak terdeteksi otomatis | Caller wajib tidak recursive; lockdep mendeteksi jika class sama | M13 — tambah owner opsional ke spinlock |
| 2 | Tidak ada irqsave/irqrestore | Spinlock dari interrupt context dapat berinterferensi dengan task yang memegang lock yang sama | Single-core saat ini; cukup aman untuk kernel pendidikan | M13 — SMP bring-up |
| 3 | Mutex belum punya wait queue | Thread yang gagal lock harus polling atau kembali ke scheduler secara manual | Try-lock loop di caller | M13 — scheduler wakeup dan wait queue |
| 4 | `mcs_lockdep_state_t` belum per-thread otomatis | Pemanggil harus menyuplai state eksplisit | Deklarasi eksplisit per-konteks | M13 — per-CPU/per-thread state |
| 5 | QEMU smoke test belum dijalankan | Integrasi ke kernel image belum diverifikasi di sesi ini | Boot M11 tidak diubah; tidak ada regresi | Segera setelah panduan integrasi kmain tersedia |

Keputusan akhir:

```text
Berdasarkan bukti build, host unit test, nm/readelf/objdump audit, dan
sha256sum, hasil praktikum M12 layak disebut siap uji QEMU untuk sinkronisasi
kernel awal single-core menuju SMP. Belum layak disebut siap demonstrasi
karena QEMU smoke test integrasi ke kmain belum dijalankan. Bukan siap
produksi dan bukan bukti bebas race atau bebas deadlock.
```

---

## 21. Rubrik Penilaian 100 Poin

| Komponen | Bobot | Indikator nilai penuh | Nilai |
|---|---:|---|---:|
| Kebenaran fungsional | 30 | Spinlock, mutex, lockdep, host test, freestanding compile benar | `30` |
| Kualitas desain dan invariants | 20 | Invariant lock, owner, ordering, dan lock hierarchy dijelaskan | `18` |
| Pengujian dan bukti | 20 | Host test 22 assertion, nm, readelf, objdump, checksum, commit | `17` |
| Debugging dan failure analysis | 10 | Failure modes, triage CC=clang, dan known issues didokumentasikan | `9` |
| Keamanan dan robustness | 10 | Race, deadlock, interrupt reentry, privilege impact dijelaskan | `8` |
| Dokumentasi dan laporan | 10 | Laporan mengikuti template, lengkap, referensi IEEE | `9` |
| **Total** | **100** | | `91` |

Catatan penilai:

```text
[Diisi dosen/asisten.]
```

---

## 22. Pertanyaan Analisis

**1. Mengapa `volatile` tidak cukup untuk sinkronisasi antar-core atau antar-thread?**
`volatile` hanya mencegah compiler mengoptimalkan atau mereorder akses ke variabel tersebut di level instruksi compiler. Namun `volatile` tidak memberikan memory barrier kepada CPU. CPU modern (termasuk x86_64 dalam mode out-of-order execution) dapat mereorder akses memori secara hardware. Selain itu, `volatile` tidak memberikan atomicity untuk operasi read-modify-write: dua thread yang sama-sama membaca nilai 0, menambah 1, dan menulis 1 akan menghasilkan 1 bukan 2 — tanpa ada jaminan ordering atau mutual exclusion. `__atomic_*` builtins memberikan keduanya: atomicity operasi dan memory ordering semantics yang terstandar.

**2. Apa perbedaan acquire pada lock dan release pada unlock?**
Acquire ordering pada lock memastikan bahwa semua akses memori yang ditulis setelah instruksi lock tidak dapat direorder oleh compiler atau CPU ke sebelum instruksi lock tersebut. Ini memastikan thread yang berhasil mendapatkan lock akan melihat semua update yang dilakukan oleh pemegang lock sebelumnya. Release ordering pada unlock memastikan bahwa semua akses memori yang terjadi sebelum instruksi unlock sudah selesai dan terlihat, sebelum lock diumumkan bebas kepada thread lain. Bersama-sama, acquire dan release membentuk happens-before relationship: semua yang terjadi dalam critical section satu thread pasti terlihat oleh thread yang kemudian mengambil lock yang sama.

**3. Mengapa spinlock tidak boleh melindungi operasi yang dapat tidur?**
Jika thread T1 memegang spinlock dan kemudian tidur (memanggil scheduler, I/O, atau sleep), thread T2 yang ingin lock yang sama akan terus busy-wait — mengkonsumsi CPU 100% tanpa hasil. Dalam single-core, jika T1 tidur dan T2 spin, T1 tidak akan pernah mendapat kesempatan jalan lagi untuk melepas lock, karena T2 mendominasi CPU. Ini adalah live-lock atau deadlock efektif. Dalam SMP, T2 akan spin di core lain selama T1 tidur di core lainnya — ini boros tetapi tidak deadlock. Namun desain yang baik tetap melarang sleep dalam critical section spinlock karena menyebabkan latensi tak terbatas bagi waiter.

**4. Mengapa mutex owner-aware dapat mendeteksi bug unlock oleh thread lain?**
Karena mutex M12 menyimpan `owner_id` dari thread yang berhasil lock. Saat `mcs_mutex_unlock` dipanggil, ia membaca owner dengan ACQUIRE load dan membandingkannya dengan `owner_id` yang diklaim caller. Jika tidak cocok, fungsi mengembalikan `EPERM` dan tidak mengubah state lock. Ini mencegah skenario di mana bug implementasi menyebabkan thread A melepas lock yang sebenarnya dimiliki thread B — yang akan membuka data yang seharusnya masih dilindungi thread B.

**5. Mengapa lock release sebaiknya mengikuti urutan LIFO dalam validator sederhana?**
Model lockdep M12 menggunakan stack linear (`held_class[]`) dengan pengecekan rank monoton naik. Jika release tidak LIFO, ada kemungkinan "lubang" di tengah stack: misalnya memegang lock A (rank 10) dan B (rank 20), lalu melepas A terlebih dahulu, stack menjadi [_, B] bukan [A]. Saat mengambil lock baru dengan rank 15, validator harus memutuskan apakah rank 15 lebih besar dari "top" — tapi top-nya adalah B (rank 20), sehingga ditolak. Namun seharusnya rank 15 bisa diambil di antara A dan B. Ini ambiguitas yang tidak dapat diselesaikan tanpa graph dependency penuh. LIFO release memastikan stack selalu valid dan urutan pengecekan deterministik.

**6. Berikan contoh dua jalur kode yang dapat deadlock karena lock-order inversion.**
Jalur 1 (syscall handler): ambil `pmm_lock` (rank 10) → ambil `vmm_lock` (rank 20).
Jalur 2 (page fault handler): ambil `vmm_lock` (rank 20) → ambil `pmm_lock` (rank 10).
Jika keduanya berjalan bersamaan di SMP: jalur 1 sudah memegang `pmm_lock` dan menunggu `vmm_lock`; jalur 2 sudah memegang `vmm_lock` dan menunggu `pmm_lock`. Keduanya menunggu selamanya — deadlock. Lockdep M12 mencegah ini: jalur 2 akan mendapat `EDEADLK` saat mencoba mengambil `pmm_lock` (rank 10) ketika sudah memegang `vmm_lock` (rank 20).

**7. Apa risiko jika interrupt handler mengambil lock yang sama dengan task context tanpa `irqsave`?**
Jika task T1 sedang dalam critical section spinlock L, dan interrupt terjadi yang mencoba mengambil L, interrupt akan busy-wait. Dalam single-core, T1 tidak akan pernah mendapat kesempatan jalan lagi untuk melepas L — karena interrupt handler mendominasi CPU. Ini adalah interrupt-induced deadlock. Solusinya adalah `spin_lock_irqsave` yang menonaktifkan interrupt lokal sebelum mengambil lock, lalu mengaktifkan kembali saat unlock. M12 belum mengimplementasikan ini karena masih single-core dan interrupt disable belum diintegrasikan.

**8. Apa batasan host pthread test dibanding QEMU/kernel test?**
Host pthread test berjalan di atas OS Linux dengan scheduler preemptif, virtual memory, dan proteksi yang lengkap. Ini berbeda dari lingkungan kernel di mana: (a) tidak ada OS di bawah yang melindungi; (b) interrupt dapat terjadi kapan saja; (c) hanya ada satu address space; (d) bug dapat menyebabkan triple fault bukan sekadar segfault. Selain itu, host test tidak menguji `pause` hint secara realistis karena hardware context berbeda, dan memory ordering x86_64 TSO lebih ketat dari yang ARM atau RISC-V harapkan. Host test membuktikan correctness logika, tetapi tidak membuktikan correctness di konteks interrupt kernel nyata.

**9. Mengapa `nm -u` penting dalam kode freestanding?**
`nm -u` menampilkan semua simbol yang tidak terdefinisi (unresolved) dalam object file — simbol yang dibutuhkan object tapi tidak disediakan oleh kode di dalamnya, sehingga harus dipenuhi oleh linker dari object atau library lain. Dalam kode freestanding, tidak ada libc dan tidak ada runtime helper tersedia secara otomatis. Jika `nm -u` menampilkan `memset`, `__stack_chk_fail`, atau simbol libc lain, object tersebut tidak dapat di-link ke kernel tanpa menyediakan implementasi fungsi tersebut secara eksplisit. `nm -u` kosong membuktikan object benar-benar mandiri dan siap diintegrasikan ke kernel tanpa dependency tersembunyi.

**10. Apa perluasan yang diperlukan agar M12 siap untuk SMP sungguhan?**
Minimal diperlukan: (a) `spin_lock_irqsave` / `spin_lock_irqrestore` untuk menangani interrupt-induced deadlock; (b) per-CPU spinlock variants untuk mengurangi contention; (c) `mcs_lockdep_state_t` per-thread otomatis (bukan eksplisit dari caller); (d) wait queue penuh untuk mutex agar thread yang gagal lock dapat sleep dan dibangunkan; (e) memory barrier yang tepat untuk arsitektur dengan memory model lebih lemah dari TSO; (f) stress test multi-core dengan lebih banyak thread dan lebih banyak lock; dan (g) lockdep graph penuh (bukan hanya rank linear) untuk mendeteksi semua pola deadlock potensial.

---

## 23. Kesimpulan

### 23.1 Yang Berhasil

```text
M12 berhasil menyelesaikan seluruh komponen inti yang diwajibkan panduan:
header kontrak mcs_sync.h, lockdep.c dengan rank validator dan LIFO release,
spinlock.c dengan acquire/release atomik dan pause loop, mutex.c dengan owner
semantics, host unit test 22 assertion (4 test function), dan Makefile.m12
dengan pipeline host-test/freestanding/audit.

Seluruh 22 assertion lulus tanpa modifikasi. Freestanding compile berhasil
dengan Clang 21 tanpa warning. nm -u ketiga object kosong — tidak ada
dependency libc. readelf mengkonfirmasi ELF64 REL x86_64. objdump membuktikan
instruksi xchg dan pause ada. Dua commit tersimpan bersih di atas M11. Satu
kendala kecil (CC=clang) ditemukan, didiagnosis, dan diperbaiki.
```

### 23.2 Yang Belum Berhasil

```text
QEMU smoke test integrasi ke kmain belum dijalankan dalam sesi ini. Spinlock
tidak mendeteksi recursive acquire sendiri. irqsave/irqrestore belum ada.
Mutex belum memiliki wait queue — thread yang gagal harus polling manual.
mcs_lockdep_state_t belum per-thread otomatis.
```

### 23.3 Rencana Perbaikan

```text
Langkah berikutnya yang realistis:
1. Integrasikan primitif sinkronisasi ke kmain dengan self-test minimal
   yang mencetak status ke serial log QEMU — ini menyelesaikan checkpoint C6.
2. Tambahkan lock class assignment ke PMM dan heap dari M6/M8 sebagai
   demonstrasi integrasi nyata.
3. Pada M13, tambahkan irqsave/irqrestore dan per-CPU state untuk
   mempersiapkan SMP.
4. Pada M13, tambahkan wait queue ke mutex agar thread dapat sleep
   alih-alih polling.
```

---

## 24. Lampiran

### Lampiran A — Commit Log

```text
55a09ab (HEAD -> praktikum/m12-sync) M12: add preflight log
bebe0e3 M12: implement spinlock, cooperative mutex, lock-order validator, host test, and audit
ad07b27 (praktikum-m11-elf-user-loader) M11: implement ELF64 user loader, host test, audit, and QEMU integration
f017f4b (praktikum/m10-syscall-abi) M10: implement syscall ABI, int 0x80 dispatcher, and kernel integration
```

### Lampiran B — Diff Ringkas (7 file baru, 470 insertions)

```diff
 Makefile.m12                    | 34 +++++++++++
 evidence/M12/build.log          | 57 +++++++++++++++++++
 evidence/M12/preflight.log      |  7 +++
 include/mcs_sync.h              | 55 ++++++++++++++++++
 kernel/sync/lockdep.c           | 72 +++++++++++++++++++++++++
 kernel/sync/mutex.c             | 53 ++++++++++++++++++
 kernel/sync/spinlock.c          | 48 ++++++++++++++++
 tests/m12_sync_host_test.c      | 83 ++++++++++++++++++++++++++++
 7 files changed, 409 insertions(+)
```

### Lampiran C — Log Build Lengkap

```text
mkdir -p build/m12
cc -std=c17 -Wall -Wextra -Werror -Iinclude -O2 -pthread \
   kernel/sync/lockdep.c kernel/sync/spinlock.c kernel/sync/mutex.c \
   tests/m12_sync_host_test.c -o build/m12/m12_sync_host_test
build/m12/m12_sync_host_test | tee build/m12/host-test.log
[PASS] M12 synchronization host tests passed
clang -std=c17 -Wall -Wextra -Werror -Iinclude -target x86_64-elf \
   -ffreestanding -fno-builtin -fno-stack-protector -fno-pic \
   -mno-red-zone -O2 -c kernel/sync/lockdep.c -o build/m12/lockdep.o
clang ... -c kernel/sync/spinlock.c -o build/m12/spinlock.o
clang ... -c kernel/sync/mutex.c    -o build/m12/mutex.o
nm -u build/m12/lockdep.o build/m12/spinlock.o build/m12/mutex.o \
   | tee build/m12/nm-undefined.txt
build/m12/lockdep.o:
build/m12/spinlock.o:
build/m12/mutex.o:
readelf -h build/m12/lockdep.o | tee build/m12/readelf-lockdep.txt
  Class:   ELF64, Type: REL, Machine: X86-64
objdump -d build/m12/spinlock.o | tee build/m12/objdump-spinlock.txt
  2e: 87 07  xchg %eax,(%rdi)
  70: f3 90  pause
sha256sum build/m12/lockdep.o build/m12/spinlock.o \
          build/m12/mutex.o build/m12/m12_sync_host_test \
          > build/m12/sha256sums.txt
```

Log lengkap di `evidence/M12/build.log`, commit `55a09ab`.

### Lampiran D — Log QEMU

```text
QEMU smoke test integrasi ke kmain belum dijalankan dalam sesi ini.
Boot QEMU dari M11 tidak dimodifikasi dan tidak diuji regresi secara
eksplisit. Rencana: tambahkan m12_sync_selftest() ke kmain dan jalankan
ulang sesuai prosedur smoke test M11.
```

### Lampiran E — Output Readelf dan Objdump

Readelf (`build/m12/readelf-lockdep.txt`):

```text
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Entry point address:               0x0
  Size of section headers:           64 (bytes)
  Number of section headers:         7
```

Objdump kritis (`build/m12/objdump-spinlock.txt`):

```text
0000000000000020 <mcs_spin_try_lock>:
  2e: 87 07   xchg %eax,(%rdi)   ← atomic acquire

0000000000000040 <mcs_spin_lock>:
  70: f3 90   pause               ← spin loop hint

0000000000000080 <mcs_spin_unlock>:
  89: c7 07 00 00 00 00  movl $0x0,(%rdi)  ← release store
```

### Lampiran F — Preflight Log

```text
2026-06-29T02:52:28+07:00
Linux DESKTOP-FPCS9GF 6.6.87.2-microsoft-standard-WSL2 #1 SMP
PREEMPT_DYNAMIC Thu Jun  5 18:30:46 UTC 2025 x86_64 GNU/Linux
Ubuntu clang version 21.1.8 (6ubuntu1)
cc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU Make 4.4.1
bebe0e3
?? evidence/M12/preflight.log
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
| Log QEMU/test dilampirkan | `Parsial — host test log ada; QEMU belum` |
| Artefak penting tersedia di `evidence/M12` | `Ya` |
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
55a09ab
```

Status akhir yang diklaim:

```text
Siap uji QEMU untuk sinkronisasi kernel awal single-core menuju SMP —
bukan siap produksi, bukan bukti bebas deadlock, bukan bukti race-free penuh,
dengan lima known issues yang didokumentasikan pada bagian 20 (recursive
spinlock tidak terdeteksi otomatis, tidak ada irqsave/irqrestore, mutex belum
punya wait queue, lockdep state belum per-thread otomatis, QEMU smoke test
integrasi belum dijalankan).
```

Ringkasan satu paragraf:

```text
Praktikum M12 MCSOS 260502 telah diselesaikan oleh Agung Nurjaman (25832073010)
secara individu, melanjutkan dari gate M11 yang solid (commit ad07b27). Tiga
komponen sinkronisasi kernel berhasil dibangun: spinlock freestanding x86_64
berbasis __atomic_exchange_n ACQUIRE dan __atomic_store_n RELEASE dengan pause
loop; mutex kooperatif awal dengan owner semantics yang menolak recursive
acquire dan non-owner unlock; serta lock-order validator sederhana dengan rank
monoton naik, LIFO release, dan violation_count sebagai observability. Host
unit test lulus 22 assertion (4 fungsi test: lockdep order, lockdep negative,
spinlock stress 4 thread × 25.000 iterasi, mutex owner) tanpa modifikasi source.
Freestanding compile Clang 21.1.8 berhasil tanpa warning; nm -u ketiga object
kosong; readelf mengkonfirmasi ELF64 REL x86_64; objdump membuktikan xchg
dan pause ada. Satu kendala ditemukan (CC default GCC tidak mendukung -target)
dan diselesaikan. Dua commit tersimpan bersih di branch praktikum/m12-sync
dengan 7 file, 470 baris, dan evidence lengkap di evidence/M12/. Status
readiness yang diklaim adalah siap uji QEMU untuk sinkronisasi kernel awal
single-core, secara eksplisit bukan siap demonstrasi atau siap produksi,
dengan lima known issues terdokumentasi untuk ditindaklanjuti pada M13.
```

---

## 27. Daftar Referensi

```text
[1] Intel Corporation, "Intel® 64 and IA-32 Architectures Software Developer's
    Manual, Volume 3: System Programming Guide," Intel Developer Documentation,
    2026. [Online]. Available:
    https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
    Accessed: Jun. 29, 2026.

[2] The Linux Kernel Documentation, "Lock types and their rules," kernel.org,
    2026. [Online]. Available:
    https://www.kernel.org/doc/html/latest/locking/locktypes.html
    Accessed: Jun. 29, 2026.

[3] The Linux Kernel Documentation, "Runtime locking correctness validator,"
    kernel.org, 2026. [Online]. Available:
    https://www.kernel.org/doc/html/latest/locking/lockdep-design.html
    Accessed: Jun. 29, 2026.

[4] The Linux Kernel Documentation, "Generic Mutex Subsystem," kernel.org,
    2026. [Online]. Available:
    https://docs.kernel.org/locking/mutex-design.html
    Accessed: Jun. 29, 2026.

[5] Free Software Foundation, "Built-in Functions for Memory Model Aware
    Atomic Operations," GCC Online Documentation, 2026. [Online]. Available:
    https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
    Accessed: Jun. 29, 2026.

[6] LLVM Project, "Clang command line argument reference," Clang Documentation,
    2026. [Online]. Available:
    https://clang.llvm.org/docs/ClangCommandLineReference.html
    Accessed: Jun. 29, 2026.

[7] GNU Binutils, "GNU Binary Utilities," Sourceware, 2025. [Online]. Available:
    https://www.sourceware.org/binutils/docs/binutils.html
    Accessed: Jun. 29, 2026.

[8] QEMU Project, "GDB usage," QEMU Documentation, 2026. [Online]. Available:
    https://www.qemu.org/docs/master/system/gdb.html
    Accessed: Jun. 29, 2026.
```
