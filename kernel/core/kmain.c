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
    m5_idle_loop();
#endif
}
