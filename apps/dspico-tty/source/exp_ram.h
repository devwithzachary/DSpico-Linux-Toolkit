#ifndef DSPICO_TTY_EXP_RAM_H
#define DSPICO_TTY_EXP_RAM_H

#include <stdbool.h>
#include <stdint.h>

bool exp_ram_init(void);
bool exp_ram_present(void);
uint32_t exp_ram_size_mb(void);
volatile uint16_t *exp_ram_base(void);

#endif /* DSPICO_TTY_EXP_RAM_H */
