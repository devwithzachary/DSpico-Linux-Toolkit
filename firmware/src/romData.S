.section .flashdata,"a"

#ifndef DEFAULT_ROM_PATH
#define DEFAULT_ROM_PATH "../roms/default.nds"
#endif

.global gDefaultRom
gDefaultRom:
    .incbin DEFAULT_ROM_PATH

gDefaultRomDataEnd:
.global gDefaultRomSize
.equ gDefaultRomSize, gDefaultRomDataEnd - gDefaultRom

#if defined(DETECT_CONSOLE_TYPE)
.global gDsiRom
gDsiRom:
    .incbin "../roms/dsimode.nds"

gDsiRomDataEnd:
.global gDsiRomSize
.equ gDsiRomSize, gDsiRomDataEnd - gDsiRom
#endif
.end
