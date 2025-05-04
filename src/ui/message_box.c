#include "ui.h"
#include "raylib.h"
#include "nuklear.h"
#include "raylib-nuklear.h"

/* This should only be the case for windows, but keep the code just in case */
int message_box(struct nk_context *ctx, const char* message)
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