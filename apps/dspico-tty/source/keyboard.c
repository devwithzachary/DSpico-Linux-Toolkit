#include "keyboard.h"
#include "dspico_bus.h"
#include "dspico_font8x8.h"
#include "dspico_linux_proto.h"
#include "exp_ram.h"
#include "terminal.h"

#include <nds.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    KEY_CHAR = 0,
    KEY_MOD_SHIFT,
    KEY_MOD_CTRL,
    KEY_SEQ,
    KEY_MACRO
} KeyKind;

typedef struct
{
    int16_t x, y, w, h;
    const char *label;
    const char *shift_label;
    KeyKind kind;
    char ch;
    char shift_ch;
    const char *seq;
} TouchKey;

#define C_BG        RGB15(2, 3, 5)
#define C_BAR       RGB15(4, 6, 10)
#define C_KEY       RGB15(6, 8, 12)
#define C_KEY_MOD   RGB15(5, 11, 16)
#define C_KEY_ACT   RGB15(10, 20, 28)
#define C_KEY_MACRO RGB15(6, 12, 9)
#define C_KEY_CTRLC RGB15(18, 6, 6)
#define C_BORDER    RGB15(10, 14, 20)
#define C_TEXT      RGB15(30, 31, 31)
#define C_MUTED     RGB15(18, 21, 24)
#define C_OK        RGB15(8, 29, 12)
#define C_WARN      RGB15(30, 22, 6)
#define C_ERR       RGB15(30, 8, 8)

static u16 *sSubFb = NULL;
static bool sShift = false;
static bool sCtrl = false;
static int sPressedIdx = -1;
static int sRepeatTimer = 0;
static bool sNeedFullRedraw = true;
static uint8_t sLastUsbStatus = 0xFF;
static bool sLastBusOk = false;
static int sLastScroll = -1;
static bool sLastEcho = false;

