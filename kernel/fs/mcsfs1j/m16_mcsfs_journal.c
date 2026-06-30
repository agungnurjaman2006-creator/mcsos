/*
 * MCSOS M16 - MCSFS1J crash-consistency teaching journal
 * Target: host unit test and x86_64-elf freestanding object.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define M16_BLOCK_SIZE 512u
#define M16_MAX_BLOCKS 128u
#define M16_MAX_INODES 16u
#define M16_DIRECT_BLOCKS 4u
#define M16_MAX_NAME 32u
#define M16_MAGIC 0x4d43534631564a31ULL /* "MCSF1VJ1"-like */
#define M16_JMAGIC 0x4d43534a524e4c31ULL /* "MCSJRNL1"-like */
#define M16_VERSION 1u
#define M16_J_EMPTY 0u
#define M16_J_COMMITTED 2u
#define M16_JOURNAL_MAX_RECORDS 8u
#define M16_JOURNAL_START 1u
#define M16_JOURNAL_BLOCKS (1u + (2u * M16_JOURNAL_MAX_RECORDS))
#define M16_INODE_BITMAP_LBA (M16_JOURNAL_START + M16_JOURNAL_BLOCKS)
#define M16_BLOCK_BITMAP_LBA (M16_INODE_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_LBA (M16_BLOCK_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_BLOCKS 4u
#define M16_ROOT_DIR_LBA (M16_INODE_TABLE_LBA + M16_INODE_TABLE_BLOCKS)
#define M16_DATA_START_LBA (M16_ROOT_DIR_LBA + 1u)

#define M16_E_OK 0
#define M16_E_INVAL -1
#define M16_E_IO -2
#define M16_E_NOSPC -3
#define M16_E_EXISTS -4
#define M16_E_NOENT -5
#define M16_E_CORRUPT -6
#define M16_E_TOOLONG -7

struct m16_blockdev {
    uint8_t blocks[M16_MAX_BLOCKS][M16_BLOCK_SIZE];
    uint32_t total_blocks;
    uint64_t writes;
    int fail_after; /* negative disables fault injection */
};

struct m16_super {
    uint64_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t journal_start;
    uint32_t journal_blocks;
    uint32_t inode_bitmap_lba;
    uint32_t block_bitmap_lba;
    uint32_t inode_table_lba;
    uint32_t inode_table_blocks;
    uint32_t root_dir_lba;
    uint32_t data_start_lba;
    uint32_t clean_generation;
    uint32_t reserved[114];
};

struct m16_inode {
    uint32_t used;
    uint32_t kind; /* 1=file, 2=dir */
    uint32_t size;
    uint32_t direct[M16_DIRECT_BLOCKS];
    uint32_t reserved[25];
};

struct m16_dirent {
    uint32_t used;
    uint32_t ino;
    char name[M16_MAX_NAME];
};

struct m16_journal_header {
    uint64_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t seq;
    uint32_t count;
    uint32_t header_checksum;
    uint32_t reserved[121];
};

struct m16_journal_desc {
    uint64_t magic;
    uint32_t target_lba;
    uint32_t payload_checksum;
    uint32_t reserved[124];
};

struct m16_jrec {
    uint32_t target_lba;
    uint8_t payload[M16_BLOCK_SIZE];
};

struct m16_tx {
    uint32_t count;
    struct m16_jrec rec[M16_JOURNAL_MAX_RECORDS];
};

_Static_assert(sizeof(struct m16_super) == M16_BLOCK_SIZE, "m16_super must occupy one block");
_Static_assert(sizeof(struct m16_inode) == 128u, "m16_inode must be 128 bytes");

static void m16_zero(void *ptr, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static void m16_copy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static size_t m16_strlen_bounded(const char *s, size_t max) {
    size_t n = 0;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return n;
}

static int m16_streq(const char *a, const char *b) {
    for (size_t i = 0; i < M16_MAX_NAME; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
    }
    return 1;
}

static uint32_t m16_checksum(const void *ptr, size_t n) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 16777619u;
    }
    return h;
}

static int m16_valid_lba(const struct m16_blockdev *dev, uint32_t lba) {
    return dev != NULL && lba < dev->total_blocks && lba < M16_MAX_BLOCKS;
}

static int m16_read_block(struct m16_blockdev *dev, uint32_t lba, void *out) {
    if (dev == NULL || out == NULL || !m16_valid_lba(dev, lba)) {
        return M16_E_INVAL;
    }
    m16_copy(out, dev->blocks[lba], M16_BLOCK_SIZE);
    return M16_E_OK;
}

