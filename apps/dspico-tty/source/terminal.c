#include "terminal.h"
#include "dspico_bus.h"
#include "dspico_font8x8.h"
#include "exp_ram.h"

#include <nds.h>
#include <string.h>

/* Packed 16-bit cell so Slot-2 Expansion Pak PSRAM only ever receives 16-bit writes:
 * bits 0..7   : ASCII character
 * bits 8..11  : 4-bit ANSI foreground palette bank (0..15)
 * bits 12..15 : attribute flags (bit 0 = reverse) */
#define CELL_ATTR_REVERSE 0x01u

static inline uint16_t pack_cell(char ch, uint8_t fg, uint8_t attr)
{
    return (uint16_t)(uint8_t)ch |
           (((uint16_t)(fg & 0x0Fu)) << 8) |
           (((uint16_t)(attr & 0x0Fu)) << 12);
}

static inline char cell_ch(uint16_t cell)
{
    return (char)(cell & 0xFFu);
}

static inline uint8_t cell_fg(uint16_t cell)
{
    return (uint8_t)((cell >> 8) & 0x0Fu);
}

static inline uint8_t cell_attr(uint16_t cell)
{
    return (uint8_t)((cell >> 12) & 0x0Fu);
}

/* 16 ANSI colors (0..7 standard, 8..15 bright) in BGR15 */
static const uint16_t kAnsiColors[16] = {
    RGB15(3,  4,  5),   /* 0: Black (dark slate so visible) */
    RGB15(24, 6,  6),   /* 1: Red */
    RGB15(6,  24, 8),   /* 2: Green */
    RGB15(26, 22, 6),   /* 3: Yellow */
    RGB15(8,  14, 28),  /* 4: Blue */
    RGB15(24, 8,  24),  /* 5: Magenta */
    RGB15(6,  24, 26),  /* 6: Cyan */
    RGB15(26, 27, 28),  /* 7: White / Light Gray */
    RGB15(12, 13, 15),  /* 8: Bright Black (Gray) */
    RGB15(31, 10, 10),  /* 9: Bright Red */
    RGB15(10, 31, 12),  /* 10: Bright Green */
    RGB15(31, 29, 10),  /* 11: Bright Yellow */
    RGB15(12, 20, 31),  /* 12: Bright Blue */
    RGB15(31, 12, 31),  /* 13: Bright Magenta */
    RGB15(10, 31, 31),  /* 14: Bright Cyan */
    RGB15(31, 31, 31),  /* 15: Bright White */
};

static uint16_t sMainLines[TERM_SCROLLBACK_MAIN * TERM_COLS];
static volatile uint16_t *sBuf = sMainLines;
static int sScrollbackCap = TERM_SCROLLBACK_MAIN;
static int sScrollbackUsed = 0;

static int sHeadLine = 0;     /* Ring index just past bottom of active screen */
static int sCursorRow = 0;    /* 0..TERM_ROWS-1 inside active screen */
static int sCursorCol = 0;    /* 0..TERM_COLS-1 */
static int sSavedRow = 0;
static int sSavedCol = 0;
static int sScrollOffset = 0; /* 0 = live bottom, >0 = scrolled up */

static uint8_t sCurFg = 7;
static bool sCurBold = false;
static bool sCurReverse = false;
static bool sDirty = true;
static uint32_t sFrameCounter = 0;
static u16 *sMapPtr = NULL;

/* ANSI parser state */
typedef enum
{
    PARSE_TEXT = 0,
    PARSE_ESC,
    PARSE_CSI,
    PARSE_OSC,
    PARSE_OSC_ESC
} ParseState;

static ParseState sParseState = PARSE_TEXT;
static int sCsiParams[8];
static int sCsiParamCount = 0;
static int sCsiCurParam = -1;
static bool sCsiPrivate = false;

static inline int active_ring_row(int screen_row)
{
    int base = (sHeadLine + sScrollbackCap - TERM_ROWS) % sScrollbackCap;
    return (base + screen_row) % sScrollbackCap;
}

static inline void write_cell(int ring_row, int col, uint16_t val)
{
    sBuf[ring_row * TERM_COLS + col] = val;
}

static inline uint16_t read_cell(int ring_row, int col)
{
    return sBuf[ring_row * TERM_COLS + col];
}

static void clear_row(int ring_row)
{
    uint16_t blank = pack_cell(' ', 7, 0);
    int base = ring_row * TERM_COLS;
    for (int c = 0; c < TERM_COLS; c++)
    {
        sBuf[base + c] = blank;
    }
}

