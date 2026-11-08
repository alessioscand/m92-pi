// m92-pi - Irem M92 arcade machine
//
// The hardware as MAME describes it (src/mame/irem/m92.cpp, m92_v.cpp; BSD-3-Clause,
// Bryan McPhail), cross-checked with the MiSTer core (Martin Donlon) for the video
// mixing: V33 main CPU, encrypted V35 sound CPU, YM2151 (ymfm), Irem GA20, uPD71059 PIC,
// GA21/GA22/GA23 video drawn one line at a time.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_M92_H
#define M92PI_M92_H

#include "emu.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// ---------------------------------------------------------------- game tables
enum { M92_REG_MAINCPU, M92_REG_SOUNDCPU, M92_REG_GFX1, M92_REG_GFX2, M92_REG_IREM, M92_REG_EEPROM, M92_REG_COUNT };
enum { M92_MAP_M92, M92_MAP_M92_BANKED, M92_MAP_LETHALTH, M92_MAP_MAJTITL2 };

struct m92_rom
{
	int region;
	const char *name;
	uint32_t offset, size;
	uint8_t load16_byte;            // every other byte, as ROM_LOAD16_BYTE
	uint32_t crc;
};

struct m92_dip_setting { uint16_t value; const char *name; };

struct m92_dip
{
	uint8_t bank3;                  // 0: DSW port (SW1 low byte, SW2 high byte); 1: SW3 (COINS_DSW3 high byte)
	uint16_t mask, def;
	const char *name, *loc;
	uint16_t first, count;          // settings, in the input set's table
	uint16_t cond_mask;             // shown only when (DSW & cond_mask) EQUALS (1) / NOTEQUALS (2) cond_value
	uint8_t cond_op;
	uint16_t cond_value;
};

struct m92_inputs
{
	const m92_dip *dips;
	const m92_dip_setting *settings;
	uint16_t dsw_default;
	uint8_t dsw3_default;
	uint8_t players;
	uint8_t buttons;
};

struct m92_game
{
	const char *name, *parent, *desc, *year, *manufacturer;
	const m92_rom *roms;
	const m92_inputs *inputs;
	const uint8_t *sound_decrypt;
	int map;
	uint8_t gfx_psoldier;           // sprites laid out as Perfect Soldiers / Dream Soccer '94
	uint8_t banked;                 // 0xa0000-0xbffff banked ROM
	uint8_t eeprom;                 // Major Title 2: 28C64 at 0xf0000
	uint32_t region_size[M92_REG_COUNT];
};

const m92_game *m92_find_game(const char *name);
int m92_game_count();
const m92_game *m92_game_at(int i);

// hiscores: MAME's hiscore.dat rows (main CPU program space). The table is in
// place when every row starts with "first" and ends with "last"; the .hi file is
// the rows' bytes one after the other, as MAME's hiscore plugin writes it.
struct m92_hiscore_row { uint32_t addr; uint16_t size; uint8_t first, last; };
struct m92_hiscore { const char *name; const m92_hiscore_row *rows; int count; };
const m92_hiscore *m92_find_hiscore(const char *name);

// ---------------------------------------------------------------- the machine
class nec_common_device;
class v35_device;
class m92_sound;
class m92_video;

class m92_machine : public scheduler_base
{
public:
	// fetch one ROM file: from the game's zip, then its parent's. Fill dst with exactly size bytes.
	typedef std::function<bool(const char *name, uint32_t crc, uint32_t size, uint8_t *dst)> rom_fetch;

	static constexpr int SCREEN_W = 320;
	static constexpr int SCREEN_H = 240;
	static constexpr int AUDIO_RATE = 48000;

	m92_machine();
	~m92_machine();

	// load a game; on failure returns false and the name of the missing ROM in missing
	bool load(const m92_game *game, const rom_fetch &fetch, std::string &missing);
	const m92_game *game() const { return m_game; }
	void reset();

	// one frame: 262 lines from the line after the visible area (vertical blank first).
	// Each of the 240 visible lines (y = 0..239) is drawn as it starts: by default here,
	// and handed to the line callback as 320 pixels 0x00RRGGBB
	typedef std::function<void(int y, const uint32_t *rgb)> line_cb;
	void set_line_callback(line_cb cb) { m_line_cb = std::move(cb); }
	// multicore: with a sink set, lines are not drawn here; the sink gets the registers the
	// line must be drawn with and draws it (on another core) with render_line(). The sync
	// function is called before VRAM, palette or sprites change: it must wait until every
	// line handed to the sink has been drawn.
	struct line_regs { uint16_t pf_control[3][4]; uint16_t master_control[4]; uint16_t videocontrol; };
	typedef std::function<void(int y, const line_regs &regs)> line_sink;
	void set_line_sink(line_sink sink) { m_line_sink = std::move(sink); }
	void set_video_sync(void (*fn)(void *), void *ctx);
	void render_line(const line_regs &regs, int y, uint32_t *out);
	// called at the start of each of the 262 lines (0 = first line of the frame counter,
	// 8 = first visible line), before it is drawn: beam racing
	typedef std::function<void(int line)> line_hook;
	void set_line_hook(line_hook hook) { m_line_hook = std::move(hook); }
	static constexpr int FIRST_VISIBLE_LINE = 8;
	static constexpr int VBLANK_LINE = 248;
	static constexpr int TOTAL_LINES = 262;
	static constexpr double FRAME_HZ = 26666666.0 / 4 / 424 / 262;
	void run_frame();

