#include "macro_deck.h"
#include "dspico_bus.h"
#include "dspico_linux_proto.h"
#include "ui_gfx.h"

#include <stdio.h>
#include <string.h>

typedef enum
{
    MACRO_KBD = 0,
    MACRO_CONSUMER
} MacroKind;

typedef struct
{
    const char *title;
    const char *subtitle;
    MacroKind kind;
    uint8_t mod;
    uint8_t key;
    uint16_t consumer;
    u16 color;
} MacroTile;

static const MacroTile kTiles[12] = {
    { "TERM",  "Ctrl+Alt+T",  MACRO_KBD,      DSPICO_HID_MOD_LCTRL | DSPICO_HID_MOD_LALT,   DSPICO_HID_KEY_T,   0, RGB15(5, 13, 18) },
    { "WS 1",  "Super+1",     MACRO_KBD,      DSPICO_HID_MOD_LGUI,                           DSPICO_HID_KEY_1,   0, RGB15(6, 10, 16) },
    { "WS 2",  "Super+2",     MACRO_KBD,      DSPICO_HID_MOD_LGUI,                           DSPICO_HID_KEY_2,   0, RGB15(6, 10, 16) },
    { "WS 3",  "Super+3",     MACRO_KBD,      DSPICO_HID_MOD_LGUI,                           DSPICO_HID_KEY_3,   0, RGB15(6, 10, 16) },

    { "PLAY",  "Media Play",  MACRO_CONSUMER, 0, 0, DSPICO_HID_CONSUMER_PLAY_PAUSE, RGB15(5, 14, 9) },
    { "MUTE",  "Audio Mute",  MACRO_CONSUMER, 0, 0, DSPICO_HID_CONSUMER_MUTE,       RGB15(16, 7, 7) },
    { "VOL-",  "Volume -",    MACRO_CONSUMER, 0, 0, DSPICO_HID_CONSUMER_VOL_DOWN,   RGB15(8, 10, 14) },
    { "VOL+",  "Volume +",    MACRO_CONSUMER, 0, 0, DSPICO_HID_CONSUMER_VOL_UP,     RGB15(8, 10, 14) },

    { "OBS 1", "C+S+F1",      MACRO_KBD,      DSPICO_HID_MOD_LCTRL | DSPICO_HID_MOD_LSHIFT, DSPICO_HID_KEY_F1,  0, RGB15(13, 8, 18) },
    { "OBS 2", "C+S+F2",      MACRO_KBD,      DSPICO_HID_MOD_LCTRL | DSPICO_HID_MOD_LSHIFT, DSPICO_HID_KEY_F2,  0, RGB15(13, 8, 18) },
    { "OBS 3", "C+S+F3",      MACRO_KBD,      DSPICO_HID_MOD_LCTRL | DSPICO_HID_MOD_LSHIFT, DSPICO_HID_KEY_F3,  0, RGB15(13, 8, 18) },
    { "LOCK",  "Super+L",     MACRO_KBD,      DSPICO_HID_MOD_LGUI,                           DSPICO_HID_KEY_L,   0, RGB15(16, 11, 5) },
};

#define GRID_X0     6
#define GRID_Y0     28
#define TILE_W      58
#define TILE_H      50
#define TILE_GAP_X  4
#define TILE_GAP_Y  4

static int sActiveTile = -1;
static int sReleaseTimer = 0;
static bool sDirty = true;

void macro_deck_init(void)
{
    sActiveTile = -1;
    sReleaseTimer = 0;
    sDirty = true;
}

void macro_deck_update(uint16_t keys_down, uint16_t keys_held, const touchPosition *tp,
                       bool *fired, char *event_desc, int desc_cap)
{
    (void)keys_held;
    if (fired) *fired = false;

    if (sReleaseTimer > 0)
    {
        sReleaseTimer--;
        if (sReleaseTimer == 0 && sActiveTile >= 0)
        {
            const MacroTile *t = &kTiles[sActiveTile];
            if (t->kind == MACRO_KBD)
            {
                dspico_hid_send_report(0, 0, 0, 0, 0, 0);
            }
            else
            {
                dspico_hid_send_consumer(DSPICO_HID_CONSUMER_NONE);
            }
            sActiveTile = -1;
            sDirty = true;
        }
    }

    if ((keys_down & KEY_TOUCH) && sReleaseTimer == 0)
    {
        for (int i = 0; i < 12; i++)
        {
            int col = i % 4;
            int row = i / 4;
            int tx = GRID_X0 + col * (TILE_W + TILE_GAP_X);
            int ty = GRID_Y0 + row * (TILE_H + TILE_GAP_Y);
            if (tp->px >= tx && tp->px < tx + TILE_W &&
                tp->py >= ty && tp->py < ty + TILE_H)
            {
                const MacroTile *t = &kTiles[i];
                if (t->kind == MACRO_KBD)
                {
                    dspico_hid_send_report(0, 0, 0, 0, t->mod, t->key);
                }
                else
                {
                    dspico_hid_send_consumer(t->consumer);
                }
                sActiveTile = i;
                sReleaseTimer = 3;
                sDirty = true;
                if (fired) *fired = true;
                if (event_desc && desc_cap > 0)
                {
                    snprintf(event_desc, (size_t)desc_cap, "%s (%s)", t->title, t->subtitle);
                }
                break;
            }
        }
    }
}

void macro_deck_render(u16 *sub_fb)
{
    if (!sDirty)
    {
        return;
    }

    ui_fill_rect(sub_fb, 0, 22, 256, 170, UI_C_BG);

    for (int i = 0; i < 12; i++)
    {
        int col = i % 4;
        int row = i / 4;
        int tx = GRID_X0 + col * (TILE_W + TILE_GAP_X);
        int ty = GRID_Y0 + row * (TILE_H + TILE_GAP_Y);

        const MacroTile *t = &kTiles[i];
        u16 fill = (i == sActiveTile) ? UI_C_BTN_ACT : t->color;
        ui_draw_box(sub_fb, tx, ty, TILE_W, TILE_H, fill, UI_C_BORDER);
        ui_draw_text_centered(sub_fb, tx, ty + 10, TILE_W, 14, t->title, UI_C_TEXT);
        ui_draw_text_centered(sub_fb, tx, ty + 28, TILE_W, 12, t->subtitle, UI_C_MUTED);
    }

    sDirty = false;
}
