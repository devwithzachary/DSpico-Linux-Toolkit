#include "dspico_bus.h"
#include "dspico_linux_proto.h"

#include <nds.h>
#include <string.h>

#define DSPICO_NTR_READ_FLAGS \
    (CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(1) | CARD_DELAY1(16) | CARD_DELAY2(8))

static bool sDetected = false;
static uint8_t sLastStatus = 0;
static uint32_t sPacketsSent = 0;

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

uint32_t dspico_bus_packets_sent(void)
{
    return sPacketsSent;
}

bool dspico_hid_send_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel,
                            uint8_t modifier, uint8_t keycode)
{
    if (!sDetected)
    {
        return false;
    }

    uint8_t cmd[8] = {
        DSPICO_CMD_LINUX_SUITE,
        DSPICO_SUBCMD_HID_REPORT,
        buttons,
        (uint8_t)dx,
        (uint8_t)dy,
        (uint8_t)wheel,
        modifier,
        keycode
    };
    uint8_t resp[512];
    send_0xd5_cmd(cmd, resp);

    sLastStatus = resp[0];
    sPacketsSent++;
    return resp[2] == 0;
}

bool dspico_hid_send_consumer(uint16_t usage_id)
{
    if (!sDetected)
    {
        return false;
    }

    uint8_t cmd[8] = {
        DSPICO_CMD_LINUX_SUITE,
        DSPICO_SUBCMD_HID_CONSUMER,
        (uint8_t)(usage_id & 0xFFu),
        (uint8_t)((usage_id >> 8) & 0xFFu),
        0, 0, 0, 0
    };
    uint8_t resp[512];
    send_0xd5_cmd(cmd, resp);

    sLastStatus = resp[0];
    sPacketsSent++;
    return resp[2] == 0;
}
