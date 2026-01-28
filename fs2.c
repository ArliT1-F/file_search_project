#include "fs.h"
#include <string.h>
#include <stdlib.h>

#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define BLOCK_BITMAP_BLOCK 2
#define INODE_TABLE_START 3

#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(Inode))
#define INODE_TABLE_BLOCKS ((MAX_INODES + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK)
#define DATA_BLOCK_START (INODE_TABLE_START + INODE_TABLE_BLOCKS)

static FILE *disk;

/* ---------------- Disk helpers ---------------- */

static void disk_read_block(uint32_t block, void *buf) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
    fread(buf, BLOCK_SIZE, 1, disk);
}

static void disk_write_block(uint32_t block, void *buf) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, BLOCK_SIZE, 1, disk);
    fflush(disk);
}

/* ---------------- Bitmaps ---------------- */

static int bitmap_find_free(uint8_t *bm, int max) {
    for (int i = 0; i < max; i++)
        if (bm[i] == 0) return i;
    return -1;
}

/* ---------------- Inodes ---------------- */

static void read_inode(int idx, Inode *ino) {
    uint32_t block = INODE_TABLE_START + idx / INODES_PER_BLOCK;
    Inode buf[INODES_PER_BLOCK];
    disk_read_block(block, buf);
    *ino = buf[idx % INODES_PER_BLOCK];
}

static void write_inode(int idx, Inode *ino) {
    uint32_t block = INODE_TABLE_START + idx / INODES_PER_BLOCK;
    Inode buf[INODES_PER_BLOCK];
    disk_read_block(block, buf);
    buf[idx % INODES_PER_BLOCK] = *ino;
    disk_write_block(block, buf);
}

/* ---------------- Directory ---------------- */

static int dir_find(const char *name) {
    Inode root;
    read_inode(ROOT_INODE, &root);

    for (int i = 0; i < INODE_DIRECT_PTRS; i++) {
        if (!root.blocks[i]) continue;

        DirEntry entries[BLOCK_SIZE / sizeof(DirEntry)];
        disk_read_block(root.blocks[i], entries);

        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry)); j++) {
            if (entries[j].inode != 0 &&
                strncmp(entries[j].name, name, FILENAME_LEN) == 0)
                return entries[j].inode;
        }
    }
    return -1;
}

static void dir_add(const char *name, int inode_idx) {
    Inode root;
    read_inode(ROOT_INODE, &root);

    uint8_t block_bm[BLOCK_SIZE];
    disk_read_block(BLOCK_BITMAP_BLOCK, block_bm);

    for (int i = 0; i < INODE_DIRECT_PTRS; i++) {
        if (root.blocks[i] == 0) {
            int b = bitmap_find_free(block_bm, TOTAL_BLOCKS);
            block_bm[b] = 1;
            root.blocks[i] = b;
            disk_write_block(BLOCK_BITMAP_BLOCK, block_bm);
            write_inode(ROOT_INODE, &root);
        }

        DirEntry entries[BLOCK_SIZE / sizeof(DirEntry)];
        disk_read_block(root.blocks[i], entries);

        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry)); j++) {
            if (entries[j].inode == 0) {
                strncpy(entries[j].name, name, FILENAME_LEN);
                entries[j].inode = inode_idx;
                disk_write_block(root.blocks[i], entries);
                return;
            }
        }
    }
}

/* ---------------- Formatting ---------------- */

static void fs_format() {
    disk = fopen(DISK_FILE, "wb+");

    char zero[BLOCK_SIZE] = {0};
    for (int i = 0; i < TOTAL_BLOCKS; i++)
        fwrite(zero, BLOCK_SIZE, 1, disk);

    Superblock sb = {DISK_SIZE, BLOCK_SIZE, TOTAL_BLOCKS, MAX_INODES};
    disk_write_block(SUPERBLOCK_BLOCK, &sb);

    uint8_t bm[BLOCK_SIZE] = {0};
    bm[ROOT_INODE] = 1;
    disk_write_block(INODE_BITMAP_BLOCK, bm);

    memset(bm, 0, BLOCK_SIZE);
    for (int i = 0; i < DATA_BLOCK_START; i++) bm[i] = 1;
    disk_write_block(BLOCK_BITMAP_BLOCK, bm);

    Inode root = {0};
    root.used = 1;
    write_inode(ROOT_INODE, &root);
}

/* ---------------- Public API ---------------- */