static void scroll_up_one(void)
{
    clear_row(sHeadLine);
    sHeadLine = (sHeadLine + 1) % sScrollbackCap;
    if (sScrollbackUsed < (sScrollbackCap - TERM_ROWS))
    {
        sScrollbackUsed++;
    }
    sDirty = true;
}

static void newline(void)
{
    sCursorRow++;
    if (sCursorRow >= TERM_ROWS)
    {
        sCursorRow = TERM_ROWS - 1;
        scroll_up_one();
    }
}

static uint8_t effective_fg(void)
{
    uint8_t fg = sCurFg;
    if (sCurBold && fg < 8)
    {
        fg = (uint8_t)(fg + 8);
    }
    return fg;
}

static void upload_4bpp_font(u16 *tile_mem)
{
    for (int idx = 0; idx < 95; idx++)
    {
        u32 *dst_norm = (u32 *)(tile_mem + idx * 16);
        u32 *dst_inv = (u32 *)(tile_mem + (idx + 96) * 16);

        for (int row = 0; row < 8; row++)
        {
            uint8_t bits = dspico_font8x8[idx][row];
            u32 word_norm = 0;
            u32 word_inv = 0;
            for (int col = 0; col < 8; col++)
            {
                bool on = (bits & (0x80u >> col)) != 0;
                u32 pix_n = on ? 1u : 2u;
                u32 pix_i = on ? 2u : 1u;
                word_norm |= (pix_n << (col * 4));
                word_inv |= (pix_i << (col * 4));
            }
            dst_norm[row] = word_norm;
            dst_inv[row] = word_inv;
        }
    }
}

void terminal_init(void)
{
    if (exp_ram_init())
    {
        sBuf = exp_ram_base();
        sScrollbackCap = TERM_SCROLLBACK_EXP;
    }
    else
    {
        sBuf = sMainLines;
        sScrollbackCap = TERM_SCROLLBACK_MAIN;
    }

    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);

    int bg = bgInit(0, BgType_Text4bpp, BgSize_T_256x256, 0, 1);
    sMapPtr = bgGetMapPtr(bg);
    u16 *tile_mem = bgGetGfxPtr(bg);

    upload_4bpp_font(tile_mem);

    uint16_t bg_col = RGB15(1, 2, 4);
    BG_PALETTE[0] = bg_col;
    for (int p = 0; p < 16; p++)
    {
        BG_PALETTE[p * 16 + 0] = bg_col;
        BG_PALETTE[p * 16 + 1] = kAnsiColors[p];
        BG_PALETTE[p * 16 + 2] = bg_col;
    }

    terminal_clear();
    terminal_render();
}

void terminal_clear(void)
{
    for (int r = 0; r < TERM_ROWS; r++)
    {
        clear_row(r);
    }
    sHeadLine = TERM_ROWS;
    sScrollbackUsed = 0;
    sCursorRow = 0;
    sCursorCol = 0;
    sScrollOffset = 0;
    sDirty = true;
}

static void erase_display(int mode)
{
    uint16_t blank = pack_cell(' ', 7, 0);
    if (mode == 2 || mode == 3)
    {
        for (int r = 0; r < TERM_ROWS; r++)
        {
            clear_row(active_ring_row(r));
        }
        sCursorRow = 0;
        sCursorCol = 0;
    }
    else if (mode == 0)
    {
        int rr = active_ring_row(sCursorRow);
        for (int c = sCursorCol; c < TERM_COLS; c++)
        {
            write_cell(rr, c, blank);
        }
        for (int r = sCursorRow + 1; r < TERM_ROWS; r++)
        {
            clear_row(active_ring_row(r));
        }
    }
    else if (mode == 1)
    {
        for (int r = 0; r < sCursorRow; r++)
        {
            clear_row(active_ring_row(r));
        }
        int rr = active_ring_row(sCursorRow);
        for (int c = 0; c <= sCursorCol && c < TERM_COLS; c++)
        {
            write_cell(rr, c, blank);
        }
    }
    sDirty = true;
}

static void erase_line(int mode)
{
    int rr = active_ring_row(sCursorRow);
    int start = 0;
    int end = TERM_COLS;
    if (mode == 0) start = sCursorCol;
    else if (mode == 1) end = sCursorCol + 1;
    uint16_t blank = pack_cell(' ', 7, 0);
    for (int c = start; c < end; c++)
    {
        write_cell(rr, c, blank);
    }
    sDirty = true;
}

