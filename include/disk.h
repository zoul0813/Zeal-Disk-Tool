/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdbool.h>

#define GB  (1073741824ULL)
#define MB  (1048576ULL)
#define KB  (1024ULL)
#define MAX(a,b)    ((a) > (b) ? (a) : (b))


#define MAX_DISKS           32
#define MAX_DISK_SIZE       (32*GB)
#define DISK_LABEL_LEN      512
#define MAX_PART_COUNT      4
#define DISK_SECTOR_SIZE    512

#define MBR_PART_ENTRY_SIZE     16
#define MBR_PART_ENTRY_BEGIN    0x1BE

typedef enum {
    ERR_SUCCESS,
    ERR_NOT_ADMIN,  /* Windows   */
    ERR_NOT_ROOT,   /* Linux/Mac */
} disk_err_t;


typedef struct {
    bool     active;
    uint8_t  type;
    uint32_t start_lba;
    uint32_t size_sectors;
    /* Formatted data to write to disk */
    uint8_t* data;
    /* We will write at most 64KB*3, 32-bit is more than enough*/
    uint32_t data_len;
} partition_t;


typedef struct {
    char        name[256];
    char        path[256];
    char        label[DISK_LABEL_LEN];

    uint64_t    size_bytes;
    bool        valid;
    /* Original MBR */
    bool        has_mbr;
    uint8_t     mbr[DISK_SECTOR_SIZE];
    partition_t partitions[MAX_PART_COUNT];
    /* Staged changes, to be applied */
    bool        has_staged_changes;
    uint8_t     staged_mbr[DISK_SECTOR_SIZE];
    partition_t staged_partitions[MAX_PART_COUNT];
    int         free_part_idx;
} disk_info_t;


extern disk_info_t disks[MAX_DISKS];
extern int disk_count;
extern int selected_disk;

disk_err_t disks_refresh();

void disk_apply_changes(disk_info_t* disk);

void disk_revert_changes(disk_info_t* disk);

disk_err_t disk_list(disk_info_t* out_disks, int max_disks, int* out_count);

void disk_parse_mbr_partitions(disk_info_t *disk);

const char* const *disk_get_partition_size_list(void);

void disk_allocate_partition(disk_info_t *disk, uint32_t lba, int size_idx);

void disk_delete_partition(disk_info_t* disk, int partition);

int disk_valid_partition_size(disk_info_t *disk, uint32_t *largest_free_lba);

const char* disk_get_fs_type(uint8_t fs_byte);

void disk_get_size_str(uint64_t size, char* buffer, int buffer_size);

const char* disk_write_changes(disk_info_t* disk);

#endif // DISK_H
