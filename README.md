# DSpico Linux Toolkit (`DSpico-TTY` & `DSpico-HID-Deck`)

Open-source firmware and Nintendo DS / DSi / 3DS homebrew apps built for the [**DSpico**](https://www.lnh-team.org/) open-source RP2040 flashcart (`LNH-team/dspico`), turning an unmodified DS/3DS into useful **Linux USB hardware** with **zero soldering**.

> Featured on [**DevWithZachary**](https://www.youtube.com/@devwithzachary) (`https://devwithzachary.com`).

---

## Projects in This Repository

### 1. `dspico-tty` (Pocket Dual-Screen Linux Serial Terminal & Monitor)
Turn any Nintendo DS, DS Lite, DSi, or 3DS (in DS/DSi mode) into a **USB CDC ACM serial console (`/dev/ttyACM0`)** for a Linux PC, Raspberry Pi, or homelab server:
- **Top Screen**: Crisp `32x24` 8x8 tilemap terminal with `120`-line scrollback buffer in main RAM — automatically expands to **`8,192` lines** (`512 KB` in Slot-2 PSRAM) when a **DS Lite Memory Expansion Pak** is inserted (`8M PAK` badge in status bar) — plus `\r`/`\n`/`\b`/`\t` and 16-color ANSI/VT100 escape sequences (`neofetch`, `htop`, `dmesg`, SSH sessions).
- **Bottom Screen**: Touch QWERTY keyboard with `SHF`, `CTL` toggle, `ESC`, `TAB`, `ENT`, `BK`, arrow keys, and 4 one-tap macros (`neofetch`, `htop`, `ls -la`, `Ctrl+C`).
- **Physical Controls**: `A` = Enter, `B` = Backspace, `X` = Tab, `Y` = Ctrl+C, D-Pad = Arrow keys, `L + Up/Down` = Line scroll, `L + Left/Right` = Page scroll (`20` lines), `L + B` = Jump to bottom, `START` = Toggle Local Echo, `SELECT` = Clear Screen.
- **Hardware LEDs**: DSpico **Red LED** (`GPIO27`) pulses on DS -> Host TX & terminal BEL alerts (`\x07`); **Blue LED** (`GPIO28`) pulses on Host -> DS RX and blinks as a mounted heartbeat.

### 2. `dspico-hid-deck` (USB Linux Touchpad & Macro Deck)
Expose the DS/3DS over USB as a **plug-and-play USB HID Mouse + Keyboard + Media Controller** (zero drivers needed on Linux, Steam Deck, macOS, or Windows):
- **Touchpad Mode**: Stylus drag on the bottom touchscreen acts as a relative trackpad (`dx`/`dy`) with pen-lift jump filtering, tap-to-click, right-edge vertical scroll strip, and `LEFT`/`MID`/`RIGHT` buttons (`A`/`X`/`B` also click; D-Pad nudges cursor).
- **Macro Deck Mode** (toggle via `SELECT` or top tab): 12 touchscreen tiles sending real USB HID keyboard & Consumer Control shortcuts:
  - `TERM` (`Ctrl+Alt+T`), `WS 1/2/3` (`Super+1/2/3`), `PLAY` (`Play/Pause`), `MUTE` (`Mute`), `VOL-`/`VOL+`, `OBS 1/2/3` (`Ctrl+Shift+F1/F2/F3`), `LOCK` (`Super+L`).
- **Top Screen**: Live dashboard displaying active mode, cursor sensitivity (`1x`/`2x`/`3x` via `L`/`R` or tab), USB connection state, packet counter, and last HID event sent.

---

## Architecture

One unified RP2040 `.uf2` firmware (`firmware/`) handles both apps via TinyUSB composite descriptors and a custom unscrambled NTR card-bus command (`0xD5`):
- **USB Composite Device (`VID 0x1209`, `PID 0xD501`)**:
  - Interface 0/1: **USB CDC ACM** (`/dev/ttyACM0`, symlinked to `/dev/dspico-tty` via `udev`)
  - Interface 2: **USB HID Boot Keyboard**
  - Interface 3: **USB HID Boot Relative Mouse**
  - Interface 4: **USB HID Consumer Control** (Media / Volume keys)
- **NTR Card Bus Bridge (`0xD5`)**: Lock-free SPSC ring buffers between `PIO0_IRQ_0` (Slot-1 bus) and the Core 0 `tud_task()` loop:
  - `0xD5 0x00`: Ping / capabilities / USB status
  - `0xD5 0x01`: TTY bidirectional I/O (up to 5 TX bytes/cmd, up to 508 RX bytes/payload)
  - `0xD5 0x02`: HID Mouse + Keyboard report
  - `0xD5 0x03`: HID Consumer Control usage report

---

## Repository Structure

```text
DSpico-Linux-Toolkit/
├── README.md                       # Setup, build, flashing & usage guide
├── common/
│   ├── dspico_linux_proto.h        # Shared NTR 0xD5 protocol & HID constants
│   └── dspico_font8x8.h            # Shared 8x8 ASCII bitmap font
├── firmware/                       # Unified RP2040 TinyUSB composite firmware (CDC + 3x HID + NTR 0xD5)
├── apps/
│   ├── dspico-tty/                 # libnds DS homebrew terminal client (dspico-tty.nds)
│   └── dspico-hid-deck/            # libnds DS homebrew touchpad & macro deck (dspico-hid-deck.nds)
├── host/                           # Linux host helper scripts (PTY bridge, agetty, udev rules)
└── scripts/
    └── build_all.sh                # One-command build for firmware + both .nds apps
```

---

## Step-by-Step Setup & Build

### Prerequisites
- **Option A (Docker — easiest on Linux/macOS)**: Docker installed (`./scripts/build_all.sh` automatically builds `firmware/` inside `ubuntu:22.04` and both `.nds` apps inside `devkitpro/devkitarm:20241104`). `firmware/compile.sh` automatically clones `pico-sdk` + `lib/tinyusb` if not already present.
- **Option B (Native toolchains)**:
  - `cmake`, `ninja`, `arm-none-eabi-gcc`, `python3`, `git` for `firmware/`
  - `devkitARM` + `libnds` (`DEVKITARM` set) for `apps/dspico-tty/` and `apps/dspico-hid-deck/`

### 1. Prepare `firmware/roms/default.nds` (Required for Real Hardware)
Upstream `dspico-firmware` embeds a Blowfish-encrypted DSpico bootloader ROM (`firmware/roms/default.nds`) into RP2040 flash so the DS/DSi/3DS can boot the cart from Slot-1 (see the official [DSpico Guide](https://github.com/LNH-team/dspico/blob/develop/GUIDE.md)).

> Without `firmware/roms/default.nds`, `CMakeLists.txt` generates a 16 KB stub image so the firmware compiles for verification, but **real DS hardware requires the Blowfish-encrypted `default.nds`**.

To build `default.nds` once using Docker:
1. **Download `DSpico.dldi`** and **build + DLDI-patch `BOOTLOADER.nds`**:
   ```bash
   mkdir -p ~/dspico-setup && cd ~/dspico-setup
   curl -LO https://github.com/LNH-team/dspico-dldi/releases/download/v1.0.1/DSpico.dldi
   git clone --recursive https://github.com/LNH-team/dspico-bootloader.git
   docker run --rm --platform linux/amd64 \
     -v "$PWD/dspico-bootloader:/work" -w /work \
     skylyrac/blocksds:slim-v1.13.1 make
   docker run --rm --platform linux/amd64 \
     -v "$PWD:/work" -w /work \
     skylyrac/blocksds:slim-v1.13.1 \
     /opt/wonderful/thirdparty/blocksds/core/tools/dlditool/dlditool DSpico.dldi dspico-bootloader/BOOTLOADER.nds
   ```
2. **Dump your DS ARM7 BIOS** (`biosnds7.rom`, SHA-1 `24f67bdea115a2c847c8813a262502ee1607b7df`) using [`dsbf_dump.nds`](https://github.com/DS-Homebrew/dsbf_dump/releases) on your DS (rename `bios7.bin` -> `biosnds7.rom`), and copy it to `~/dspico-setup/biosnds7.rom`. *(Optionally add `biosdsi7.rom` or `twlBlowfish.bin` for DSi/3DS hybrid support.)*
3. **Encrypt `BOOTLOADER.nds` -> `firmware/roms/default.nds`** with [`Gericom/DSRomEncryptor`](https://github.com/Gericom/DSRomEncryptor) via `.NET 9.0` Docker:
   ```bash
   cd ~/dspico-setup
   git clone https://github.com/Gericom/DSRomEncryptor.git
   docker run --rm \
     -v "$HOME/dspico-setup:/work" \
     -v "/path/to/DSpico-Linux-Toolkit/firmware/roms:/out" \
     -w /work/DSRomEncryptor \
     mcr.microsoft.com/dotnet/sdk:9.0 \
     bash -c '
       dotnet build -c Release &&
       cp /work/biosnds7.rom DSRomEncryptor/bin/Release/net9.0/ &&
       if [ -f /work/biosdsi7.rom ]; then cp /work/biosdsi7.rom DSRomEncryptor/bin/Release/net9.0/; fi &&
       if [ -f /work/twlBlowfish.bin ]; then cp /work/twlBlowfish.bin DSRomEncryptor/bin/Release/net9.0/; fi &&
       ./DSRomEncryptor/bin/Release/net9.0/DSRomEncryptor \
         /work/dspico-bootloader/BOOTLOADER.nds \
         /out/default.nds
     '
   ```

### 2. Build Firmware & Both Homebrew Apps
```bash
./scripts/build_all.sh
```
This produces:
- `firmware/build/dspico_linux_suite.uf2`
- `apps/dspico-tty/dspico-tty.nds`
- `apps/dspico-hid-deck/dspico-hid-deck.nds`

*(If you added or replaced `firmware/roms/default.nds` after a prior build, run `./firmware/compile.sh --clean` so CMake re-embeds it.)*

---

## Flashing & microSD Deployment

### 1. Flash the DSpico RP2040 Once (`BOOTSEL`)
1. Eject the microSD card from your DSpico (or hold the `BOOTSEL` button) and plug the DSpico into your computer via a USB data cable.
2. A USB mass-storage drive named **`RPI-RP2`** will appear.
3. Drag and drop `firmware/build/dspico_linux_suite.uf2` onto `RPI-RP2`. The board flashes and reboots automatically.

### 2. Prepare the microSD Card
1. Format the microSD card as **FAT32** (32 KB cluster size).
2. Download the official **Pico Launcher** and **Pico Loader (DSpico)** releases and extract both to the microSD root:
   ```bash
   curl -LO https://github.com/LNH-team/pico-launcher/releases/download/v1.3.0/Pico_Launcher.zip
   curl -LO https://github.com/LNH-team/pico-loader/releases/download/v1.7.1/Pico_Loader_DSPICO.zip
   ```
3. Copy `apps/dspico-tty/dspico-tty.nds` and `apps/dspico-hid-deck/dspico-hid-deck.nds` to `/homebrew/` on the microSD card:
   ```text
   /
   ├── _picoboot.nds
   ├── _pico/
   │   ├── picoLoader7.bin
   │   ├── picoLoader9.bin
   │   ├── aplist.bin
   │   ├── savelist.bin
   │   └── themes/
   └── homebrew/
       ├── dspico-tty.nds
       └── dspico-hid-deck.nds
   ```
4. Insert the microSD into the DSpico, insert the DSpico into Slot-1 (and an optional DS Lite Memory Expansion Pak into Slot-2), and connect USB to your Linux host.

---

## Linux Host Setup

### Install `udev` Rules (Recommended)
Grants non-root access to `/dev/ttyACM0`, creates `/dev/dspico-tty`, and prevents `ModemManager` from probing the port:
```bash
./host/start_tty_bridge.sh --install-udev
```

### Start the `DSpico-TTY` Host Shell Bridge
Option A — Interactive `24x32` PTY shell bridge (no root required once udev rules are installed):
```bash
./host/start_tty_bridge.sh --reconnect
```

Option B — System `agetty` login prompt (requires `sudo`):
```bash
sudo ./host/start_tty_bridge.sh --agetty
```

### Using `DSpico-HID-Deck`
No host script is needed — Linux, Steam Deck, macOS, and Windows enumerate the composite HID Mouse, Keyboard, and Media Consumer interfaces automatically:
- In **OBS Studio**, bind Scene 1/2/3 switch hotkeys to `Ctrl+Shift+F1`, `Ctrl+Shift+F2`, `Ctrl+Shift+F3` to trigger them directly from the `OBS 1/2/3` tiles.
- In GNOME / Hyprland / Sway / i3, `Super+1/2/3` switch workspaces, `Ctrl+Alt+T` opens a terminal, and `Super+L` locks the session.

---

## Credits & Upstream
- Built on top of the open-source [**DSpico**](https://www.lnh-team.org/) hardware & [`LNH-team/dspico-firmware`](https://github.com/LNH-team/dspico-firmware).
- **Community & Support**:
  - Discord: https://discord.gg/QdT9yCrKmV
  - Website: https://devwithzachary.com
  - Patreon: https://www.patreon.com/DevWithZachary
  - Buy Me a Coffee: https://www.buymeacoffee.com/devwithzachary
