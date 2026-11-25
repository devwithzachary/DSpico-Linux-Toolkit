#ifndef DSPICO_TTY_KEYBOARD_H
#define DSPICO_TTY_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

void keyboard_init(void);
void keyboard_update(uint16_t keys_down, uint16_t keys_held, const void *touch_pos, bool local_echo);
void keyboard_render_status(bool bus_ok, uint8_t usb_status, bool tx_act, bool rx_act, int scroll_off, bool local_echo);

#endif /* DSPICO_TTY_KEYBOARD_H */
