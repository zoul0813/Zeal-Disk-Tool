/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include "disk.h"

disk_err_t disk_list(disk_info_t* out_disks, int max_disks, int* out_count) {
    *out_count = 0;
    for (int i = 0; i < max_disks && *out_count < max_disks; ++i) {
        char path[256];
        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", i);

        HANDLE hDisk = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hDisk == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_ACCESS_DENIED) {
                printf("Program must be run as Administrator!\n");
                return ERR_NOT_ADMIN;
            }
            continue;
        }

        disk_info_t* info = &out_disks[*out_count];
        snprintf(info->name, sizeof(info->name), "PhysicalDrive%d", i);
        strcpy(info->path, path);

        /* Get the size of the disk, exclude any disk bigger than 32GB to prevent mistakes */
        GET_LENGTH_INFORMATION lenInfo;
        DWORD bytesReturned;
        if (DeviceIoControl(hDisk, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lenInfo, sizeof(lenInfo), &bytesReturned, NULL)) {
            info->size_bytes = lenInfo.Length.QuadPart;
        } else {
            info->size_bytes = 0;
        }

        if (info->size_bytes > MAX_DISK_SIZE) {
            CloseHandle(hDisk);
            fprintf(stderr, "%s exceeds max disk size of %lluGB with %lluGB bytes\n", path, MAX_DISK_SIZE/GB, info->size_bytes/GB);
            continue;
        }

        /* Read MBR */
        DWORD bytesRead;
        SetFilePointer(hDisk, 0, NULL, FILE_BEGIN);
        if (ReadFile(hDisk, info->mbr, sizeof(info->mbr), &bytesRead, NULL) && bytesRead == DISK_SECTOR_SIZE) {
            info->has_mbr = (info->mbr[DISK_SECTOR_SIZE - 2] == 0x55 &&
                             info->mbr[DISK_SECTOR_SIZE - 1] == 0xAA);
        } else {
            info->has_mbr = false;
        }

        CloseHandle(hDisk);
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


    HANDLE fd = CreateFileA(disk->path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (fd == INVALID_HANDLE_VALUE) {
        snprintf(error_msg, sizeof(error_msg),
            "Could not open disk %s: %lu\n", disk->path, GetLastError());
        return error_msg;
    }

    /* Let's be safe and set the pointer */
    SetFilePointer(fd, 0, NULL, FILE_BEGIN);
    DWORD wr = 0;
    BOOL success = WriteFile(fd, disk->staged_mbr, 512, &wr, NULL);
    if (!success || wr != DISK_SECTOR_SIZE) {
        snprintf(error_msg, sizeof(error_msg),
            "Could not write disk %s: %lu\n", disk->name, GetLastError());
        goto error;
    }

    /* Write any new partition */
    for (int i = 0; i < MAX_PART_COUNT; i++) {
        const partition_t* part = &disk->staged_partitions[i];
        if (part->data != NULL && part->data_len != 0) {
            /* Data need to be written back to the disk */
            LARGE_INTEGER offset = {
                .QuadPart = 0
            };
            LARGE_INTEGER part_offset = {
                .QuadPart = part->start_lba * DISK_SECTOR_SIZE
            };
            success = SetFilePointerEx(fd, part_offset, &offset, FILE_BEGIN);
            printf("[DISK] Writing partition %d @ %08llx, %d bytes\n", i, offset.QuadPart, part->data_len);
            if (offset.QuadPart != part_offset.QuadPart){
                sprintf(error_msg, "Could not offset in the disk %s: %lu\n", disk->name, GetLastError());
                goto error;
            }
            success = WriteFile(fd, part->data, part->data_len, &wr, NULL);
            if (!success || wr != part->data_len) {
                sprintf(error_msg, "Could not write partition to disk %s: %lu\n", disk->name, GetLastError());
                goto error;
            }
        } else {
            printf("[DISK] Partition %d has no changes\n", i);
        }
    }

    /* Apply the changes in RAM too */
    disk_apply_changes(disk);
    CloseHandle(fd);
    return NULL;
error:
    CloseHandle(fd);
    return error_msg;
}
