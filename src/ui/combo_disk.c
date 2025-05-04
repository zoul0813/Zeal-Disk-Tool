#include <stdio.h>
#include <assert.h>

#include "nuklear.h"
#include "raylib-nuklear.h"
#include "raylib.h"
#include "ui.h"

int ui_combo_disk(struct nk_context *ctx, int selected, disk_info_t *disks, int disk_count, int width) {
    /* If we didn't find any disk, at least show the first one in teh combo box, it that has a dummy message */
    const int show_items = NK_MAX(disk_count, 1);

    struct nk_vec2 size = nk_vec2(width, winHeight);

    int i = 0;
    int max_height;
    struct nk_vec2 item_spacing;
    struct nk_vec2 window_padding;

    assert(ctx);
    assert(disks);
    assert(ctx->current);
    if (!ctx || !disks || !show_items)
        return selected;

    item_spacing = ctx->style.window.spacing;
    window_padding = ctx->style.window.combo_padding;
    max_height = show_items * COMBO_HEIGHT + show_items * (int)item_spacing.y;
    max_height += (int)item_spacing.y * 2 + (int)window_padding.y * 2;
    size.y = NK_MIN(size.y, (float)max_height);
    if (nk_combo_begin_label(ctx, disks[selected].label, size)) {
        nk_layout_row_dynamic(ctx, (float)COMBO_HEIGHT, 1);
        for (i = 0; i < show_items; ++i) {
            if(disks[i].valid) {
                if(selected == i) {
                    nk_style_push_color(ctx, &ctx->style.contextual_button.text_normal, nk_rgb(0,127,127));
                    char label[DISK_LABEL_LEN];
                    snprintf(label, DISK_LABEL_LEN, "%s", disks[i].label);
                    label[0] = '*';
                    nk_combo_item_label(ctx, label, NK_TEXT_LEFT);
                    nk_style_pop_color(ctx);
                } else {
                    if (nk_combo_item_label(ctx, disks[i].label, NK_TEXT_LEFT))
                        selected = i;
                }
            } else {
                nk_style_push_color(ctx, &ctx->style.text.color, nk_rgb(255,87,51));
                nk_label(ctx, disks[i].label, NK_TEXT_LEFT);
                nk_style_pop_color(ctx);
            }
        }
        nk_combo_end(ctx);
    }
    return selected;
}