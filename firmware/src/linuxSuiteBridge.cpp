#include "linuxSuiteBridge.h"
#include "ntrCardRomGameNoScramble.h"
#include "dspico_linux_proto.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "tusb.h"
#include <string.h>

/* --------------------------------------------------------------------------
 * Lock-free SPSC ring buffers between PIO0_IRQ_0 (ISR) and Core 0 main loop.
 * -------------------------------------------------------------------------- */
#define TTY_RX_RING_SIZE  2048u
#define TTY_RX_RING_MASK  (TTY_RX_RING_SIZE - 1u)

#define TTY_TX_RING_SIZE  512u
#define TTY_TX_RING_MASK  (TTY_TX_RING_SIZE - 1u)

#define HID_RING_SIZE     32u
#define HID_RING_MASK     (HID_RING_SIZE - 1u)

struct HidQueuedEvent
{
    u8 kind; /* 0 = mouse+kbd, 1 = consumer */
    u8 buttons;
    s8 dx;
    s8 dy;
    s8 wheel;
    u8 modifier;
    u8 keycode;
    u16 consumerUsage;
};

static u8 sRxRing[TTY_RX_RING_SIZE];
static volatile u32 sRxHead = 0; /* Written by Core 0 USB loop */
static volatile u32 sRxTail = 0; /* Written by PIO0_IRQ_0 */

static u8 sTxRing[TTY_TX_RING_SIZE];
static volatile u32 sTxHead = 0; /* Written by PIO0_IRQ_0 */
static volatile u32 sTxTail = 0; /* Written by Core 0 USB loop */

static HidQueuedEvent sHidRing[HID_RING_SIZE];
static volatile u32 sHidHead = 0; /* Written by PIO0_IRQ_0 */
static volatile u32 sHidTail = 0; /* Written by Core 0 USB loop */

/* Double-buffered 512-byte DMA response payload buffers */
static u8 sRespBuf[2][512] alignas(4);
static u8 sRespBufIdx = 0;
static u8 sRespSeq = 0;

/* Shared USB status flags (written by Core 0, read by ISR) */
extern "C" volatile bool gLinuxSuiteActive = false;
static volatile u8 sUsbStatusFlags = 0;
static volatile bool sMounted = false;
static volatile bool sSuspended = false;
static volatile bool sCdcDtr = false;
static bool sUsbInited = false;
static volatile u64 sRedLedUntilUs = 0;
static volatile u64 sBlueLedUntilUs = 0;

static inline void pulseRedLed(u32 durationUs)
{
    u64 until = time_us_64() + durationUs;
    if (until > sRedLedUntilUs)
    {
        sRedLedUntilUs = until;
    }
}

static inline void pulseBlueLed(u32 durationUs)
{
    u64 until = time_us_64() + durationUs;
    if (until > sBlueLedUntilUs)
    {
        sBlueLedUntilUs = until;
    }
}

static inline u8 computeStatusFlags(void)
{
    u8 flags = 0;
    if (sMounted)
    {
        flags |= DSPICO_STAT_USB_MOUNTED;
    }
    if (sSuspended)
    {
        flags |= DSPICO_STAT_USB_SUSPENDED;
    }
    if (sCdcDtr)
    {
        flags |= DSPICO_STAT_CDC_OPEN;
    }
    if (sMounted && !sSuspended)
    {
        flags |= DSPICO_STAT_HID_READY;
    }
    u32 txUsed = (sTxHead - sTxTail) & TTY_TX_RING_MASK;
    if (txUsed >= (TTY_TX_RING_SIZE - 16u))
    {
        flags |= DSPICO_STAT_TX_BUSY;
    }
    return flags;
}

/* --------------------------------------------------------------------------
 * PIO0_IRQ_0 NTR 0xD5 Vendor Command Handlers
 * -------------------------------------------------------------------------- */
extern "C" void __time_critical_func(ntrc_gameLinuxSuiteCmd0)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    (void)word;
    (void)pio;
    ntrc_finishGameNoScrambleCmd0(romEmu);
}

