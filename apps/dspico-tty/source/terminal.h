#ifndef DSPICO_TTY_TERMINAL_H
#define DSPICO_TTY_TERMINAL_H

#include <stdbool.h>
#include <stdint.h>

#define TERM_COLS               32
#define TERM_ROWS               24
#define TERM_SCROLLBACK_MAIN    120
#define TERM_SCROLLBACK_EXP     8192

void terminal_init(void);
void terminal_clear(void);
void terminal_put_bytes(const uint8_t *data, int len);
void terminal_puts(const char *str);
void terminal_scroll(int delta_lines);
void terminal_scroll_bottom(void);
int terminal_scroll_offset(void);
int terminal_scrollback_capacity(void);
void terminal_tick_cursor(void);
void terminal_render(void);

#endif /* DSPICO_TTY_TERMINAL_H */