void fs_init() {
    disk = fopen(DISK_FILE, "rb+");
    if (!disk) fs_format();
}

void fs_list() {
    Inode root;
    read_inode(ROOT_INODE, &root);

    printf("Files:\n");

    for (int i = 0; i < INODE_DIRECT_PTRS; i++) {
        if (!root.blocks[i]) continue;

        DirEntry entries[BLOCK_SIZE / sizeof(DirEntry)];
        disk_read_block(root.blocks[i], entries);

        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry)); j++) {
            if (entries[j].inode) {
                Inode f;
                read_inode(entries[j].inode, &f);
                printf("  %s (%u bytes)\n", entries[j].name, f.size);
            }
        }
    }
}

void fs_create(const char *name) {
    if (dir_find(name) != -1) {
        printf("File exists\n");
        return;
    }

    uint8_t bm[BLOCK_SIZE];
    disk_read_block(INODE_BITMAP_BLOCK, bm);

    int idx = bitmap_find_free(bm, MAX_INODES);
    if (idx < 0) { printf("No inodes\n"); return; }

    bm[idx] = 1;
    disk_write_block(INODE_BITMAP_BLOCK, bm);

    Inode ino = {0};
    ino.used = 1;
    write_inode(idx, &ino);

    dir_add(name, idx);
}

void fs_write(const char *name, const char *text) {
    int idx = dir_find(name);
    if (idx < 0) { printf("Not found\n"); return; }

    Inode ino;
    read_inode(idx, &ino);

    uint8_t bm[BLOCK_SIZE];
    disk_read_block(BLOCK_BITMAP_BLOCK, bm);

    int len = strlen(text);
    int written = 0;

    for (int i = 0; i < INODE_DIRECT_PTRS && written < len; i++) {
        if (ino.blocks[i] == 0) {
            int b = bitmap_find_free(bm, TOTAL_BLOCKS);
            if (b < 0) break;
            bm[b] = 1;
            ino.blocks[i] = b;
        }

        char buf[BLOCK_SIZE] = {0};
        disk_read_block(ino.blocks[i], buf);

        int off = ino.size % BLOCK_SIZE;
        int n = BLOCK_SIZE - off;
        if (n > len - written) n = len - written;

        memcpy(buf + off, text + written, n);
        disk_write_block(ino.blocks[i], buf);

        ino.size += n;
        written += n;
    }

    disk_write_block(BLOCK_BITMAP_BLOCK, bm);
    write_inode(idx, &ino);
}

void fs_read(const char *name) {
    int idx = dir_find(name);
    if (idx < 0) { printf("Not found\n"); return; }

    Inode ino;
    read_inode(idx, &ino);

    int remaining = ino.size;

    for (int i = 0; i < INODE_DIRECT_PTRS && remaining > 0; i++) {
        if (!ino.blocks[i]) break;

        char buf[BLOCK_SIZE];
        disk_read_block(ino.blocks[i], buf);

        int n = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        fwrite(buf, 1, n, stdout);
        remaining -= n;
    }
    printf("\n");
}

void fs_delete(const char *name) {
    Inode root;
    read_inode(ROOT_INODE, &root);

    for (int i = 0; i < INODE_DIRECT_PTRS; i++) {
        if (!root.blocks[i]) continue;

        DirEntry entries[BLOCK_SIZE / sizeof(DirEntry)];
        disk_read_block(root.blocks[i], entries);

        for (int j = 0; j < (int)(BLOCK_SIZE / sizeof(DirEntry)); j++) {
            if (entries[j].inode &&
                strncmp(entries[j].name, name, FILENAME_LEN) == 0) {

                int idx = entries[j].inode;
                entries[j].inode = 0;
                disk_write_block(root.blocks[i], entries);

                uint8_t bm[BLOCK_SIZE];
                disk_read_block(INODE_BITMAP_BLOCK, bm);
                bm[idx] = 0;
                disk_write_block(INODE_BITMAP_BLOCK, bm);

                Inode ino;
                read_inode(idx, &ino);

                disk_read_block(BLOCK_BITMAP_BLOCK, bm);
                for (int k = 0; k < INODE_DIRECT_PTRS; k++)
                    if (ino.blocks[k]) bm[ino.blocks[k]] = 0;
                disk_write_block(BLOCK_BITMAP_BLOCK, bm);

                memset(&ino, 0, sizeof(ino));
                write_inode(idx, &ino);
                return;
            }
        }
    }

    printf("Not found\n");
}