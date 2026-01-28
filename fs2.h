#ifndef FS_H
#define FS_H

#include <stdio.h>
#include <stdint.h>

#define DISK_FILE "disk.img"

#define DISK_SIZE (1024 * 1024)   // 1 MB
#define BLOCK_SIZE 512
#define TOTAL_BLOCKS (DISK_SIZE / BLOCK_SIZE)

#define MAX_INODES 128
#define INODE_DIRECT_PTRS 10

#define ROOT_INODE 0

#define FILENAME_LEN 28

typedef struct {
    uint32_t disk_size;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t total_inodes;
} Superblock;

typedef struct {
    uint32_t size;
    uint32_t blocks[INODE_DIRECT_PTRS];
    uint8_t used;
} Inode;

typedef struct {
    char name[FILENAME_LEN];
    uint32_t inode;
} DirEntry;

/* FS API */
void fs_init();
void fs_list();
void fs_create(const char *name);
void fs_write(const char *name, const char *text);
void fs_read(const char *name);
void fs_delete(const char *name);

#endif