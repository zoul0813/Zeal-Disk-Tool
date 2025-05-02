/**
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "app_version.h"
#include "app_icon.h"
#include "raylib.h"
#include "nuklear.h"
#include "raylib-nuklear.h"
#include "disk.h"
#include "popup.h"


#define MIN_WIN_WIDTH   800
#define MIN_WIN_HEIGHT  400
#define WIN_SCALE       0.5
#define WIN_ASPECT      (9.0/16.0)
#define NK_WHITE        nk_rgb(0xff, 0xff, 0xff)
#define NK_BLACK        nk_rgb(0, 0, 0)
#define NK_TRANSPARENT  nk_rgba_f(0, 0, 0, 0)


/* Background color for the selected partition in the diagram */
#define NK_SELECTED nk_rgb(0xf8, 0xf8, 0xba)

/* Background color for the selected partition in the list */
#define NK_LIST_SELECTED nk_rgb(0x55, 0x55, 0x55)

#define COMBO_HEIGHT     30


static struct nk_context *ctx;
static disk_info_t disks[MAX_DISKS];

int winWidth, winHeight;

/* This should only be the case for windows, but keep the code just in case */
static int message_box(const char* message)
{

    const int fontSize = 24;
    Font font = LoadFontFromNuklear(fontSize);
    ctx = InitNuklearEx(font, fontSize);
    nk_style_push_color(ctx, &ctx->style.text.color, nk_rgba(255,0,0,255));
    SetWindowSize(MIN_WIN_WIDTH, MIN_WIN_HEIGHT);

    while (!WindowShouldClose()) {
        UpdateNuklear(ctx);

        if (nk_begin(ctx, "Disks", nk_rect(0, 0, MIN_WIN_WIDTH, MIN_WIN_HEIGHT), 0)) {
            nk_layout_row_dynamic(ctx, MIN_WIN_HEIGHT, 1);
            nk_label(ctx, message, NK_TEXT_CENTERED);
        }
        nk_end(ctx);


        BeginDrawing(); {
            ClearBackground(WHITE);
            DrawNuklear(ctx);
        }
        EndDrawing();
    }

    UnloadNuklear(ctx);
    CloseWindow();
    return 0;
}


static struct nk_color get_partition_color(int i)
{
    switch (i) {
        case 0:  return nk_rgb(0x4f, 0xad, 0x4f); break;
        case 1:  return nk_rgb(0x39, 0x5b, 0x7e); break;
        case 2:  return nk_rgb(0x9f, 0x62, 0xb6); break;
        case 3:  return nk_rgb(0xc9, 0x4b, 0x24); break;
        default: return nk_rgb(200, 200, 200); break;
    }
}