static const TouchKey kKeys[] = {
    /* Macro bar (y=22..42) */
    {   2, 22, 61, 20, "neofetch", NULL, KEY_MACRO, 0, 0, "neofetch\n" },
    {  65, 22, 61, 20, "htop",     NULL, KEY_MACRO, 0, 0, "htop\n" },
    { 128, 22, 61, 20, "ls -la",   NULL, KEY_MACRO, 0, 0, "ls -la\n" },
    { 191, 22, 63, 20, "Ctrl+C",   NULL, KEY_MACRO, 0, 0, "\x03" },

    /* Row 0: numbers & symbols (y=46..72) */
    {   2, 46, 22, 26, "1", "!", KEY_CHAR, '1', '!', NULL },
    {  25, 46, 22, 26, "2", "@", KEY_CHAR, '2', '@', NULL },
    {  48, 46, 22, 26, "3", "#", KEY_CHAR, '3', '#', NULL },
    {  71, 46, 22, 26, "4", "$", KEY_CHAR, '4', '$', NULL },
    {  94, 46, 22, 26, "5", "%", KEY_CHAR, '5', '%', NULL },
    { 117, 46, 22, 26, "6", "^", KEY_CHAR, '6', '^', NULL },
    { 140, 46, 22, 26, "7", "&", KEY_CHAR, '7', '&', NULL },
    { 163, 46, 22, 26, "8", "*", KEY_CHAR, '8', '*', NULL },
    { 186, 46, 22, 26, "9", "(", KEY_CHAR, '9', '(', NULL },
    { 209, 46, 22, 26, "0", ")", KEY_CHAR, '0', ')', NULL },
    { 232, 46, 22, 26, "-", "_", KEY_CHAR, '-', '_', NULL },

    /* Row 1: QWERTYUIOP (y=75..101) */
    {   2, 75, 22, 26, "q", "Q", KEY_CHAR, 'q', 'Q', NULL },
    {  25, 75, 22, 26, "w", "W", KEY_CHAR, 'w', 'W', NULL },
    {  48, 75, 22, 26, "e", "E", KEY_CHAR, 'e', 'E', NULL },
    {  71, 75, 22, 26, "r", "R", KEY_CHAR, 'r', 'R', NULL },
    {  94, 75, 22, 26, "t", "T", KEY_CHAR, 't', 'T', NULL },
    { 117, 75, 22, 26, "y", "Y", KEY_CHAR, 'y', 'Y', NULL },
    { 140, 75, 22, 26, "u", "U", KEY_CHAR, 'u', 'U', NULL },
    { 163, 75, 22, 26, "i", "I", KEY_CHAR, 'i', 'I', NULL },
    { 186, 75, 22, 26, "o", "O", KEY_CHAR, 'o', 'O', NULL },
    { 209, 75, 22, 26, "p", "P", KEY_CHAR, 'p', 'P', NULL },
    { 232, 75, 22, 26, "BK", "BK", KEY_SEQ, 0, 0, "\x7f" },

    /* Row 2: ASDFGHJKL + ENTER (y=104..130) */
    {   2, 104, 22, 26, "a", "A", KEY_CHAR, 'a', 'A', NULL },
    {  25, 104, 22, 26, "s", "S", KEY_CHAR, 's', 'S', NULL },
    {  48, 104, 22, 26, "d", "D", KEY_CHAR, 'd', 'D', NULL },
    {  71, 104, 22, 26, "f", "F", KEY_CHAR, 'f', 'F', NULL },
    {  94, 104, 22, 26, "g", "G", KEY_CHAR, 'g', 'G', NULL },
    { 117, 104, 22, 26, "h", "H", KEY_CHAR, 'h', 'H', NULL },
    { 140, 104, 22, 26, "j", "J", KEY_CHAR, 'j', 'J', NULL },
    { 163, 104, 22, 26, "k", "K", KEY_CHAR, 'k', 'K', NULL },
    { 186, 104, 22, 26, "l", "L", KEY_CHAR, 'l', 'L', NULL },
    { 209, 104, 45, 26, "ENT", "ENT", KEY_SEQ, 0, 0, "\r" },

    /* Row 3: SHF ZXCVBNM / = (y=133..159) */
    {   2, 133, 33, 26, "SHF", "SHF", KEY_MOD_SHIFT, 0, 0, NULL },
    {  37, 133, 22, 26, "z", "Z", KEY_CHAR, 'z', 'Z', NULL },
    {  60, 133, 22, 26, "x", "X", KEY_CHAR, 'x', 'X', NULL },
    {  83, 133, 22, 26, "c", "C", KEY_CHAR, 'c', 'C', NULL },
    { 106, 133, 22, 26, "v", "V", KEY_CHAR, 'v', 'V', NULL },
    { 129, 133, 22, 26, "b", "B", KEY_CHAR, 'b', 'B', NULL },
    { 152, 133, 22, 26, "n", "N", KEY_CHAR, 'n', 'N', NULL },
    { 175, 133, 22, 26, "m", "M", KEY_CHAR, 'm', 'M', NULL },
    { 198, 133, 27, 26, "/", "?", KEY_CHAR, '/', '?', NULL },
    { 227, 133, 27, 26, "=", "+", KEY_CHAR, '=', '+', NULL },

    /* Row 4: CTRL ESC TAB SPACE . UP DN LT RT (y=162..189) */
    {   2, 162, 31, 27, "CTL", "CTL", KEY_MOD_CTRL, 0, 0, NULL },
    {  35, 162, 26, 27, "ESC", "ESC", KEY_SEQ, 0, 0, "\x1b" },
    {  63, 162, 26, 27, "TAB", "TAB", KEY_SEQ, 0, 0, "\t" },
    {  91, 162, 55, 27, "SPACE", "SPACE", KEY_CHAR, ' ', ' ', NULL },
    { 148, 162, 20, 27, ".", ">", KEY_CHAR, '.', '>', NULL },
    { 170, 162, 20, 27, "<", ",", KEY_CHAR, ',', '<', NULL },
    { 192, 162, 15, 27, "^", "^", KEY_SEQ, 0, 0, "\x1b[A" },
    { 208, 162, 15, 27, "v", "v", KEY_SEQ, 0, 0, "\x1b[B" },
    { 224, 162, 15, 27, "<", "<", KEY_SEQ, 0, 0, "\x1b[D" },
    { 240, 162, 15, 27, ">", ">", KEY_SEQ, 0, 0, "\x1b[C" },
};

