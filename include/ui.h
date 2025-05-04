#include "disk.h"
#include "nuklear.h"
#include "raylib-nuklear.h"

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


extern int winWidth, winHeight;

int message_box(struct nk_context *ctx, const char* message);
int ui_combo_disk(struct nk_context *ctx, int selection, const char **disk_labels, int disk_count, int width);