static void ui_draw_disk(struct nk_context *ctx, const disk_info_t *disk, int* selected_part) {
    const uint64_t total_sectors = disk->size_bytes / DISK_SECTOR_SIZE;

    nk_layout_row_dynamic(ctx, 100, 1);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    float full_width = bounds.w;
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

    nk_fill_rect(canvas, bounds, 0, nk_rgb(220, 220, 220));

    for (int i = 0; i < MAX_PART_COUNT; ++i) {
        const partition_t *p = &disk->staged_partitions[i];
        if (!p->active || p->size_sectors == 0)
            continue;

        float start_frac = (float)p->start_lba / (float)total_sectors;
        float size_frac = (float)p->size_sectors / (float)total_sectors;

        struct nk_rect part_rect = nk_rect(
            bounds.x + full_width * start_frac,
            bounds.y,
            MAX(full_width * size_frac, 10),
            bounds.h
        );

        struct nk_color part_color = get_partition_color(i);

        if (*selected_part == i) {
            nk_fill_rect(canvas, part_rect, 0, NK_SELECTED);
        } else {
            nk_fill_rect(canvas, part_rect, 0, NK_WHITE);
        }
        nk_stroke_rect(canvas, part_rect, 0, 5.0f, part_color);

        char label[128];
        snprintf(label, sizeof(label), "Part. %d", i);

        /* Measure text size */
        float text_width = ctx->style.font->width(ctx->style.font->userdata, ctx->style.font->height, label, strlen(label));
        float text_height = ctx->style.font->height;

        /* Draw text centered if there is enough space (not counting the borders) */
        if (text_width < part_rect.w - 10) {
            /* Compute centered position */
            const float label_x = part_rect.x + (part_rect.w - text_width) / 2.0f;
            const float label_y = part_rect.y + (part_rect.h - text_height) / 2.0f;
            nk_draw_text(canvas, nk_rect(label_x, label_y, text_width, text_height),
                label, strlen(label), ctx->style.font, NK_BLACK, NK_BLACK);
        }
    }

    // Draw table header
    const float ratios[] = {
        0.04f,  // Color
        0.05f,  // Padding
        0.15f,  // Number
        0.20f,  // File System
        0.15f,  // Start address
        0.15f,  // Size
        0.41f,  // Padding
    };

    // Draw table header
    nk_layout_row(ctx, NK_DYNAMIC, 25, 7, ratios);
    nk_label(ctx, "Color",              NK_TEXT_CENTERED);
    nk_label(ctx, " ",                  NK_TEXT_LEFT);
    nk_label(ctx, "Partition",          NK_TEXT_LEFT);
    nk_label(ctx, "File System (Type)", NK_TEXT_LEFT);
    nk_label(ctx, "Start address",      NK_TEXT_LEFT);
    nk_label(ctx, "Size",               NK_TEXT_CENTERED);
    nk_label(ctx, " ",                  NK_TEXT_LEFT);

    /* Make all the elements' background transparent in the list */
    nk_style_push_color(ctx, &ctx->style.selectable.normal.data.color, NK_TRANSPARENT);
    nk_style_push_color(ctx, &ctx->style.selectable.hover.data.color,  NK_TRANSPARENT);
    nk_style_push_color(ctx, &ctx->style.selectable.pressed.data.color,  NK_TRANSPARENT);
    nk_style_push_color(ctx, &ctx->style.selectable.normal_active.data.color, NK_TRANSPARENT);
    nk_style_push_color(ctx, &ctx->style.selectable.hover_active.data.color,  NK_TRANSPARENT);
    nk_style_push_color(ctx, &ctx->style.selectable.pressed_active.data.color,  NK_TRANSPARENT);

    for (int i = 0; i < MAX_PART_COUNT; i++) {
        char buffer[256];
        const partition_t* part = &disk->staged_partitions[i];
        if (!part->active || part->size_sectors == 0) {
            continue;
        }

        /* Fill the whole line first to create a "selected" effect */
        if (*selected_part == i) {
            bounds = nk_widget_bounds(ctx);
            bounds.w = winWidth;
            bounds.x = 0;
            nk_fill_rect(canvas, bounds, 2.0f, NK_LIST_SELECTED);
        }

        /* Partition color */
        /* Rectangles don't count as an element in the flow so add a dummy element right after */
        bounds = nk_widget_bounds(ctx);
        /* Arrange a bit the colored square*/
        bounds.h -= 10;
        bounds.w -= 10;
        bounds.y += 5;
        bounds.x += 5;
        nk_fill_rect(canvas, bounds, 2.f, get_partition_color(i));
        nk_bool select = false;
        nk_selectable_label(ctx, " ", NK_TEXT_LEFT, &select);
        nk_selectable_label(ctx, " ", NK_TEXT_LEFT, &select);

        /* Partition number */
        sprintf(buffer, "%d", i);
        nk_selectable_label(ctx, buffer, NK_TEXT_LEFT, &select);

        /* Partition file system */
        nk_selectable_label(ctx, disk_get_fs_type(part->type), NK_TEXT_LEFT, &select);

        /* Partition start address */
        sprintf(buffer, "0x%08x", part->start_lba * DISK_SECTOR_SIZE);
        nk_selectable_label(ctx, buffer, NK_TEXT_LEFT, &select);

        /* Partition size */
        disk_get_size_str(part->size_sectors * DISK_SECTOR_SIZE, buffer, sizeof(buffer));
        nk_selectable_label(ctx, buffer, NK_TEXT_RIGHT, &select);
        nk_selectable_label(ctx, " ", NK_TEXT_LEFT, &select);

        if (select) {
            *selected_part = i;
        }
    }

    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
}


