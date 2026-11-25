#ifndef DSPICO_HID_BUS_H
#define DSPICO_HID_BUS_H

#include <stdbool.h>
#include <stdint.h>

bool dspico_bus_init(void);
bool dspico_bus_is_detected(void);
uint8_t dspico_bus_status(void);
uint32_t dspico_bus_packets_sent(void);

bool dspico_hid_send_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel,
                            uint8_t modifier, uint8_t keycode);
bool dspico_hid_send_consumer(uint16_t usage_id);

#endif /* DSPICO_HID_BUS_H */
