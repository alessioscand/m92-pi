// m92-pi - Irem M90 / M97 / M99 arcade machine (see m90.h)
//
// Memory maps and I/O from MAME's src/mame/irem/m90.cpp and m72_a.cpp (BSD-3-Clause,
// Bryan McPhail); video timing, the sample clock and the M99 sample player from the
// MiSTer core (ga25.sv, sound.sv; GPL-2.0-or-later, Martin Donlon).
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m90.h"
#include "m90_video.h"
#include "m90_sound.h"
#include "z80_device.h"
#include "nec/v25.h"
#include "irem_cpu.h"
#include <cstring>

#include "m90_games.inc"
#include "m90_hiscore.inc"

static constexpr u32 MAIN_CLOCK = 32000000 / 2;         // V35 (divided by its PCK)
static constexpr u32 SOUND_CLOCK = 3579545;              // Z80
static constexpr int HTOTAL = 424, VTOTAL = 262;
// pixel clock 26.666666 MHz / 4, as the M92
static constexpr s64 LINE_PS = s64(HTOTAL * 1e12 / (26666666.0 / 4) + 0.5);
// the samples are clocked at 32 MHz / 4096 (MiSTer sound.sv; MAME has 128 x 60 Hz)
static constexpr s64 SAMPLE_PS = s64(4096 * 1e12 / 32000000.0 + 0.5);

static const u32 STATE_MAGIC = 0x2030394d;              // "M90 " little-endian
static const u32 STATE_VERSION = 1;

// ---------------------------------------------------------------- game list

const m90_game *m90_find_game(const char *name)
{
	for (const auto &g : m90_games)
		if (!strcmp(g.name, name))
			return &g;
	return nullptr;
}

const m92_hiscore *m90_find_hiscore(const char *name)
{
	for (const auto &h : m90_hiscores)
		if (!strcmp(h.name, name))
			return &h;
	return nullptr;
}

int m90_game_count() { return int(sizeof(m90_games) / sizeof(m90_games[0])); }
const m90_game *m90_game_at(int i) { return (i >= 0 && i < m90_game_count()) ? &m90_games[i] : nullptr; }

// ---------------------------------------------------------------- construction

m90_machine::m90_machine()
{
	m_video = std::make_unique<m90_video>();
	m_sound = std::make_unique<m90_sound>(*this);
}

m90_machine::~m90_machine()
{
}

int m90_machine::screen_w() const { return m_game ? m_game->vis_w : 320; }
int m90_machine::screen_h() const { return m_game ? m_game->vis_h : 240; }

// the first visible line: 8 as the M92 when the picture is 240 lines high, earlier when
// it is taller (Quiz F1, Match It II: 256 lines)
static int first_line(const m90_game *g)
{
	const int f = irem_machine::FIRST_VISIBLE_LINE - (g->vis_h - 240) / 2;
	return f < 0 ? 0 : f;
}

// ---------------------------------------------------------------- loading