static void ui_mbr_handle(struct nk_context *ctx, disk_info_t* disk)
{
    void* arg;
    struct nk_rect position;
    if (popup_is_opened(POPUP_MBR, &position, &arg)) {
        popup_info_t* info = (popup_info_t*) arg;

        if(nk_begin(ctx, info->title, position, NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
            nk_window_set_bounds(ctx, info->title, position);
            nk_layout_row_dynamic(ctx, 40, 1);
            nk_label_wrap(ctx, info->msg);
            if (nk_button_label(ctx, "Okay")) {
                popup_close(POPUP_MBR);
            }
        }
        nk_end(ctx);
    }

    (void) disk;
}


static void ui_apply_handle(struct nk_context *ctx, disk_info_t* disk)
{
    struct nk_rect position;
    if (popup_is_opened(POPUP_APPLY, &position, NULL)) {

        if(nk_begin(ctx, "Apply changes", position, NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label_wrap(ctx, "Apply changes to disk? This action is permanent and cannot be undone.");
            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Yes")) {
                static popup_info_t result_info = {
                    .title = "Apply changes",
                    .msg = "Success!"
                };
                const char* error_str = disk_write_changes(disk);
                if (error_str) {
                    result_info.msg = error_str;
                    printf("%s\n", error_str);
                } else {
                    /* Success! Remove the pending changes mark */
                    disk->label[0] = ' ';
                }
                popup_close(POPUP_APPLY);
                popup_open(POPUP_MBR, 300, 140, &result_info);
            } else if (nk_button_label(ctx, "No")) {
                popup_close(POPUP_APPLY);
            }
        }
        nk_end(ctx);
    }

    (void) disk;
}


static void ui_cancel_handle(struct nk_context *ctx, disk_info_t* disk)
{
    struct nk_rect position;
    if (popup_is_opened(POPUP_CANCEL, &position, NULL)) {

        if(nk_begin(ctx, "Cancel changes", position, NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label_wrap(ctx, "Discard all changes? All unsaved changes will be lost.");
            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Yes")) {
                disk_revert_changes(disk);
                /* Remove the symbol showing pending changes */
                disk->label[0] = ' ';
                popup_close(POPUP_CANCEL);
            } else if (nk_button_label(ctx, "No")) {
                popup_close(POPUP_CANCEL);
            }
        }
        nk_end(ctx);
    }

    (void) disk;
}



/**
 * @brief Render the new partition popup
 */
static void ui_new_partition(struct nk_context *ctx, disk_info_t* disk)
{
    struct nk_rect position;
    void *arg;
    if (!popup_is_opened(POPUP_NEWPART, &position, &arg)) {
        return;
    }
    if(nk_begin(ctx, "Create a new partition", position, NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE)) {
        /* If there are no empty partition, show an error */
        if (disk->free_part_idx == -1) {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "No free partition found on this disk", NK_TEXT_CENTERED);
            if (nk_button_label(ctx, "Cancel")) {
                popup_close(POPUP_NEWPART);
            }
            nk_end(ctx);
            return;
        }

        /* There is a free partition on the disk */
        uint32_t largest_free_lba_addr = 0;

        const float ratio[] = { 0.3f, 0.6f };
        nk_layout_row(ctx, NK_DYNAMIC, COMBO_HEIGHT, 2, ratio);

        /* Combo box for the partition type, only ZealFS v2 (for now?) */
        nk_label(ctx, "Type:", NK_TEXT_CENTERED);
        const char* types[] = { "ZealFSv2" };
        const float width = nk_widget_width(ctx);
        nk_combo(ctx, types, 1, 0, COMBO_HEIGHT, nk_vec2(width, 150));

        /* For the partition size, do not propose anything bigger than the disk size of course */
        nk_label(ctx, "Size:", NK_TEXT_CENTERED);
        const int valid_entries = disk_valid_partition_size(disk, &largest_free_lba_addr);
        int *selected = (int*) arg;
        if (valid_entries > 0) {
            /* Make sure the selection isn't bigger than the last valid size */
            *selected = NK_MIN(*selected, valid_entries - 1);
            *selected = nk_combo(ctx, disk_get_partition_size_list(), valid_entries,
                                 *selected, COMBO_HEIGHT, nk_vec2(width, 150));
        } else {
            nk_label(ctx, "No size available", NK_TEXT_LEFT);
        }

        /* Show the address where it will be created */
        char address[16];
        const long unsigned largest_free_addr = largest_free_lba_addr * DISK_SECTOR_SIZE;
        nk_label(ctx, "Address:", NK_TEXT_CENTERED);
        snprintf(address, sizeof(address), "0x%08lx", largest_free_addr);
        nk_label(ctx, address, NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 30, 2);

        /* One line padding */
        nk_label(ctx, "", NK_TEXT_CENTERED);
        nk_label(ctx, "", NK_TEXT_CENTERED);

        if (valid_entries != -1 && nk_button_label(ctx, "Create")) {
            /* The user clicked on `Create`, allocate a new ZealFS partition */
            disk_allocate_partition(disk, largest_free_lba_addr, *selected);
            /* Show in the disk list that some changes are pending for the disk */
            disk->label[0] = '*';
            popup_close(POPUP_NEWPART);
        }
        if (nk_button_label(ctx, "Cancel")) {
            popup_close(POPUP_NEWPART);
        }
    }
    nk_end(ctx);
}