static int m16_write_block(struct m16_blockdev *dev, uint32_t lba, const void *in) {
    if (dev == NULL || in == NULL || !m16_valid_lba(dev, lba)) {
        return M16_E_INVAL;
    }
    if (dev->fail_after == 0) {
        return M16_E_IO;
    }
    if (dev->fail_after > 0) {
        dev->fail_after--;
    }
    m16_copy(dev->blocks[lba], in, M16_BLOCK_SIZE);
    dev->writes++;
    return M16_E_OK;
}

void m16_dev_init(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return;
    }
    m16_zero(dev, sizeof(*dev));
    dev->total_blocks = M16_MAX_BLOCKS;
    dev->fail_after = -1;
}

static void m16_bitmap_set(uint8_t *bm, uint32_t bit) {
    bm[bit / 8u] = (uint8_t)(bm[bit / 8u] | (uint8_t)(1u << (bit % 8u)));
}

static int m16_bitmap_get(const uint8_t *bm, uint32_t bit) {
    return (bm[bit / 8u] & (uint8_t)(1u << (bit % 8u))) != 0u;
}

static int m16_load_inode_table(struct m16_blockdev *dev, struct m16_inode *inodes) {
    uint8_t *raw = (uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        int rc = m16_read_block(dev, M16_INODE_TABLE_LBA + i, raw + ((size_t)i * M16_BLOCK_SIZE));
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return M16_E_OK;
}

static int m16_store_inode_table(struct m16_tx *tx, const struct m16_inode *inodes) {
    if (tx == NULL || inodes == NULL || tx->count + M16_INODE_TABLE_BLOCKS > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_NOSPC;
    }
    const uint8_t *raw = (const uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        tx->rec[tx->count].target_lba = M16_INODE_TABLE_LBA + i;
        m16_copy(tx->rec[tx->count].payload, raw + ((size_t)i * M16_BLOCK_SIZE), M16_BLOCK_SIZE);
        tx->count++;
    }
    return M16_E_OK;
}

static int m16_tx_add(struct m16_tx *tx, uint32_t lba, const void *payload) {
    if (tx == NULL || payload == NULL || tx->count >= M16_JOURNAL_MAX_RECORDS) {
        return M16_E_NOSPC;
    }
    tx->rec[tx->count].target_lba = lba;
    m16_copy(tx->rec[tx->count].payload, payload, M16_BLOCK_SIZE);
    tx->count++;
    return M16_E_OK;
}

static uint32_t m16_header_checksum(struct m16_journal_header *h) {
    uint32_t saved = h->header_checksum;
    h->header_checksum = 0;
    uint32_t sum = m16_checksum(h, sizeof(*h));
    h->header_checksum = saved;
    return sum;
}

static int m16_journal_clear(struct m16_blockdev *dev) {
    struct m16_journal_header h;
    m16_zero(&h, sizeof(h));
    return m16_write_block(dev, M16_JOURNAL_START, &h);
}

static int m16_journal_commit(struct m16_blockdev *dev, const struct m16_tx *tx, uint32_t seq, int stop_after_commit_record) {
    if (dev == NULL || tx == NULL || tx->count > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_INVAL;
    }
    int rc = m16_journal_clear(dev);
    if (rc != M16_E_OK) {
        return rc;
    }
    for (uint32_t i = 0; i < tx->count; i++) {
        uint32_t desc_lba = M16_JOURNAL_START + 1u + (i * 2u);
        uint32_t data_lba = desc_lba + 1u;
        struct m16_journal_desc d;
        m16_zero(&d, sizeof(d));
        d.magic = M16_JMAGIC;
        d.target_lba = tx->rec[i].target_lba;
        d.payload_checksum = m16_checksum(tx->rec[i].payload, M16_BLOCK_SIZE);
        rc = m16_write_block(dev, desc_lba, &d);
        if (rc != M16_E_OK) {
            return rc;
        }
        rc = m16_write_block(dev, data_lba, tx->rec[i].payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    struct m16_journal_header h;
    m16_zero(&h, sizeof(h));
    h.magic = M16_JMAGIC;
    h.version = M16_VERSION;
    h.state = M16_J_COMMITTED;
    h.seq = seq;
    h.count = tx->count;
    h.header_checksum = m16_header_checksum(&h);
    rc = m16_write_block(dev, M16_JOURNAL_START, &h);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (stop_after_commit_record != 0) {
        return M16_E_OK;
    }
    for (uint32_t i = 0; i < tx->count; i++) {
        rc = m16_write_block(dev, tx->rec[i].target_lba, tx->rec[i].payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return m16_journal_clear(dev);
}

int m16_journal_recover(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    struct m16_journal_header h;
    int rc = m16_read_block(dev, M16_JOURNAL_START, &h);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (h.magic == 0u && h.state == M16_J_EMPTY) {
        return M16_E_OK;
    }
    if (h.magic != M16_JMAGIC || h.version != M16_VERSION || h.state != M16_J_COMMITTED || h.count > M16_JOURNAL_MAX_RECORDS) {
        return M16_E_CORRUPT;
    }
    if (m16_header_checksum(&h) != h.header_checksum) {
        return M16_E_CORRUPT;
    }
    uint8_t payload[M16_BLOCK_SIZE];
    for (uint32_t i = 0; i < h.count; i++) {
        uint32_t desc_lba = M16_JOURNAL_START + 1u + (i * 2u);
        uint32_t data_lba = desc_lba + 1u;
        struct m16_journal_desc d;
        rc = m16_read_block(dev, desc_lba, &d);
        if (rc != M16_E_OK) {
            return rc;
        }
        if (d.magic != M16_JMAGIC || !m16_valid_lba(dev, d.target_lba)) {
            return M16_E_CORRUPT;
        }
        rc = m16_read_block(dev, data_lba, payload);
        if (rc != M16_E_OK) {
            return rc;
        }
        if (m16_checksum(payload, M16_BLOCK_SIZE) != d.payload_checksum) {
            return M16_E_CORRUPT;
        }
        rc = m16_write_block(dev, d.target_lba, payload);
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    return m16_journal_clear(dev);
}

int m16_format(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    m16_zero(dev->blocks, sizeof(dev->blocks));
    struct m16_super sb;
    m16_zero(&sb, sizeof(sb));
    sb.magic = M16_MAGIC;
    sb.version = M16_VERSION;
    sb.block_size = M16_BLOCK_SIZE;
    sb.total_blocks = dev->total_blocks;
    sb.journal_start = M16_JOURNAL_START;
    sb.journal_blocks = M16_JOURNAL_BLOCKS;
    sb.inode_bitmap_lba = M16_INODE_BITMAP_LBA;
    sb.block_bitmap_lba = M16_BLOCK_BITMAP_LBA;
    sb.inode_table_lba = M16_INODE_TABLE_LBA;
    sb.inode_table_blocks = M16_INODE_TABLE_BLOCKS;
    sb.root_dir_lba = M16_ROOT_DIR_LBA;
    sb.data_start_lba = M16_DATA_START_LBA;
    sb.clean_generation = 1u;
    int rc = m16_write_block(dev, 0u, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    m16_zero(ib, sizeof(ib));
    m16_zero(bb, sizeof(bb));
    m16_bitmap_set(ib, 0u); /* root inode */
    for (uint32_t i = 0; i < M16_DATA_START_LBA; i++) {
        m16_bitmap_set(bb, i);
    }
    rc = m16_write_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_write_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    struct m16_inode inodes[M16_MAX_INODES];
    m16_zero(inodes, sizeof(inodes));
    inodes[0].used = 1u;
    inodes[0].kind = 2u;
    inodes[0].size = 0u;
    inodes[0].direct[0] = M16_ROOT_DIR_LBA;
    uint8_t *raw = (uint8_t *)inodes;
    for (uint32_t i = 0; i < M16_INODE_TABLE_BLOCKS; i++) {
        rc = m16_write_block(dev, M16_INODE_TABLE_LBA + i, raw + ((size_t)i * M16_BLOCK_SIZE));
        if (rc != M16_E_OK) {
            return rc;
        }
    }
    uint8_t root[M16_BLOCK_SIZE];
    m16_zero(root, sizeof(root));
    rc = m16_write_block(dev, M16_ROOT_DIR_LBA, root);
    if (rc != M16_E_OK) {
        return rc;
    }
    return m16_journal_clear(dev);
}

int m16_mount(struct m16_blockdev *dev, struct m16_super *sb) {
    if (dev == NULL || sb == NULL) {
        return M16_E_INVAL;
    }
    int rc = m16_journal_recover(dev);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, 0u, sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (sb->magic != M16_MAGIC || sb->version != M16_VERSION || sb->block_size != M16_BLOCK_SIZE) {
        return M16_E_CORRUPT;
    }
    if (sb->data_start_lba != M16_DATA_START_LBA || sb->root_dir_lba != M16_ROOT_DIR_LBA) {
        return M16_E_CORRUPT;
    }
    return M16_E_OK;
}

static int m16_find_free_inode(const uint8_t *ib) {
    for (uint32_t i = 1; i < M16_MAX_INODES; i++) {
        if (!m16_bitmap_get(ib, i)) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

static int m16_find_free_block(const uint8_t *bb) {
    for (uint32_t i = M16_DATA_START_LBA; i < M16_MAX_BLOCKS; i++) {
        if (!m16_bitmap_get(bb, i)) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

static int m16_find_dirent(struct m16_dirent *dir, const char *name) {
    uint32_t n = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < n; i++) {
        if (dir[i].used != 0u && m16_streq(dir[i].name, name)) {
            return (int)i;
        }
    }
    return M16_E_NOENT;
}

static int m16_find_free_dirent(struct m16_dirent *dir) {
    uint32_t n = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < n; i++) {
        if (dir[i].used == 0u) {
            return (int)i;
        }
    }
    return M16_E_NOSPC;
}

int m16_write_file_ex(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size, int stop_after_commit_record) {
    if (dev == NULL || name == NULL || data == NULL) {
        return M16_E_INVAL;
    }
    size_t name_len = m16_strlen_bounded(name, M16_MAX_NAME);
    if (name_len == 0u || name_len >= M16_MAX_NAME) {
        return M16_E_TOOLONG;
    }
    if (size > M16_BLOCK_SIZE) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_read_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (m16_find_dirent(dir, name) >= 0) {
        return M16_E_EXISTS;
    }
    int ino = m16_find_free_inode(ib);
    if (ino < 0) {
        return ino;
    }
    int data_block = m16_find_free_block(bb);
    if (data_block < 0) {
        return data_block;
    }
    int slot = m16_find_free_dirent(dir);
    if (slot < 0) {
        return slot;
    }
    uint8_t data_blk[M16_BLOCK_SIZE];
    m16_zero(data_blk, sizeof(data_blk));
    m16_copy(data_blk, data, size);
    m16_bitmap_set(ib, (uint32_t)ino);
    m16_bitmap_set(bb, (uint32_t)data_block);
    inodes[ino].used = 1u;
    inodes[ino].kind = 1u;
    inodes[ino].size = size;
    inodes[ino].direct[0] = (uint32_t)data_block;
    dir[slot].used = 1u;
    dir[slot].ino = (uint32_t)ino;
    m16_zero(dir[slot].name, M16_MAX_NAME);
    m16_copy(dir[slot].name, name, name_len);
    struct m16_tx tx;
    m16_zero(&tx, sizeof(tx));
    rc = m16_tx_add(&tx, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_tx_add(&tx, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_store_inode_table(&tx, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    /* M16 educational simplification: inode table is 4 records, so root/data are committed in separate transactions. */
    rc = m16_journal_commit(dev, &tx, sb.clean_generation + 1u, 0);
    if (rc != M16_E_OK) {
        return rc;
    }
    m16_zero(&tx, sizeof(tx));
    rc = m16_tx_add(&tx, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_tx_add(&tx, (uint32_t)data_block, data_blk);
    if (rc != M16_E_OK) {
        return rc;
    }
    return m16_journal_commit(dev, &tx, sb.clean_generation + 2u, stop_after_commit_record);
}

int m16_write_file(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size) {
    return m16_write_file_ex(dev, name, data, size, 0);
}

int m16_read_file(struct m16_blockdev *dev, const char *name, uint8_t *out, uint32_t out_cap, uint32_t *out_size) {
    if (dev == NULL || name == NULL || out == NULL || out_size == NULL) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    int slot = m16_find_dirent(dir, name);
    if (slot < 0) {
        return slot;
    }
    uint32_t ino = dir[slot].ino;
    if (ino >= M16_MAX_INODES || inodes[ino].used == 0u || inodes[ino].kind != 1u) {
        return M16_E_CORRUPT;
    }
    if (inodes[ino].size > out_cap || inodes[ino].direct[0] >= dev->total_blocks) {
        return M16_E_INVAL;
    }
    uint8_t blk[M16_BLOCK_SIZE];
    rc = m16_read_block(dev, inodes[ino].direct[0], blk);
    if (rc != M16_E_OK) {
        return rc;
    }
    m16_copy(out, blk, inodes[ino].size);
    *out_size = inodes[ino].size;
    return M16_E_OK;
}

int m16_fsck(struct m16_blockdev *dev) {
    if (dev == NULL) {
        return M16_E_INVAL;
    }
    struct m16_super sb;
    int rc = m16_mount(dev, &sb);
    if (rc != M16_E_OK) {
        return rc;
    }
    uint8_t ib[M16_BLOCK_SIZE];
    uint8_t bb[M16_BLOCK_SIZE];
    struct m16_inode inodes[M16_MAX_INODES];
    uint8_t dir_block[M16_BLOCK_SIZE];
    struct m16_dirent *dir = (struct m16_dirent *)dir_block;
    rc = m16_read_block(dev, M16_INODE_BITMAP_LBA, ib);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_BLOCK_BITMAP_LBA, bb);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_load_inode_table(dev, inodes);
    if (rc != M16_E_OK) {
        return rc;
    }
    rc = m16_read_block(dev, M16_ROOT_DIR_LBA, dir);
    if (rc != M16_E_OK) {
        return rc;
    }
    if (!m16_bitmap_get(ib, 0u) || inodes[0].used != 1u || inodes[0].kind != 2u || inodes[0].direct[0] != M16_ROOT_DIR_LBA) {
        return M16_E_CORRUPT;
    }
    for (uint32_t b = 0; b < M16_DATA_START_LBA; b++) {
        if (!m16_bitmap_get(bb, b)) {
            return M16_E_CORRUPT;
        }
    }
    uint32_t dir_count = M16_BLOCK_SIZE / (uint32_t)sizeof(struct m16_dirent);
    for (uint32_t i = 0; i < dir_count; i++) {
        if (dir[i].used != 0u) {
            if (dir[i].ino >= M16_MAX_INODES || !m16_bitmap_get(ib, dir[i].ino)) {
                return M16_E_CORRUPT;
            }
            struct m16_inode *inode = &inodes[dir[i].ino];
            if (inode->used == 0u || inode->kind != 1u || inode->size > M16_BLOCK_SIZE) {
                return M16_E_CORRUPT;
            }
            if (inode->direct[0] < M16_DATA_START_LBA || inode->direct[0] >= dev->total_blocks || !m16_bitmap_get(bb, inode->direct[0])) {
                return M16_E_CORRUPT;
            }
        }
    }
    return M16_E_OK;
}

#ifdef MCSOS_M16_HOST_TEST
#include <stdio.h>

static int m16_expect(int cond, const char *msg) {
    if (!cond) {
        printf("FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

int main(void) {
    int fails = 0;
    struct m16_blockdev dev;
    uint8_t out[64];
    uint32_t out_size = 0;
    const uint8_t hello[] = { 'h', 'e', 'l', 'l', 'o', '-', 'm', '1', '6' };
    const uint8_t crashy[] = { 'c', 'r', 'a', 's', 'h', '-', 'r', 'e', 'p', 'l', 'a', 'y' };

    m16_dev_init(&dev);
    fails += m16_expect(m16_format(&dev) == M16_E_OK, "format");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after format");
    fails += m16_expect(m16_write_file(&dev, "hello.txt", hello, (uint32_t)sizeof(hello)) == M16_E_OK, "write hello");
    fails += m16_expect(m16_read_file(&dev, "hello.txt", out, sizeof(out), &out_size) == M16_E_OK, "read hello");
    fails += m16_expect(out_size == sizeof(hello), "hello size");
    fails += m16_expect(out[0] == 'h' && out[8] == '6', "hello content");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after hello");

    /* Simulate power loss after the second transaction commit record but before home-location writes. */
    fails += m16_expect(m16_write_file_ex(&dev, "crash.txt", crashy, (uint32_t)sizeof(crashy), 1) == M16_E_OK, "write crash transaction until commit record");
    fails += m16_expect(m16_journal_recover(&dev) == M16_E_OK, "journal replay after committed crash");
    fails += m16_expect(m16_read_file(&dev, "crash.txt", out, sizeof(out), &out_size) == M16_E_OK, "read crash after replay");
    fails += m16_expect(out_size == sizeof(crashy), "crash size after replay");
    fails += m16_expect(out[0] == 'c' && out[11] == 'y', "crash content after replay");
    fails += m16_expect(m16_fsck(&dev) == M16_E_OK, "fsck after replay");

    /* Corrupt committed journal descriptor: recovery must fail closed instead of applying unknown target. */
    m16_dev_init(&dev);
    fails += m16_expect(m16_format(&dev) == M16_E_OK, "format for corrupt test");
    fails += m16_expect(m16_write_file_ex(&dev, "bad.txt", crashy, (uint32_t)sizeof(crashy), 1) == M16_E_OK, "commit bad transaction");
    dev.blocks[M16_JOURNAL_START + 1u][0] ^= 0x7fu;
    fails += m16_expect(m16_journal_recover(&dev) == M16_E_CORRUPT, "corrupt descriptor rejected");

    if (fails == 0) {
        printf("M16 host tests PASS\n");
    }
    return fails == 0 ? 0 : 1;
}
#endif
