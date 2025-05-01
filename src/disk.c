/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "disk.h"
#include "zealfs_v2.h"

static int disk_find_free_partition(disk_info_t* disk)
{
    /* Find free partition */
    for (int i = 0; i < MAX_PART_COUNT; ++i) {
        if (!disk->staged_partitions[i].active) {
            return i;
        }
    }
    return -1;
}

static void disk_write_mbr_entry(uint8_t *entry, const partition_t *part)
{
    entry[0] = 0x00;
    /* CHS fields not used */
    entry[1] = 0xFF;
    entry[2] = 0xFF;
    entry[3] = 0xFF;
    /* Partition type */
    entry[4] = part->type;
    /* CHS end not used either */
    entry[5] = 0xFF;
    entry[6] = 0xFF;
    entry[7] = 0xFF;
    /* Start LBA */
    entry[8]  = (uint8_t)(part->start_lba & 0xff);
    entry[9]  = (uint8_t)((part->start_lba >> 8) & 0xff);
    entry[10] = (uint8_t)((part->start_lba >> 16) & 0xff);
    entry[11] = (uint8_t)((part->start_lba >> 24) & 0xff);
    /* Size in sectors */
    entry[12] = (uint8_t)(part->size_sectors & 0xff);
    entry[13] = (uint8_t)((part->size_sectors >> 8) & 0xff);
    entry[14] = (uint8_t)((part->size_sectors >> 16) & 0xff);
    entry[15] = (uint8_t)((part->size_sectors >> 24) & 0xff);
}


void disk_allocate_partition(disk_info_t *disk, uint32_t lba, int size_idx)
{
    /* Calculate the size in sector size */
    const uint64_t part_size_bytes = 1ULL << (16ULL + size_idx);
    const uint32_t size_sector = part_size_bytes / DISK_SECTOR_SIZE;

    assert(disk->free_part_idx >= 0 && disk->free_part_idx < 4);

    partition_t* part = &disk->staged_partitions[disk->free_part_idx];
    assert(!part->active);
    printf("[DISK] Allocating ZealFS in partition %d\n", disk->free_part_idx);
    disk->has_staged_changes = true;
    part->active = true;
    part->start_lba = lba;
    part->type = 0x5a;
    part->size_sectors = size_sector;

    /* Encode the partition in the staged MBR */
    uint8_t *entry = &disk->staged_mbr[MBR_PART_ENTRY_BEGIN + disk->free_part_idx * MBR_PART_ENTRY_SIZE];
    disk_write_mbr_entry(entry, part);

    /* Format the partition with data. We need to allocate 3 pages at all time:
     * - One for the header
     * - Two for the FAT
     */
    assert(part->data == NULL && part->data_len == 0);
    const int page_size = zealfsv2_page_size(part_size_bytes);
    part->data_len = page_size * 3;
    part->data = calloc(3, page_size);
    if (part->data == NULL) {
        printf("Could not allocate memory!\n");
        exit(1);
    } else {
        printf("[DISK] Allocated %d bytes (3 pages)\n", 3*page_size);
    }
    zealfsv2_format(part->data, part_size_bytes);
    printf("[DISK] Partition %d data: %p, length: %d\n", disk->free_part_idx, part->data, part->data_len);

    /* Reuse the free partition index */
    disk->free_part_idx = disk_find_free_partition(disk);
}


static void disk_free_staged_partitions_data(disk_info_t* disk)
{
    /* Free the pointers in the partitions */
    for (int i = 0; i < MAX_PART_COUNT; i++) {
        free(disk->staged_partitions[i].data);
        disk->staged_partitions[i].data = NULL;
        disk->staged_partitions[i].data_len = 0;
    }
}


