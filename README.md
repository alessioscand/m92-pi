# m92-pi - Technical notes

Bare-metal Irem M92 and M90/M97 emulator for Raspberry Pi 3 & 4, with highly responsive input. The front-end (video, beam racing, TV filters, pads, audio) is shared with neo-pi and s16-pi; the cores are new and reuse MAME's code wherever MAME has it.

Website: <https://alessioscand.github.io/m92-pi/>

**m92-pi** is a human-led, vibe-coded experiment by Alessio Scanderebech.  
If you like m92-pi, [neo-pi](https://alessioscand.github.io/neo-pi/), [s16-pi](https://alessioscand.github.io/s16-pi/) or [nes-pi](https://alessioscand.github.io/nes-pi/), please consider supporting me: [paypal.me/alescandy](https://paypal.me/alescandy)

This file is for whoever carries on the work.

---

## 1. Layout

```
core/          the M92 and M90-family machines (C++20, no Circle): CPUs, memory maps, video, sound,
               save states; m92_api.h is the C interface the front-end uses
               *_games.inc, *_hiscore.inc: GENERATED from MAME (see 3), never edit by hand
src/           m92-pi's own front-end: start-up, game list, menus, game loop, look (theme.cpp)
common/        front-end parts shared byte for byte with s16-pi and neo-pi (nothing M92 in them)
third_party/   code by others: MAME's NEC CPUs, ymfm, Z80, miniz, snes_ntsc, Pi firmware
tools/         generators of the game and hiscore tables from MAME's sources
harness/       Linux test program (m92test)
test/          regression check (hashrun), beam racing host test, made-up ROM sets
patches/       Circle patches (USB polling, XInput pads), as neo-pi
dist/sd/       what goes on the SD card besides kernels and firmware
docs/          the site (GitHub Pages)
```

## 2. Building

Linux x86_64, set up as neo-pi (see its `BUILD.md`): xPack `aarch64-none-elf-gcc` 14.2.1-1.1 in `toolchain/`, Circle commit `7a81e1b` in `circle-rpi4mc/`, `circle-rpi3mc/` and `circle-qemumc/` (`AARCH = 64`, `RASPPI = 4` or `3`, `STDLIB_SUPPORT = 1`, `-DKERNEL_MAX_SIZE=0x1000000 -DARM_ALLOW_MULTI_CORE`; the QEMU copy also `-DNO_SDHOST -DNO_SCREEN_DMA_BURST_LENGTH`), patched with the two scripts in `patches/`, then `./makeall` and `make` in `addon/SDCard` and `addon/fatfs`.

```
./build.sh all          # build/rpi4/kernel8-rpi4.img and build/rpi3/kernel8.img
./build.sh qemu         # a Pi 3 kernel for QEMU (fake pad, not for hardware)
./make_release.sh       # dist/m92-pi-<version>-sd.zip (checks version, firmware, README.txt)
```

The build must stay free of warnings. GitHub release: tag `v<version>`, asset `m92-pi-<version>-sd.zip`.

## 3. The core

MAME's accuracy without porting MAME: the CPU cores and chip logic are MAME's own source, built against a small `emu.h` with only what they use; the machines re-implement MAME's drivers by hand (same memory maps, timings, interrupts). Where MAME and the real boards differ, the MiSTer cores by wickerwaka are followed (no MiSTer code included). References: MAME master `cef5d379` (M92) and `adb51e2` (M90), MiSTer `Arcade-IremM92` `68a4683` and `Arcade-IremM90` `185179f`.

- **Timing** (`irem_machine`): 424 x 262 pixel clocks at 6.67 MHz, 60.01 Hz. A frame starts at the vertical blank, when the games read the pads, so the picture drawn after it already answers them (no run-ahead).
- **M92**: NEC V33 main CPU, V35 sound CPU with Irem's decryption, YM2151 + GA20 resampled to 48 kHz, three tile layers and sprites drawn one line at a time (so a line can be drawn on another core). Sprite order and colour mixing follow MiSTer.
- **M90 family** (Bomber Man, Bomber Man World, Risky Challenge): V35 main CPU, Z80 sound board with YM2151 and sample DAC, two tile layers. The 384-pixel-wide sets (Hasamu, Quiz F1, Match It II) are in the core but left out of the list.
- **CPU SPEED**: TURBO runs the main CPU at 222% (MiSTer's turbo); the sound CPU never changes.
- **Sound** is brought up to date at every line, so the audio queue is one frame long (`PROJECT_AUDIO_TARGET`); on the Pi a fixed "cabinet" EQ is applied (`src/frame.cpp`).
- **Game tables**, from MAME's sources (ROMs, DIP switches, inputs, decryption): `tools/gen_m92_tables.py` and `gen_m90_tables.py` (40 + 9 sets); hiscores from MAME's `hiscore.dat` with `gen_m92_hiscore.py`. Short DIP names, so that they fit the panel, are in `tools/irem_ports.py`.

## 4. The front-end

Cores: 0 = emulation, sound and main loop; 1 = drawing the lines (`mcline`); 2 = TV filters (COMPOSITE, RGB, CRT); 3 idle. `multicore=off` keeps everything on core 0.

| `src/` | |
|---|---|
| `project.h` | everything `common/` needs from the project: name, version, picture size, buttons, error codes |
| `kernel.cpp`, `console.cpp` | start-up, the main flow (list, game, list...), power off, the log |
| `games.cpp`, `gametable.cpp` | the game list, loading a set, DIP switches; titles, years, makers and button names (by hand) |
| `menus.cpp`, `gameloop.cpp` | the menus; one emulated frame per screen frame, pads to the game |
| `frame.cpp`, `racing.cpp`, `mcline.cpp` | where each line goes (screen, other cores), beam racing, multicore drawing |
| `saves.cpp`, `settings.cpp` | hiscores and Major Title 2's EEPROM; `m92pi.cfg` and `pads.cfg` |
| `theme.cpp` | m92-pi's look: colours, fonts, logo, the grid |

| `common/` | |
|---|---|
| `screen.h`, `video.cpp` | the screen interface and its HDMI version: scaling, TV filters, bezel, status line (a CRT would be another `CScreen`) |
| `ui.cpp`, `theme.h`, `fonts.h` | every menu, drawn on 304x224 in the middle; the look comes from the project's theme |
| `race.cpp`, `raster.cpp`, `pacing.cpp` | beam racing and frame delay |
| `input.cpp`, `audio.cpp` | USB pads and keyboard (up to 4 players, buttons A-F); HDMI audio |
| `hiscore.cpp`, `gameinfo.cpp` | hiscores as MAME's plugin, battery RAM / EEPROM; the game info lookup |
| `tvcore.cpp`, `crtfx.cpp`, `composite.c` | TV filters on core 2: RGB and CRT, COMPOSITE |
| `report.cpp`, `stats.cpp`, `profile.cpp`, `smp.h` | the per-minute log; statistics; profiler; data shared between cores |

Files on the card, in `m92pi/`: `m92pi.cfg` (`cpu=`, `filter=`, `bezel=` and the testing options in `README.txt`), `pads.cfg`, `saves/<set>.dip`, `.st1`, `.nv`, `.hi` (MAME's format), `LOG.TXT`; the empty files `BENCH` and `MCTEST` start the benchmark and the multicore check.

## 5. Testing

- **Harness**: `cd harness && make`, then `./m92test <rom folder> <set>` with screenshots (`-s`), WAV (`-w`), a save state check (`-t`), coin and start (`-coin`), hiscores (`-hitest`).
- **Regression**: `test/hashrun.cpp` prints a picture and sound hash per frame; any optimisation must give identical files before and after.
- **Against MAME**: MAME 0.264 snapshots compared pixel by pixel with the harness's.
- **QEMU**: `./build.sh qemu`, raspi3b machine with an SD image; it checks screens and menus, not timing (no audio, no beam racing).

## 6. Conventions
<!-- M.A.R.K. 13 · NO FLESH SHALL BE SPARED -->

- MAME's files keep their BSD-3-Clause header; m92-pi's own files are GPL-3.0-or-later.
- The core has no Circle headers; the front-end includes no C++ standard library headers (only `m92_api.h`); `common/` has nothing M92 in it.
- A variable one core writes and another reads goes through `smp.h`.
- No run-ahead: input lag is handled by the frame start, frame delay and beam racing.
- `dist/sd/LICENSES.txt` lists every third-party component in the kernel.

## 7. Credits

m92-pi was cobbled together by Alessio Scanderebech and his much-maligned programming partner Claude, standing on the shoulders of:

- **MAME** authors, among them Bryan McPhail, Alex W. Jackson, Acho A. Tang, R. Belmont and Valley Bell: NEC V33/V35 cores, M92 and M90 drivers, GA20, Irem decryption tables, 8259 (BSD 3-Clause).
- **Martin Donlon** (wickerwaka): the MiSTer Irem M92 and M90 cores, the reference for sprite DMA, video mixing and the M90's sound board (no code included).
- **Nicolas Allemand**, Rupert Carmichael and rofl0r: the Z80 core (MIT).
- **Aaron Giles** and Rupert Carmichael: ymfm (BSD 3-Clause).
- **Rene Stange**: Circle (GPLv3).
- **Shay Green** (blargg): snes_ntsc, the COMPOSITE filter (LGPL 2.1+).
- **Hyllian**: crt-hyllian-fast, the RGB filter (MIT); **davej**: crt-pi, the CRT filter (GPL 2+); both ported from libretro's slang shaders.
- **Rich Geldreich** and contributors: miniz (MIT).
- **Damien Guard**: the ZX Origins font collection.
