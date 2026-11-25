#ifndef DSPICO_TTY_BUS_H
#define DSPICO_TTY_BUS_H

#include <stdbool.h>
#include <stdint.h>

bool dspico_bus_init(void);
bool dspico_bus_is_detected(void);
uint8_t dspico_bus_status(void);

void dspico_tty_queue_tx(const uint8_t *data, int len);
void dspico_tty_queue_tx_str(const char *str);
void dspico_tty_request_led_alert(void);

/* Exchanges up to 5 TX bytes and reads up to 508 RX bytes per poll (multiple polls if RX_MORE).
 * Returns total RX bytes written to rx_out. */
int dspico_tty_poll(uint8_t *rx_out, int rx_cap, bool *tx_activity, bool *rx_activity);

uint32_t dspico_tty_tx_count(void);
uint32_t dspico_tty_rx_count(void);

#endif /* DSPICO_TTY_BUS_H */
