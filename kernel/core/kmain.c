#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>
#include "pmm.h"

extern char __kernel_start[];
extern char __kernel_end[];

static struct pmm_state g_pmm;
static uint8_t g_pmm_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));

#define M6_DUMMY_MAX_PHYS_BYTES (512ULL * 1024ULL * 1024ULL)

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
    /*
     * __kernel_start dan __kernel_end adalah alamat VIRTUAL higher-half
     * (0xffffffff80000000 dst, lihat linker.ld), bukan alamat fisik.
     * pmm_init_from_map mengasumsikan setiap region.base adalah alamat
     * fisik. Tanpa membaca Limine kernel-address-request atau HHDM
     * offset asli (belum diimplementasikan pada M6 ini), kita TIDAK
     * memiliki cara sah mengonversi virtual ke physical di sini.
     *
     * Sebagai simplifikasi M6 awal yang didokumentasikan secara jujur
     * di laporan (bukan asumsi diam-diam), kernel image direpresentasikan
     * dengan rentang fisik rendah konservatif (0x100000-0x300000, 2 MiB)
     * tempat kernel x86_64 freestanding lazimnya di-load oleh bootloader
     * sebelum higher-half remapping. Ini BUKAN alamat fisik kernel yang
     * sesungguhnya, hanya placeholder protektif. Integrasi Limine asli
     * (kernel-address-request) adalah pekerjaan lanjutan M6/M7.
     */
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

    log_writeln("[m6] pmm initialized");
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
    log_writeln("[m6] sample alloc/free roundtrip ok");
}

void kmain(void) {
    cpu_cli();

    log_init();
    log_write(MCSOS_NAME); log_write(" ");
    log_write(MCSOS_VERSION); log_write(" ");
    log_write(MCSOS_MILESTONE);
    log_writeln(" kernel entered");
    log_key_value_hex64("kernel_start",    (uint64_t)(uintptr_t)__kernel_start);
    log_key_value_hex64("kernel_end",      (uint64_t)(uintptr_t)__kernel_end);
    log_key_value_hex64("rflags_before_idt", cpu_read_rflags());

    x86_64_idt_init();
    m4_selftest();

    m6_pmm_init_dummy();

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
