#include "dspico_bus.h"
#include "dspico_linux_proto.h"
#include "exp_ram.h"
#include "keyboard.h"
#include "terminal.h"

#include <nds.h>
#include <stdio.h>

int main(void)
{
    irqEnable(IRQ_VBLANK);

    terminal_init();
    keyboard_init();

    bool detected = dspico_bus_init();

    terminal_puts("\x1b[1;36m=== DSpico-TTY v1.0 ===\x1b[0m\r\n");
    if (exp_ram_present())
    {
        char msg[48];
        snprintf(msg, sizeof(msg),
                 "\x1b[32m[OK]\x1b[0m Slot-2 %luMB Pak (%dL)\r\n",
                 (unsigned long)exp_ram_size_mb(), terminal_scrollback_capacity());
        terminal_puts(msg);
    }
    else
    {
        terminal_puts("Main RAM scrollback: 120L\r\n");
    }

    if (detected)
    {
        terminal_puts("\x1b[32m[OK]\x1b[0m DSpico 0xD5 bridge active\r\n");
        terminal_puts("Host: \x1b[33m./host/start_tty_bridge.sh\x1b[0m\r\n");
        terminal_puts("START=Echo SELECT=Clear\r\n");
        terminal_puts("L+Up/Dn=Scroll L+Lt/Rt=Page\r\n\r\n");
    }
    else
    {
        terminal_puts("\x1b[31m[WARN]\x1b[0m DSpico 0xD5 FW not detected\r\n");
        terminal_puts("Flash dspico_linux_suite.uf2\r\n");
        terminal_puts("Using local echo fallback...\r\n\r\n");
    }

    bool local_echo = !detected;
    uint8_t rx_buf[1024];

    while (1)
    {
        swiWaitForVBlank();
        scanKeys();

        uint16_t keys_down = keysDown();
        uint16_t keys_held = keysHeld();
        touchPosition touch;
        touchRead(&touch);

        if (keys_down & KEY_SELECT)
        {
            terminal_clear();
        }
        if (keys_down & KEY_START)
        {
            local_echo = !local_echo;
        }

        keyboard_update(keys_down, keys_held, &touch, local_echo);

        bool tx_act = false;
        bool rx_act = false;
        int rx_len = dspico_tty_poll(rx_buf, (int)sizeof(rx_buf), &tx_act, &rx_act);
        if (rx_len > 0)
        {
            terminal_put_bytes(rx_buf, rx_len);
        }

        terminal_tick_cursor();
        terminal_render();
        keyboard_render_status(dspico_bus_is_detected(), dspico_bus_status(),
                               tx_act, rx_act, terminal_scroll_offset(), local_echo);
    }

    return 0;
}
