#include <mcsos/arch/io.h>
#include <mcsos/arch/pic.h>
#include <mcsos/kernel/log.h>

void x86_64_pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    uint8_t saved_master_mask = inb(X86_64_PIC_MASTER_DATA);
    uint8_t saved_slave_mask  = inb(X86_64_PIC_SLAVE_DATA);

    outb(X86_64_PIC_MASTER_COMMAND, 0x11u);
    io_wait();
    outb(X86_64_PIC_SLAVE_COMMAND, 0x11u);
    io_wait();

    outb(X86_64_PIC_MASTER_DATA, master_offset);
    io_wait();
    outb(X86_64_PIC_SLAVE_DATA, slave_offset);
    io_wait();

    outb(X86_64_PIC_MASTER_DATA, 0x04u);
    io_wait();
    outb(X86_64_PIC_SLAVE_DATA, 0x02u);
    io_wait();

    outb(X86_64_PIC_MASTER_DATA, 0x01u);
    io_wait();
    outb(X86_64_PIC_SLAVE_DATA, 0x01u);
    io_wait();

    outb(X86_64_PIC_MASTER_DATA, saved_master_mask);
    outb(X86_64_PIC_SLAVE_DATA, saved_slave_mask);

    log_key_value_hex64("pic_master_offset", (uint64_t)master_offset);
    log_key_value_hex64("pic_slave_offset",  (uint64_t)slave_offset);
    log_writeln("[M5] PIC remapped");
}

void x86_64_pic_mask_all(void) {
    outb(X86_64_PIC_MASTER_DATA, 0xFFu);
    outb(X86_64_PIC_SLAVE_DATA, 0xFFu);
}

void x86_64_pic_unmask_irq(uint8_t irq) {
    uint16_t port = X86_64_PIC_MASTER_DATA;
    uint8_t  line = irq;

    if (irq >= 8u) {
        port = X86_64_PIC_SLAVE_DATA;
        line = (uint8_t)(irq - 8u);
    }

    uint8_t mask = inb(port);
    mask = (uint8_t)(mask & (uint8_t)~(1u << line));
    outb(port, mask);
}

void x86_64_pic_mask_irq(uint8_t irq) {
    uint16_t port = X86_64_PIC_MASTER_DATA;
    uint8_t  line = irq;

    if (irq >= 8u) {
        port = X86_64_PIC_SLAVE_DATA;
        line = (uint8_t)(irq - 8u);
    }

    uint8_t mask = inb(port);
    mask = (uint8_t)(mask | (uint8_t)(1u << line));
    outb(port, mask);
}

void x86_64_pic_send_eoi(uint8_t irq) {
    if (irq >= 8u) {
        outb(X86_64_PIC_SLAVE_COMMAND, X86_64_PIC_EOI);
    }
    outb(X86_64_PIC_MASTER_COMMAND, X86_64_PIC_EOI);
}

uint8_t x86_64_pic_read_master_mask(void) {
    return inb(X86_64_PIC_MASTER_DATA);
}

uint8_t x86_64_pic_read_slave_mask(void) {
    return inb(X86_64_PIC_SLAVE_DATA);
}