static void setup_window() {
    InitWindow(0, 0, "Zeal Disk Tool " VERSION);

    // get current monitor details
    int mw, mh;
    int monitor = GetCurrentMonitor();
    mw = GetMonitorWidth(monitor);
    mh = GetMonitorHeight(monitor);

    // clamp the window size to either WIN_SCALE or MIN
    winWidth = NK_MAX(MIN_WIN_WIDTH, mw * WIN_SCALE);
    winHeight = NK_MAX(MIN_WIN_HEIGHT, winWidth * WIN_ASPECT);
    SetWindowSize(winWidth, winHeight);

    // center the window on the current monitor
    Vector2 mon_pos = GetMonitorPosition(monitor);
    int pos_x = mon_pos.x, pos_y = mon_pos.y;
    pos_x += (mw - winWidth) / 2;
    pos_y += (mh - winHeight) / 2;
    SetWindowPosition(pos_x, pos_y);

#ifndef __APPLE__
    /* Set an icon for the application */
    Image icon = LoadImageFromMemory(".png", s_app_icon_png, sizeof(s_app_icon_png));
    SetWindowIcon(icon);
#endif
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    setup_window();

    SetTargetFPS(60);
    popup_init(winWidth, winHeight);

    int disk_count = 0;
    disk_err_t err = disk_list(disks, MAX_DISKS, &disk_count);

    /* Mac/Linux targets only */
    if (err == ERR_NOT_ROOT) {
        printf("You must run this program as root\n");
        return 1;
    } else if (err == ERR_NOT_ADMIN) {
        return message_box("You must run this program as Administrator!\n");
    }

    const int fontSize = 13;
    Font font = LoadFontFromNuklear(fontSize);
    ctx = InitNuklearEx(font, fontSize);

    /* Construct the labels for the disks */
    static const char* disk_labels[MAX_DISKS] = { 0 };
    int selected_disk = 0;
    for (int i = 0; i < disk_count; ++i) {
        char size_str[128];
        disk_get_size_str(disks[i].size_bytes, size_str, sizeof(size_str));
        /* Keep the first character empty, it will be a `*` in case there is any pending change */
        snprintf(disks[i].label, DISK_LABEL_LEN, " %.*s (%s)", (int) sizeof(disks[i].name), disks[i].name, size_str);
        disk_labels[i] = disks[i].label;
        disk_parse_mbr_partitions(&disks[i]);
    }

    if (disk_count == 0) {
        disk_labels[0] = "No disk found";
    }

    int selected_partition = 0;

    while (!WindowShouldClose()) {
        UpdateNuklear(ctx);

        /* If any popup is opened, the main window must not be focusable */
        const int flags = popup_any_opened() ? NK_WINDOW_NO_INPUT : 0;
        disk_info_t* current_disk = &disks[selected_disk];

        if (nk_begin(ctx, "Disks", nk_rect(0, 0, winWidth, winHeight), flags)) {

            /* Create the top row with the buttons and the disk selection */
            const float ratio[] = { 0.15f, 0.15f, 0.07f, 0.07f, 0.15f, 0.4f };
            nk_layout_row(ctx, NK_DYNAMIC, COMBO_HEIGHT, 6, ratio);

            /* Create the button with label "MBR" */
            if (nk_widget_is_hovered(ctx)) {
                nk_tooltip(ctx, "Create an MBR on the disk");
            }
            /* Only enable the buttons if we have at least one disk */
            if (nk_button_label(ctx, "Create MBR") && disk_count > 0) {
                static popup_info_t info;
                info.title = "Create MBR table";
                if (current_disk->has_mbr) {
                    info.msg = "Selected disk already has an MBR";
                } else {
                    info.msg = "Feature not supported yet";
                    // mbr_last_msg = "MBR created successfully!";
                }
                popup_open(POPUP_MBR, 300, 140, &info);
            }

            /* Create the button to add a new partition */
            if (nk_widget_is_hovered(ctx)) {
                nk_tooltip(ctx, "Create a new partition on the disk");
            }
            if (nk_button_label(ctx, "New partition") && disk_count > 0) {
                static int choosen_option = 0;
                popup_open(POPUP_NEWPART, 300, 300, &choosen_option);
            }

            /* Create the button to commit the changes */
            if (nk_widget_is_hovered(ctx)) {
                nk_tooltip(ctx, "Apply all the changes to the selected disk");
            }
            if (nk_button_label(ctx, "Apply") && disk_count > 0 && current_disk->has_staged_changes) {
                popup_open(POPUP_APPLY, 300, 130, NULL);
            }
            if (nk_widget_is_hovered(ctx)) {
                nk_tooltip(ctx, "Cancel all the changes to the selected disk");
            }
            if (nk_button_label(ctx, "Cancel") && disk_count > 0) {
                popup_open(POPUP_CANCEL, 300, 130, NULL);
            }

            nk_label(ctx, "Select a disk:", NK_TEXT_RIGHT);
            float combo_width = nk_widget_width(ctx);

            /* If we didn't find any disk, at least show the first one in teh combo box, it that has a dummy message */
            const int show_items = MAX(disk_count, 1);
            int selection_height = winHeight * 0.8;
            int new_selection = nk_combo(ctx, &disk_labels[0], show_items, selected_disk, COMBO_HEIGHT, nk_vec2(combo_width, selection_height));
            if (new_selection != selected_disk) {
                if (current_disk->has_staged_changes) {
                    static popup_info_t info = {
                        .title = "Cannot switch disk",
                        .msg = "The selected disk has unsaved changes. Please apply or discard them before switching disks."
                    };
                    popup_open(POPUP_MBR, 300, 140, &info);
                } else {
                    selected_disk = new_selection;
                }
            }
            ui_draw_disk(ctx, current_disk, &selected_partition);
        }
        nk_end(ctx);

        /* Manage the popups here */
        ui_mbr_handle(ctx, current_disk);
        ui_apply_handle(ctx, current_disk);
        ui_cancel_handle(ctx, current_disk);
        ui_new_partition(ctx, current_disk);

        BeginDrawing();
            ClearBackground(WHITE);
            DrawNuklear(ctx);
        EndDrawing();
    }

    UnloadNuklear(ctx);
    CloseWindow();
    return 0;
}
