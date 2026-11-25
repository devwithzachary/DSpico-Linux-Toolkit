#ifndef DSPICO_HID_UI_GFX_H
#define DSPICO_HID_UI_GFX_H

#include <nds.h>
#include <stdint.h>

#define UI_C_BG         RGB15(2, 3, 5)
#define UI_C_PANEL      RGB15(4, 6, 9)
#define UI_C_BAR        RGB15(5, 8, 12)
#define UI_C_BTN        RGB15(6, 10, 15)
#define UI_C_BTN_ACT    RGB15(10, 22, 30)
#define UI_C_BORDER     RGB15(11, 16, 22)
#define UI_C_TEXT       RGB15(30, 31, 31)
#define UI_C_MUTED      RGB15(17, 20, 24)
#define UI_C_OK         RGB15(8, 29, 12)
#define UI_C_WARN       RGB15(30, 22, 6)
#define UI_C_ERR        RGB15(30, 8, 8)
#define UI_C_ACCENT     RGB15(8, 24, 30)

void ui_fill_rect(u16 *fb, int x, int y, int w, int h, u16 col);
void ui_draw_box(u16 *fb, int x, int y, int w, int h, u16 fill, u16 border);
void ui_draw_char(u16 *fb, int x, int y, char ch, u16 col);
void ui_draw_text(u16 *fb, int x, int y, const char *str, u16 col);
void ui_draw_text_centered(u16 *fb, int x, int y, int w, int h, const char *str, u16 col);

#endif /* DSPICO_HID_UI_GFX_H */