void disk_revert_changes(disk_info_t* disk)
{
    /* Cancel all the changes made to the disk */
    if (!disk->has_staged_changes) {
        /* No changes made */
        return;
    }

    /* Free the staged partitions data BEFORE replacing them */
    disk_free_staged_partitions_data(disk);

    /* Create a mirror for the RAM changes */
    disk->has_staged_changes = false;
    memcpy(disk->staged_mbr, disk->mbr, sizeof(disk->mbr));
    memcpy(disk->staged_partitions, disk->partitions, sizeof(disk->partitions));
    /* Make sure to call the function AFTER restoring the stages partitions */
    disk->free_part_idx = disk_find_free_partition(disk);
}


void disk_apply_changes(disk_info_t* disk)
{
    disk->has_staged_changes = false;
    /* Before copying the staged partitions as the real partitions, make sure to
     * free the pointers and sizes (since they have been copied to the disk) */
    disk_free_staged_partitions_data(disk);
    memcpy(disk->mbr, disk->staged_mbr, sizeof(disk->mbr));
    memcpy(disk->partitions, disk->staged_partitions, sizeof(disk->partitions));
}


const char* disk_get_fs_type(uint8_t fs_byte)
{
    switch (fs_byte) {
        case 0x01:  return "FAT12";
        case 0x04:  return "FAT16";
        case 0x06:  return "FAT16";
        case 0x0b:  return "FAT32";
        case 0x0c:  return "FAT32";
        case 0x07:  return "NTFS";
        case 0x83:  return "ext3";
        case 0x8e:  return "ext4";
        case 0xa5:  return "exFAT";
        case 0x5a:  return "ZealFS";
        case 0x5e:  return "UFS";
        case 0xaf:  return "Mac OS Extended (HFS+)";
        case 0xc0:  return "Mac OS Extended (HFSX)";
        case 0x17:  return "Mac OS HFS";
        case 0x82:  return "ext2";
        case 0xee:  return "GPT";
        case 0xef:  return "exFAT";
        default:    return "Unknown";
    }
}


void disk_get_size_str(uint64_t size, char* buffer, int buffer_size)
{
    if (size < MB) {
        snprintf(buffer, buffer_size, "%.2f KiB", (float) size / KB);
    } else if (size < GB) {
        snprintf(buffer, buffer_size, "%.2f MiB", (float) size / MB);
    } else {
        snprintf(buffer, buffer_size, "%.2f GiB", (float) size / GB);
    }
}



/**
 * @brief Populate the `partitions` field in the given `disk_info_t`.
 * This will sort the partitions by LBA address.
 */
void disk_parse_mbr_partitions(disk_info_t *disk)
{
    int free_part_idx = -1;

    for (int i = 0; i < MAX_PART_COUNT; ++i) {
        const uint8_t *entry = disk->mbr + 446 + i * 16;

        partition_t *p  = &disk->partitions[i];
        p->type         = entry[4];
        p->start_lba    = entry[8] | (entry[9] << 8) | (entry[10] << 16) | (entry[11] << 24);
        p->size_sectors = entry[12] | (entry[13] << 8) | (entry[14] << 16) | (entry[15] << 24);
        /* Be very conservative to make sure nothing is erased! */
        p->active       = (entry[0] & 0x80) != 0 || p->type != 0 ||
                          p->start_lba != 0 || p->size_sectors != 0;

        if (!p->active && free_part_idx == -1) {
            free_part_idx = i;
        }
    }
    /* Create a mirror for the RAM changes */
    disk->has_staged_changes = false;
    disk->free_part_idx = free_part_idx;
    memcpy(disk->staged_mbr, disk->mbr, sizeof(disk->mbr));
    memcpy(disk->staged_partitions, disk->partitions, sizeof(disk->partitions));
}


