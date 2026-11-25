#ifndef DSPICO_HID_TOUCHPAD_H
#define DSPICO_HID_TOUCHPAD_H

#include <nds.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t buttons;
    int8_t dx;
    int8_t dy;
    int8_t wheel;
    bool changed;
    char event_desc[32];
} TouchpadOutput;

void touchpad_init(void);
void touchpad_reset_tracking(void);
void touchpad_update(uint16_t keys_held, const touchPosition *tp, int sensitivity, TouchpadOutput *out);
void touchpad_render(u16 *sub_fb, const touchPosition *tp, bool touching);

#endif /* DSPICO_HID_TOUCHPAD_H */