bool m90_machine::load(const m90_game *game, const rom_fetch &fetch, std::string &missing)
{
	m_game = game;
	m_hi = m90_find_hiscore(game->name);
	for (int r = 0; r < M90_REG_COUNT; r++)
		m_region[r].assign(game->region_size[r], 0);

	std::vector<u8> tmp;
	for (const m92_rom *rom = game->roms; rom->name; rom++)
	{
		std::vector<u8> &reg = m_region[rom->region];
		tmp.assign(rom->size, 0);
		if (!fetch(rom->name, rom->crc, rom->size, tmp.data()))
		{
			missing = rom->name;
			return false;
		}
		if (rom->load16_byte)
		{
			for (u32 i = 0; i < rom->size && rom->offset + i * 2 < reg.size(); i++)
				reg[rom->offset + i * 2] = tmp[i];
		}
		else
		{
			const u32 n = std::min<u32>(rom->size, u32(reg.size()) - std::min<u32>(rom->offset, u32(reg.size())));
			memcpy(&reg[rom->offset], tmp.data(), n);
		}
	}
	// the start vector, as MAME's ROM_COPY
	std::vector<u8> &main = m_region[M90_REG_MAINCPU];
	if (main.size() >= 0x100000 && game->vector + 0x10 <= main.size())
		memcpy(&main[0xffff0], &main[game->vector], 0x10);

	u32 pow2 = 1;
	while (pow2 < m_region[M90_REG_SAMPLES].size())
		pow2 <<= 1;
	m_sample_mask = pow2 - 1;
	if (m_region[M90_REG_SAMPLES].empty())
		m_region[M90_REG_SAMPLES].assign(1, 0);

	m_video->decode_gfx(m_region[M90_REG_GFX].data(), m_region[M90_REG_GFX].size());
	m_video->set_area(game->vis_x0, game->vis_w);

	// devices; everything registered with m_save in a fixed order for save states
	delete_timers();
	m_save.items.clear();

	m_maincpu = std::make_unique<v35_device>(MAIN_CLOCK);
	m_soundcpu = std::make_unique<z80_device>(SOUND_CLOCK, this, sound_read, sound_write, sound_in, sound_out);
	for (cpu_device *c : { (cpu_device *)m_maincpu.get(), (cpu_device *)m_soundcpu.get() })
	{
		c->m_sched = this;
		c->m_save = &m_save;
	}
	m_maincpu->set_decryption_table(game->decrypt);
	m_maincpu->space(AS_PROGRAM).configure(20, 16, main_read, main_write, this);
	m_maincpu->space(AS_IO).configure(16, 16, main_io_read, main_io_write, this);
	map_main();

	m_maincpu->device_start();
	m_soundcpu->device_start();
	m_slot[0].cpu = m_maincpu.get();
	m_slot[1].cpu = m_soundcpu.get();
	irem_machine::register_save(m_save);

	m_scan_timer = new_timer(m_save, [this](s32 p) { scanline(p); });
	m_sample_timer = new_timer(m_save, [this](s32 p) { sample_tick(p); });

	m_sound->start(m_save);
	m_video->register_save(m_save);

	m_save.add(&m_now, sizeof(m_now));
	m_save.add(m_ram, sizeof(m_ram));
	m_save.add(m_sound_ram, sizeof(m_sound_ram));
	m_save.add(&m_bank, sizeof(m_bank));
	m_save.add(&m_soundlatch, sizeof(m_soundlatch));
	m_save.add(&m_soundlatch_pending, sizeof(m_soundlatch_pending));
	m_save.add(&m_ym_irq, sizeof(m_ym_irq));
	m_save.add(&m_sample_addr, sizeof(m_sample_addr));
	m_save.add(&m_sample_play, sizeof(m_sample_play));
	m_save.add(&m_line, sizeof(m_line));
	m_save.add(&m_frame, sizeof(m_frame));

	reset();
	return true;
}

void m90_machine::map_main()
{
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	u8 *rom = m_region[M90_REG_MAINCPU].data();
	u8 *vram = m_video->vram_bytes();
	// multicore: VRAM writes go through main_write, which waits for the drawing core
	u8 *vram_w = m_video_synced ? nullptr : vram;

	for (offs_t a = 0; a < 0x100000; a += 0x800)
	{
		mp.map_read(a, a, nullptr);
		mp.map_write(a, a, nullptr);
	}
	mp.map_read(0x00000, 0x7ffff, rom);
	if (m_game->quizf1 && !m_region[M90_REG_BANK].empty())
		mp.map_read(0x80000, 0x8ffff, m_region[M90_REG_BANK].data() + (m_bank & 0x0f) * 0x10000);
	mp.map_read(0xa0000, 0xa3fff, m_ram);
	mp.map_write(0xa0000, 0xa3fff, m_ram);
	mp.map_read(0xd0000, 0xdffff, vram);
	mp.map_write(0xd0000, 0xdffff, vram_w);
	mp.map_read(0xff800, 0xfffff, rom + 0xff800);
}

