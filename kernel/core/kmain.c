#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>
#include "pmm.h"
#include "vmm.h"
#include "mcsos/kmem.h"
#include "mcsos/user/m11_elf_loader.h"
#include "mcsos_thread.h"
#include "mcs_vfs.h"
#define MCSOS_M10_TEST_INT80 1
#include "mcsos/syscall.h"

extern void x86_64_syscall_int80_stub(void);

extern char __kernel_start[];
extern char __kernel_end[];

static struct pmm_state g_pmm;
static uint8_t g_pmm_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));

static struct vmm_space g_vmm;

#define M6_DUMMY_MAX_PHYS_BYTES (512ULL * 1024ULL * 1024ULL)

/* HHDM offset placeholder untuk M7 — pada kernel nyata diambil dari
   Limine HhdmRequest. Nilai 0 berarti physical == virtual (identity map),
   yang hanya valid selama bootloader masih menyediakan identity mapping. */
static uint64_t g_hhdm_offset = 0ULL;

static mcsos_scheduler_t g_sched;
static mcsos_thread_t g_boot_thread;
static mcsos_thread_t g_thread_a;
static mcsos_thread_t g_thread_b;
static unsigned char g_stack_a[8192] __attribute__((aligned(16)));
static unsigned char g_stack_b[8192] __attribute__((aligned(16)));

static void m9_demo_thread_a(void *arg) {
    (void)arg;
    for (;;) {
        log_writeln("[M9] thread A tick");
        mcsos_sched_yield(&g_sched);
    }
}

static void m9_demo_thread_b(void *arg) {
    (void)arg;
    for (;;) {
        log_writeln("[M9] thread B tick");
        mcsos_sched_yield(&g_sched);
    }
}

static void m4_selftest(void) {
    KERNEL_ASSERT(__kernel_end > __kernel_start);
    KERNEL_ASSERT(sizeof(uintptr_t) == 8u);
    KERNEL_ASSERT(sizeof(x86_64_idt_entry_t) == 16u);
    KERNEL_ASSERT(x86_64_idt_base_for_test() != 0u);
    KERNEL_ASSERT(x86_64_idt_limit_for_test() == 4095u);
    log_writeln("[M4] selftest: IDT invariants passed");
}

__attribute__((unused)) static void m5_selftest(void) {
    uint8_t master_mask = x86_64_pic_read_master_mask();
    KERNEL_ASSERT((master_mask & 0x01u) == 0u);
    KERNEL_ASSERT((master_mask & 0xFEu) == 0xFEu);
    log_writeln("[M5] selftest: PIC mask invariants passed");
}

__attribute__((noreturn)) __attribute__((unused)) static void m5_idle_loop(void) {
    for (;;) {
        cpu_hlt();
    }
}

static void m6_pmm_init_dummy(void) {
    uint64_t kernel_placeholder_base = 0x0000000000100000ULL;
    uint64_t kernel_placeholder_len  = 0x0000000000200000ULL;

    struct boot_mem_region regions[] = {
        { .base = 0x0000000000000000ULL, .length = 0x000000000009f000ULL, .type = BOOT_MEM_USABLE },
        { .base = 0x000000000009f000ULL, .length = 0x0000000000001000ULL, .type = BOOT_MEM_RESERVED },
        { .base = 0x0000000000100000ULL, .length = (M6_DUMMY_MAX_PHYS_BYTES - 0x100000ULL), .type = BOOT_MEM_USABLE },
        { .base = kernel_placeholder_base, .length = kernel_placeholder_len, .type = BOOT_MEM_KERNEL_AND_MODULES },
    };

    bool ok = pmm_init_from_map(&g_pmm, regions,
                                 sizeof(regions) / sizeof(regions[0]),
                                 g_pmm_bitmap, sizeof(g_pmm_bitmap),
                                 M6_DUMMY_MAX_PHYS_BYTES);
    if (!ok) {
        KERNEL_PANIC("pmm_init_from_map failed", 0u);
    }

    log_writeln("[M6] PMM initialized");
    log_key_value_hex64("pmm_frame_count", pmm_frame_count(&g_pmm));
    log_key_value_hex64("pmm_free_frames", pmm_free_count(&g_pmm));
    log_key_value_hex64("pmm_used_frames", pmm_used_count(&g_pmm));

    uint64_t frame = pmm_alloc_frame(&g_pmm);
    if (frame == PMM_INVALID_FRAME) {
        KERNEL_PANIC("pmm_alloc_frame returned invalid", 0u);
    }
    log_key_value_hex64("pmm_sample_frame", frame);
    if (!pmm_free_frame(&g_pmm, frame)) {
        KERNEL_PANIC("pmm_free_frame failed", 0u);
    }
    log_writeln("[M6] sample alloc/free roundtrip ok");
}

