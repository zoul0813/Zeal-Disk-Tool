/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef POPUP_H
#define POPUP_H

#include <stdint.h>
#include "nuklear.h"

#define POPUP_COUNT    4

typedef enum {
    POPUP_MBR     = 0,
    POPUP_NEWPART = 1,
    POPUP_APPLY   = 2,
    POPUP_CANCEL  = 3,
} popup_t;


typedef struct {
    const char* title;
    const char* msg;
} popup_info_t;

void popup_init(int win_width, int win_height);

int popup_any_opened(void);

int popup_is_opened(popup_t id, struct nk_rect* pos, void** arg);

void popup_open(popup_t id, float w, float h, void* arg);

void popup_close(popup_t id);

#endif // POPUP_H
