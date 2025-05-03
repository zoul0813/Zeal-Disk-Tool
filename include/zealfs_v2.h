/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#ifndef BIT
#define BIT(X)  (1ULL << (X))
#endif

#ifndef KB
#define KB  (1024ULL)
#endif

#ifndef MB
#define MB  (1048576ULL)
#endif

#ifndef GB
#define GB  (1073741824ULL)
#endif


/* Type for partition header */
typedef struct {
  uint8_t magic;    /* Must be 'Z' ascii code */
  uint8_t version;  /* Version of the file system */
  /* Number of bytes composing the bitmap */
  uint16_t bitmap_size;
  /* Number of free pages, we always have at most 65535 pages */
  uint16_t free_pages;
  /* Size of the pages:
   * 0 - 256
   * 1 - 512
   * 2 - 1024
   * 3 - 2048
   * 4 - 4096
   * 5 - 8192
   * 6 - 16384
   * 7 - 32768
   * 8 - 65536
   */
  uint8_t page_size;
  uint8_t pages_bitmap[];
} __attribute__((packed)) ZealFSHeader;


/**
 * @brief Helper to get the recommended page size from a disk size.
 *
 */
static inline int zealfsv2_page_size(long long part_size)
{
    if (part_size <= 64*KB) {
        return 256;
    } else if (part_size <= 256*KB) {
        return 512;
    } else if (part_size <= 1*MB) {
        return 1*KB;
    } else if (part_size <= 4*MB) {
        return 2*KB;
    } else if (part_size <= 16*MB) {
        return 4*KB;
    } else if (part_size <= 64*MB) {
        return 8*KB;
    } else if (part_size <= 256*MB) {
        return 16*KB;
    } else if (part_size <= 1*GB) {
        return 32*KB;
    }

    return 64*KB;
}


/**
 * @brief Format the partition.
 *
 * @param partition Pointer to the partition data.
 * @param size Size of the whole partition.
 *
 * @return 0 on success, error else
 */
static int zealfsv2_format(uint8_t* partition, uint64_t size) {
    /* Initialize image header */
    ZealFSHeader* header = (ZealFSHeader*) partition;
    header->magic = 'Z';
    header->version = 2;
    /* According to the size of the disk, we have to calculate the size of the pages */
    const int page_size_bytes = zealfsv2_page_size(size);
    /* The page size in the header is the log2(page_bytes/256) - 1 */
    header->page_size = ((sizeof(int) * 8) - (__builtin_clz(page_size_bytes >> 8))) - 1;
    header->bitmap_size = size / page_size_bytes / 8;
    /* If the page size is 256, there will be only one page for the FAT */
    const int fat_pages_count = 1 + (page_size_bytes == 256 ? 0 : 1);
    /* Do not count the first page and the second page */
    header->free_pages = size / page_size_bytes - 1 - fat_pages_count;
    /* All the pages are free (0), mark the first one as occupied */
    header->pages_bitmap[0] = 3 | ((fat_pages_count > 1) ? 4 : 0);

    printf("[ZEALFS] Bitmap size: %d bytes\n", header->bitmap_size);
    printf("[ZEALFS] Pages size: %d bytes (code %d)\n", page_size_bytes, ((page_size_bytes >> 8) - 1));
#if 0
    printf("[ZEALFS] Maximum root entries: %d\n", getRootDirMaxEntries(header));
    printf("[ZEALFS] Maximum dir entries: %d\n", getDirMaxEntries(header));
    printf("[ZEALFS] Header size/Root entries: %d (0x%x)\n", getFSHeaderSize(header), getFSHeaderSize(header));
#endif

    return 0;
}