/* Adapter VMM: alokasi frame dari PMM M6 */
static uint64_t m7_vmm_alloc(void *ctx) {
    (void)ctx;
    return pmm_alloc_frame(&g_pmm);
}

static void m7_vmm_free(void *ctx, uint64_t paddr) {
    (void)ctx;
    pmm_free_frame(&g_pmm, paddr);
}

/* Adapter VMM: physical -> virtual melalui HHDM offset */
static void *m7_phys_to_virt(void *ctx, uint64_t paddr) {
    uint64_t offset = *(const uint64_t *)ctx;
    return (void *)(offset + paddr);
}

static void m7_vmm_init(void) {
    /* Alokasi root page table dari PMM */
    uint64_t root = pmm_alloc_frame(&g_pmm);
    if (root == PMM_INVALID_FRAME) {
        KERNEL_PANIC("M7: cannot allocate root page table frame", 0u);
    }

    /* Zero root page table melalui identity map (HHDM offset = 0) */
    uint64_t *root_virt = (uint64_t *)(g_hhdm_offset + root);
    for (size_t i = 0; i < 512; i++) {
        root_virt[i] = 0;
    }

    int rc = vmm_space_init(&g_vmm, root, &g_hhdm_offset,
                            m7_vmm_alloc, m7_vmm_free, m7_phys_to_virt);
    if (rc != VMM_MAP_OK) {
        KERNEL_PANIC("M7: vmm_space_init failed", (uint64_t)(uint32_t)rc);
    }

    log_writeln("[M7] VMM core initialized");
    log_key_value_hex64("vmm_root_paddr", root);

    /* Uji map/query/unmap satu halaman sebagai smoke test VMM */
    uint64_t test_vaddr = 0x0000000000600000ULL;
    uint64_t test_paddr = 0x0000000000700000ULL;
    rc = vmm_map_page(&g_vmm, test_vaddr, test_paddr, VMM_PTE_WRITABLE);
    if (rc != VMM_MAP_OK) {
        KERNEL_PANIC("M7: vmm_map_page smoke test failed", (uint64_t)(uint32_t)rc);
    }

    struct vmm_mapping m;
    rc = vmm_query_page(&g_vmm, test_vaddr, &m);
    if (rc != VMM_MAP_OK || m.paddr != test_paddr) {
        KERNEL_PANIC("M7: vmm_query_page smoke test failed", (uint64_t)(uint32_t)rc);
    }

    rc = vmm_unmap_page(&g_vmm, test_vaddr);
    if (rc != VMM_MAP_OK) {
        KERNEL_PANIC("M7: vmm_unmap_page smoke test failed", (uint64_t)(uint32_t)rc);
    }

    log_writeln("[M7] VMM map/query/unmap smoke test passed");
    log_writeln("[M7] ready for QEMU smoke test and GDB audit");
}


#define M8_BOOT_HEAP_SIZE (64u * 1024u)
static unsigned char m8_boot_heap[M8_BOOT_HEAP_SIZE] __attribute__((aligned(4096)));

static void m8_heap_bootstrap(void) {
    int rc = kmem_init(m8_boot_heap, sizeof(m8_boot_heap));
    if (rc != 0) {
        KERNEL_PANIC("M8 kmem_init failed", (uint64_t)(uint32_t)rc);
    }
    void *probe = kmem_alloc(128);
    if (probe == (void *)0) {
        KERNEL_PANIC("M8 kmem_alloc probe failed", 0u);
    }
    if (kmem_free_checked(probe) != 0) {
        KERNEL_PANIC("M8 kmem_free_checked probe failed", 0u);
    }
    kmem_stats_t st;
    kmem_get_stats(&st);
    log_writeln("[M8] kmem initialized");
    log_key_value_hex64("kmem_total",   (uint64_t)st.total_bytes);
    log_key_value_hex64("kmem_free",    (uint64_t)st.free_bytes);
    log_key_value_hex64("kmem_largest", (uint64_t)st.largest_free);
    log_key_value_hex64("kmem_blocks",  (uint64_t)st.block_count);
    log_writeln("[M8] heap probe alloc/free roundtrip ok");
}

