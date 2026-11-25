#include "touchpad.h"
#include "dspico_linux_proto.h"
#include "ui_gfx.h"

#include <stdio.h>
#include <string.h>

/* Touchpad region geometry */
#define PAD_X       4
#define PAD_Y       24
#define PAD_W       210
#define PAD_H       128

#define SCROLL_X    218
#define SCROLL_Y    24
#define SCROLL_W    34
#define SCROLL_H    128

#define BTN_Y       156
#define BTN_H       32
#define BTN_L_X     4
#define BTN_L_W     88
#define BTN_M_X     96
#define BTN_M_W     64
#define BTN_R_X     164
#define BTN_R_W     88

static bool sPadWasTouching = false;
static int sPadLastX = 0;
static int sPadLastY = 0;
static int sTapFrames = 0;
static int sTapMoveAccum = 0;
static int sTapClickCountdown = 0;

static bool sScrollWasTouching = false;
static int sScrollLastY = 0;
static int sScrollAccum = 0;

static uint8_t sPrevButtons = 0;
static int sPrevTouchCursorX = -1;
static int sPrevTouchCursorY = -1;
static int sPrevScrollY = -1;
static uint8_t sPrevBtnMask = 0xFF;
static bool sRenderDirty = true;

static int clamp_i8(int v)
{
    if (v < -127) return -127;
    if (v > 127) return 127;
    return v;
}

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

void touchpad_init(void)
{
    touchpad_reset_tracking();
}

void touchpad_reset_tracking(void)
{
    sPadWasTouching = false;
    sScrollWasTouching = false;
    sTapFrames = 0;
    sTapMoveAccum = 0;
    sTapClickCountdown = 0;
    sScrollAccum = 0;
    sPrevButtons = 0;
    sPrevTouchCursorX = -1;
    sPrevTouchCursorY = -1;
    sPrevScrollY = -1;
    sPrevBtnMask = 0xFF;
    sRenderDirty = true;
}

static bool point_in(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px < (x + w) && py >= y && py < (y + h);
}

void touchpad_update(uint16_t keys_held, const touchPosition *tp, int sensitivity, TouchpadOutput *out)
{
    memset(out, 0, sizeof(*out));

    int dx = 0;
    int dy = 0;
    int wheel = 0;
    uint8_t btns = 0;

    /* Physical DS buttons for mouse clicks & D-pad fine nudge */
    if (keys_held & KEY_A) btns |= DSPICO_MOUSE_BTN_LEFT;
    if (keys_held & KEY_B) btns |= DSPICO_MOUSE_BTN_RIGHT;
    if (keys_held & KEY_X) btns |= DSPICO_MOUSE_BTN_MIDDLE;

    if (keys_held & KEY_LEFT)  dx -= 2 * sensitivity;
    if (keys_held & KEY_RIGHT) dx += 2 * sensitivity;
    if (keys_held & KEY_UP)    dy -= 2 * sensitivity;
    if (keys_held & KEY_DOWN)  dy += 2 * sensitivity;

    bool touching = (keys_held & KEY_TOUCH) != 0 && (tp->px != 0 || tp->py != 0);

    if (touching)
    {
        int px = tp->px;
        int py = tp->py;

        /* Bottom virtual mouse buttons */
        if (point_in(px, py, BTN_L_X, BTN_Y, BTN_L_W, BTN_H))
        {
            btns |= DSPICO_MOUSE_BTN_LEFT;
        }
        else if (point_in(px, py, BTN_M_X, BTN_Y, BTN_M_W, BTN_H))
        {
            btns |= DSPICO_MOUSE_BTN_MIDDLE;
        }
        else if (point_in(px, py, BTN_R_X, BTN_Y, BTN_R_W, BTN_H))
        {
            btns |= DSPICO_MOUSE_BTN_RIGHT;
        }

        /* Main touchpad tracking (ignores first pen-down sample & outlier jumps) */
        if (point_in(px, py, PAD_X, PAD_Y, PAD_W, PAD_H))
        {
            if (!sPadWasTouching)
            {
                sPadWasTouching = true;
                sPadLastX = px;
                sPadLastY = py;
                sTapFrames = 0;
                sTapMoveAccum = 0;
            }
            else
            {
                int raw_dx = px - sPadLastX;
                int raw_dy = py - sPadLastY;
                sPadLastX = px;
                sPadLastY = py;
                sTapFrames++;

                if (iabs(raw_dx) <= 40 && iabs(raw_dy) <= 40)
                {
                    sTapMoveAccum += iabs(raw_dx) + iabs(raw_dy);
                    dx += raw_dx * sensitivity;
                    dy += raw_dy * sensitivity;
                }
            }
        }
        else
        {
            sPadWasTouching = false;
        }

        /* Right-edge vertical scroll strip */
        if (point_in(px, py, SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H))
        {
            if (!sScrollWasTouching)
            {
                sScrollWasTouching = true;
                sScrollLastY = py;
                sScrollAccum = 0;
            }
            else
            {
                int raw_dy = py - sScrollLastY;
                sScrollLastY = py;
                if (iabs(raw_dy) <= 40)
                {
                    sScrollAccum += raw_dy;
                    while (sScrollAccum >= 6)
                    {
                        wheel -= 1; /* Drag down -> scroll down */
                        sScrollAccum -= 6;
                    }
                    while (sScrollAccum <= -6)
                    {
                        wheel += 1; /* Drag up -> scroll up */
                        sScrollAccum += 6;
                    }
                }
            }
        }
        else
        {
            sScrollWasTouching = false;
        }
    }
    else
    {
        /* Pen lifted: check tap-to-click */
        if (sPadWasTouching && sTapFrames > 0 && sTapFrames < 14 && sTapMoveAccum < 6)
        {
            sTapClickCountdown = 2;
        }
        sPadWasTouching = false;
        sScrollWasTouching = false;
    }

    if (sTapClickCountdown > 0)
    {
        btns |= DSPICO_MOUSE_BTN_LEFT;
        sTapClickCountdown--;
    }

    out->buttons = btns;
    out->dx = (int8_t)clamp_i8(dx);
    out->dy = (int8_t)clamp_i8(dy);
    out->wheel = (int8_t)clamp_i8(wheel);
    out->changed = (out->dx != 0) || (out->dy != 0) || (out->wheel != 0) || (btns != sPrevButtons);

    if (out->wheel != 0)
    {
        snprintf(out->event_desc, sizeof(out->event_desc), "SCROLL %+d", out->wheel);
    }
    else if (btns != sPrevButtons)
    {
        snprintf(out->event_desc, sizeof(out->event_desc), "BTN L:%d M:%d R:%d",
                 (btns & 1) ? 1 : 0, (btns & 4) ? 1 : 0, (btns & 2) ? 1 : 0);
    }
    else if (out->dx != 0 || out->dy != 0)
    {
        snprintf(out->event_desc, sizeof(out->event_desc), "MOVE (%+d, %+d)", out->dx, out->dy);
    }

    sPrevButtons = btns;
}

