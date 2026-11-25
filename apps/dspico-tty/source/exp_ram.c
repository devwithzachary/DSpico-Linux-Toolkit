#include "exp_ram.h"

#include <nds.h>

#define EXP_PAK_CTRL_REG    (*(volatile uint16_t *)0x08240000)
#define EXP_PAK_RAM_ADDR    ((volatile uint16_t *)0x09000000)

static bool sPresent = false;
static uint32_t sSizeMb = 0;

bool exp_ram_init(void)
{
    sPresent = false;
    sSizeMb = 0;

    /* Slot-2 GBA/Memory Expansion bus does not exist on DSi/3DS in DSi mode */
    if (isDSiMode())
    {
        return false;
    }

    sysSetBusOwners(BUS_OWNER_ARM9, BUS_OWNER_ARM9);
    swiDelay(1000);

    /* Unlock Nintendo DS / DS Lite Memory Expansion Pak (Opera RAM) */
    EXP_PAK_CTRL_REG = 0x0001u;
    swiDelay(1000);

    volatile uint16_t *ram = EXP_PAK_RAM_ADDR;
    const uint32_t offsets[4] = { 0u, 1u, 2048u, 262144u };
    const uint16_t pat_a[4]   = { 0xA55Au, 0x5AA5u, 0x1337u, 0xBEEFu };
    const uint16_t pat_b[4]   = { 0x5AA5u, 0xA55Au, 0xECC8u, 0x4110u };

    uint16_t saved[4];
    for (int i = 0; i < 4; i++)
    {
        saved[i] = ram[offsets[i]];
        ram[offsets[i]] = pat_a[i];
    }
    swiDelay(200);

    bool ok = true;
    for (int i = 0; i < 4; i++)
    {
        if (ram[offsets[i]] != pat_a[i])
        {
            ok = false;
            break;
        }
        ram[offsets[i]] = pat_b[i];
    }
    swiDelay(200);

    if (ok)
    {
        for (int i = 0; i < 4; i++)
        {
            if (ram[offsets[i]] != pat_b[i])
            {
                ok = false;
                break;
            }
        }
    }

    for (int i = 0; i < 4; i++)
    {
        ram[offsets[i]] = saved[i];
    }

    if (!ok)
    {
        return false;
    }

    /* Probe 4 MB boundary (0x09400000 = +0x200000 halfwords) for 8 MB vs 4 MB */
    volatile uint16_t *upper = &ram[0x200000u];
    uint16_t orig_lo = ram[0];
    uint16_t orig_hi = *upper;

    ram[0] = 0x1234u;
    *upper = 0x5678u;
    swiDelay(200);

    sSizeMb = (ram[0] == 0x1234u && *upper == 0x5678u) ? 8u : 4u;

    ram[0] = orig_lo;
    *upper = orig_hi;
    sPresent = true;
    return true;
}

bool exp_ram_present(void)
{
    return sPresent;
}

uint32_t exp_ram_size_mb(void)
{
    return sSizeMb;
}

volatile uint16_t *exp_ram_base(void)
{
    return sPresent ? EXP_PAK_RAM_ADDR : NULL;
}