static void m9_scheduler_bootstrap(void) {
    int rc = mcsos_scheduler_init(&g_sched, &g_boot_thread);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9 mcsos_scheduler_init failed", (uint64_t)(uint32_t)rc);
    }

    rc = mcsos_thread_prepare(&g_thread_a, "demo-a", m9_demo_thread_a, (void *)0,
                              g_stack_a, sizeof(g_stack_a), g_sched.next_id++);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9 mcsos_thread_prepare a failed", (uint64_t)(uint32_t)rc);
    }

    rc = mcsos_thread_prepare(&g_thread_b, "demo-b", m9_demo_thread_b, (void *)0,
                              g_stack_b, sizeof(g_stack_b), g_sched.next_id++);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9 mcsos_thread_prepare b failed", (uint64_t)(uint32_t)rc);
    }

    rc = mcsos_sched_enqueue(&g_sched, &g_thread_a);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9 mcsos_sched_enqueue a failed", (uint64_t)(uint32_t)rc);
    }

    rc = mcsos_sched_enqueue(&g_sched, &g_thread_b);
    if (rc != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9 mcsos_sched_enqueue b failed", (uint64_t)(uint32_t)rc);
    }

    if (mcsos_sched_validate(&g_sched) != MCSOS_SCHED_OK) {
        KERNEL_PANIC("M9 mcsos_sched_validate failed after setup", 0u);
    }

    log_writeln("[M9] scheduler initialized");
    log_key_value_hex64("m9_ready_count", (uint64_t)mcsos_sched_ready_count(&g_sched));
}

static int64_t k_write_serial_bounded(const char *buf, size_t len) {
    if (len > 256u) {
        len = 256u;
    }
    for (size_t i = 0; i < len; i++) {
        char c[2];
        c[0] = buf[i];
        c[1] = 0;
        log_write(c);
    }
    return (int64_t)len;
}

static uint64_t k_get_ticks(void) {
    return x86_64_timer_ticks();
}

static void k_yield_current(void) {
    /* mcsos_sched_yield mengembalikan kode error MCSOS_SCHED_*, tapi
       kontrak mcsos_syscall_ops_t.yield_current adalah void(void).
       Kegagalan yield (sched belum init, current invalid) sengaja
       diabaikan di sini -- pemanggil syscall tetap menerima MCSOS_OK
       selama g_ops.yield_current != NULL, sesuai kontrak M10 bahwa
       hanya ketersediaan callback yang dicek, bukan hasil internalnya. */
    (void)mcsos_sched_yield(&g_sched);
}

static void k_exit_current(int code) {
    /* M9 belum memiliki thread_exit/teardown stack yang aman (state
       MCSOS_THREAD_ZOMBIE ada di enum tapi tidak pernah dipakai).
       Sesuai rekomendasi panduan M10 bagian Langkah 7, exit_current
       dibuat sebagai stub yang mencatat kode keluar dan memanggil
       panic terkendali, bukan diam-diam no-op atau melepas stack
       yang sedang dipakai thread itu sendiri. */
    log_write("[M10] exit_thread stub called with code=");
    log_hex64((uint64_t)(int64_t)code);
    log_writeln("");
    KERNEL_PANIC("M10 exit_thread stub: M9 has no real thread teardown yet", (uint64_t)(int64_t)code);
}