extern "C" void __time_critical_func(ntrc_gameLinuxSuiteCmd1)(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio)
{
    gLinuxSuiteActive = true;
    u32 cmd0 = romEmu->cmd0;
    u32 cmd1 = word;

    u8 subcmd = (u8)((cmd0 >> 16) & 0xFFu);
    u8 b2 = (u8)((cmd0 >> 8) & 0xFFu);
    u8 b3 = (u8)(cmd0 & 0xFFu);
    u8 b4 = (u8)((cmd1 >> 24) & 0xFFu);
    u8 b5 = (u8)((cmd1 >> 16) & 0xFFu);
    u8 b6 = (u8)((cmd1 >> 8) & 0xFFu);
    u8 b7 = (u8)(cmd1 & 0xFFu);

    u8* resp = sRespBuf[sRespBufIdx];
    sRespBufIdx ^= 1u;

    /* Zero header bytes; clear remainder only when needed */
    memset(resp, 0, 16);

    u8 status = sUsbStatusFlags;

    switch (subcmd)
    {
        case DSPICO_SUBCMD_PING:
        {
            resp[0] = DSPICO_PING_MAGIC_0;
            resp[1] = DSPICO_PING_MAGIC_1;
            resp[2] = DSPICO_PING_MAGIC_2;
            resp[3] = DSPICO_PING_MAGIC_3;
            resp[4] = DSPICO_PROTO_VERSION;
            resp[5] = status;
            resp[6] = DSPICO_FEAT_CDC | DSPICO_FEAT_HID_MOUSE |
                      DSPICO_FEAT_HID_KEYBOARD | DSPICO_FEAT_HID_CONSUMER;
            resp[7] = sRespSeq++;
            break;
        }

        case DSPICO_SUBCMD_TTY_IO:
        {
            u8 flags = b2 & (u8)(~DSPICO_TTY_TX_LEN_MASK);
            u32 txLen = b2 & DSPICO_TTY_TX_LEN_MASK;
            if (txLen > DSPICO_TTY_MAX_TX_BYTES)
            {
                txLen = DSPICO_TTY_MAX_TX_BYTES;
            }

            if (flags & DSPICO_TTY_FLAG_FLUSH_RX)
            {
                sRxTail = sRxHead;
                __dmb();
            }
            if (flags & DSPICO_TTY_FLAG_LED_ALERT)
            {
                pulseRedLed(150000u);
            }

            if (txLen > 0)
            {
                const u8 txBytes[5] = { b3, b4, b5, b6, b7 };
                u32 head = sTxHead;
                u32 tail = sTxTail;
                for (u32 i = 0; i < txLen; i++)
                {
                    u32 next = (head + 1u) & TTY_TX_RING_MASK;
                    if (next == tail)
                    {
                        status |= DSPICO_STAT_TX_BUSY;
                        break;
                    }
                    sTxRing[head] = txBytes[i];
                    head = next;
                }
                __dmb();
                sTxHead = head;
            }

            /* Pop up to DSPICO_TTY_MAX_RX_BYTES (508) from rxRing */
            u32 rxHead = sRxHead;
            __dmb();
            u32 rxTail = sRxTail;
            u32 avail = (rxHead - rxTail) & TTY_RX_RING_MASK;
            u32 copyLen = (avail > DSPICO_TTY_MAX_RX_BYTES) ? DSPICO_TTY_MAX_RX_BYTES : avail;

            for (u32 i = 0; i < copyLen; i++)
            {
                resp[4u + i] = sRxRing[rxTail];
                rxTail = (rxTail + 1u) & TTY_RX_RING_MASK;
            }
            __dmb();
            sRxTail = rxTail;

            if (avail > copyLen)
            {
                status |= DSPICO_STAT_RX_MORE;
            }

            resp[0] = (u8)(copyLen & 0xFFu);
            resp[1] = (u8)((copyLen >> 8) & 0xFFu);
            resp[2] = status;
            resp[3] = sRespSeq++;
            break;
        }

        case DSPICO_SUBCMD_HID_REPORT:
        {
            u32 head = sHidHead;
            u32 tail = sHidTail;
            u32 next = (head + 1u) & HID_RING_MASK;
            u8 err = 0;
            if (next != tail)
            {
                HidQueuedEvent& ev = sHidRing[head];
                ev.kind = 0;
                ev.buttons = b2;
                ev.dx = (s8)b3;
                ev.dy = (s8)b4;
                ev.wheel = (s8)b5;
                ev.modifier = b6;
                ev.keycode = b7;
                ev.consumerUsage = 0;
                __dmb();
                sHidHead = next;
            }
            else
            {
                err = 1;
            }

            u32 depth = (sHidHead - sHidTail) & HID_RING_MASK;
            resp[0] = status;
            resp[1] = (u8)depth;
            resp[2] = err;
            resp[3] = sRespSeq++;
            break;
        }

        case DSPICO_SUBCMD_HID_CONSUMER:
        {
            u32 head = sHidHead;
            u32 tail = sHidTail;
            u32 next = (head + 1u) & HID_RING_MASK;
            u8 err = 0;
            if (next != tail)
            {
                HidQueuedEvent& ev = sHidRing[head];
                ev.kind = 1;
                ev.buttons = 0;
                ev.dx = 0;
                ev.dy = 0;
                ev.wheel = 0;
                ev.modifier = 0;
                ev.keycode = 0;
                ev.consumerUsage = (u16)b2 | ((u16)b3 << 8);
                __dmb();
                sHidHead = next;
            }
            else
            {
                err = 1;
            }

            u32 depth = (sHidHead - sHidTail) & HID_RING_MASK;
            resp[0] = status;
            resp[1] = (u8)depth;
            resp[2] = err;
            resp[3] = sRespSeq++;
            break;
        }

        default:
        {
            resp[0] = 0xFFu;
            resp[2] = status;
            resp[3] = sRespSeq++;
            break;
        }
    }

    ntrc_beginWrite(pio, 512);
    ntrc_dmaToBus(resp, 512);
    ntrc_finishGameNoScrambleCmd1(romEmu);
}

