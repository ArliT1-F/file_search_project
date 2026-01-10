#ifndef FS_H
#define FS_H

#include <stdint.h>

#define DISK_FILE "disk.img"

#define DISK_SIZE (1024 * 1024)
#define BLOCK_SIZE 512
#define TOTAL_BLOCKS (DISK_SIZE / BLOCK_SIZE)

#define MAX_INODES 128
#define FILENAME_LEN 28

#define ROOT_INODE 0

typedef struct
{
    uint32_t disk_size;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t total_inodes;
} Superblock;

typedef struct
{
    uint8_t used;
} Inode;

typedef struct
{
    char name[FILENAME_LEN];
    uint32_t inode;
} DirEntry;

/* API */
void fs_init();
void fs_create(const char *name);

#endif