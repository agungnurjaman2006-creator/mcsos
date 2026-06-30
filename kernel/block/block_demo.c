#include "mcsos/block.h"
#include "mcsos/kernel/log.h"
#include "mcsos/kernel/panic.h"

static unsigned char g_m14_ramdisk_storage[512u * 64u];
static mcsos_blk_device_t g_m14_ramdisk_dev;
static mcsos_ramblk_t g_m14_ramdisk;

void m14_block_demo_init(void) {
    mcsos_blk_status_t st;

    mcsos_blk_registry_reset();
    st = mcsos_ramblk_init(&g_m14_ramdisk_dev,
                           &g_m14_ramdisk,
                           "ram0",
                           g_m14_ramdisk_storage,
                           sizeof(g_m14_ramdisk_storage),
                           512u);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14 ramblk_init failed", (uint64_t)(uint32_t)(int)st);
    }

    st = mcsos_blk_register(&g_m14_ramdisk_dev);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14 blk_register failed", (uint64_t)(uint32_t)(int)st);
    }

    log_writeln("[M14] block layer initialized");
    log_key_value_hex64("blk_count",       (uint64_t)mcsos_blk_count());
    log_key_value_hex64("ram0_block_size", (uint64_t)g_m14_ramdisk_dev.block_size);
    log_key_value_hex64("ram0_block_count",(uint64_t)g_m14_ramdisk_dev.block_count);

    /* Smoke test: write/read satu blok untuk membuktikan jalur RAM block driver */
    unsigned char tmp[512];
    unsigned char out[512];
    for (size_t i = 0; i < sizeof(tmp); i++) {
        tmp[i] = (unsigned char)(0x5A + i);
    }
    st = mcsos_blk_write(&g_m14_ramdisk_dev, 0u, 1u, tmp);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14 blk_write smoke test failed", (uint64_t)(uint32_t)(int)st);
    }
    st = mcsos_blk_read(&g_m14_ramdisk_dev, 0u, 1u, out);
    if (st != MCSOS_BLK_OK) {
        KERNEL_PANIC("M14 blk_read smoke test failed", (uint64_t)(uint32_t)(int)st);
    }
    for (size_t i = 0; i < sizeof(tmp); i++) {
        if (tmp[i] != out[i]) {
            KERNEL_PANIC("M14 blk read/write mismatch", (uint64_t)i);
        }
    }
    log_writeln("[M14] ram0 write/read roundtrip ok");
}