void m90_machine::reset()
{
	reset_scheduler();

	memset(m_ram, 0, sizeof(m_ram));
	memset(m_sound_ram, 0, sizeof(m_sound_ram));
	m_bank = 0;
	map_main();
	m_soundlatch = m_soundlatch_pending = 0;
	m_ym_irq = 0;
	m_sample_addr = 0;
	m_sample_play = 0;
	m_frame = 0;

	m_video->reset();
	m_maincpu->device_reset();
	m_soundcpu->device_reset();
	m_sound->reset();
	update_sound_irq();
	update_cpu_rates();

	m_line = 0;
	m_scan_timer->adjust(attotime::zero, 0, attotime(LINE_PS));
	m_sample_timer->adjust(attotime(SAMPLE_PS), 0, attotime(SAMPLE_PS));
}

void m90_machine::set_cpu_speed(int percent)
{
	m_cpu_speed = percent < 50 ? 50 : percent > 400 ? 400 : percent;
	if (m_maincpu)
		update_cpu_rates();
}

void m90_machine::update_cpu_rates()
{
	m_slot[0].cycles_per_sec = m_maincpu->execute_clocks_to_cycles(MAIN_CLOCK) * m_cpu_speed / 100;
	m_slot[1].cycles_per_sec = SOUND_CLOCK;
}

// the V35's clock divider (PCK) is set by its program
void m90_machine::cpu_rate(int slot)
{
	if (slot == 0)
		m_slot[0].cycles_per_sec = m_maincpu->execute_clocks_to_cycles(MAIN_CLOCK) * m_cpu_speed / 100;
}

uint32_t m90_machine::main_pc() const { return m_maincpu->pc(); }
uint32_t m90_machine::sound_pc() const { return m_soundcpu->pc(); }

// ---------------------------------------------------------------- video timing

void m90_machine::run_frame()
{
	// from just after the start of line 248 to the start of the next line 248
	const s64 lines_to_vblank = (VBLANK_LINE - m_line + VTOTAL) % VTOTAL;
	attotime target(m_scan_timer->expire.ps + lines_to_vblank * LINE_PS);
	run_until(target);
	m_sound->update();
	m_frame++;
}

void m90_machine::scanline(s32)
{
	const int line = m_line;
	m_line = (m_line + 1) % VTOTAL;

	if (m_line_hook)
		m_line_hook(line);

	const int first = first_line(m_game);
	const int h = m_game->vis_h;
	if (line >= first && line < first + h)
	{
		m90_video::line_regs r;
		m_video->capture(r);
		if (m_line_sink)
			m_line_sink(line - first, &r);
		else
		{
			u32 buf[384];
			m_video->render_line(r, m_game->vis_y0 + line - first, buf);
			if (m_line_cb)
				m_line_cb(line - first, buf);
		}
	}

	// vertical blank on INTP0, from the line after the picture to its first line
	if (line == first + h || (first + h >= VTOTAL && line == 0))
		m_maincpu->set_input_line(NEC_INPUT_LINE_INTP0, ASSERT_LINE);
	else if (line == first)
		m_maincpu->set_input_line(NEC_INPUT_LINE_INTP0, CLEAR_LINE);
}

void m90_machine::render_line(const void *regs, int y, uint32_t *out)
{
	m_video->render_line(*static_cast<const m90_video::line_regs *>(regs), m_game->vis_y0 + y, out);
}

void m90_machine::set_video_sync(void (*fn)(void *), void *ctx)
{
	m_video->set_sync(fn, ctx);
	m_video_synced = fn != nullptr;
	if (m_maincpu)
		map_main();
}

// ---------------------------------------------------------------- main CPU memory and I/O

u16 m90_machine::main_read(void *ctx, offs_t a, u16)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	if (a >= 0xe0000 && a < 0xe0400)
		return m.m_video->palette_r((a - 0xe0000) >> 1);
	return 0;
}

void m90_machine::main_write(void *ctx, offs_t a, u16 d, u16 mask)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	if (a >= 0xd0000 && a < 0xe0000)
		m.m_video->vram_w((a - 0xd0000) >> 1, d, mask);
	else if (a >= 0xe0000 && a < 0xe0400)
		m.m_video->palette_w((a - 0xe0000) >> 1, d, mask);
}