uint32_t disk_largest_free_space(disk_info_t *disk, uint32_t *largest_free_lba)
{
    /* Total disk sector in bytes */
    const uint32_t disk_size_sectors = disk->size_bytes / DISK_SECTOR_SIZE;
    uint32_t largest_free_space = 0;
    /* Make sure the first sector is taken (MBR), so start checking at sector 1 */
    uint32_t largest_start_address = 1;
    /* Check all the gaps between the sections and keep the maximum in `largest_free_space` */
    uint32_t previous_end_lba = largest_start_address;


    /* Build a sorted list of active partition indexes */
    int sorted_indexes[MAX_PART_COUNT];
    int sorted_count = 0;

    for (int i = 0; i < MAX_PART_COUNT; i++) {
        if (disk->staged_partitions[i].active) {
            int j = sorted_count;
            while (j > 0 &&
                   disk->staged_partitions[sorted_indexes[j - 1]].start_lba >
                   disk->staged_partitions[i].start_lba) {
                sorted_indexes[j] = sorted_indexes[j - 1];
                j--;
            }
            sorted_indexes[j] = i;
            sorted_count++;
        }
    }

    for (int i = 0; i < sorted_count; i++) {
        partition_t *partition = &disk->staged_partitions[sorted_indexes[i]];
        assert(partition->active);

        const uint32_t start_lba = partition->start_lba;
        const uint32_t end_lba = start_lba + partition->size_sectors;

        /* Calculate the free space between the previous partition and the current one */
        if (start_lba > previous_end_lba) {
            const uint32_t free_space = start_lba - previous_end_lba;
            if (free_space > largest_free_space) {
                largest_free_space = free_space;
                largest_start_address = previous_end_lba;
            }
        }

        previous_end_lba = end_lba;
    }

    /* Check for free space after the last partition until the end of the disk */
    const uint32_t free_space = disk_size_sectors - previous_end_lba;
    if (free_space > largest_free_space) {
        largest_free_space = free_space;
        largest_start_address = previous_end_lba;
    }

    if (largest_free_lba) {
        *largest_free_lba = largest_start_address;
    }

    return largest_free_space;
}


static uint64_t align_lba_address(uint32_t lba_start_address, uint32_t alignment, uint32_t* sectors)
{
    const uint32_t align_sector = (alignment / DISK_SECTOR_SIZE) - 1;
    const uint32_t new_lba_address = (lba_start_address + align_sector) & ~align_sector;

    if (sectors) {
        *sectors -= new_lba_address - lba_start_address;
    }

    return new_lba_address;
}


const char* const *disk_get_partition_size_list(void)
{
    static const char* const sizes[] = {
        "64KiB", "128KiB", "256KiB", "512KiB",
        "1MiB", "2MiB", "4MiB", "8MiB",
        "16MiB", "32MiB", "64MiB", "128MiB", "256MiB", "512MiB",
        "1GiB", "2GiB", "4GiB"
    };
    return sizes;
}


/**
 * @brief Get the number of valid entries for a new partition
 */
int disk_valid_partition_size(disk_info_t *disk, uint32_t *largest_free_lba)
{
    const uint32_t free_sectors = disk_largest_free_space(disk, largest_free_lba);
    if (free_sectors == 0) {
        return 0;
    }

    /* Try to align the LBA address on 1MB, if it results in a free size of 0, try to align on 4KB, else ... no space*/
    uint32_t aligned_lba_sectors = free_sectors;
    uint32_t aligned_lba_address = align_lba_address(*largest_free_lba, MB, &aligned_lba_sectors);
    if (aligned_lba_sectors == 0) {
        /* Try to align on 4KB instead */
        aligned_lba_sectors = free_sectors;
        aligned_lba_address = align_lba_address(*largest_free_lba, 4*KB, &aligned_lba_sectors);
        /* If still no free sector, give up */
        if (aligned_lba_sectors == 0) {
            return 0;
        }
    }
    *largest_free_lba = aligned_lba_address;

    const uint64_t free_space = aligned_lba_sectors * DISK_SECTOR_SIZE;
    const uint64_t sizes[] = {
        64*KB, 128*KB, 256*KB, 512*KB, 1*MB, 2*MB, 4*MB, 8*MB,
        16*MB, 32*MB, 64*MB, 128*MB, 256*MB, 512*MB,
        1*GB, 2*GB, 4*GB
    };

    for (int i = 0; i < (sizeof(sizes) / sizeof(*sizes)); i++) {
        if (sizes[i] > free_space) {
            return i;
        }
    }

    return -1;
}

