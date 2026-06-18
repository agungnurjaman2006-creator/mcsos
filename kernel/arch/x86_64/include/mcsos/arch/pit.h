#ifndef MCSOS_ARCH_PIT_H
#define MCSOS_ARCH_PIT_H

#include <stdint.h>

#define X86_64_PIT_CHANNEL0_DATA  0x40u
#define X86_64_PIT_COMMAND        0x43u
#define X86_64_PIT_BASE_FREQUENCY_HZ  1193182u
#define X86_64_PIT_CMD_CH0_LOHI_MODE3_BINARY  0x36u

void     x86_64_pit_configure_hz(uint32_t hz);
uint64_t x86_64_timer_ticks(void);
void     x86_64_timer_on_irq0(void);

#endif
