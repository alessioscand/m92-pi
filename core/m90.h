// m92-pi - Irem M90 / M97 / M99 arcade machine (Bomber Man, Bomber Man World,
// Risky Challenge, Hasamu, Quiz F1, Match It II)
//
// The hardware as MAME describes it (src/mame/irem/m90.cpp, m90_v.cpp, m72_a.cpp;
// BSD-3-Clause, Bryan McPhail), with the details measured on a real board by the
// MiSTer core (Martin Donlon): encrypted V35 main CPU, Z80 sound CPU with a YM2151
// and an 8-bit sample DAC, one GA25 video chip drawn a line at a time.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_M90_H
#define M92PI_M90_H

#include "emu.h"
#include "irem_machine.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------- game tables
enum { M90_REG_MAINCPU, M90_REG_BANK, M90_REG_SOUNDCPU, M90_REG_GFX, M90_REG_SAMPLES, M90_REG_COUNT };

struct m90_game
{
	const char *name, *parent, *desc, *year, *manufacturer;
	const m92_rom *roms;
	const m92_inputs *inputs;
	const uint8_t *decrypt;          // the V35's opcode table
	uint8_t m99;                     // M99 sound board: the samples play by themselves
	uint8_t quizf1;                  // Quiz F1: 64 KB bank at 0x80000
	uint16_t vis_x0, vis_w, vis_y0, vis_h;   // visible area in MAME's 512x512 bitmap
	uint32_t vector;                 // where the start vector at 0xffff0 is copied from
	uint32_t region_size[M90_REG_COUNT];
};

const m90_game *m90_find_game(const char *name);
int m90_game_count();
const m90_game *m90_game_at(int i);
const m92_hiscore *m90_find_hiscore(const char *name);

// ---------------------------------------------------------------- the machine
class v35_device;
class z80_device;
class m90_video;
class m90_sound;

class m90_machine : public irem_machine
{
public:
	typedef std::function<bool(const char *name, uint32_t crc, uint32_t size, uint8_t *dst)> rom_fetch;

	m90_machine();
	~m90_machine();

	// load a game; on failure returns false and the name of the missing ROM in missing
	bool load(const m90_game *game, const rom_fetch &fetch, std::string &missing);
	const m90_game *game() const { return m_game; }

	// irem_machine
	void reset() override;
	void run_frame() override;
	int screen_w() const override;
	int screen_h() const override;
	// main CPU speed: 100 = the board's 16 MHz V35
	void set_cpu_speed(int percent) override;
	int audio_read(int16_t *dst, int max_frames) override;
	void set_video_sync(void (*fn)(void *), void *ctx) override;
	void render_line(const void *regs, int y, uint32_t *out) override;
	size_t state_size() const override;
	bool save_state(std::vector<uint8_t> &out) override;
	bool load_state(const std::vector<uint8_t> &in) override;
	size_t hi_size() const override;
	bool hi_ready() override;
	void hi_get(uint8_t *dst) override;
	void hi_put(const uint8_t *src) override;
	uint32_t main_pc() const override;
	uint32_t sound_pc() const override;

	// for the harness and diagnostics
	const uint8_t *main_ram() const { return m_ram; }
	const m90_video *video() const { return m_video.get(); }

	// ---- used by the handlers (public so the static thunks can reach them)
	static u16 main_read(void *ctx, offs_t a, u16 mask);
	static void main_write(void *ctx, offs_t a, u16 d, u16 mask);
	static u16 main_io_read(void *ctx, offs_t a, u16 mask);
	static void main_io_write(void *ctx, offs_t a, u16 d, u16 mask);
	static u8 sound_read(void *ctx, u16 a);
	static void sound_write(void *ctx, u16 a, u8 d);
	static u8 sound_in(void *ctx, u16 port);
	static void sound_out(void *ctx, u16 port, u8 d);

	// the sound CPU's interrupt: the RST 18h (sound latch) and RST 28h (YM2151) lines
	void set_ym_irq(bool on);

protected:
	void cpu_rate(int slot) override;

private:
	friend class m90_sound;

	void map_main();
	void update_cpu_rates();
	void scanline(s32 line);
	void sample_tick(s32);
	void update_sound_irq();

	const m90_game *m_game = nullptr;
	const m92_hiscore *m_hi = nullptr;
	std::vector<u8> m_region[M90_REG_COUNT];
	u32 m_sample_mask = 0;

	std::unique_ptr<v35_device> m_maincpu;
	std::unique_ptr<z80_device> m_soundcpu;
	int m_cpu_speed = 100;

	save_registry m_save;
	emu_timer *m_scan_timer = nullptr;
	emu_timer *m_sample_timer = nullptr;

	// memory
	u8 m_ram[0x4000];                // 0xa0000
	u8 m_sound_ram[0x1000];          // 0xf000 on the Z80
	u8 m_bank = 0;                   // Quiz F1

	// sound board
	u8 m_soundlatch = 0, m_soundlatch_pending = 0;
	u8 m_ym_irq = 0;
	u32 m_sample_addr = 0;
	u8 m_sample_play = 0;            // M99: playing until a 00 byte

	s32 m_line = 0;

	std::unique_ptr<m90_video> m_video;
	std::unique_ptr<m90_sound> m_sound;
	bool m_video_synced = false;
};

#endif