static void m10_syscall_bootstrap(void) {
    mcsos_syscall_ops_t ops = {
        .get_ticks = k_get_ticks,
        .yield_current = k_yield_current,
        .exit_current = k_exit_current,
        .write_serial = k_write_serial_bounded,
    };
    mcsos_syscall_init(&ops);

    static char m10_user_buf[64] = "ping-from-simulated-user-region";
    mcsos_syscall_set_user_region((mcsos_user_region_t){
        .base = (uintptr_t)&m10_user_buf[0],
        .limit = (uintptr_t)&m10_user_buf[0] + sizeof(m10_user_buf),
    });

    log_writeln("[M10] syscall init");

    int64_t r = mcsos_syscall_dispatch(MCSOS_SYS_PING, 0, 0, 0, 0, 0, 0);
    if (r != 0x2605020A) {
        KERNEL_PANIC("M10 syscall ping failed", (uint64_t)r);
    }
    log_writeln("[M10] syscall ping ok");

    int64_t ticks = mcsos_syscall_dispatch(MCSOS_SYS_GET_TICKS, 0, 0, 0, 0, 0, 0);
    if (ticks < 0) {
        KERNEL_PANIC("M10 syscall get_ticks failed", (uint64_t)ticks);
    }
    log_writeln("[M10] syscall get_ticks ok");

    log_writeln("[M10] syscall smoke done");

#ifdef MCSOS_M10_TEST_INT80
    x86_64_idt_set_gate(0x80, (uint64_t)(uintptr_t)x86_64_syscall_int80_stub, X86_64_IDT_GATE_INTERRUPT);
    log_writeln("[M10] int 0x80 gate installed");

    long int80_ret;
    __asm__ volatile (
        "movq $0, %%rax\n"
        "int $0x80\n"
        : "=a"(int80_ret)
        :
        : "rcx", "r11", "memory"
    );
    if (int80_ret != 0x2605020A) {
        KERNEL_PANIC("M10 int 0x80 ping mismatch", (uint64_t)int80_ret);
    }
    log_writeln("[M10] int 0x80 ping ok");
#endif
}

__attribute__((noreturn)) static void m9_scheduler_idle_loop(void) {
    for (;;) {
        mcsos_sched_yield(&g_sched);
    }
}


static void m11_elf_smoke_test(void) {
    static unsigned char synthetic_elf[12288];
    struct m11_elf64_ehdr *eh = (struct m11_elf64_ehdr *)(void *)synthetic_elf;
    struct m11_elf64_phdr *ph;
    struct m11_process_image_plan plan;
    struct m11_user_region region;
    int rc;

    for (size_t i = 0; i < sizeof(synthetic_elf); i++) {
        synthetic_elf[i] = 0;
    }

    eh->e_ident[0] = 0x7fu;
    eh->e_ident[1] = 'E';
    eh->e_ident[2] = 'L';
    eh->e_ident[3] = 'F';
    eh->e_ident[4] = 2u;
    eh->e_ident[5] = 1u;
    eh->e_ident[6] = 1u;
    eh->e_type      = 2u;
    eh->e_machine   = 62u;
    eh->e_version   = 1u;
    eh->e_entry     = 0x0000000000401000ull;
    eh->e_phoff     = sizeof(struct m11_elf64_ehdr);
    eh->e_ehsize    = (uint16_t)sizeof(struct m11_elf64_ehdr);
    eh->e_phentsize = (uint16_t)sizeof(struct m11_elf64_phdr);
    eh->e_phnum     = 2u;

    ph = (struct m11_elf64_phdr *)(void *)(synthetic_elf + eh->e_phoff);
    ph[0].p_type   = 1u;
    ph[0].p_flags  = 5u;
    ph[0].p_offset = 0x1000u;
    ph[0].p_vaddr  = 0x0000000000400000ull;
    ph[0].p_filesz = 16u;
    ph[0].p_memsz  = 4096u;
    ph[0].p_align  = 4096u;
    ph[1].p_type   = 1u;
    ph[1].p_flags  = 6u;
    ph[1].p_offset = 0x2000u;
    ph[1].p_vaddr  = 0x0000000000401000ull;
    ph[1].p_filesz = 8u;
    ph[1].p_memsz  = 4096u;
    ph[1].p_align  = 4096u;

    region.base  = 0x0000000000400000ull;
    region.limit = 0x0000008000000000ull;

    rc = m11_elf64_plan_load(synthetic_elf, sizeof(synthetic_elf), region, &plan);
    if (rc != 0) {
        log_writeln("[M11] elf: FAIL plan_load returned error");
        KERNEL_PANIC("M11 elf smoke test failed", (uint64_t)(uint32_t)rc);
    }
    log_writeln("[M11] elf: ident ok");
    log_key_value_hex64("elf_phnum",  (uint64_t)plan.segment_count);
    log_key_value_hex64("elf_entry",  plan.entry);
    log_key_value_hex64("seg0_vaddr", plan.segments[0].vaddr);
    log_key_value_hex64("seg0_flags", (uint64_t)plan.segments[0].flags);
    log_key_value_hex64("seg1_vaddr", plan.segments[1].vaddr);
    log_key_value_hex64("seg1_flags", (uint64_t)plan.segments[1].flags);
    log_writeln("[M11] elf: plan ok");
    log_writeln("[M11] user image plan ready");
}

