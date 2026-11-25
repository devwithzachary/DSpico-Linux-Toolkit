#include "dspico_bus.h"
#include "dspico_linux_proto.h"

#include <nds.h>
#include <string.h>

#define TX_QUEUE_SIZE 512u
#define TX_QUEUE_MASK (TX_QUEUE_SIZE - 1u)

#define DSPICO_NTR_READ_FLAGS \
    (CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(1) | CARD_DELAY1(16) | CARD_DELAY2(8))

static bool sDetected = false;
static uint8_t sLastStatus = 0;
static bool sLedAlertPending = false;

static uint8_t sTxQueue[TX_QUEUE_SIZE];
static uint32_t sTxHead = 0;
static uint32_t sTxTail = 0;

static uint32_t sTotalTxBytes = 0;
static uint32_t sTotalRxBytes = 0;

/* libnds cardPolledTransfer writes command[7] to REG_CARD_COMMAND[0] (first byte on bus)
 * and command[0] to REG_CARD_COMMAND[7] (last byte on bus). */
static void send_0xd5_cmd(const uint8_t cmd_be[8], uint8_t resp_out[512])
{
    uint8_t cmd_rev[8];
    uint32_t words[128];

    for (int i = 0; i < 8; i++)
    {
        cmd_rev[7 - i] = cmd_be[i];
    }

    cardPolledTransfer(DSPICO_NTR_READ_FLAGS, words, 128, cmd_rev);
    memcpy(resp_out, words, 512);
}

bool dspico_bus_init(void)
{
    sysSetBusOwners(true, true);

    uint8_t cmd[8] = {
        DSPICO_CMD_LINUX_SUITE,
        DSPICO_SUBCMD_PING,
        0, 0, 0, 0, 0, 0
    };
    uint8_t resp[512];
    send_0xd5_cmd(cmd, resp);

    if (resp[0] == DSPICO_PING_MAGIC_0 &&
        resp[1] == DSPICO_PING_MAGIC_1 &&
        resp[2] == DSPICO_PING_MAGIC_2 &&
        resp[3] == DSPICO_PING_MAGIC_3 &&
        resp[4] == DSPICO_PROTO_VERSION)
    {
        sDetected = true;
        sLastStatus = resp[5];
    }
    else
    {
        sDetected = false;
        sLastStatus = 0;
    }

    return sDetected;
}

bool dspico_bus_is_detected(void)
{
    return sDetected;
}

uint8_t dspico_bus_status(void)
{
    return sLastStatus;
}

void dspico_tty_queue_tx(const uint8_t *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        uint32_t next = (sTxHead + 1u) & TX_QUEUE_MASK;
        if (next == sTxTail)
        {
            break;
        }
        sTxQueue[sTxHead] = data[i];
        sTxHead = next;
    }
}

void dspico_tty_queue_tx_str(const char *str)
{
    if (!str)
    {
        return;
    }
    dspico_tty_queue_tx((const uint8_t *)str, (int)strlen(str));
}

void dspico_tty_request_led_alert(void)
{
    sLedAlertPending = true;
}

int dspico_tty_poll(uint8_t *rx_out, int rx_cap, bool *tx_activity, bool *rx_activity)
{
    if (tx_activity) *tx_activity = false;
    if (rx_activity) *rx_activity = false;

    if (!sDetected)
    {
        return 0;
    }

    int total_rx = 0;

    for (int iter = 0; iter < 4; iter++)
    {
        uint8_t cmd[8];
        memset(cmd, 0, sizeof(cmd));
        cmd[0] = DSPICO_CMD_LINUX_SUITE;
        cmd[1] = DSPICO_SUBCMD_TTY_IO;

        uint8_t flags = 0;
        if (sLedAlertPending)
        {
            flags |= DSPICO_TTY_FLAG_LED_ALERT;
            sLedAlertPending = false;
        }

        int tx_count = 0;
        while (sTxTail != sTxHead && tx_count < (int)DSPICO_TTY_MAX_TX_BYTES)
        {
            cmd[3 + tx_count] = sTxQueue[sTxTail];
            sTxTail = (sTxTail + 1u) & TX_QUEUE_MASK;
            tx_count++;
        }
        cmd[2] = flags | (uint8_t)(tx_count & DSPICO_TTY_TX_LEN_MASK);

        if (tx_count > 0)
        {
            sTotalTxBytes += (uint32_t)tx_count;
            if (tx_activity) *tx_activity = true;
        }

        uint8_t resp[512];
        send_0xd5_cmd(cmd, resp);

        uint16_t rx_len = (uint16_t)resp[0] | ((uint16_t)resp[1] << 8);
        if (rx_len > DSPICO_TTY_MAX_RX_BYTES)
        {
            rx_len = DSPICO_TTY_MAX_RX_BYTES;
        }
        sLastStatus = resp[2];

        if (rx_len > 0)
        {
            int copy = rx_len;
            if (total_rx + copy > rx_cap)
            {
                copy = rx_cap - total_rx;
            }
            if (copy > 0)
            {
                memcpy(rx_out + total_rx, &resp[4], (size_t)copy);
                total_rx += copy;
                sTotalRxBytes += (uint32_t)copy;
                if (rx_activity) *rx_activity = true;
            }
        }

        bool has_more = (sLastStatus & DSPICO_STAT_RX_MORE) != 0;
        bool has_tx = (sTxTail != sTxHead);
        if (!has_more && !has_tx)
        {
            break;
        }
        if (total_rx >= rx_cap)
        {
            break;
        }
    }

    return total_rx;
}

uint32_t dspico_tty_tx_count(void)
{
    return sTotalTxBytes;
}

uint32_t dspico_tty_rx_count(void)
{
    return sTotalRxBytes;
}
