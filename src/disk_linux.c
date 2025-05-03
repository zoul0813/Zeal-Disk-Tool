/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#define DEBUG_DISKS 0

disk_err_t disk_list(disk_info_t* out_disks, int max_disks, int* out_count) {

#if DEBUG_DISKS
    out_disks[0] =  (disk_info_t) {
        .name = "/dev/sda",
        .size_bytes = 4026531840ULL,
        .has_mbr = true,
    };
    out_disks[1] =  (disk_info_t) {
        .name = "/dev/sdb",
        .size_bytes = 32*1024*1024,
        .has_mbr = true,
    };
    *out_count = 2;

    return ERR_SUCCESS;
#endif
    memset(out_disks, 0, sizeof(disk_info_t) * max_disks);
    *out_count = 0;

    for (char c = 'a'; c <= 'z' && *out_count < max_disks; ++c) {
        char path[256];
        snprintf(path, sizeof(path), "/dev/sd%c", c);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            if (errno == EACCES) {
                return ERR_NOT_ADMIN;
            } else if (errno == ENOENT) {
                continue;
            }
            perror("Could not open the disk");
            continue;
        }

        disk_info_t* info = &out_disks[*out_count];
        strcpy(info->name, path);
        strcpy(info->path, path);

        /* Get the size of the disk, make sure it is not bigger than expected */
        if (ioctl(fd, BLKGETSIZE64, &info->size_bytes) != 0) {
            fprintf(stderr, "Could not get disk %s size: %s\n", path, strerror(errno));
            close(fd);
            return 1;
        }


        if (info->size_bytes > MAX_DISK_SIZE) {
            close(fd);
            fprintf(stderr, "%s exceeds max disk size of %lluGB with %lluGB bytes\n", path, MAX_DISK_SIZE/GB, info->size_bytes/GB);
            continue;
        }

        /* Read MBR */
        ssize_t r = read(fd, info->mbr, DISK_SECTOR_SIZE);
        if (r == DISK_SECTOR_SIZE) {
            info->has_mbr = (info->mbr[DISK_SECTOR_SIZE - 2] == 0x55 &&
                             info->mbr[DISK_SECTOR_SIZE - 1] == 0xAA);
        } else {
            info->has_mbr = false;
        }

        close(fd);
        (*out_count)++;
    }

    return ERR_SUCCESS;
}


const char* disk_write_changes(disk_info_t* disk)
{
#if DEBUG_DISKS
    return NULL;
#endif
    static char error_msg[1024];
    assert(disk);
    assert(disk->has_mbr);
    assert(disk->has_staged_changes);
    /* Reopen the disk to write it back */

    int fd = open(disk->name, O_WRONLY);
    if (fd < 0) {
        sprintf(error_msg, "Could not open disk %s: %s\n", disk->name, strerror(errno));
        return error_msg;
    }

    /* Write MBR */
    ssize_t wr = write(fd, disk->staged_mbr, sizeof(disk->staged_mbr));
    if (wr != DISK_SECTOR_SIZE) {
        sprintf(error_msg, "Could not write disk %s: %s\n", disk->name, strerror(errno));
        goto error;
    }

    /* Write any new partition */
    for (int i = 0; i < MAX_PART_COUNT; i++) {
        const partition_t* part = &disk->staged_partitions[i];
        if (part->data != NULL && part->data_len != 0) {
            /* Data need to be written back to the disk */
            off_t part_offset = part->start_lba * DISK_SECTOR_SIZE;
            const off_t offset = lseek(fd, part_offset, SEEK_SET);
            printf("[DISK] Writing partition %d @ %08lx, %d bytes\n", i, offset, part->data_len);
            if (offset != part_offset){
                sprintf(error_msg, "Could not offset in the disk %s: %s\n", disk->name, strerror(errno));
                goto error;
            }
            wr = write(fd, part->data, part->data_len);
            if (wr != part->data_len) {
                sprintf(error_msg, "Could not write partition to disk %s: %s\n", disk->name, strerror(errno));
                goto error;
            }
        } else {
            printf("[DISK] Partition %d has no changes\n", i);
        }
    }

    /* Apply the changes in RAM too */
    disk_apply_changes(disk);
    close(fd);
    return NULL;
error:
    close(fd);
    return error_msg;
}