u16 m90_machine::main_io_read(void *ctx, offs_t a, u16)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	switch (a & 0xfe)
	{
	case 0x00: return u16(~(m.m_player[0] | (m.m_player[1] << 8)));
	// start1, start2, coin1, coin2, service1, service (active low), unused, vblank (0)
	case 0x02: return u16((~m.m_system & 0x3f) | 0x40);
	case 0x04: return m.m_dsw;
	case 0x06: return u16(~(m.m_player[2] | (m.m_player[3] << 8)));
	default: return 0xffff;
	}
}

void m90_machine::main_io_write(void *ctx, offs_t a, u16 d, u16 mask)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	a &= 0xfe;
	if (a >= 0x80 && a < 0x90)
	{
		m.m_video->ctrl_w((a - 0x80) >> 1, d, mask);
		return;
	}
	switch (a)
	{
	case 0x00:
		if (mask & 0x00ff)
			m.defer([&m, d]() {
				m.m_soundlatch = u8(d);
				m.m_soundlatch_pending = 1;
				m.update_sound_irq();
			});
		break;
	case 0x04:
		if ((mask & 0x00ff) && m.m_game->quizf1)
		{
			m.m_bank = u8(d & 0x0f);
			m.map_main();
		}
		break;
	default:
		break;                       // 0x02: coin counters
	}
}

// ---------------------------------------------------------------- sound board

void m90_machine::set_ym_irq(bool on)
{
	m_ym_irq = on ? 1 : 0;
	update_sound_irq();
}

// IM 0: the RST buffer puts RST 18h for the sound latch, RST 28h for the YM2151,
// RST 08h for both on the bus
void m90_machine::update_sound_irq()
{
	u8 v = 0xff;
	if (m_soundlatch_pending) v &= ~0x20;
	if (m_ym_irq) v &= ~0x10;
	m_soundcpu->set_irq_vector(v);
	m_soundcpu->set_input_line(0, (m_soundlatch_pending || m_ym_irq) ? ASSERT_LINE : CLEAR_LINE);
}

void m90_machine::sample_tick(s32)
{
	if (!m_game->m99)
	{
		m_soundcpu->set_input_line(INPUT_LINE_NMI, ASSERT_LINE);   // the NMI routine plays the samples
		return;
	}
	// M99: the board plays a sample by itself, up to a 00 byte
	if (!m_sample_play)
		return;
	const u8 s = m_region[M90_REG_SAMPLES][m_sample_addr & m_sample_mask];
	if (s == 0)
		m_sample_play = 0;
	else
	{
		m_sound->dac_write(s);
		m_sample_addr = (m_sample_addr + 1) & 0x3ffff;
	}
}

u8 m90_machine::sound_read(void *ctx, u16 a)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	if (a >= 0xf000)
		return m.m_sound_ram[a & 0xfff];
	return m.m_region[M90_REG_SOUNDCPU][a];
}

void m90_machine::sound_write(void *ctx, u16 a, u8 d)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	if (a >= 0xf000)
		m.m_sound_ram[a & 0xfff] = d;
}

u8 m90_machine::sound_in(void *ctx, u16 port)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	port &= 0xff;
	if (m.m_game->m99)
	{
		switch (port)
		{
		case 0x40: case 0x41: return m.m_sound->ym_read(port);
		case 0x42: return m.m_soundlatch;
		default: return 0xff;
		}
	}
	switch (port)
	{
	case 0x00: case 0x01: return m.m_sound->ym_read(port);
	case 0x80: return m.m_soundlatch;          // separate acknowledge: reading does not clear it
	case 0x84: return m.m_region[M90_REG_SAMPLES][m.m_sample_addr & m.m_sample_mask];
	default: return 0xff;
	}
}

