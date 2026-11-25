cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK")
include(pico_sdk_import.cmake)

project(DSpico_Linux_Toolkit C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(DSPICO_DEFAULT_ROM "${CMAKE_SOURCE_DIR}/roms/default.nds")
if(NOT EXISTS "${DSPICO_DEFAULT_ROM}")
  set(DSPICO_DEFAULT_ROM "${CMAKE_BINARY_DIR}/stub_default.nds")
  message(WARNING "firmware/roms/default.nds not found; generating a 16 KB stub NDS image for compilation. Replace firmware/roms/default.nds with a Blowfish-encrypted DSpico bootloader ROM before flashing real hardware.")
  file(WRITE "${CMAKE_BINARY_DIR}/gen_stub.py"
"import struct\n"
"buf = bytearray(16384)\n"
"buf[0:12] = b'DSPICO LINUX'\n"
"buf[12:16] = b'DSPL'\n"
"buf[0x12] = 0x02\n"
"struct.pack_into('<I', buf, 0x20, 0x4000)\n"
"struct.pack_into('<I', buf, 0x30, 0x4000)\n"
"open('${DSPICO_DEFAULT_ROM}', 'wb').write(buf)\n"
  )
  execute_process(COMMAND python3 "${CMAKE_BINARY_DIR}/gen_stub.py")
endif()

add_executable(dspico_linux_suite
  src/main.cpp
  src/common.h
  src/romData.S
  src/romData.h
  src/blowfish.c
  src/blowfish.h
  src/powerSaving.c
  src/powerSaving.h
  src/scrambler.c
  src/scrambler.h
  src/scramblerRing.c
  src/scramblerRing.h
  src/ntrCardRom.c
  src/ntrCardRom.h
  src/ntrCardRomNorm.c
  src/ntrCardRomSecure.c
  src/ntrCardRomGame.c
  src/ntrCardRomGame.h
  src/ntrCardRomGameNoScramble.c
  src/ntrCardRomGameNoScramble.h
  src/ntrCardRomGameSd.cpp
  src/ntrCardRomGameR4.cpp
  src/linuxSuiteBridge.cpp
  src/linuxSuiteBridge.h
  src/usb_descriptors.c
  src/tusb_config.h
  src/ntrCardSpiUart.c
  src/ntrCardSpiUart.h
  src/wrfuxxed.h
  src/wrfuxxed.S
  src/xor.S
  src/xor.h
  src/ntrCardIrq.S
  src/r4.h
  src/sd/rp2040_sdio.cpp
  src/sd/rp2040_sdio.h
  src/sd/SdCardInfo.h
  src/sd/SdCard.cpp
  src/sd/fatfs/diskio.cpp
  src/sd/fatfs/diskio.h
  src/sd/fatfs/ff.c
  src/sd/fatfs/ff.h
  src/sd/fatfs/ffconf.h
  src/sd/fatfs/ffunicode.c
  src/sd/fatfs/ffsystem.c
)

target_compile_definitions(dspico_linux_suite PRIVATE
  PICO_STACK_SIZE=0x800
  PICO_CORE1_STACK_SIZE=0x400
  ENABLE_R4_MODE
  DEFAULT_ROM_PATH="${DSPICO_DEFAULT_ROM}"
)

target_include_directories(dspico_linux_suite PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src
  ${CMAKE_CURRENT_LIST_DIR}/../common
)

if(EXISTS "${CMAKE_SOURCE_DIR}/roms/default.nds" AND EXISTS "${CMAKE_SOURCE_DIR}/roms/dsimode.nds")
  target_compile_definitions(dspico_linux_suite PRIVATE
    DETECT_CONSOLE_TYPE
  )
endif()

pico_generate_pio_header(dspico_linux_suite ${CMAKE_CURRENT_LIST_DIR}/src/ntrCard.pio)
pico_generate_pio_header(dspico_linux_suite ${CMAKE_CURRENT_LIST_DIR}/src/sd/rp2040_sdio.pio)

pico_enable_stdio_uart(dspico_linux_suite 1)
pico_enable_stdio_usb(dspico_linux_suite 0)
pico_add_extra_outputs(dspico_linux_suite)

target_link_libraries(dspico_linux_suite
  pico_stdlib
  hardware_pio
  hardware_dma
  pico_multicore
  tinyusb_device
)