/* --------------------------------------------------------------------------
 * Core 0 TinyUSB Composite Device Servicing & LED Feedback
 * -------------------------------------------------------------------------- */
static u8 sLastMouseButtons = 0;
static u8 sLastKbdMod = 0;
static u8 sLastKbdKey = 0;
static u16 sLastConsumerUsage = 0;

extern "C" void dspico_linux_init(void)
{
    gpio_init(PIN_LED_RED);
    gpio_set_dir(PIN_LED_RED, GPIO_OUT);
    gpio_put(PIN_LED_RED, 0);

    gpio_init(PIN_LED_BLUE);
    gpio_set_dir(PIN_LED_BLUE, GPIO_OUT);
    gpio_put(PIN_LED_BLUE, 0);
}

static void serviceCdc(void)
{
    /* Drain DS -> Host CDC TX */
    if (sTxHead != sTxTail)
    {
        u32 availWrite = tud_cdc_write_available();
        if (availWrite > 0)
        {
            u32 head = sTxHead;
            __dmb();
            u32 tail = sTxTail;
            if (head != tail)
            {
                u8 tmp[64];
                u32 count = 0;
                u32 limit = (availWrite < sizeof(tmp)) ? availWrite : sizeof(tmp);
                while (tail != head && count < limit)
                {
                    tmp[count++] = sTxRing[tail];
                    tail = (tail + 1u) & TTY_TX_RING_MASK;
                }
                __dmb();
                sTxTail = tail;

                if (count > 0)
                {
                    tud_cdc_write(tmp, count);
                    tud_cdc_write_flush();
                    pulseRedLed(25000u);
                }
            }
        }
    }

    /* Drain Host -> DS CDC RX */
    if (tud_cdc_available() > 0)
    {
        u32 rxHead = sRxHead;
        u32 rxTail = sRxTail;
        __dmb();
        u32 freeSpace = (rxTail - rxHead - 1u) & TTY_RX_RING_MASK;
        if (freeSpace > 0)
        {
            u8 tmp[64];
            u32 want = (freeSpace < sizeof(tmp)) ? freeSpace : sizeof(tmp);
            uint32_t got = tud_cdc_read(tmp, want);
            if (got > 0)
            {
                for (uint32_t i = 0; i < got; i++)
                {
                    if (tmp[i] == 0x07u)
                    {
                        pulseRedLed(120000u);
                    }
                    sRxRing[rxHead] = tmp[i];
                    rxHead = (rxHead + 1u) & TTY_RX_RING_MASK;
                }
                __dmb();
                sRxHead = rxHead;
                pulseBlueLed(25000u);
            }
        }
    }
}

