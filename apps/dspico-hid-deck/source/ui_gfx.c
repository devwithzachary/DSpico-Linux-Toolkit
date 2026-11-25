#include "ui_gfx.h"
#include "dspico_font8x8.h"

#include <string.h>

void ui_fill_rect(u16 *fb, int x, int y, int w, int h, u16 col)
{
    u16 c = col | BIT(15);
    for (int py = y; py < y + h; py++)
    {
        if (py < 0 || py >= 192) continue;
        u16 *row = &fb[py * 256];
        for (int px = x; px < x + w; px++)
        {
            if (px >= 0 && px < 256)
            {
                row[px] = c;
            }
        }
    }
}

void ui_draw_box(u16 *fb, int x, int y, int w, int h, u16 fill, u16 border)
{
    ui_fill_rect(fb, x, y, w, h, fill);
    u16 b = border | BIT(15);
    for (int px = x; px < x + w; px++)
    {
        if (px >= 0 && px < 256)
        {
            if (y >= 0 && y < 192) fb[y * 256 + px] = b;
            if (y + h - 1 >= 0 && y + h - 1 < 192) fb[(y + h - 1) * 256 + px] = b;
        }
    }
    for (int py = y; py < y + h; py++)
    {
        if (py >= 0 && py < 192)
        {
            if (x >= 0 && x < 256) fb[py * 256 + x] = b;
            if (x + w - 1 >= 0 && x + w - 1 < 256) fb[py * 256 + x + w - 1] = b;
        }
    }
}

void ui_draw_char(u16 *fb, int x, int y, char ch, u16 col)
{
    if (ch < 0x20 || ch > 0x7E) return;
    u16 c = col | BIT(15);
    const uint8_t *glyph = dspico_font8x8[ch - 0x20];
    for (int r = 0; r < 8; r++)
    {
        int py = y + r;
        if (py < 0 || py >= 192) continue;
        uint8_t bits = glyph[r];
        for (int ci = 0; ci < 8; ci++)
        {
            if (bits & (1u << ci))
            {
                int px = x + ci;
                if (px >= 0 && px < 256)
                {
                    fb[py * 256 + px] = c;
                }
            }
        }
    }
}

void ui_draw_text(u16 *fb, int x, int y, const char *str, u16 col)
{
    int cx = x;
    for (int i = 0; str[i] != '\0'; i++)
    {
        ui_draw_char(fb, cx, y, str[i], col);
        cx += 6;
    }
}

void ui_draw_text_centered(u16 *fb, int x, int y, int w, int h, const char *str, u16 col)
{
    int len = (int)strlen(str);
    int tx = x + (w - len * 6) / 2;
    int ty = y + (h - 8) / 2;
    ui_draw_text(fb, tx, ty, str, col);
}
