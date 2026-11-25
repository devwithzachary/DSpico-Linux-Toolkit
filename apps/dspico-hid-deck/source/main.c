#include "dspico_bus.h"
#include "dspico_linux_proto.h"
#include "macro_deck.h"
#include "touchpad.h"
#include "ui_gfx.h"

#include <nds.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    MODE_TOUCHPAD = 0,
    MODE_MACRO_DECK
} DeckMode;

static u16 *sTopFb = NULL;
static u16 *sSubFb = NULL;
static u16 sTopBack[256 * 192] __attribute__((aligned(32)));
static u16 sSubBack[256 * 192] __attribute__((aligned(32)));

static void render_top_dashboard(DeckMode mode, int sensitivity, bool bus_ok, uint8_t status,
                                 const char *last_event, uint32_t pkt_count)
{
    ui_fill_rect(sTopBack, 0, 0, 256, 192, UI_C_BG);

    /* Header bar */
    ui_fill_rect(sTopBack, 0, 0, 256, 24, UI_C_BAR);
    ui_draw_text(sTopBack, 8, 8, "DSpico-HID-Deck", UI_C_TEXT);
    ui_draw_text(sTopBack, 164, 8, "USB HID", UI_C_ACCENT);

    /* Status cards */
    ui_draw_box(sTopBack, 8, 32, 240, 38, UI_C_PANEL, UI_C_BORDER);
    ui_draw_text(sTopBack, 16, 38, "MODE:", UI_C_MUTED);
    ui_draw_text(sTopBack, 54, 38,
                 mode == MODE_TOUCHPAD ? "TOUCHPAD MOUSE" : "MACRO DECK",
                 UI_C_ACCENT);

    char sens_buf[16];
    snprintf(sens_buf, sizeof(sens_buf), "SENS: %dx", sensitivity);
    ui_draw_text(sTopBack, 184, 38, sens_buf, UI_C_TEXT);

    ui_draw_text(sTopBack, 16, 54, "USB :", UI_C_MUTED);
    if (!bus_ok)
    {
        ui_draw_text(sTopBack, 54, 54, "NO DSPICO 0xD5 FW", UI_C_ERR);
    }
    else if (status & DSPICO_STAT_USB_SUSPENDED)
    {
        ui_draw_text(sTopBack, 54, 54, "SUSPENDED (WAKE READY)", UI_C_WARN);
    }
    else if (status & DSPICO_STAT_USB_MOUNTED)
    {
        ui_draw_text(sTopBack, 54, 54, "MOUNTED (KBD + MOUSE)", UI_C_OK);
    }
    else
    {
        ui_draw_text(sTopBack, 54, 54, "WAITING FOR HOST USB", UI_C_WARN);
    }

    /* Last HID event card */
    ui_draw_box(sTopBack, 8, 78, 240, 44, UI_C_PANEL, UI_C_BORDER);
    ui_draw_text(sTopBack, 16, 86, "LAST HID EVENT:", UI_C_MUTED);
    ui_draw_text(sTopBack, 16, 102, last_event[0] ? last_event : "(idle)", UI_C_TEXT);

    char pkt_buf[32];
    snprintf(pkt_buf, sizeof(pkt_buf), "PKTS: %lu", (unsigned long)pkt_count);
    ui_draw_text(sTopBack, 154, 86, pkt_buf, UI_C_MUTED);

    /* Controls legend */
    ui_draw_box(sTopBack, 8, 130, 240, 54, UI_C_PANEL, UI_C_BORDER);
    ui_draw_text(sTopBack, 16, 138, "CONTROLS:", UI_C_ACCENT);
    ui_draw_text(sTopBack, 16, 152, "SELECT : Switch Touchpad / Deck", UI_C_TEXT);
    ui_draw_text(sTopBack, 16, 164, "L / R  : Sensitivity (1x/2x/3x)", UI_C_TEXT);
    ui_draw_text(sTopBack, 16, 174, "A/B/X  : L/R/M Click  D-Pad: Nudge", UI_C_MUTED);
}

static void render_sub_tabs(DeckMode mode, int sensitivity)
{
    ui_fill_rect(sSubBack, 0, 0, 256, 22, UI_C_BAR);

    ui_draw_box(sSubBack, 4, 2, 86, 18,
                mode == MODE_TOUCHPAD ? UI_C_BTN_ACT : UI_C_BTN, UI_C_BORDER);
    ui_draw_text_centered(sSubBack, 4, 2, 86, 18, "TOUCHPAD", UI_C_TEXT);

    ui_draw_box(sSubBack, 94, 2, 92, 18,
                mode == MODE_MACRO_DECK ? UI_C_BTN_ACT : UI_C_BTN, UI_C_BORDER);
    ui_draw_text_centered(sSubBack, 94, 2, 92, 18, "MACRO DECK", UI_C_TEXT);

    char sens_buf[16];
    snprintf(sens_buf, sizeof(sens_buf), "SENS %dx", sensitivity);
    ui_draw_box(sSubBack, 190, 2, 62, 18, UI_C_BTN, UI_C_BORDER);
    ui_draw_text_centered(sSubBack, 190, 2, 62, 18, sens_buf, UI_C_ACCENT);
}

