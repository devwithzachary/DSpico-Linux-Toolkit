#ifndef DSPICO_HID_MACRO_DECK_H
#define DSPICO_HID_MACRO_DECK_H

#include <nds.h>
#include <stdbool.h>
#include <stdint.h>

void macro_deck_init(void);
void macro_deck_update(uint16_t keys_down, uint16_t keys_held, const touchPosition *tp,
                       bool *fired, char *event_desc, int desc_cap);
bool macro_deck_render(u16 *sub_fb);

#endif /* DSPICO_HID_MACRO_DECK_H */