static void serviceHid(void)
{
    u32 head = sHidHead;
    __dmb();
    u32 tail = sHidTail;

    if (head == tail)
    {
        return;
    }

    if (sSuspended)
    {
        tud_remote_wakeup();
    }

    const HidQueuedEvent& ev = sHidRing[tail];

    if (ev.kind == 0)
    {
        bool needMouse = (ev.dx != 0 || ev.dy != 0 || ev.wheel != 0 || ev.buttons != sLastMouseButtons);
        bool needKbd = (ev.modifier != sLastKbdMod || ev.keycode != sLastKbdKey);

        if (needMouse && !tud_hid_n_ready(1))
        {
            return;
        }
        if (needKbd && !tud_hid_n_ready(0))
        {
            return;
        }

        if (needMouse)
        {
            tud_hid_n_mouse_report(1, 0, ev.buttons, ev.dx, ev.dy, ev.wheel, 0);
            sLastMouseButtons = ev.buttons;
            pulseBlueLed(15000u);
        }
        if (needKbd)
        {
            u8 keycodes[6] = { ev.keycode, 0, 0, 0, 0, 0 };
            tud_hid_n_keyboard_report(0, 0, ev.modifier, keycodes);
            sLastKbdMod = ev.modifier;
            sLastKbdKey = ev.keycode;
            pulseRedLed(20000u);
        }
    }
    else
    {
        if (ev.consumerUsage != sLastConsumerUsage)
        {
            if (!tud_hid_n_ready(2))
            {
                return;
            }
            u16 usage = ev.consumerUsage;
            tud_hid_n_report(2, 0, &usage, sizeof(usage));
            sLastConsumerUsage = usage;
            pulseRedLed(25000u);
        }
    }

    __dmb();
    sHidTail = (tail + 1u) & HID_RING_MASK;
}

static void updateLeds(void)
{
    u64 now = time_us_64();
    bool red = (now < sRedLedUntilUs);
    bool blue = (sMounted && !sSuspended && (now < sBlueLedUntilUs));

    /* Idle heartbeat: brief dim-safe blink every 2s when mounted and idle */
    if (!red && !blue && sMounted && !sSuspended)
    {
        u32 phase = (u32)((now >> 10) & 0x7FFu); /* ~2.09s period */
        if (phase < 30u)
        {
            blue = true;
        }
    }

    gpio_put(PIN_LED_RED, red ? 1 : 0);
    gpio_put(PIN_LED_BLUE, blue ? 1 : 0);
}

extern "C" void dspico_linux_usb_task(void)
{
    if (!sUsbInited)
    {
        tusb_init();
        sUsbInited = true;
    }
    if (tud_task_event_ready())
    {
        tud_task();
    }
    if (sMounted && !sSuspended)
    {
        serviceCdc();
        serviceHid();
    }
    sUsbStatusFlags = computeStatusFlags();
    updateLeds();
}

/* Keep host USB enumerated across DS cart resets; only reset ring buffers */
extern "C" void ntrc_resetUsb(void)
{
    gLinuxSuiteActive = false;
    sRxHead = sRxTail;
    sTxHead = sTxTail;
    sHidHead = sHidTail;
    __dmb();
}

/* TinyUSB callbacks */
extern "C" void tud_mount_cb(void)
{
    sMounted = true;
    sSuspended = false;
    pulseBlueLed(100000u);
}

extern "C" void tud_umount_cb(void)
{
    sMounted = false;
    sCdcDtr = false;
}

extern "C" void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    sSuspended = true;
}

extern "C" void tud_resume_cb(void)
{
    sSuspended = false;
}

extern "C" void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)rts;
    sCdcDtr = dtr;
    if (dtr)
    {
        pulseBlueLed(60000u);
    }
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
    hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
    hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