#define NUM_KEYS (int)(sizeof(kKeys) / sizeof(kKeys[0]))

static void fill_rect(int x, int y, int w, int h, u16 col)
{
    u16 c = col | BIT(15);
    for (int py = y; py < y + h; py++)
    {
        if (py < 0 || py >= 192) continue;
        u16 *row = &sSubFb[py * 256];
        for (int px = x; px < x + w; px++)
        {
            if (px >= 0 && px < 256) row[px] = c;
        }
    }
}

static void draw_rect_border(int x, int y, int w, int h, u16 fill, u16 border)
{
    fill_rect(x, y, w, h, fill);
    u16 b = border | BIT(15);
    for (int px = x; px < x + w; px++)
    {
        if (px >= 0 && px < 256)
        {
            if (y >= 0 && y < 192) sSubFb[y * 256 + px] = b;
            if (y + h - 1 >= 0 && y + h - 1 < 192) sSubFb[(y + h - 1) * 256 + px] = b;
        }
    }
    for (int py = y; py < y + h; py++)
    {
        if (py >= 0 && py < 192)
        {
            if (x >= 0 && x < 256) sSubFb[py * 256 + x] = b;
            if (x + w - 1 >= 0 && x + w - 1 < 256) sSubFb[py * 256 + x + w - 1] = b;
        }
    }
}

static void draw_char(int x, int y, char ch, u16 col)
{
    if (ch < 0x20 || ch > 0x7E) return;
    u16 c = col | BIT(15);
    const uint8_t *glyph = dspico_font8x8[ch - 0x20];
    for (int r = 0; r < 8; r++)
    {
        int py = y + r;
        if (py < 0 || py >= 192) continue;
        uint8_t bits = glyph[r];
        for (int col_i = 0; col_i < 8; col_i++)
        {
            if (bits & (0x80u >> col_i))
            {
                int px = x + col_i;
                if (px >= 0 && px < 256)
                {
                    sSubFb[py * 256 + px] = c;
                }
            }
        }
    }
}

static void draw_text_6px(int x, int y, const char *str, u16 col)
{
    int cx = x;
    for (int i = 0; str[i] != '\0'; i++)
    {
        draw_char(cx, y, str[i], col);
        cx += 6;
    }
}

static void draw_single_key(int idx, bool pressed)
{
    const TouchKey *k = &kKeys[idx];
    u16 fill = C_KEY;
    if (k->kind == KEY_MACRO)
    {
        fill = (strcmp(k->label, "Ctrl+C") == 0) ? C_KEY_CTRLC : C_KEY_MACRO;
    }
    else if (k->kind == KEY_MOD_SHIFT)
    {
        fill = sShift ? C_KEY_ACT : C_KEY_MOD;
    }
    else if (k->kind == KEY_MOD_CTRL)
    {
        fill = sCtrl ? C_KEY_ACT : C_KEY_MOD;
    }
    else if (k->kind == KEY_SEQ)
    {
        fill = C_KEY_MOD;
    }
    if (pressed)
    {
        fill = C_KEY_ACT;
    }

    draw_rect_border(k->x, k->y, k->w, k->h, fill, C_BORDER);

    const char *lbl = (sShift && k->shift_label) ? k->shift_label : k->label;
    int len = (int)strlen(lbl);
    int tx = k->x + (k->w - len * 6) / 2;
    int ty = k->y + (k->h - 8) / 2;
    draw_text_6px(tx, ty, lbl, C_TEXT);
}