static mcs_ramfs_t g_m13_ramfs;
static mcs_process_t g_m13_process;


void m14_block_demo_init(void);
void m16_kernel_smoke_test(void);

static void m13_vfs_bootstrap(void) {
    mcs_ramfs_init(&g_m13_ramfs);
    g_m13_process.pid = 1u;
    mcs_fd_table_init(&g_m13_process.fd_table);

    int rc = mcs_ramfs_seed_file(&g_m13_ramfs, "/motd.txt", (const uint8_t *)"mcsos-m13-ramfs", 15u);
    if (rc != MCS_OK) {
        KERNEL_PANIC("M13 seed file failed", (uint64_t)(int64_t)rc);
    }

    int fd = mcs_sys_open(&g_m13_process, &g_m13_ramfs, "/motd.txt", MCS_O_RDONLY);
    if (fd < 0) {
        KERNEL_PANIC("M13 open failed", (uint64_t)(int64_t)fd);
    }

    char readbuf[16];
    for (int i = 0; i < 16; i++) { readbuf[i] = 0; }
    mcs_ssize_t n = mcs_sys_read(&g_m13_process, fd, readbuf, 15u);
    if (n != 15) {
        KERNEL_PANIC("M13 read failed", (uint64_t)(int64_t)n);
    }

    int wfd = mcs_sys_open(&g_m13_process, &g_m13_ramfs, "/log.txt", MCS_O_CREAT | MCS_O_RDWR | MCS_O_TRUNC);
    if (wfd < 0) {
        KERNEL_PANIC("M13 create log.txt failed", (uint64_t)(int64_t)wfd);
    }
    mcs_ssize_t wn = mcs_sys_write(&g_m13_process, wfd, "boot-ok", 7u);
    if (wn != 7) {
        KERNEL_PANIC("M13 write log.txt failed", (uint64_t)(int64_t)wn);
    }

    if (mcs_sys_close(&g_m13_process, fd) != MCS_OK) {
        KERNEL_PANIC("M13 close motd.txt failed", 0u);
    }
    if (mcs_sys_close(&g_m13_process, wfd) != MCS_OK) {
        KERNEL_PANIC("M13 close log.txt failed", 0u);
    }

    log_writeln("[M13] ramfs+vfs smoke test passed");
    log_write("[M13] motd.txt content: ");
    log_writeln(readbuf);
}

void kmain(void) {
    cpu_cli();

    log_init();
    log_write(MCSOS_NAME); log_write(" ");
    log_write(MCSOS_VERSION); log_write(" ");
    log_write(MCSOS_MILESTONE);
    log_writeln(" kernel entered");
    log_key_value_hex64("kernel_start",      (uint64_t)(uintptr_t)__kernel_start);
    log_key_value_hex64("kernel_end",        (uint64_t)(uintptr_t)__kernel_end);
    log_key_value_hex64("rflags_before_idt", cpu_read_rflags());

    x86_64_idt_init();
    m4_selftest();

    m6_pmm_init_dummy();
    m7_vmm_init();
    m8_heap_bootstrap();
    m9_scheduler_bootstrap();
    m10_syscall_bootstrap();
    m11_elf_smoke_test();
    m13_vfs_bootstrap();
    m14_block_demo_init();
    m16_kernel_smoke_test();

#ifdef MCSOS_M4_TRIGGER_BREAKPOINT
    log_writeln("[M4] triggering intentional breakpoint exception");
    x86_64_trigger_breakpoint_for_test();
    log_writeln("[M4] returned from breakpoint handler");
#endif

#ifdef MCSOS_M4_TRIGGER_PANIC
    KERNEL_PANIC("intentional M4 panic test", 0x4D43534F533034u);
#else
    log_writeln("[M5] boot: external interrupt bring-up start");
    x86_64_pic_remap(X86_64_PIC_MASTER_VECTOR_OFFSET, X86_64_PIC_SLAVE_VECTOR_OFFSET);
    x86_64_pic_mask_all();
    x86_64_pic_unmask_irq(0u);
    m5_selftest();
    x86_64_pit_configure_hz(100u);
    log_writeln("[M5] sti: enabling interrupts");
    cpu_sti();
    log_writeln("[M4] IDT and exception dispatch path installed");
    log_writeln("[M5] ready for QEMU smoke test and GDB audit");
    m9_scheduler_idle_loop();
#endif
}
