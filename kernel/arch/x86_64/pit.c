#include <mcsos/arch/io.h>
#include <mcsos/arch/pit.h>
#include <mcsos/kernel/log.h>

static volatile uint64_t g_ticks = 0u;

void x86_64_pit_configure_hz(uint32_t hz) {
    uint32_t divisor = X86_64_PIT_BASE_FREQUENCY_HZ / hz;

    if (divisor > 0xFFFFu) {
        divisor = 0xFFFFu;
    }
    if (divisor == 0u) {
        divisor = 1u;
    }

    outb(X86_64_PIT_COMMAND, X86_64_PIT_CMD_CH0_LOHI_MODE3_BINARY);
    outb(X86_64_PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFFu));
    outb(X86_64_PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8u) & 0xFFu));

    log_key_value_hex64("pit_hz", (uint64_t)hz);
    log_key_value_hex64("pit_divisor", (uint64_t)divisor);
    log_writeln("[M5] PIT configured");
}

uint64_t x86_64_timer_ticks(void) {
    return g_ticks;
}

void x86_64_timer_on_irq0(void) {
    g_ticks++;
    if ((g_ticks % 100u) == 0u) {
        log_key_value_hex64("ticks", g_ticks);
    }
}
