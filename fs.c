#include "fs.h"
#include <stdio.h>
#include <string.h>

#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define INODE_TABLE_BLOCK 2
#define ROOT_DIR_BLOCK 3

static FILE *disk;

/* -- Ndihmuesit e Diskut -- */
static void read_block(int block, void *buf) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
    fread(buf, BLOCK_SIZE, 1, disk);
}

static void write_block(int block, void *buf) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, BLOCK_SIZE, 1, disk);
    fflush(disk);
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
    write_block(SUPERBLOCK_BLOCK, &sb);


    uint8_t inode_bm[BLOCK_SIZE] = {0};
    inode_bm[ROOT_INODE] = 1;
    write_block(INODE_BITMAP_BLOCK, inode_bm);

    Inode table[BLOCK_SIZE / sizeof(Inode)] = {0};
    table[ROOT_INODE].used = 1;
    write_block(INODE_TABLE_BLOCK, table);

    /* -- Blloku i root directory -- */
    char empty[BLOCK_SIZE] = {0};
    write_block(ROOT_DIR_BLOCK, empty);
}

void fs_init() {
    disk = fopen(DISK_FILE, "rb+");
    if (!disk) {
        fs_format();
    }
}

void fs_create(const char *name) {
    uint8_t inode_bm[BLOCK_SIZE];
    read_block(INODE_BITMAP_BLOCK, inode_bm);

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
    write_block(INODE_BITMAP_BLOCK, inode_bm);

    Inode table[BLOCK_SIZE / sizeof(Inode)];
    read_block(INODE_TABLE_BLOCK, table);
    table[free_inode].used = 1;
    write_block(INODE_TABLE_BLOCK, table);

    DirEntry dir[BLOCK_SIZE / sizeof(DirEntry)];
    read_block(ROOT_DIR_BLOCK, dir);


    for (int i = 0; i < (int)(BLOCK_SIZE / sizeof(DirEntry)); i++) {
        if (dir[i].inode == 0) {
            strncpy(dir[i].name, name, FILENAME_LEN);
            dir[i].inode = free_inode;
            write_block(ROOT_DIR_BLOCK, dir);
            printf("File created: %s\n", name);
            return;
        }
    }

    printf("Directory full\n");
}