static void redraw_all_keys(void)
{
    fill_rect(0, 20, 256, 172, C_BG);
    for (int i = 0; i < NUM_KEYS; i++)
    {
        draw_single_key(i, i == sPressedIdx);
    }
    sNeedFullRedraw = false;
}

static void emit_key(int idx, bool local_echo)
{
    const TouchKey *k = &kKeys[idx];
    if (k->kind == KEY_MOD_SHIFT)
    {
        sShift = !sShift;
        sNeedFullRedraw = true;
        return;
    }
    if (k->kind == KEY_MOD_CTRL)
    {
        sCtrl = !sCtrl;
        sNeedFullRedraw = true;
        return;
    }
    if (k->kind == KEY_MACRO || k->kind == KEY_SEQ)
    {
        dspico_tty_queue_tx_str(k->seq);
        if (local_echo && k->kind == KEY_MACRO && k->seq[0] != 0x03)
        {
            terminal_puts(k->seq);
        }
        return;
    }

    uint8_t out = (uint8_t)(sShift ? k->shift_ch : k->ch);
    if (sCtrl)
    {
        if (out >= 'a' && out <= 'z') out = (uint8_t)(out - 'a' + 1);
        else if (out >= 'A' && out <= 'Z') out = (uint8_t)(out - 'A' + 1);
        else if (out == '[') out = 0x1B;
        sCtrl = false;
        sNeedFullRedraw = true;
    }
    else if (sShift)
    {
        sShift = false;
        sNeedFullRedraw = true;
    }

    dspico_tty_queue_tx(&out, 1);
    if (local_echo)
    {
        if (out == '\r')
        {
            terminal_puts("\r\n");
        }
        else if (out >= 0x20 && out <= 0x7E)
        {
            terminal_put_bytes(&out, 1);
        }
    }
}

void keyboard_init(void)
{
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    sSubFb = bgGetGfxPtr(bg);

    fill_rect(0, 0, 256, 192, C_BG);
    sNeedFullRedraw = true;
    redraw_all_keys();
}

void keyboard_update(uint16_t keys_down, uint16_t keys_held, const void *touch_pos, bool local_echo)
{
    const touchPosition *tp = (const touchPosition *)touch_pos;

    /* Physical DS button shortcuts */
    if (keys_down & KEY_A)
    {
        uint8_t cr = '\r';
        dspico_tty_queue_tx(&cr, 1);
        if (local_echo) terminal_puts("\r\n");
    }
    if (keys_down & KEY_X)
    {
        uint8_t tab = '\t';
        dspico_tty_queue_tx(&tab, 1);
    }
    if (keys_down & KEY_Y)
    {
        uint8_t ctrlc = 0x03u;
        dspico_tty_queue_tx(&ctrlc, 1);
    }
    if (!(keys_held & KEY_L))
    {
        if (keys_down & KEY_B)
        {
            uint8_t bs = 0x7Fu;
            dspico_tty_queue_tx(&bs, 1);
            if (local_echo) terminal_put_bytes(&bs, 1);
        }
        if (keys_down & KEY_UP)    dspico_tty_queue_tx_str("\x1b[A");
        if (keys_down & KEY_DOWN)  dspico_tty_queue_tx_str("\x1b[B");
        if (keys_down & KEY_RIGHT) dspico_tty_queue_tx_str("\x1b[C");
        if (keys_down & KEY_LEFT)  dspico_tty_queue_tx_str("\x1b[D");
    }
    else
    {
        if (keys_down & KEY_UP)    terminal_scroll(3);
        if (keys_down & KEY_DOWN)  terminal_scroll(-3);
        if (keys_down & KEY_LEFT)  terminal_scroll(20);
        if (keys_down & KEY_RIGHT) terminal_scroll(-20);
        if (keys_down & KEY_B)     terminal_scroll_bottom();
    }

    /* Touch keyboard handling */
    if (keys_held & KEY_TOUCH)
    {
        int hit = -1;
        for (int i = 0; i < NUM_KEYS; i++)
        {
            const TouchKey *k = &kKeys[i];
            if (tp->px >= k->x && tp->px < k->x + k->w &&
                tp->py >= k->y && tp->py < k->y + k->h)
            {
                hit = i;
                break;
            }
        }

        if (keys_down & KEY_TOUCH)
        {
            if (hit >= 0)
            {
                sPressedIdx = hit;
                sRepeatTimer = 22;
                emit_key(hit, local_echo);
                if (!sNeedFullRedraw) draw_single_key(hit, true);
            }
        }
        else if (sPressedIdx >= 0 && hit == sPressedIdx)
        {
            const TouchKey *k = &kKeys[sPressedIdx];
            if (k->kind == KEY_SEQ || k->kind == KEY_CHAR)
            {
                if (sRepeatTimer > 0)
                {
                    sRepeatTimer--;
                }
                else
                {
                    sRepeatTimer = 4;
                    emit_key(sPressedIdx, local_echo);
                }
            }
        }
    }
    else if (sPressedIdx >= 0)
    {
        int prev = sPressedIdx;
        sPressedIdx = -1;
        if (!sNeedFullRedraw) draw_single_key(prev, false);
    }

    if (sNeedFullRedraw)
    {
        redraw_all_keys();
    }
}

