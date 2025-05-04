#include <stdio.h>
#include "raylib.h"
#include "nuklear.h"
#include "raylib-nuklear.h"
#include "ui.h"

int ui_combo_disk(struct nk_context *ctx, int selection, const char **disk_labels, int disk_count, int width) {
    /* If we didn't find any disk, at least show the first one in teh combo box, it that has a dummy message */
    const int show_items = NK_MAX(disk_count, 1);
    return nk_combo(ctx, disk_labels, show_items, selection, COMBO_HEIGHT, nk_vec2(width, winHeight));
}