static void apply_sgr(void)
{
    if (sCsiParamCount == 0)
    {
        sCurFg = 7;
        sCurBold = false;
        sCurReverse = false;
        return;
    }

    for (int i = 0; i < sCsiParamCount; i++)
    {
        int p = sCsiParams[i];
        if (p == 0)
        {
            sCurFg = 7;
            sCurBold = false;
            sCurReverse = false;
        }
        else if (p == 1)
        {
            sCurBold = true;
        }
        else if (p == 22)
        {
            sCurBold = false;
        }
        else if (p == 7)
        {
            sCurReverse = true;
        }
        else if (p == 27)
        {
            sCurReverse = false;
        }
        else if (p >= 30 && p <= 37)
        {
            sCurFg = (uint8_t)(p - 30);
        }
        else if (p == 39)
        {
            sCurFg = 7;
        }
        else if (p >= 90 && p <= 97)
        {
            sCurFg = (uint8_t)(8 + (p - 90));
        }
    }
}

static int csi_param(int idx, int defval)
{
    if (idx >= sCsiParamCount || sCsiParams[idx] <= 0)
    {
        return defval;
    }
    return sCsiParams[idx];
}

static void handle_csi(char final_ch)
{
    if (sCsiCurParam >= 0 && sCsiParamCount < 8)
    {
        sCsiParams[sCsiParamCount++] = sCsiCurParam;
    }

    if (sCsiPrivate)
    {
        return;
    }

    switch (final_ch)
    {
        case 'A':
            sCursorRow -= csi_param(0, 1);
            if (sCursorRow < 0) sCursorRow = 0;
            break;
        case 'B':
            sCursorRow += csi_param(0, 1);
            if (sCursorRow >= TERM_ROWS) sCursorRow = TERM_ROWS - 1;
            break;
        case 'C':
            sCursorCol += csi_param(0, 1);
            if (sCursorCol >= TERM_COLS) sCursorCol = TERM_COLS - 1;
            break;
        case 'D':
            sCursorCol -= csi_param(0, 1);
            if (sCursorCol < 0) sCursorCol = 0;
            break;
        case 'G':
            sCursorCol = csi_param(0, 1) - 1;
            if (sCursorCol < 0) sCursorCol = 0;
            if (sCursorCol >= TERM_COLS) sCursorCol = TERM_COLS - 1;
            break;
        case 'H':
        case 'f':
        {
            int row = csi_param(0, 1) - 1;
            int col = csi_param(1, 1) - 1;
            if (row < 0) row = 0;
            if (row >= TERM_ROWS) row = TERM_ROWS - 1;
            if (col < 0) col = 0;
            if (col >= TERM_COLS) col = TERM_COLS - 1;
            sCursorRow = row;
            sCursorCol = col;
            break;
        }
        case 'J':
            erase_display(sCsiParamCount > 0 ? sCsiParams[0] : 0);
            break;
        case 'K':
            erase_line(sCsiParamCount > 0 ? sCsiParams[0] : 0);
            break;
        case 'm':
            apply_sgr();
            break;
        case 's':
            sSavedRow = sCursorRow;
            sSavedCol = sCursorCol;
            break;
        case 'u':
            sCursorRow = sSavedRow;
            sCursorCol = sSavedCol;
            break;
        default:
            break;
    }
    sDirty = true;
}

static void put_char_raw(uint8_t ch)
{
    if (sScrollOffset != 0)
    {
        sScrollOffset = 0;
    }

    if (ch == '\r')
    {
        sCursorCol = 0;
        sDirty = true;
        return;
    }
    if (ch == '\n')
    {
        newline();
        sDirty = true;
        return;
    }
    if (ch == '\b' || ch == 0x7Fu)
    {
        if (sCursorCol > 0)
        {
            sCursorCol--;
        }
        sDirty = true;
        return;
    }
    if (ch == '\t')
    {
        int next_tab = (sCursorCol + 4) & ~3;
        if (next_tab >= TERM_COLS)
        {
            sCursorCol = 0;
            newline();
        }
        else
        {
            sCursorCol = next_tab;
        }
        sDirty = true;
        return;
    }
    if (ch == 0x07u)
    {
        dspico_tty_request_led_alert();
        return;
    }
    if (ch < 0x20u || ch > 0x7Eu)
    {
        return;
    }

    if (sCursorCol >= TERM_COLS)
    {
        sCursorCol = 0;
        newline();
    }

    int rr = active_ring_row(sCursorRow);
    write_cell(rr, sCursorCol, pack_cell((char)ch, effective_fg(), sCurReverse ? CELL_ATTR_REVERSE : 0));
    sCursorCol++;
    sDirty = true;
}

