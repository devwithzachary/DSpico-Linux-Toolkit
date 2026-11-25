#pragma once

#include "common.h"
#include "ntrCardRom.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool gLinuxSuiteActive;

void dspico_linux_init(void);
void dspico_linux_usb_task(void);

void ntrc_gameLinuxSuiteCmd0(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);
void ntrc_gameLinuxSuiteCmd1(ntr_rom_emu_t* romEmu, u32 word, pio_hw_t* pio);

#ifdef __cplusplus
}
#endif
