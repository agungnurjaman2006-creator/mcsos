#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/arch/pic.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>

extern char __kernel_start[];
extern char __kernel_end[];

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