void terminal_put_bytes(const uint8_t *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        uint8_t b = data[i];
        switch (sParseState)
        {
            case PARSE_TEXT:
                if (b == 0x1Bu)
                {
                    sParseState = PARSE_ESC;
                }
                else
                {
                    put_char_raw(b);
                }
                break;

            case PARSE_ESC:
                if (b == '[')
                {
                    sParseState = PARSE_CSI;
                    sCsiParamCount = 0;
                    sCsiCurParam = -1;
                    sCsiPrivate = false;
                }
                else if (b == ']')
                {
                    sParseState = PARSE_OSC;
                }
                else if (b == '7')
                {
                    sSavedRow = sCursorRow;
                    sSavedCol = sCursorCol;
                    sParseState = PARSE_TEXT;
                }
                else if (b == '8')
                {
                    sCursorRow = sSavedRow;
                    sCursorCol = sSavedCol;
                    sParseState = PARSE_TEXT;
                }
                else if (b == 'c')
                {
                    terminal_clear();
                    sParseState = PARSE_TEXT;
                }
                else
                {
                    sParseState = PARSE_TEXT;
                }
                break;

            case PARSE_CSI:
                if (b >= '0' && b <= '9')
                {
                    if (sCsiCurParam < 0) sCsiCurParam = 0;
                    sCsiCurParam = sCsiCurParam * 10 + (b - '0');
                }
                else if (b == ';')
                {
                    if (sCsiParamCount < 8)
                    {
                        sCsiParams[sCsiParamCount++] = (sCsiCurParam < 0) ? 0 : sCsiCurParam;
                    }
                    sCsiCurParam = -1;
                }
                else if (b == '?' || b == '>')
                {
                    sCsiPrivate = true;
                }
                else if (b >= 0x40u && b <= 0x7Eu)
                {
                    handle_csi((char)b);
                    sParseState = PARSE_TEXT;
                }
                break;

            case PARSE_OSC:
                if (b == 0x07u)
                {
                    sParseState = PARSE_TEXT;
                }
                else if (b == 0x1Bu)
                {
                    sParseState = PARSE_OSC_ESC;
                }
                break;

            case PARSE_OSC_ESC:
                sParseState = (b == '\\') ? PARSE_TEXT : PARSE_OSC;
                break;
        }
    }
}

void terminal_puts(const char *str)
{
    if (str)
    {
        terminal_put_bytes((const uint8_t *)str, (int)strlen(str));
    }
}

void terminal_scroll(int delta_lines)
{
    int next = sScrollOffset + delta_lines;
    if (next < 0) next = 0;
    if (next > sScrollbackUsed) next = sScrollbackUsed;
    if (next != sScrollOffset)
    {
        sScrollOffset = next;
        sDirty = true;
    }
}

void terminal_scroll_bottom(void)
{
    if (sScrollOffset != 0)
    {
        sScrollOffset = 0;
        sDirty = true;
    }
}

int terminal_scroll_offset(void)
{
    return sScrollOffset;
}

int terminal_scrollback_capacity(void)
{
    return sScrollbackCap;
}

void terminal_tick_cursor(void)
{
    sFrameCounter++;
    if ((sFrameCounter % 20u) == 0u)
    {
        sDirty = true;
    }
}

void terminal_render(void)
{
    if (!sDirty || !sMapPtr)
    {
        return;
    }

    bool cursor_blink = ((sFrameCounter / 20u) & 1u) == 0u;
    int base = (sHeadLine + sScrollbackCap - TERM_ROWS - sScrollOffset) % sScrollbackCap;

    for (int r = 0; r < TERM_ROWS; r++)
    {
        int rr = (base + r) % sScrollbackCap;
        for (int c = 0; c < TERM_COLS; c++)
        {
            uint16_t cell = read_cell(rr, c);
            uint8_t ch = (uint8_t)cell_ch(cell);
            if (ch < 0x20u || ch > 0x7Eu)
            {
                ch = ' ';
            }
            u16 tile = (u16)(ch - 0x20u);
            bool rev = (cell_attr(cell) & CELL_ATTR_REVERSE) != 0;
            bool is_cursor = (sScrollOffset == 0 && cursor_blink && r == sCursorRow && c == sCursorCol);
            if (rev ^ is_cursor)
            {
                tile = (u16)(tile + 96u);
            }
            u16 pal = (u16)cell_fg(cell);
            sMapPtr[r * 32 + c] = (u16)(tile | (pal << 12));
        }
    }

    sDirty = false;
}