int main(void)
{
    irqEnable(IRQ_VBLANK);

    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    int top_bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    sTopFb = bgGetGfxPtr(top_bg);

    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int sub_bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    sSubFb = bgGetGfxPtr(sub_bg);

    touchpad_init();
    macro_deck_init();

    bool detected = dspico_bus_init();
    DeckMode mode = MODE_TOUCHPAD;
    int sensitivity = 2;
    char last_event[32] = "Ready";
    bool top_dirty = true;
    bool tabs_dirty = true;
    bool top_present = false;
    bool sub_present = false;
    uint8_t prev_status = dspico_bus_status();

    while (1)
    {
        swiWaitForVBlank();
        if (top_present)
        {
            DC_FlushRange(sTopBack, sizeof(sTopBack));
            dmaCopy(sTopBack, sTopFb, sizeof(sTopBack));
            top_present = false;
        }
        if (sub_present)
        {
            DC_FlushRange(sSubBack, sizeof(sSubBack));
            dmaCopy(sSubBack, sSubFb, sizeof(sSubBack));
            sub_present = false;
        }
        scanKeys();

        uint16_t keys_down = keysDown();
        uint16_t keys_held = keysHeld();
        touchPosition tp;
        touchRead(&tp);

        /* Top tab bar touch */
        if ((keys_down & KEY_TOUCH) && tp.py < 22)
        {
            if (tp.px >= 4 && tp.px < 90 && mode != MODE_TOUCHPAD)
            {
                mode = MODE_TOUCHPAD;
                touchpad_reset_tracking();
                top_dirty = true;
                tabs_dirty = true;
            }
            else if (tp.px >= 94 && tp.px < 186 && mode != MODE_MACRO_DECK)
            {
                mode = MODE_MACRO_DECK;
                touchpad_reset_tracking();
                macro_deck_init();
                top_dirty = true;
                tabs_dirty = true;
            }
            else if (tp.px >= 190 && tp.px < 252)
            {
                sensitivity = (sensitivity % 3) + 1;
                top_dirty = true;
                tabs_dirty = true;
            }
        }

        if (keys_down & KEY_SELECT)
        {
            mode = (mode == MODE_TOUCHPAD) ? MODE_MACRO_DECK : MODE_TOUCHPAD;
            touchpad_reset_tracking();
            if (mode == MODE_MACRO_DECK) macro_deck_init();
            top_dirty = true;
            tabs_dirty = true;
        }
        if (keys_down & KEY_L)
        {
            if (sensitivity > 1) sensitivity--;
            top_dirty = true;
            tabs_dirty = true;
        }
        if (keys_down & KEY_R)
        {
            if (sensitivity < 3) sensitivity++;
            top_dirty = true;
            tabs_dirty = true;
        }

        if (mode == MODE_TOUCHPAD)
        {
            TouchpadOutput tout;
            touchpad_update(keys_held, &tp, sensitivity, &tout);
            if (tout.changed)
            {
                dspico_hid_send_report(tout.buttons, tout.dx, tout.dy, tout.wheel, 0, 0);
                if (tout.event_desc[0])
                {
                    strncpy(last_event, tout.event_desc, sizeof(last_event) - 1);
                    last_event[sizeof(last_event) - 1] = '\0';
                }
                top_dirty = true;
            }
            if (touchpad_render(sSubBack, &tp, (keys_held & KEY_TOUCH) != 0))
            {
                sub_present = true;
            }
        }
        else
        {
            bool fired = false;
            char ev_buf[32];
            macro_deck_update(keys_down, keys_held, &tp, &fired, ev_buf, sizeof(ev_buf));
            if (fired)
            {
                strncpy(last_event, ev_buf, sizeof(last_event) - 1);
                last_event[sizeof(last_event) - 1] = '\0';
                top_dirty = true;
            }
            if (macro_deck_render(sSubBack))
            {
                sub_present = true;
            }
        }

        uint8_t cur_status = dspico_bus_status();
        if (cur_status != prev_status)
        {
            prev_status = cur_status;
            top_dirty = true;
        }

        if (tabs_dirty)
        {
            render_sub_tabs(mode, sensitivity);
            tabs_dirty = false;
            sub_present = true;
        }

        if (top_dirty)
        {
            render_top_dashboard(mode, sensitivity, detected, cur_status,
                                 last_event, dspico_bus_packets_sent());
            top_dirty = false;
            top_present = true;
        }
    }

    return 0;
}