void m90_machine::sound_out(void *ctx, u16 port, u8 d)
{
	m90_machine &m = *static_cast<m90_machine *>(ctx);
	port &= 0xff;
	if (m.m_game->m99)
	{
		switch (port)
		{
		case 0x40: case 0x41: m.m_sound->ym_write(port, d); break;
		case 0x42: m.m_soundlatch_pending = 0; m.update_sound_irq(); break;
		case 0x00: m.m_sample_addr = (m.m_sample_addr & ~0xfffu) | u32(d) << 4; break;
		case 0x01: m.m_sample_addr = (m.m_sample_addr & 0xfff) | u32(d & 0x3f) << 12; break;
		case 0x06: m.m_sample_play = 1; break;
		default: break;
		}
		return;
	}
	switch (port)
	{
	case 0x00: case 0x01: m.m_sound->ym_write(port, d); break;
	case 0x80: m.m_sample_addr = (m.m_sample_addr & ~0x1fffu) | u32(d) << 5; break;
	case 0x81: m.m_sample_addr = (m.m_sample_addr & 0x1fff) | u32(d & 0x1f) << 13; break;
	case 0x82:
		m.m_sound->dac_write(d);
		m.m_sample_addr = (m.m_sample_addr + 1) & 0x3ffff;
		break;
	case 0x83: m.m_soundlatch_pending = 0; m.update_sound_irq(); break;
	default: break;
	}
}

// ---------------------------------------------------------------- audio out

int m90_machine::audio_read(int16_t *dst, int max_frames) { return m_sound->read(dst, max_frames); }

// ---------------------------------------------------------------- hiscores

size_t m90_machine::hi_size() const
{
	size_t n = 0;
	if (m_hi)
		for (int i = 0; i < m_hi->count; i++)
			n += m_hi->rows[i].size;
	return n;
}

bool m90_machine::hi_ready()
{
	if (!m_hi || !m_maincpu)
		return false;
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	for (int i = 0; i < m_hi->count; i++)
	{
		const m92_hiscore_row &r = m_hi->rows[i];
		if (mp.read_byte(r.addr) != r.first || mp.read_byte(r.addr + r.size - 1) != r.last)
			return false;
	}
	return true;
}

void m90_machine::hi_get(uint8_t *dst)
{
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	for (int i = 0; m_hi && i < m_hi->count; i++)
		for (u32 a = 0; a < m_hi->rows[i].size; a++)
			*dst++ = mp.read_byte(m_hi->rows[i].addr + a);
}

void m90_machine::hi_put(const uint8_t *src)
{
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	for (int i = 0; m_hi && i < m_hi->count; i++)
		for (u32 a = 0; a < m_hi->rows[i].size; a++)
			mp.write_byte(m_hi->rows[i].addr + a, *src++);
}

// ---------------------------------------------------------------- save states

size_t m90_machine::state_size() const
{
	return 16 + m_save.total() + 8192;
}

bool m90_machine::save_state(std::vector<uint8_t> &out)
{
	if (!m_game)
		return false;
	std::vector<u8> opm;
	m_sound->save_opm(opm);
	const size_t body = m_save.total();
	out.resize(16 + body + 4 + opm.size());
	u32 hdr[4] = { STATE_MAGIC, STATE_VERSION, u32(body), u32(opm.size()) };
	memcpy(out.data(), hdr, 16);
	m_save.save(out.data() + 16);
	memcpy(out.data() + 16 + body, "OPM!", 4);
	memcpy(out.data() + 20 + body, opm.data(), opm.size());
	return true;
}

bool m90_machine::load_state(const std::vector<uint8_t> &in)
{
	if (!m_game || in.size() < 16)
		return false;
	u32 hdr[4];
	memcpy(hdr, in.data(), 16);
	if (hdr[0] != STATE_MAGIC || hdr[1] != STATE_VERSION || hdr[2] != m_save.total()
		|| in.size() != 20 + size_t(hdr[2]) + hdr[3])
		return false;
	m_video->sync();
	m_save.load(in.data() + 16);
	m_soundcpu->after_load();
	update_sound_irq();
	std::vector<u8> opm(in.begin() + 20 + hdr[2], in.end());
	m_sound->load_opm(opm);
	map_main();                      // bank
	update_cpu_rates();
	return true;
}
