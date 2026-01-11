#include "fs.h"
#include <stdio.h>
#include <string.h>

#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define BLOCK_BITMAP_BLOCK 2
#define INODE_TABLE_START 3

#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(Inode))
#define INODE_TABLE_BLOCKS ((MAX_INODES + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK)
#define DATA_BLOCK_START (INODE_TABLE_START + INODE_TABLE_BLOCKS)

static FILE *disk;

/* -- Ndihmuesit e Diskut -- */
static void disk_read_block(int block, void *buf) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
    fread(buf, BLOCK_SIZE, 1, disk);
}

static void disk_write_block(int block, void *buf) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, BLOCK_SIZE, 1, disk);
    fflush(disk);
}

/* -- Bitmapat -- */
static int bitmap_find_tree(uint8_t *bm, int max) {
    for (int i = 0; i < max; i++)
        if (bm[i] == 0) return i;
    return -1;
}


/* -- Inodat -- */

static void read_inode(int idx, Inode *ino) {
    uint32_t block = INODE_TABLE_START + idx / INODES_PER_BLOCK;
    Inode buf[INODES_PER_BLOCK];
    disk_read_blocks(block, buf);
    *ino = buf[idx % INODES_PER_BLOCK];
}

static void write_inode(int idx, Inode *ino)
{
    uint32_t block = INODE_TABLE_START + idx / INODES_PER_BLOCK;
    Inode buf[INODES_PER_BLOCK];
    disk_read_block(block, buf);
    buf[idx % INODES_PER_BLOCK] = *ino;
    disk_write_block(block, buf);
}

/* -- Formatimi i Diskut -- */
static void fs_format() {
    disk = fopen(DISK_FILE, "wb+");

    char zero[BLOCK_SIZE] = {0};
    for (int i = 0; i < TOTAL_BLOCKS; i++)
        fwrite(zero, BLOCK_SIZE, 1, disk);

    Superblock sb = {
        DISK_SIZE,
        BLOCK_SIZE,
        TOTAL_BLOCKS,
        MAX_INODES
    };
    disk_write_block(SUPERBLOCK_BLOCK, &sb);


    uint8_t inode_bm[BLOCK_SIZE] = {0};
    inode_bm[ROOT_INODE] = 1;
    disk_write_block(INODE_BITMAP_BLOCK, inode_bm);

    Inode table[BLOCK_SIZE / sizeof(Inode)] = {0};
    table[ROOT_INODE].used = 1;
    disk_write_block(INODE_TABLE_START, table);

    /* -- Blloku i root directory -- */
    char empty[BLOCK_SIZE] = {0};
    disk_write_block(DATA_BLOCK_START, empty);
}

void fs_init() {
    disk = fopen(DISK_FILE, "rb+");
    if (!disk) {
        fs_format();
    }
}

void fs_create(const char *name) {
    uint8_t inode_bm[BLOCK_SIZE];
    disk_read_block(INODE_BITMAP_BLOCK, inode_bm);

    int free_inode = -1;
    for (int i = 1; i < MAX_INODES; i++) {
        if (inode_bm[i] == 0) {
            free_inode = i;
            break;
        }
    }

    if (free_inode == -1) {
        printf("No free inodes\n");
        return;
    }

    inode_bm[free_inode] = 1;
    disk_write_block(INODE_BITMAP_BLOCK, inode_bm);

    Inode table[BLOCK_SIZE / sizeof(Inode)];
    disk_read_block(INODE_TABLE_START, table);
    table[free_inode].used = 1;
    disk_write_block(INODE_TABLE_START, table);

    DirEntry dir[BLOCK_SIZE / sizeof(DirEntry)];
    disk_read_block(DATA_BLOCK_START, dir);


    for (int i = 0; i < (int)(BLOCK_SIZE / sizeof(DirEntry)); i++) {
        if (dir[i].inode == 0) {
            strncpy(dir[i].name, name, FILENAME_LEN);
            dir[i].inode = free_inode;
            disk_write_block(DATA_BLOCK_START, dir);
            printf("File created: %s\n", name);
            return;
        }
    }

    printf("Directory full\n");
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