	// inputs: one byte per player, active high:
	// bit 0 right, 1 left, 2 down, 3 up, 4 button 4, 5 button 3, 6 button 2, 7 button 1
	void set_player(int n, uint8_t bits) { m_player[n & 3] = bits; }
	// bit 0 start1, 1 start2, 2 coin1, 3 coin2, 4 service, 5 test, 6 start3/coin3 ... (active high)
	void set_system(uint8_t bits) { m_system = bits; }
	uint16_t dsw() const { return m_dsw; }
	uint8_t dsw3() const { return m_dsw3; }
	void set_dsw(uint16_t dsw, uint8_t dsw3) { m_dsw = dsw; m_dsw3 = dsw3; }

	// main CPU speed: 100 = original 9 MHz; the MiSTer core's Turbo runs it at 20 MHz (222)
	void set_cpu_speed(int percent);

	// sound: 48 kHz stereo pairs made during the frames run so far
	int audio_available() const;
	int audio_read(int16_t *dst, int max_frames);

	// save states (whole machine, about 400 KB)
	size_t state_size() const;
	bool save_state(std::vector<uint8_t> &out);
	bool load_state(const std::vector<uint8_t> &in);

	// for the harness and diagnostics
	uint64_t frame_count() const { return m_frame; }
	uint32_t main_pc() const;
	uint32_t sound_pc() const;
	const uint8_t *main_ram() const { return m_ram; }
	const m92_video *video() const { return m_video.get(); }
	uint8_t *eeprom(size_t &size) { size = sizeof(m_eeprom); return m_game && m_game->eeprom ? m_eeprom : nullptr; }

	// hiscores (see m92_hiscore)
	size_t hi_size() const;
	bool hi_ready();
	void hi_get(uint8_t *dst);
	void hi_put(const uint8_t *src);

	// scheduler_base
	attotime current_time() const override;
	void timer_changed(emu_timer *t) override;

	// ---- used by the handlers (public so the static thunks can reach them)
	struct pic8259
	{
		m92_machine *m = nullptr;
		emu_timer *timer = nullptr;
		u8 state, isr, irr, prio, imr, irq_lines, ocw3;
		u8 level_trig_mode, vector_size, cascade, icw4_needed, vector_addr_low, base, vector_addr_high;
		u8 slave, nested, mode, auto_eoi, is_x86;
		s8 current_level;
		void reset();
		void update();
		void set_irq_line(int irq, int state);
		u8 acknowledge();
		u8 read(int offset);
		void write(int offset, u8 data);
	};

	void main_cpu_irq(int state);

	// memory and I/O
	static u16 main_read(void *ctx, offs_t a, u16 mask);
	static void main_write(void *ctx, offs_t a, u16 d, u16 mask);
	static u16 main_io_read(void *ctx, offs_t a, u16 mask);
	static void main_io_write(void *ctx, offs_t a, u16 d, u16 mask);
	static u16 sound_read(void *ctx, offs_t a, u16 mask);
	static void sound_write(void *ctx, offs_t a, u16 d, u16 mask);
	static u16 sound_io_read(void *ctx, offs_t a, u16 mask);
	static void sound_io_write(void *ctx, offs_t a, u16 d, u16 mask);

private:
	friend class m92_sound;
	friend class m92_video;

	struct cpu_slot
	{
		cpu_device *cpu = nullptr;
		u64 cycles_per_sec = 0;      // cycles, not clocks (V35: clock / PCK)
		s64 ps_frac = 0;             // carried picoseconds * cycles_per_sec, for exact time
		int requested = 0;           // cycles asked for in the current run
	};

	void map_main();
	void run_until(attotime target);
	void run_cpu(cpu_slot &s, attotime until);
	void defer(std::function<void()> fn);
	void update_cpu_rates();

	void scanline(s32 line);
	void dma_done(s32);
	void soundlatch_write(u8 data);
	void soundlatch2_write(u8 data);
	void set_soundlatch_pending(bool p);
	void set_soundlatch2_pending(bool p);
	void sound_reset_w(u16 data);

	const m92_game *m_game = nullptr;
	std::vector<u8> m_region[M92_REG_COUNT];

	std::unique_ptr<nec_common_device> m_maincpu;
	std::unique_ptr<v35_device> m_soundcpu;
	cpu_slot m_slot[2];
	cpu_slot *m_running = nullptr;
	attotime m_now;                  // machine time between slices
	attotime m_slice_end;
	std::vector<std::function<void()>> m_deferred;
	bool m_abort = false;
	int m_cpu_speed = 100;

	save_registry m_save;
	pic8259 m_pic;
	emu_timer *m_scan_timer = nullptr;
	emu_timer *m_dma_timer = nullptr;

	// memory
	u8 m_ram[0x10000];               // 0xe0000
	u8 m_sound_ram[0x4000];          // 0xa0000 on the V35
	const m92_hiscore *m_hi = nullptr;
	u8 m_eeprom[0x2000];
	u8 m_bank = 0;

	// inputs
	u8 m_player[4] = { 0, 0, 0, 0 };
	u8 m_system = 0;
	u16 m_dsw = 0xffff;
	u8 m_dsw3 = 0xff;

	// latches
	u8 m_soundlatch = 0, m_soundlatch_pending = 0;
	u8 m_soundlatch2 = 0, m_soundlatch2_pending = 0;
	u8 m_sound_in_reset = 0;

	// raster
	s32 m_raster_irq_position = -1;
	s32 m_line = 0;
	u8 m_dma_busy = 1;

	std::unique_ptr<m92_video> m_video;
	std::unique_ptr<m92_sound> m_sound;
	line_cb m_line_cb;
	line_sink m_line_sink;
	line_hook m_line_hook;
	bool m_video_synced = false;
	u64 m_frame = 0;
};

#endif
