/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "popup.h"


static struct {
    int shown;
    float w;
    float h;
    void* arg;
} s_popup_state[POPUP_COUNT] = { 0 };

static int s_window_width;
static int s_window_height;


void popup_init(int win_width, int win_height)
{
    s_window_width = win_width;
    s_window_height = win_height;
}


int popup_any_opened(void)
{
    for (int i = 0; i < POPUP_COUNT; i++)  {
        if (s_popup_state[i].shown) {
            return 1;
        }
    }
    return 0;
}


int popup_is_opened(popup_t id, struct nk_rect* pos, void** arg)
{
    if (!s_popup_state[id].shown) {
        return 0;
    }
    if (pos) {
        pos->x = s_window_width  / 2 - s_popup_state[id].w / 2;
        pos->y = s_window_height / 2 - s_popup_state[id].h / 2;
        pos->w = s_popup_state[id].w;
        pos->h = s_popup_state[id].h;
    }
    if (arg) {
        *arg = s_popup_state[id].arg;
    }
    return 1;
}


void popup_open(popup_t id, float w, float h, void* arg)
{
    s_popup_state[id].shown = 1;
    s_popup_state[id].w = w;
    s_popup_state[id].h = h;
    s_popup_state[id].arg = arg;
}


void popup_close(popup_t id)
{
    s_popup_state[id].shown = 0;
}

