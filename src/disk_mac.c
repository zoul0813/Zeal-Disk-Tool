/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disk.h>

#define DEBUG_DISKS 0


disk_err_t disk_list(disk_info_t* out_disks, int max_disks, int* out_count) {
    memset(out_disks, 0, sizeof(disk_info_t) * max_disks);
    *out_count = 0;

    for (int i = 1; i <= 9 && *out_count < max_disks; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/rdisk%d", i);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            if (errno == EACCES) {
                return ERR_NOT_ADMIN;
            }
            continue;
        }

        uint64_t block_count = 0, block_size = 0;
        if (ioctl(fd, DKIOCGETBLOCKCOUNT, &block_count) != 0 ||
            ioctl(fd, DKIOCGETBLOCKSIZE, &block_size) != 0) {
            close(fd);
            continue;
        }

        uint64_t size_bytes = block_count * block_size;
        if (size_bytes > MAX_DISK_SIZE) {
            close(fd);
            continue;
        }

        disk_info_t* info = &out_disks[*out_count];
        strncpy(info->name, path, sizeof(info->name) - 1);
        info->size_bytes = size_bytes;

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

    int fd = open(disk->name, O_WRONLY);
    if (fd < 0) {
        snprintf(error_msg, sizeof(error_msg), "Could not open disk %s: %s\n",
                 disk->name, strerror(errno));
        return error_msg;
    }

    ssize_t wr = write(fd, disk->staged_mbr, DISK_SECTOR_SIZE);
    close(fd);

    if (wr != DISK_SECTOR_SIZE) {
        snprintf(error_msg, sizeof(error_msg), "Could not write disk %s: %s\n",
                 disk->name, strerror(errno));
        return error_msg;
    }

    disk_apply_changes(disk);

    return NULL;
}