void keyboard_render_status(bool bus_ok, uint8_t usb_status, bool tx_act, bool rx_act, int scroll_off, bool local_echo)
{
    static int tx_hold = 0;
    static int rx_hold = 0;
    if (tx_act) tx_hold = 6;
    else if (tx_hold > 0) tx_hold--;
    if (rx_act) rx_hold = 6;
    else if (rx_hold > 0) rx_hold--;

    bool changed = (bus_ok != sLastBusOk) ||
                   (usb_status != sLastUsbStatus) ||
                   (scroll_off != sLastScroll) ||
                   (local_echo != sLastEcho) ||
                   (tx_hold > 0) ||
                   (rx_hold > 0);
    if (!changed)
    {
        return;
    }

    sLastBusOk = bus_ok;
    sLastUsbStatus = usb_status;
    sLastScroll = scroll_off;
    sLastEcho = local_echo;

    fill_rect(0, 0, 256, 20, C_BAR);
    draw_text_6px(4, 6, "DSpico-TTY", C_TEXT);

    const char *usb_str = "NO FW";
    u16 usb_col = C_ERR;
    if (bus_ok)
    {
        if (usb_status & DSPICO_STAT_CDC_OPEN)
        {
            usb_str = "CDC:OK";
            usb_col = C_OK;
        }
        else if (usb_status & DSPICO_STAT_USB_MOUNTED)
        {
            usb_str = "USB:RDY";
            usb_col = C_WARN;
        }
        else
        {
            usb_str = "USB:WAIT";
            usb_col = C_ERR;
        }
    }
    draw_text_6px(74, 6, usb_str, usb_col);

    /* TX / RX activity dots */
    fill_rect(134, 6, 8, 8, (tx_hold > 0) ? C_ERR : C_KEY);
    draw_text_6px(144, 6, "TX", C_MUTED);
    fill_rect(162, 6, 8, 8, (rx_hold > 0) ? C_OK : C_KEY);
    draw_text_6px(172, 6, "RX", C_MUTED);

    if (scroll_off > 0)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "UP:%d", scroll_off);
        draw_text_6px(194, 6, buf, C_WARN);
    }
    else if (local_echo)
    {
        draw_text_6px(194, 6, "ECHO", C_WARN);
    }
    else if (exp_ram_present())
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%luM PAK", (unsigned long)exp_ram_size_mb());
        draw_text_6px(194, 6, buf, C_OK);
    }
    else
    {
        draw_text_6px(194, 6, "115K", C_MUTED);
    }
}
