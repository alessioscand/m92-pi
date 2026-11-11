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
#include "irem_machine.h"
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

class m92_machine : public irem_machine
{
public:
	// fetch one ROM file: from the game's zip, then its parent's. Fill dst with exactly size bytes.
	typedef std::function<bool(const char *name, uint32_t crc, uint32_t size, uint8_t *dst)> rom_fetch;

	static constexpr int SCREEN_W = 320;
	static constexpr int SCREEN_H = 240;

	m92_machine();
	~m92_machine();

	// load a game; on failure returns false and the name of the missing ROM in missing
	bool load(const m92_game *game, const rom_fetch &fetch, std::string &missing);
	const m92_game *game() const { return m_game; }

	// irem_machine
	void reset() override;
	void run_frame() override;
	int screen_w() const override { return SCREEN_W; }
	int screen_h() const override { return SCREEN_H; }
	// main CPU speed: 100 = original 9 MHz; the MiSTer core's Turbo runs it at 20 MHz (222)
	void set_cpu_speed(int percent) override;
	int audio_read(int16_t *dst, int max_frames) override;
	void set_video_sync(void (*fn)(void *), void *ctx) override;
	void render_line(const void *regs, int y, uint32_t *out) override;
	size_t state_size() const override;
	bool save_state(std::vector<uint8_t> &out) override;
	bool load_state(const std::vector<uint8_t> &in) override;
	uint8_t *eeprom(size_t &size) override { size = sizeof(m_eeprom); return m_game && m_game->eeprom ? m_eeprom : nullptr; }
	size_t hi_size() const override;
	bool hi_ready() override;
	void hi_get(uint8_t *dst) override;
	void hi_put(const uint8_t *src) override;
	uint32_t main_pc() const override;
	uint32_t sound_pc() const override;

	// the registers a line is drawn with (see set_line_sink)
	struct line_regs { uint16_t pf_control[3][4]; uint16_t master_control[4]; uint16_t videocontrol; };
	static_assert(sizeof(line_regs) <= LINE_REGS_BYTES, "line regs");

	// for the harness and diagnostics
	int audio_available() const;
	const uint8_t *main_ram() const { return m_ram; }
	const m92_video *video() const { return m_video.get(); }

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

protected:
	bool cpu_held(int slot) const override { return slot == 1 && m_sound_in_reset; }
	void cpu_rate(int slot) override;

private:
	friend class m92_sound;
	friend class m92_video;

	void map_main();
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
	bool m_video_synced = false;
};

#endif