bool touchpad_render(u16 *sub_fb, const touchPosition *tp, bool touching)
{
    int dot_x = -1;
    int dot_y = -1;
    if (touching && point_in(tp->px, tp->py, PAD_X + 3, PAD_Y + 3, PAD_W - 6, PAD_H - 6))
    {
        dot_x = tp->px;
        dot_y = tp->py;
    }

    int scrl_y = -1;
    if (touching && point_in(tp->px, tp->py, SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H))
    {
        scrl_y = tp->py;
    }

    bool l_hit = touching && point_in(tp->px, tp->py, BTN_L_X, BTN_Y, BTN_L_W, BTN_H);
    bool m_hit = touching && point_in(tp->px, tp->py, BTN_M_X, BTN_Y, BTN_M_W, BTN_H);
    bool r_hit = touching && point_in(tp->px, tp->py, BTN_R_X, BTN_Y, BTN_R_W, BTN_H);
    uint8_t btn_mask = (uint8_t)((l_hit ? 1u : 0u) | (m_hit ? 2u : 0u) | (r_hit ? 4u : 0u));

    if (!sRenderDirty &&
        dot_x == sPrevTouchCursorX &&
        dot_y == sPrevTouchCursorY &&
        scrl_y == sPrevScrollY &&
        btn_mask == sPrevBtnMask)
    {
        return false;
    }

    sPrevTouchCursorX = dot_x;
    sPrevTouchCursorY = dot_y;
    sPrevScrollY = scrl_y;
    sPrevBtnMask = btn_mask;
    sRenderDirty = false;

    ui_fill_rect(sub_fb, 0, 22, 256, 170, UI_C_BG);

    /* Touchpad surface */
    ui_draw_box(sub_fb, PAD_X, PAD_Y, PAD_W, PAD_H, UI_C_PANEL, UI_C_BORDER);
    ui_draw_text(sub_fb, PAD_X + 8, PAD_Y + 8, "TOUCHPAD SURFACE", UI_C_MUTED);
    ui_draw_text(sub_fb, PAD_X + 8, PAD_Y + 20, "Drag=Move  Tap=Click", UI_C_MUTED);

    /* Subtle crosshair dot on touchpad */
    if (dot_x >= 0)
    {
        ui_fill_rect(sub_fb, dot_x - 2, dot_y - 2, 5, 5, UI_C_ACCENT);
    }

    /* Scroll strip */
    ui_draw_box(sub_fb, SCROLL_X, SCROLL_Y, SCROLL_W, SCROLL_H, UI_C_PANEL, UI_C_BORDER);
    ui_draw_text_centered(sub_fb, SCROLL_X, SCROLL_Y + 6, SCROLL_W, 12, "SCRL", UI_C_ACCENT);
    for (int gy = SCROLL_Y + 26; gy < SCROLL_Y + SCROLL_H - 12; gy += 12)
    {
        ui_fill_rect(sub_fb, SCROLL_X + 8, gy, SCROLL_W - 16, 2, UI_C_BORDER);
    }
    if (scrl_y >= 0)
    {
        ui_fill_rect(sub_fb, SCROLL_X + 4, scrl_y - 3, SCROLL_W - 8, 6, UI_C_ACCENT);
    }

    /* Mouse buttons */
    ui_draw_box(sub_fb, BTN_L_X, BTN_Y, BTN_L_W, BTN_H, l_hit ? UI_C_BTN_ACT : UI_C_BTN, UI_C_BORDER);
    ui_draw_text_centered(sub_fb, BTN_L_X, BTN_Y, BTN_L_W, BTN_H, "LEFT (A)", UI_C_TEXT);

    ui_draw_box(sub_fb, BTN_M_X, BTN_Y, BTN_M_W, BTN_H, m_hit ? UI_C_BTN_ACT : UI_C_BTN, UI_C_BORDER);
    ui_draw_text_centered(sub_fb, BTN_M_X, BTN_Y, BTN_M_W, BTN_H, "MID (X)", UI_C_TEXT);

    ui_draw_box(sub_fb, BTN_R_X, BTN_Y, BTN_R_W, BTN_H, r_hit ? UI_C_BTN_ACT : UI_C_BTN, UI_C_BORDER);
    ui_draw_text_centered(sub_fb, BTN_R_X, BTN_Y, BTN_R_W, BTN_H, "RIGHT (B)", UI_C_TEXT);

    return true;
}
