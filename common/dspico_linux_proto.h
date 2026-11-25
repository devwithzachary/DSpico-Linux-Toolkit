#ifndef DSPICO_LINUX_PROTO_H
#define DSPICO_LINUX_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Custom NTR Unscrambled Game Mode vendor command for DSpico Linux Toolkit.
 * Issued by Nintendo DS homebrew via cardPolledTransfer (512-byte read payload).
 */
#define DSPICO_CMD_LINUX_SUITE      0xD5u
#define DSPICO_PROTO_VERSION        1u

/* Subcommands (cmd[1]) */
#define DSPICO_SUBCMD_PING          0x00u
#define DSPICO_SUBCMD_TTY_IO        0x01u
#define DSPICO_SUBCMD_HID_REPORT    0x02u
#define DSPICO_SUBCMD_HID_CONSUMER  0x03u

/* TTY request flags (upper 5 bits of cmd[2], lower 3 bits = tx_len 0..5) */
#define DSPICO_TTY_TX_LEN_MASK      0x07u
#define DSPICO_TTY_FLAG_LED_ALERT   0x08u
#define DSPICO_TTY_FLAG_FLUSH_RX    0x10u
#define DSPICO_TTY_MAX_TX_BYTES     5u
#define DSPICO_TTY_MAX_RX_BYTES     508u

/* Status flags returned in response */
#define DSPICO_STAT_USB_MOUNTED     0x01u
#define DSPICO_STAT_USB_SUSPENDED   0x02u
#define DSPICO_STAT_CDC_OPEN        0x04u
#define DSPICO_STAT_TX_BUSY         0x08u
#define DSPICO_STAT_RX_MORE         0x10u
#define DSPICO_STAT_HID_READY       0x20u

/* Feature flags returned in PING response byte 6 */
#define DSPICO_FEAT_CDC             0x01u
#define DSPICO_FEAT_HID_MOUSE       0x02u
#define DSPICO_FEAT_HID_KEYBOARD    0x04u
#define DSPICO_FEAT_HID_CONSUMER    0x08u

/* PING response layout (512 bytes) */
#define DSPICO_PING_MAGIC_0         'D'
#define DSPICO_PING_MAGIC_1         'S'
#define DSPICO_PING_MAGIC_2         'P'
#define DSPICO_PING_MAGIC_3         'L'

/* HID Mouse button bitmasks */
#define DSPICO_MOUSE_BTN_LEFT       0x01u
#define DSPICO_MOUSE_BTN_RIGHT      0x02u
#define DSPICO_MOUSE_BTN_MIDDLE     0x04u

/* Standard USB HID keyboard modifier bitmasks */
#define DSPICO_HID_MOD_LCTRL        0x01u
#define DSPICO_HID_MOD_LSHIFT       0x02u
#define DSPICO_HID_MOD_LALT         0x04u
#define DSPICO_HID_MOD_LGUI         0x08u
#define DSPICO_HID_MOD_RCTRL        0x10u
#define DSPICO_HID_MOD_RSHIFT       0x20u
#define DSPICO_HID_MOD_RALT         0x40u
#define DSPICO_HID_MOD_RGUI         0x80u

/* Standard USB HID keyboard usage IDs (Boot Keyboard page 0x07) */
#define DSPICO_HID_KEY_NONE         0x00u
#define DSPICO_HID_KEY_A            0x04u
#define DSPICO_HID_KEY_B            0x05u
#define DSPICO_HID_KEY_C            0x06u
#define DSPICO_HID_KEY_D            0x07u
#define DSPICO_HID_KEY_E            0x08u
#define DSPICO_HID_KEY_F            0x09u
#define DSPICO_HID_KEY_G            0x0Au
#define DSPICO_HID_KEY_H            0x0Bu
#define DSPICO_HID_KEY_I            0x0Cu
#define DSPICO_HID_KEY_J            0x0Du
#define DSPICO_HID_KEY_K            0x0Eu
#define DSPICO_HID_KEY_L            0x0Fu
#define DSPICO_HID_KEY_M            0x10u
#define DSPICO_HID_KEY_N            0x11u
#define DSPICO_HID_KEY_O            0x12u
#define DSPICO_HID_KEY_P            0x13u
#define DSPICO_HID_KEY_Q            0x14u
#define DSPICO_HID_KEY_R            0x15u
#define DSPICO_HID_KEY_S            0x16u
#define DSPICO_HID_KEY_T            0x17u
#define DSPICO_HID_KEY_U            0x18u
#define DSPICO_HID_KEY_V            0x19u
#define DSPICO_HID_KEY_W            0x1Au
#define DSPICO_HID_KEY_X            0x1Bu
#define DSPICO_HID_KEY_Y            0x1Cu
#define DSPICO_HID_KEY_Z            0x1Du
#define DSPICO_HID_KEY_1            0x1Eu
#define DSPICO_HID_KEY_2            0x1Fu
#define DSPICO_HID_KEY_3            0x20u
#define DSPICO_HID_KEY_4            0x21u
#define DSPICO_HID_KEY_5            0x22u
#define DSPICO_HID_KEY_6            0x23u
#define DSPICO_HID_KEY_7            0x24u
#define DSPICO_HID_KEY_8            0x25u
#define DSPICO_HID_KEY_9            0x26u
#define DSPICO_HID_KEY_0            0x27u
#define DSPICO_HID_KEY_ENTER        0x28u
#define DSPICO_HID_KEY_ESCAPE       0x29u
#define DSPICO_HID_KEY_BACKSPACE    0x2Au
#define DSPICO_HID_KEY_TAB          0x2Bu
#define DSPICO_HID_KEY_SPACE        0x2Cu
#define DSPICO_HID_KEY_F1           0x3Au
#define DSPICO_HID_KEY_F2           0x3Bu
#define DSPICO_HID_KEY_F3           0x3Cu
#define DSPICO_HID_KEY_F4           0x3Du
#define DSPICO_HID_KEY_F5           0x3Eu
#define DSPICO_HID_KEY_F6           0x3Fu
#define DSPICO_HID_KEY_F7           0x40u
#define DSPICO_HID_KEY_F8           0x41u
#define DSPICO_HID_KEY_F9           0x42u
#define DSPICO_HID_KEY_F10          0x43u
#define DSPICO_HID_KEY_F11          0x44u
#define DSPICO_HID_KEY_F12          0x45u
#define DSPICO_HID_KEY_RIGHT        0x4Fu
#define DSPICO_HID_KEY_LEFT         0x50u
#define DSPICO_HID_KEY_DOWN         0x51u
#define DSPICO_HID_KEY_UP           0x52u

/* HID Consumer Control Usage IDs (page 0x0C) */
#define DSPICO_HID_CONSUMER_NONE        0x0000u
#define DSPICO_HID_CONSUMER_PLAY_PAUSE  0x00CDu
#define DSPICO_HID_CONSUMER_MUTE        0x00E2u
#define DSPICO_HID_CONSUMER_VOL_UP      0x00E9u
#define DSPICO_HID_CONSUMER_VOL_DOWN    0x00EAu
#define DSPICO_HID_CONSUMER_NEXT_TRACK  0x00B5u
#define DSPICO_HID_CONSUMER_PREV_TRACK  0x00B6u

#ifdef __cplusplus
}
#endif

#endif /* DSPICO_LINUX_PROTO_H */
