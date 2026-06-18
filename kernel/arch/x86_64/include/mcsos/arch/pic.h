#ifndef MCSOS_ARCH_PIC_H
#define MCSOS_ARCH_PIC_H

#include <stdint.h>

#define X86_64_PIC_MASTER_COMMAND  0x20u
#define X86_64_PIC_MASTER_DATA     0x21u
#define X86_64_PIC_SLAVE_COMMAND   0xA0u
#define X86_64_PIC_SLAVE_DATA      0xA1u

#define X86_64_PIC_MASTER_VECTOR_OFFSET  0x20u
#define X86_64_PIC_SLAVE_VECTOR_OFFSET   0x28u

#define X86_64_PIC_EOI  0x20u

void    x86_64_pic_remap(uint8_t master_offset, uint8_t slave_offset);
void    x86_64_pic_mask_all(void);
void    x86_64_pic_unmask_irq(uint8_t irq);
void    x86_64_pic_mask_irq(uint8_t irq);
void    x86_64_pic_send_eoi(uint8_t irq);
uint8_t x86_64_pic_read_master_mask(void);
uint8_t x86_64_pic_read_slave_mask(void);

#endif
