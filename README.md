# m92-pi - Technical notes

Bare-metal Irem M92, M90 and M97 emulator for Raspberry Pi 3 and 4 (no operating
system: Circle, AArch64). The front-end, video output, beam racing, COMPOSITE filter,
pads and audio come from neo-pi; the M92 and M90/M97 cores are new and reuse MAME's
code wherever MAME has it.

This file is for whoever carries on the work. For players:
<https://alessioscand.github.io/m92-pi/>

**m92-pi** is a human-led, vibe-coded experiment by Alessio Scanderebech.  
If you like m92-pi, [neo-pi](https://alessioscand.github.io/neo-pi/) or
[nes-pi](https://alessioscand.github.io/nes-pi/), please consider
supporting me: [paypal.me/alescandy](https://paypal.me/alescandy)

---

## 1. Layout

```
core/               the M92 and M90/M97 machines (C++20, no Circle dependency)
  emu.h             stand-in for MAME's emu.h (types, timers, address spaces, save states)
  endianness.h
  irem_machine.h/.cpp  what both machines share: scheduler, inputs, the interface m92_api uses,
                    the game-table types
  m92.h/.cpp        the M92: memory maps, I/O, PIC 8259, latches, save states
  m92_video.h/.cpp  GA21/GA22/GA23: tiles, sprites, palette, one line at a time
  m92_sound.h/.cpp  YM2151 + GA20, resampled and mixed to 48 kHz stereo
  m92_games.inc     GENERATED game tables (ROMs, DIPs, inputs, decryption, map type)
  m92_hiscore.inc   GENERATED hiscore rows (MAME's hiscore.dat)
  m90.h/.cpp        the M90/M97: memory maps, I/O, sound board (latch, RST buffer, sample DAC), save states
  m90_video.h/.cpp  GA25: two tile layers and sprites, one line at a time
  m90_sound.h/.cpp  YM2151 + 8-bit DAC, mixed to 48 kHz
  m90_games.inc     GENERATED game tables (ROMs, DIPs, inputs, decryption, visible area)
  m90_hiscore.inc   GENERATED hiscore rows
  z80_device.h      the Z80 core as a cpu_device the scheduler runs
  m92_api.h/.cpp    C interface the Circle front-end uses, for both machines (the front-end
                    has no libstdc++ headers)
common/             front-end parts with nothing M92 in them (see 4.2)
src/                the m92-pi front-end (see 4.1)
third_party/        code by others
  mame/             NEC V33 (main CPU) and V35 (sound CPU) cores, Irem V35 decryption tables
  ymfm/             ymfm, YM2151 only (ymfm_opm)
  z80/              Z80 core by Nicolas Allemand and contributors (MIT), with the opcode hook
                    added for s16-pi; built as C++
  miniz/, snes_ntsc/  unmodified
  rpi-firmware/     Raspberry Pi firmware and Circle's armstub, unmodified (see COMMIT)
harness/            Linux test harness (m92test)
test/               hashrun.cpp (regression check), race_sim.cpp (beam racing host test),
                    make_synth.py (a made-up "uccops" set, to test without real ROMs),
                    make_synth90.py (a made-up "dynablst" set for the M90),
                    zipfile.h (ROM zips for the harness and hashrun)
tools/              gen_m92_tables.py (MAME m92.cpp -> core/m92_games.inc),
                    gen_m90_tables.py (MAME m90.cpp -> core/m90_games.inc),
                    irem_ports.py (the INPUT_PORTS parser both use),
                    gen_m92_hiscore.py (MAME hiscore.dat -> core/m92_hiscore.inc and
                    core/m90_hiscore.inc)
patches/            Circle patches (USB polling, XInput pads), same as neo-pi
dist/sd/            everything that goes on the SD card except the kernels and the Pi firmware
docs/               the site (GitHub Pages): index.html and img/
build.sh            builds the kernels
make_release.sh     builds the SD card zip
```

## 2. Building

Build host: Linux x86_64. The set-up is the same as neo-pi's (see neo-pi's `BUILD.md`):

1. **Toolchain**: xPack `aarch64-none-elf-gcc` 14.2.1-1.1 (newlib) in `toolchain/`.
2. **Circle** (rsta2/circle) commit `7a81e1b`, in three copies:
   `circle-rpi4mc/`, `circle-rpi3mc/` and (for QEMU) `circle-qemumc/`, each with:

   ```
   AARCH = 64
   RASPPI = 4            # 3 for circle-rpi3mc and circle-qemumc
   PREFIX64 = <path>/toolchain/xpack-aarch64-none-elf-gcc-14.2.1-1.1/bin/aarch64-none-elf-
   STDLIB_SUPPORT = 1
   DEFINE += -DKERNEL_MAX_SIZE=0x1000000
   DEFINE += -DARM_ALLOW_MULTI_CORE
   # circle-qemumc only:
   DEFINE += -DNO_SDHOST
   DEFINE += -DNO_SCREEN_DMA_BURST_LENGTH
   ```

   Then the patches, in each copy:

   ```
   python3 patches/circle-usb-poll.py circle-rpi4mc
   python3 patches/circle-xinput.py   circle-rpi4mc
   ```

   then `./makeall` in each copy, and `make` in `addon/SDCard` and `addon/fatfs`.
   In the development machine `toolchain` and the three `circle-*` directories are
   symlinks to neo-pi's (they are in `.gitignore`).
3. **Raspberry Pi firmware**: in `third_party/rpi-firmware`, the same files as neo-pi's;
   `make_release.sh` puts them on the card.
4. `./build.sh all` → `build/rpi4/kernel8-rpi4.img` and `build/rpi3/kernel8.img`.
   `./build.sh 4` or `./build.sh 3` for one only. `./build.sh qemu` →
   `build/rpiqemu/kernel8.img` (Pi 3 kernel with `-DPROJECT_FAKEPAD`, not for hardware).

How `build.sh` works:
- copies `src/*`, `common/*` and the third-party C files into `build/rpiN/`;
- copies the core and the CPU and sound sources in `third_party/` flattened as
  `*.core.cpp` (`nec/v25.cpp` → `v25.core.cpp`...), only when changed, so the
  core is not rebuilt every time; a changed core header rebuilds the whole core;
- generates the Makefile from `src/Makefile.in` (`@CIRCLEHOME@`, `@SRC@`,
  `@COMMON@`, `@CORE@`, `@THIRD@`, `@MINIZ@`, `@NTSC@`);
- `make libm92.a` first (the core: `-std=gnu++20 -fno-exceptions -fno-rtti`, linked
  against the toolchain's libstdc++/newlib; the system stubs it needs are in
  `src/m92_sys.c`), then the kernel;
- checks that the kernel (code + data + bss) ends below `0x80000 + KERNEL_MAX_SIZE`.

Warnings: the front-end builds with `-Wextra -Wshadow`, the `m92*` core files
with `-Wall -Wextra -Wshadow`; MAME's and ymfm's sources are built as they
are (`-w`, headers through `-isystem`). The build must stay free of warnings.

### Release package

```
./build.sh all
./make_release.sh       # -> dist/m92-pi-<version>-sd.zip
```

`make_release.sh` checks that both kernels contain `PROJECT_VERSION` and are
newer than `src/project.h`, that the firmware (`third_party/rpi-firmware/`) and the card
files (`dist/sd/`) are there, that `README.txt` starts with `m92-pi <version>`, and leaves out
of the zip anything in `dist/sd/m92pi/` except `PUT_GAMES_HERE.txt` and
`list.txt` (games, saves, `BENCH`, logs, settings).
GitHub release: tag `v<version>`, asset `m92-pi-<version>-sd.zip`.

## 3. The core

### 3.1 Approach

The goal is MAME's accuracy without porting MAME. The CPU cores and the chip
logic are MAME's own source, compiled almost unchanged against a small
`emu.h` that provides only what they use. The machine (`m92.cpp`) re-implements
MAME's driver (`src/mame/irem/m92.cpp`, `m92_v.cpp`) by hand, with the same
memory maps, timings and interrupt logic. Where MAME and the real hardware
differ on video mixing, the MiSTer core (Martin Donlon, `Arcade-IremM92`) is
followed; no MiSTer code is included.

Reference sources used:
- MAME master, sparse checkout at commit `cef5d379` (4 Nov 2026):
  `src/devices/cpu/nec/*`, `src/mame/irem/m92*.cpp`, `irem_cpu.cpp`,
  `src/devices/sound/iremga20.cpp`, `src/devices/machine/pic8259.cpp`.
- MiSTer `Arcade-IremM92` commit `68a4683` (`rtl/*.sv`, especially `palram.sv`,
  `GA21`, `GA22`).
- For the M90/M97 (see 3.10): MAME master at commit `adb51e2` (10 Nov 2026),
  `src/mame/irem/m90*.cpp`, `m72_a.cpp`; MiSTer `Arcade-IremM90` commit
  `185179f` (`rtl/m90.sv`, `ga25*.sv`, `sound.sv`).

### 3.2 `emu.h`

- integer types, `BIT()`, `COMBINE_DATA`, `offs_t`, `attotime` (picoseconds, 64 bit);
- `cpu_device` with `execute_run()`, `m_icount`, `set_input_line()`;
- `address_space`: 2 KB pages, each page either a direct pointer (ROM/RAM) or
  read/write handlers (`static u16 fn(void *ctx, offs_t a, u16 mask)`);
- `emu_timer`: driven by the machine's scheduler (`scheduler_base`);
- `save_registry`: every piece of state registered with its size; save states are
  the registry written in order, with a magic (`M692`) and a version
  (`STATE_VERSION`, bump it when the registered state changes).

### 3.3 CPUs

- **Main CPU**: NEC V33 at 9 MHz (18 MHz / 2). `third_party/mame/nec/nec.cpp` reduced to the V33
  (debugger, device glue and state interface removed).
- **Sound CPU**: NEC V35 at 14.318181 MHz (divided by its PCK), opcodes decrypted
  through Irem's 256-byte table (per game, in `m92_games.inc`, from
  `third_party/mame/irem_cpu.cpp`).
- **CPU SPEED**: `set_cpu_speed(percent)` scales the main CPU's cycles per second.
  100 = 9 MHz, 222 = 20 MHz (MiSTer's Turbo). The sound CPU is never changed.
  Turbo roughly doubles the emulation time, so it is best on Pi 4.

### 3.4 Scheduler and timing

The scheduler lives in `irem_machine` and serves both machines; each
machine only says whether a CPU is held in reset (`cpu_held`) and when its
speed changes (`cpu_rate`, for the V35's PCK).

- Time in picoseconds. Pixel clock 26.666666 MHz / 4; 424 pixel clocks per line,
  262 lines per frame → **60.01 Hz** (`FRAME_HZ`).
- `run_until(target)`: the two CPUs run in slices up to the next timer; each CPU
  carries its fractional picoseconds (`ps_frac`), so the clocks never drift.
- `defer()`: writes that must be seen by the other CPU at the right moment
  (sound latches, sound reset) end the current slice first, as MAME's
  `synchronize()`.
- **A frame starts at line 248** (start of vertical blank) and ends at the next
  248: the games read the pads in the vblank interrupt, so the picture drawn
  during the rest of the frame already answers the pads read at its start
  (lowest input lag, no run-ahead).
- `scanline()` runs at the start of every line: it calls the line hook (beam
  racing), draws the line if it is visible (8-247) **before** the raster IRQ of
  that line is raised, then raises vblank/raster IRQs on the PIC.
- Sprite DMA (`0xf9000`): its length in pixel clocks is returned by
  `m92_video::dmacontrol_w`; `m_dma_busy` stays set until `dma_done` (as MAME).

### 3.5 Interrupts

`pic8259` is MAME's `pic8259.cpp` (Wilbert Pol) moved into `m92_machine`.
IRQ lines as MAME's driver: 0 vblank, 1 sprite DMA end, 2 raster, 3 sound latch 2.

### 3.6 Video (`m92_video.cpp`)

- Graphics decoded once at load into one byte per pixel (8x8 chars, 16x16 sprites);
  `gfx_psoldier` selects the Perfect Soldiers / Dream Soccer '94 sprite layout.
- **Tile layers (GA23)**: as MAME `m92_v.cpp`: three layers, scroll offsets,
  row scroll, wide layers, the three tile priority groups.
- **Sprites (GA21/GA22)**: the DMA copies sprites as the MiSTer core does:
  objects are written from slot `0x100 - ptr` downwards, so the first object
  copied ends up on top (MAME copies them in another order).
- **Mixing**: as MiSTer's `palram.sv`: one sprite line buffer in which the last
  sprite written wins, then per pixel a choice between sprite and tile colour.
- **Per-line sprite lists**: when the sprite buffer changes (DMA, CPU write, save
  state) `m_obj_dirty` is set; the core that draws rebuilds, once, the list of the
  objects crossing each visible line (`build_obj_lists`), in buffer order, so
  `draw_sprites` never scans the 256 slots and a line without sprites skips the
  sprite mixing. Whole 8-pixel tiles take a fast path in `draw_layer`. Both are
  pixel-identical to the plain loops (checked with `test/hashrun.cpp`, see 5.1).
- **Lines drawn one at a time**, registers captured at the start of the line
  (`line_regs`), so a line can be drawn later on another core. Everything the
  drawing reads (VRAM, palette, sprite buffer) calls `sync()` before changing.
- Output: 320x240, `0x00RRGGBB`. Visible area in MAME coordinates: x 80-399,
  y 8-247.

### 3.7 Sound (`m92_sound.cpp`)

- YM2151 by ymfm at 3.579545 MHz → 55.9 kHz, resampled to 48 kHz with linear
  interpolation.
- GA20: channel logic from MAME's `iremga20.cpp`, re-implemented inside
  `m92_sound` (MAME's file is not included), at 895 kHz, box-averaged
  to 48 kHz.
- Mix as MAME's driver: `0.4 * (L + R)` for the YM2151 plus GA20 / 2, mono into
  both channels.
- The core's output is MAME's (the hashes of `test/hashrun.cpp` check it). On
  the Pi, `FrameTakeAudio` in `src/frame.cpp` then applies the cabinet voicing, always
  on: low shelf +4 dB below 120 Hz and 2nd-order low-pass at 8 kHz (RBJ biquads,
  float, one sample at a time, no latency), -0.5 dB to keep the loudness.

### 3.8 Game tables (`tools/gen_m92_tables.py`)

```
python3 tools/gen_m92_tables.py <mame>/src/mame/irem > core/m92_games.inc
```

- Parses `m92.cpp`: `ROM_START` blocks (regions, `ROM_LOAD16_BYTE`, CRC, sizes),
  `GAME()` lines (parent, year, maker, description, machine config), the
  per-game decryption table name and the map type (normal, banked, Lethal
  Thunder, Major Title 2 with EEPROM).
- Input ports are run through `cpp` (macros `PORT_*` turned into markers) and
  merged as MAME's `ioport_port::insert_field` does: a later field removes the
  fields it overlaps, **unless both carry different conditions** (then both stay,
  as with Coinage in some sets). DIPs sorted by switch location.
- Special cases handled: `xxxx` entries in the decryption table (parsed as
  comma tokens), the machine map matched on `m_maincpu` only, the `eeprom`
  region of Major Title 2.
- 40 sets. Never edit `m92_games.inc` by hand.

Hiscores: `core/m92_hiscore.inc`, from MAME's hiscore plugin data:

```
python3 tools/gen_m92_hiscore.py <mame>/plugins/hiscore/hiscore.dat \
        core/m92_games.inc > core/m92_hiscore.inc
```

Only the sets in `m92_games.inc`, only `:maincpu,program` rows (it stops on
`@delay` or a prefill). MAME master `f337ad7` (7 Nov 2026): 34 of the 40
sets have an entry. Never edit `m92_hiscore.inc` by hand.

### 3.9 C API (`m92_api.h`)

Everything the front-end uses: game list and DIP descriptions, `m92_load()`
with a ROM fetch callback (by CRC, then by name; game zip then parent zip),
`m92_run_frame()`, inputs, audio, line callback / line sink / video sync /
line hook, save states, EEPROM, PCs for diagnostics, each game's picture size
(`m92_game_width()`, `m92_game_height()`). Inputs are active high;
players 3 and 4 use bit 4 = start, bit 5 = coin. Every M92 game uses 2 buttons
(`m92_game_buttons()` = 2), except Superior Soldiers / Perfect Soldiers (6):
three punches on the panel (bits 7-5) and three kicks on MAME's extra connector,
read as the high byte of the players 3-4 port (player 1 bits 1-3, player 2
bits 5-7), so the front-end writes them as player 4's byte.

### 3.10 The M90/M97 (`m90.cpp`, `m90_video.cpp`, `m90_sound.cpp`)

Bomber Man / Dynablaster / Atomic Punk, Bomber Man World / New Atomic Punk,
Risky Challenge / Gussun Oyoyo: 9 sets, 320x240, the same timing as the M92
(424 x 262 at 6.67 MHz, 60.01 Hz), so the front-end needs nothing new. The
machine is a second `irem_machine`; `m92_api` lists the M92 sets first, then
the M90/M97 ones.

- **Main CPU**: NEC V35 at 16 MHz (divided by its PCK), opcodes decrypted with
  the game's Irem table (the same tables as the M92 sound CPUs). Vertical blank
  on INTP0, from the line after the picture to its first line. CPU SPEED
  scales it as on the M92.
- **Sound board**: Z80 at 3.58 MHz, YM2151, 8-bit DAC. Interrupts through the
  RST buffer (IM 0): RST 18h for the sound latch, RST 28h for the YM2151,
  RST 08h for both; the latch is acknowledged by a write, not by reading it.
  M72-style boards play the samples in the Z80's NMI; the M99 board (Bomber
  Man World) plays them by itself up to a 00 byte, started by a write.
- **Video (GA25)**: two 64x64 (or 128x64, "wide") tile layers with row
  scroll and row select, 84 sprites (a later entry over an earlier one), one
  line at a time as on the M92 (line sink and multicore unchanged).
- **Where the MiSTer core is followed, not MAME** (it was made on a real
  board): which pens of a priority tile cover the sprites (pens 8-15 with
  attribute bit 4, every pen but 0 with bit 5); which sprite colours go over
  priority tiles (video control bits 0-1); the samples clocked at 32 MHz /
  4096 = 7812.5 Hz (MAME: 7680 Hz); only the YM2151's left output wired; the
  samples through a 3.5 kHz low-pass filter and -10 dB; the picture switched
  off shows colour 0.
- **Not supported**: Hasamu, Quiz F1 and Match It II / Shisensho II (384
  pixels wide, Quiz F1 and Match It II 256 lines high): the core handles them
  (visible area per game, Quiz F1's ROM bank) but the front-end is built for
  320x240, so `games.cpp` leaves them out. The bootlegs (other video and sound
  hardware) are not in the tables.
- Game tables: `python3 tools/gen_m90_tables.py <mame>/src/mame/irem >
  core/m90_games.inc` (shorter DIP names where the panel needs them, in
  `irem_ports.nice()`). Hiscores: `gen_m92_hiscore.py ... core/m90_games.inc
  m90 > core/m90_hiscore.inc` (8 of the 9 sets have an entry; not `newapunk`).
- Save states: magic `M90 `, version 1; the Z80's callbacks are put back
  after a load.

## 4. The front-end

Mostly neo-pi's, see neo-pi's `BUILD.md` for the shared parts.

### 4.1 `src/`

| file | role |
|---|---|
| `project.h` | name, version, picture size, buttons, frame phase line, LED error codes, `project_log` / `project_fatal`: everything `common/` needs from the project |
| `kernel.h`, `kernel.cpp` | the kernel class; start-up, starting cores 1 and 2, the main flow (list, game, list...), power off |
| `console.cpp` | the start-up log on screen, serial and `LOG.TXT`; fatal errors |
| `settings.cpp` | `m92pi.cfg`, the video mode it asks for, `pads.cfg` |
| `games.cpp` | game list (`ScanGames`, `list.txt`, `SplitMameName`), loading a set with its parent, save files, DIP switches (`LoadDips`) |
| `hiscore.cpp` | hiscores (`HiStart` / `HiTick` / `HiSave`) |
| `menus.cpp` | game list, in-game menu, OPTIONS, DIP SWITCHES, CONTROLS, power off countdown, credits |
| `gameloop.cpp` | `RunGame`: pads to the M92 panels, one emulated frame per screen frame, the in-game menu, the status line |
| `frame.h/.cpp` | the emulated frame: picture and sound buffers, where the lines go (screen, core 1, COMPOSITE on core 2), cabinet voicing |
| `racing.h/.cpp` | beam racing state for the game loop (`g_Race`), the line hook |
| `report.cpp` | the per-minute report in `LOG.TXT` |
| `bench.cpp` | BENCH and MCTEST |
| `mcline.h/.cpp` | multicore line drawing: core 0 queues `(y, line_regs)` in a ring, core 1 draws; `McSync()` before VRAM/palette/sprites change; if core 1 stops answering for 0.5 s it falls back to single core and logs it |
| `screen.h` | `CScreen`, the only interface the kernel draws through (game, menus, console) |
| `video.h/.cpp` | `CVideoOut`, the HDMI `CScreen`: framebuffer, HVS / CPU integer scaling, scanlines, status line (bands or side columns) |
| `ui.h/.cpp` | all screens, drawn at 304x224 in the middle of a 320x240 buffer; the logo (`s_Logo`, 88x15 drawn at 1x above the game list, 4 titles each side) and the palette (`ui.h`: petrol, white titles, cyan values like the logo's i) |
| `fontdata.c` / `fontlist.c` | You Squared (menus, `g_MenuFont`) / Cyberwire (list, `g_ListFont`), ZX Origins |
| `ziphelp.c` | zip reading and PNG writing (miniz) |
| `m92_sys.c` | libc / libstdc++ stubs for the core |

A CRT output on the composite jack would be another `CScreen`, in its own
files, chosen in `kernel.h`; nothing else in the kernel changes.

### 4.2 `common/`

Nothing M92 in it: it takes what it needs from `project.h`. The same files
are in neo-pi and nes-pi, byte for byte.

| file | role |
|---|---|
| `main.cpp` | Circle's entry point |
| `smp.h` | work shared between cores: `SharedLoad` / `SharedStore`, `SharedAcquire` / `SharedRelease`, `SignalEvent` / `WaitEvent`, `Ticks` |
| `race.cpp`, `raster.cpp` | beam racing (HVS line counter measured at start-up); `RACE_ROWS` is the picture's height, from `project.h`. If rows are late in too many frames racing gives up for that game; `Rearm()` tries again at the next game started from the list. `test/race_sim.cpp` tests them on the host |
| `tvcore.cpp`, `composite.c` | COMPOSITE (blargg's snes_ntsc) on core 2, line by line; the look and the line sizes come from the project (`FrameTvInit` in `src/frame.cpp`, `PROJECT_TV_*` in `project.h`) |
| `fbmap.cpp` | the framebuffer mapped as write-combining memory |
| `input.cpp` | USB pads and keyboard, one pad per player (`PROJECT_PLAYERS` in `project.h`: 4; pads beyond are not taken), `PROJECT_FAKEPAD` for QEMU. Logical buttons A-F (`BTN_E`/`BTN_F` are bits 10-11, used when `PROJECT_BUTTONS` is 6; `BTN_MENU` bit 12); CONTROLS asks A B C D E F SELECT START, all of them (`pads.cfg` files with `E=-` from earlier versions still load). Without a set-up: LB/RB on known layouts, buttons 5-6 on generic pads, B/N on the keyboard |
| `audio.cpp` | HDMI audio with drift correction |
| `profile.cpp` | sampling profiler for the benchmark |
| `pacing.h/.cpp` | frame phase (`PROJECT_PHASE_LINE`) and frame delay (`framedelay=`) when not racing |
| `stats.h/.cpp` | timing statistics: histograms, averages, percentiles (`BENCH.TXT`, the per-minute log lines) |

Buttons in `RunGame`: A and B are buttons 1 and 2, the only ones the games use.
C D E F (buttons 3-6) are for Superior Soldiers / Perfect Soldiers only, as on
the original 6-button panel: A B C = light/medium/strong punch, D E F =
light/medium/strong kick. There is no button combination: a pad without E and
F has no medium and strong kick. Colours on screen: A red, B yellow, C green,
D blue, E violet, F orange.

Cores: 0 = emulation + sound + main loop, 1 = M92 lines (`mcline`),
2 = COMPOSITE (`tvcore`), 3 idle. `multicore=off` keeps everything on core 0.

Game list: one entry per M92 MAME zip in `SD:/m92pi`. Display names from
`m92pi/list.txt` (`set=Title|Region`), otherwise MAME's description
(`SplitMameName` drops " sound CPU", " M92 hardware", commas); the bottom line
shows `SET  REGION`.

Per-game files in `m92pi/saves/`: `<set>.dip` (`dsw=FFBE dsw3=FF`), `<set>.st1`
(save state), `<set>.nv` (Major Title 2 EEPROM), `<set>.hi` (hiscores).

Hiscores (`HiStart` / `HiTick` / `HiSave` in `hiscore.cpp`, rows in
`core/m92_hiscore.inc`) work as MAME's hiscore plugin: after each frame,
once every row of the game's entry starts and ends with the expected bytes
(the game has set its table up), `<set>.hi` is put over it; the table is
written back, if it differs from the file and from the game's own, when the
in-game menu opens, at GAME LIST, RESET, another game and POWER OFF (never
during play: no SD writes while racing). The file is the rows' bytes one
after the other, MAME's format: `.hi` files can be copied from and to MAME.

Sets without hiscores (no entry in MAME's `hiscore.dat`; as MAME, no
fall-back to the parent, whose table may be elsewhere in a clone):

| set | game |
|---|---|
| `mysticrib` | Mystic Riders (bootleg?) |
| `majtitl2a` | Major Title 2 (World, set 2, 016 sound CPU) |
| `majtitl2b` | Major Title 2 (World, set 3) |
| `inthuntk` | In The Hunt (Korea?) |
| `leaguemna` | Yakyuu Kakutou League-Man (Japan, set 2) |
| `geostorma` | Geo Storm (Japan, 026 sound CPU) |

To add one: find the table with the harness (set a record, compare the RAM),
add the entry to MAME's `hiscore.dat` and regenerate `m92_hiscore.inc`.

DIP SWITCHES leaves out Unknown, Flip Screen (not emulated) and Service Mode;
`LoadDips` puts Flip Screen and Service Mode back to MAME's default, so an
old `.dip` cannot keep a game in service mode.

Settings file `m92pi/m92pi.cfg`: `cpu=normal|turbo`, `filter=...`, and the testing
options `scaler=cpu`, `usbpoll=500|250|native`, `multicore=off`,
`beamracing=on|off`, `framedelay=off|N`.

Diagnostic flag files: `m92pi/BENCH` (benchmark → `BENCH.TXT`, `PROFILE.TXT`),
`m92pi/MCTEST` (single core vs multicore picture check → `MCTEST.TXT`).
LED error codes: 2 HDMI, 3 SD, 5 no game, 6 core, 7 memory (`src/project.h`).

## 5. Testing

### 5.1 Linux harness

```
cd harness && make
./m92test <rom folder> <set> [-f frames] [-s every] [-o dir] [-w] [-t] [-turbo] [-coin frame] [-hitest]
```

- `-s N`: PPM screenshot every N frames (always `<set>_last.ppm`); `-w`: WAV;
- `-t`: save state at the middle frame, run on, load, run again, compare hashes
  (must print `STATE TEST: identical`);
- `-coin F`: coin at frame F, start one second later;
- `-hitest`: hiscores: when the game sets its table up, then a changed table
  put back must still be there 30 s later (exit code 0 = right);
- environment: `CTLTRACE=1` (video registers every 10 frames),
  `SPRDUMP=<frame>` (sprite buffer and layer registers),
  `PIXDUMP=<frame> PX=x PY=y` (how one pixel was chosen).

**Regression check** (`test/hashrun.cpp`, build command in the file): one line per
frame with the picture and sound hashes, for fixed scripted inputs. Run it on
the old and the new build and compare the files: any optimisation must give
identical files. `MC=1` draws the lines late, as the multicore build does.

### 5.2 Against MAME

The reference is MAME 0.264 (`/usr/games/mame` on the development machine) run
with a Lua autoboot script that saves a snapshot every N frames and the audio;
the same frames from `m92test` are compared pixel by pixel. MAME 0.264 wants
the sets' PLD dumps: empty dummy files with the right names added to a copy of
the zip are enough.

### 5.3 QEMU

```
./build.sh qemu
qemu-system-aarch64 -M raspi3b -kernel build/rpiqemu/kernel8.img \
  -drive file=sd.img,if=sd,format=raw -serial file:serial.txt \
  -device usb-kbd -global bcm2835-fb.xres=1280 -global bcm2835-fb.yres=720
```

`sd.img`: FAT32 image with `dist/sd/` and some games. The USB keyboard is
player 1 (arrows, Z X C V B N = A B C D E F, Enter = Start, Space = Select, Esc = menu;
press keys for about 120 ms through the QEMU monitor's `sendkey`).
`PROJECT_FAKEPAD` also adds a fake USB pad driven by the arrows and Q-P
(buttons 1-10), to exercise the CONTROLS set-up. QEMU has no HDMI audio, no HVS line counter (no
beam racing) and runs slower than real time: it checks screens and menus, not
timing.

## 6. Conventions
<!-- M.A.R.K. 13 · NO FLESH SHALL BE SPARED -->

- Files from MAME keep their `license:BSD-3-Clause` / `copyright-holders` header;
  m92-pi's own files are GPL-3.0-or-later (`SPDX-License-Identifier`).
- The core must stay free of Circle headers; the front-end must not include C++
  standard library headers (only `m92_api.h`).
- `common/` must stay free of anything M92: what it needs comes from
  `project.h`.
- A variable one core writes and another reads goes through `smp.h`, never a
  plain access.
- No run-ahead: input lag is handled by starting the frame at vblank, frame
  delay and beam racing.
- `dist/sd/LICENSES.txt` must list every third-party component in the kernel.

## 7. Credits

- **MAME** authors, among them Bryan McPhail, Alex W. Jackson, Acho A. Tang,
  R. Belmont and Valley Bell: NEC V33/V35 cores, M92 and M90 drivers, GA20, Irem
  decryption tables, 8259 (BSD 3-Clause).
- **Martin Donlon** (wickerwaka): the MiSTer Irem M92 and M90 cores, the
  reference for sprite DMA, video mixing and the M90's sound board (no code
  included).
- **Nicolas Allemand**, Rupert Carmichael and rofl0r: the Z80 core (MIT).
- **Aaron Giles** and Rupert Carmichael: ymfm (BSD 3-Clause).
- **Rene Stange**: Circle (GPLv3).
- **Shay Green** (blargg): snes_ntsc, the COMPOSITE filter (LGPL 2.1+).
- **Rich Geldreich** and contributors: miniz (MIT).
- **Damien Guard**: the ZX Origins fonts You Squared and Cyberwire.
- **Alessio Scanderebech**: idea, direction, design, testing on hardware.
- **Claude** (Anthropic): code and documentation, written with Alessio.

Full licence texts: `dist/sd/LICENSES.txt`.
