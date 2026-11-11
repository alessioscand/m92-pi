// m92-pi - Irem M92 arcade machine (see m92.h)
//
// Memory maps, I/O, interrupts and timings from MAME's src/mame/irem/m92.cpp
// (BSD-3-Clause, Bryan McPhail); 8259 from MAME's pic8259.cpp (BSD-3-Clause, Wilbert Pol).
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m92.h"
#include "m92_video.h"
#include "m92_sound.h"
#include "nec/nec.h"
#include "nec/v25.h"
#include "irem_cpu.h"
#include <cstring>

#include "m92_games.inc"
#include "m92_hiscore.inc"

static constexpr u32 MAIN_CLOCK = 18000000 / 2;        // V33
static constexpr u32 SOUND_CLOCK = 14318181;            // V35 (divided by its PCK)
static constexpr int HTOTAL = 424, VTOTAL = 262;
// pixel clock 26.666666 MHz / 4
static constexpr s64 LINE_PS = s64(HTOTAL * 1e12 / (26666666.0 / 4) + 0.5);
static constexpr s64 FRAME_PS = LINE_PS * VTOTAL;
static constexpr s64 PIXEL_PS = s64(1e12 / (26666666.0 / 4) + 0.5);

static const u32 STATE_MAGIC = 0x3239364d;              // "M692" little-endian
static const u32 STATE_VERSION = 1;

// ---------------------------------------------------------------- game list

const m92_game *m92_find_game(const char *name)
{
	for (const auto &g : m92_games)
		if (!strcmp(g.name, name))
			return &g;
	return nullptr;
}

const m92_hiscore *m92_find_hiscore(const char *name)
{
	for (const auto &h : m92_hiscores)
		if (!strcmp(h.name, name))
			return &h;
	return nullptr;
}

int m92_game_count() { return int(sizeof(m92_games) / sizeof(m92_games[0])); }
const m92_game *m92_game_at(int i) { return (i >= 0 && i < m92_game_count()) ? &m92_games[i] : nullptr; }

// ---------------------------------------------------------------- 8259 (uPD71059)

enum { PIC_ICW1, PIC_ICW2, PIC_ICW3, PIC_ICW4, PIC_READY };

void m92_machine::pic8259::reset()
{
	state = PIC_READY;
	isr = irr = irq_lines = prio = imr = ocw3 = 0;
	level_trig_mode = vector_size = cascade = icw4_needed = base = 0;
	slave = nested = mode = auto_eoi = is_x86 = 0;
	vector_addr_low = vector_addr_high = 0;
	current_level = -1;
}

void m92_machine::pic8259::update()
{
	for (int n = 0, irq = prio; n < 8; n++, irq = (irq + 1) & 7)
	{
		const u8 mask = 1 << irq;
		if (isr & mask)              // in service: lower priorities wait
			break;
		if (state == PIC_READY && (irr & mask) && !(imr & mask))
		{
			current_level = s8(irq);
			m->main_cpu_irq(1);
			return;
		}
	}
	current_level = -1;
	m->main_cpu_irq(0);
}

void m92_machine::pic8259::set_irq_line(int irq, int st)
{
	const u8 mask = 1 << irq;
	if (st && !(irq_lines & mask))
	{
		irr |= mask;
		irq_lines |= mask;
	}
	else if (!st && (irq_lines & mask))
	{
		irq_lines &= ~mask;
		irr &= ~mask;
	}
	update();
}

u8 m92_machine::pic8259::acknowledge()
{
	// x86 mode (the V33 reads one vector byte)
	if (current_level != -1)
	{
		const u8 mask = 1 << current_level;
		if (!level_trig_mode)
			irr &= ~mask;
		if (!auto_eoi)
			isr |= mask;
		const u8 v = u8(current_level + base);
		update();
		return v;
	}
	return u8(base + 7);
}

u8 m92_machine::pic8259::read(int offset)
{
	u8 data = 0;
	if (offset == 0)
	{
		if (ocw3 & 0x04)             // polling
		{
			if (current_level != -1)
			{
				data = u8(0x80 | current_level);
				if (!level_trig_mode)
					irr &= ~(1 << current_level);
				if (!auto_eoi)
					isr |= 1 << current_level;
				update();
			}
			ocw3 &= 0xfb;
		}
		else
			data = (ocw3 & 0x01) ? (isr & ~imr) : irr;
	}
	else
		data = imr;
	return data;
}

void m92_machine::pic8259::write(int offset, u8 data)
{
	if (offset == 0)
	{
		if (data & 0x10)             // ICW1
		{
			imr = isr = slave = 0;
			level_trig_mode = (data & 0x08) ? 1 : 0;
			vector_size = (data & 0x04) ? 1 : 0;
			cascade = (data & 0x02) ? 0 : 1;
			icw4_needed = (data & 0x01) ? 1 : 0;
			vector_addr_low = data & 0xe0;
			state = PIC_ICW2;
			current_level = -1;
			irr = level_trig_mode ? irq_lines : 0;
			m->main_cpu_irq(0);
		}
		else if (state == PIC_READY)
		{
			if ((data & 0x98) == 0x08)   // OCW3
			{
				if (BIT(data, 1))
					ocw3 = (ocw3 & 0xfe) | (data & 0x01);
				if (BIT(data, 2))
					ocw3 |= 0x04;
				if (BIT(data, 6))
					ocw3 = (ocw3 & 0xdf) | (data & 0x20);
			}
			else if ((data & 0x18) == 0x00)   // OCW2
			{
				int n = data & 7;
				u8 mask = 1 << n;
				switch (data & 0xe0)
				{
				case 0x00: prio = 0; break;
				case 0x20:
					for (n = 0, mask = 1 << prio; n < 8; n++, mask = (mask << 1) | (mask >> 7))
						if (isr & mask) { isr &= ~mask; break; }
					break;
				case 0x40: break;
				case 0x60: if (isr & mask) isr &= ~mask; break;
				case 0x80: prio = (prio + 1) & 7; break;
				case 0xa0:
					for (n = 0, mask = 1 << prio; n < 8; n++, mask = (mask << 1) | (mask >> 7))
						if (isr & mask) { isr &= ~mask; prio = (prio + 1) & 7; break; }
					break;
				case 0xc0: prio = (n + 1) & 7; break;
				case 0xe0: if (isr & mask) { isr &= ~mask; prio = (n + 1) & 7; } break;
				}
			}
		}
	}
	else
	{
		switch (state)
		{
		case PIC_ICW2:
			base = data & 0xf8;
			vector_addr_high = data;
			state = cascade ? PIC_ICW3 : (icw4_needed ? PIC_ICW4 : PIC_READY);
			break;
		case PIC_ICW3:
			slave = data;
			state = icw4_needed ? PIC_ICW4 : PIC_READY;
			break;
		case PIC_ICW4:
			nested = (data & 0x10) ? 1 : 0;
			mode = (data >> 2) & 3;
			auto_eoi = (data & 0x02) ? 1 : 0;
			is_x86 = (data & 0x01) ? 1 : 0;
			state = PIC_READY;
			break;
		case PIC_READY:
			imr = data;              // OCW1
			break;
		default:
			break;
		}
	}
	update();
}

void m92_machine::main_cpu_irq(int st)
{
	m_maincpu->set_input_line(0, st ? ASSERT_LINE : CLEAR_LINE);
}

// ---------------------------------------------------------------- construction

m92_machine::m92_machine()
{
	m_pic.m = this;
	m_video = std::make_unique<m92_video>();
	m_sound = std::make_unique<m92_sound>(*this);
}

m92_machine::~m92_machine()
{
}

// ---------------------------------------------------------------- loading

bool m92_machine::load(const m92_game *game, const rom_fetch &fetch, std::string &missing)
{
	m_game = game;
	m_hi = m92_find_hiscore(game->name);
	for (int r = 0; r < M92_REG_COUNT; r++)
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

	m_video->decode_gfx(m_region[M92_REG_GFX1].data(), m_region[M92_REG_GFX1].size(),
			m_region[M92_REG_GFX2].data(), m_region[M92_REG_GFX2].size(), game->gfx_psoldier);

	// devices; everything registered with m_save in a fixed order for save states
	delete_timers();
	m_save.items.clear();

	m_maincpu = std::make_unique<nec_common_device>(MAIN_CLOCK);
	m_soundcpu = std::make_unique<v35_device>(SOUND_CLOCK);
	for (cpu_device *c : { (cpu_device *)m_maincpu.get(), (cpu_device *)m_soundcpu.get() })
	{
		c->m_sched = this;
		c->m_save = &m_save;
	}
	m_maincpu->m_irq_ack = [this](int) { return int(m_pic.acknowledge()); };
	m_soundcpu->set_decryption_table(game->sound_decrypt);

	address_space &mp = m_maincpu->space(AS_PROGRAM);
	mp.configure(20, 16, main_read, main_write, this);
	m_maincpu->space(AS_IO).configure(16, 16, main_io_read, main_io_write, this);
	address_space &sp = m_soundcpu->space(AS_PROGRAM);
	sp.configure(20, 16, sound_read, sound_write, this);
	m_soundcpu->space(AS_IO).configure(16, 16, sound_io_read, sound_io_write, this);

	map_main();
	u8 *srom = m_region[M92_REG_SOUNDCPU].data();
	sp.map_read(0x00000, 0x1ffff, srom);
	sp.map_read(0xa0000, 0xa3fff, m_sound_ram);
	sp.map_write(0xa0000, 0xa3fff, m_sound_ram);

	m_maincpu->device_start();
	m_soundcpu->device_start();
	m_slot[0].cpu = m_maincpu.get();
	m_slot[1].cpu = m_soundcpu.get();
	irem_machine::register_save(m_save);

	m_scan_timer = new_timer(m_save, [this](s32 p) { scanline(p); });
	m_dma_timer = new_timer(m_save, [this](s32 p) { dma_done(p); });

	m_sound->start(m_region[M92_REG_IREM].data(), m_region[M92_REG_IREM].size(), m_save);
	m_video->register_save(m_save);

	m_save.add(&m_now, sizeof(m_now));
	m_save.add(&m_pic, sizeof(m_pic));        // plain bytes; the pointers inside are put back on load
	m_save.add(m_ram, sizeof(m_ram));
	m_save.add(m_sound_ram, sizeof(m_sound_ram));
	m_save.add(m_eeprom, sizeof(m_eeprom));
	m_save.add(&m_bank, sizeof(m_bank));
	m_save.add(&m_soundlatch, sizeof(m_soundlatch));
	m_save.add(&m_soundlatch_pending, sizeof(m_soundlatch_pending));
	m_save.add(&m_soundlatch2, sizeof(m_soundlatch2));
	m_save.add(&m_soundlatch2_pending, sizeof(m_soundlatch2_pending));
	m_save.add(&m_sound_in_reset, sizeof(m_sound_in_reset));
	m_save.add(&m_raster_irq_position, sizeof(m_raster_irq_position));
	m_save.add(&m_line, sizeof(m_line));
	m_save.add(&m_dma_busy, sizeof(m_dma_busy));
	m_save.add(&m_frame, sizeof(m_frame));

	memset(m_eeprom, 0xff, sizeof(m_eeprom));
	if (!m_region[M92_REG_EEPROM].empty())   // Major Title 2: factory contents
		memcpy(m_eeprom, m_region[M92_REG_EEPROM].data(), std::min(sizeof(m_eeprom), m_region[M92_REG_EEPROM].size()));
	reset();
	return true;
}

void m92_machine::map_main()
{
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	u8 *rom = m_region[M92_REG_MAINCPU].data();
	u8 *vram = m_video->vram_bytes();
	// multicore: VRAM writes go through main_write, which waits for the drawing core
	u8 *vram_w = m_video_synced ? nullptr : vram;

	for (offs_t a = 0; a < 0x100000; a += 0x800)
	{
		mp.map_read(a, a, nullptr);
		mp.map_write(a, a, nullptr);
	}
	switch (m_game->map)
	{
	case M92_MAP_LETHALTH:
		mp.map_read(0x00000, 0x7ffff, rom);
		mp.map_read(0x80000, 0x8ffff, vram);
		mp.map_write(0x80000, 0x8ffff, vram_w);
		break;
	case M92_MAP_M92_BANKED:
	case M92_MAP_MAJTITL2:
		mp.map_read(0x00000, 0x9ffff, rom);
		mp.map_read(0xa0000, 0xbffff, rom + 0x80000 + m_bank * 0x20000);
		mp.map_read(0xc0000, 0xcffff, rom);
		mp.map_read(0xd0000, 0xdffff, vram);
		mp.map_write(0xd0000, 0xdffff, vram_w);
		break;
	default:
		mp.map_read(0x00000, 0xbffff, rom);
		mp.map_read(0xc0000, 0xcffff, rom);   // mirror, used by In The Hunt as protection
		mp.map_read(0xd0000, 0xdffff, vram);
		mp.map_write(0xd0000, 0xdffff, vram_w);
		break;
	}
	mp.map_read(0xe0000, 0xeffff, m_ram);
	mp.map_write(0xe0000, 0xeffff, m_ram);
}

void m92_machine::reset()
{
	reset_scheduler();

	memset(m_ram, 0, sizeof(m_ram));
	memset(m_sound_ram, 0, sizeof(m_sound_ram));
	m_bank = 0;
	map_main();
	m_soundlatch = m_soundlatch_pending = 0;
	m_soundlatch2 = m_soundlatch2_pending = 0;
	m_sound_in_reset = 0;
	m_raster_irq_position = -1;
	m_dma_busy = 1;
	m_frame = 0;

	m_pic.reset();
	m_video->reset();
	m_maincpu->device_reset();
	m_soundcpu->device_reset();
	m_sound->reset();
	update_cpu_rates();

	// scanline 0 now; MAME's scanline timer fires at the start of every line
	m_line = 0;
	m_scan_timer->adjust(attotime::zero, 0, attotime(LINE_PS));
}

void m92_machine::set_cpu_speed(int percent)
{
	m_cpu_speed = percent < 50 ? 50 : percent > 400 ? 400 : percent;
	if (m_maincpu)
		update_cpu_rates();
}

void m92_machine::update_cpu_rates()
{
	m_slot[0].cycles_per_sec = u64(MAIN_CLOCK) * m_cpu_speed / 100;
	m_slot[1].cycles_per_sec = m_soundcpu->execute_clocks_to_cycles(SOUND_CLOCK);
}

// the V35's clock divider (PCK) is set by its program
void m92_machine::cpu_rate(int slot)
{
	if (slot == 1)
		m_slot[1].cycles_per_sec = m_soundcpu->execute_clocks_to_cycles(SOUND_CLOCK);
}

uint32_t m92_machine::main_pc() const { return m_maincpu->pc(); }
uint32_t m92_machine::sound_pc() const { return m_soundcpu->pc(); }

void m92_machine::run_frame()
{
	// from just after the start of line 248 (vertical blank) to the start of the next line 248
	const s64 lines_to_vblank = (VBLANK_LINE - m_line + VTOTAL) % VTOTAL;
	attotime target(m_scan_timer->expire.ps + lines_to_vblank * LINE_PS);
	run_until(target);
	m_sound->update();
	m_frame++;
}

// ---------------------------------------------------------------- video timing

void m92_machine::scanline(s32)
{
	const int line = m_line;
	m_line = (m_line + 1) % VTOTAL;

	if (m_line_hook)
		m_line_hook(line);

	// draw the line as it starts, with the state the raster interrupt handler has not
	// changed yet (MAME: update_partial before the interrupt)
	if (line >= m92_video::VIS_Y0 && line < m92_video::VIS_Y0 + m92_video::VIS_H)
	{
		m92_video::line_regs r;
		m_video->capture(r);
		if (m_line_sink)
			m_line_sink(line - m92_video::VIS_Y0, &r);
		else
		{
			u32 buf[m92_video::VIS_W];
			m_video->render_line(r, line, buf);
			if (m_line_cb)
				m_line_cb(line - m92_video::VIS_Y0, buf);
		}
	}

	m_pic.set_irq_line(2, line == m_raster_irq_position);
	m_pic.set_irq_line(0, line == VBLANK_LINE);
}

void m92_machine::dma_done(s32)
{
	m_dma_busy = 1;
	m_pic.set_irq_line(1, 1);
}

void m92_machine::render_line(const void *regs, int y, uint32_t *out)
{
	m_video->render_line(*static_cast<const line_regs *>(regs), y + m92_video::VIS_Y0, out);
}

void m92_machine::set_video_sync(void (*fn)(void *), void *ctx)
{
	m_video->set_sync(fn, ctx);
	m_video_synced = fn != nullptr;
	if (m_maincpu)
		map_main();
}

// ---------------------------------------------------------------- latches

void m92_machine::set_soundlatch_pending(bool p)
{
	if (m_soundlatch_pending != p)
	{
		m_soundlatch_pending = p;
		m_soundcpu->set_input_line(NEC_INPUT_LINE_INTP1, p ? ASSERT_LINE : CLEAR_LINE);
	}
}

void m92_machine::set_soundlatch2_pending(bool p)
{
	if (m_soundlatch2_pending != p)
	{
		m_soundlatch2_pending = p;
		m_pic.set_irq_line(3, p);
	}
}

void m92_machine::soundlatch_write(u8 data)
{
	defer([this, data]() { m_soundlatch = data; set_soundlatch_pending(true); });
}

void m92_machine::soundlatch2_write(u8 data)
{
	defer([this, data]() { m_soundlatch2 = data; set_soundlatch2_pending(true); });
}

void m92_machine::sound_reset_w(u16 data)
{
	if (data)
		m_sound_in_reset = 0;
	else if (!m_sound_in_reset)
	{
		m_sound_in_reset = 1;
		m_soundcpu->device_reset();
	}
}

// ---------------------------------------------------------------- main CPU memory

u16 m92_machine::main_read(void *ctx, offs_t a, u16)
{
	m92_machine &m = *static_cast<m92_machine *>(ctx);
	if (a >= 0xf8000 && a < 0xf8800)
		return m.m_video->spriteram_r((a - 0xf8000) >> 1);
	if (a >= 0xf8800 && a < 0xf9000)
		return m.m_video->paletteram_r((a - 0xf8800) >> 1);
	if (a >= 0xffff0)
	{
		const u8 *rom = m.m_region[M92_REG_MAINCPU].data() + 0x7fff0 + (a & 0xe);
		return rom[0] | (rom[1] << 8);
	}
	if (m.m_game->eeprom && a >= 0xf0000 && a < 0xf4000)
		return m.m_eeprom[((a - 0xf0000) >> 1) & 0x1fff];
	return 0;
}

void m92_machine::main_write(void *ctx, offs_t a, u16 d, u16 mask)
{
	m92_machine &m = *static_cast<m92_machine *>(ctx);
	const offs_t vbase = m.m_game->map == M92_MAP_LETHALTH ? 0x80000 : 0xd0000;
	if (a >= vbase && a < vbase + 0x10000)
		m.m_video->vram_w((a - vbase) >> 1, d, mask);
	else if (a >= 0xf8000 && a < 0xf8800)
		m.m_video->spriteram_w((a - 0xf8000) >> 1, d, mask);
	else if (a >= 0xf8800 && a < 0xf9000)
		m.m_video->paletteram_w((a - 0xf8800) >> 1, d, mask);
	else if (a >= 0xf9000 && a < 0xf9010)
	{
		const int clocks = m.m_video->dmacontrol_w((a - 0xf9000) >> 1, d, mask);
		if (clocks)
		{
			// assume one word per 2 pixel clocks... as MAME: pixel period / 2 per clock
			m.m_dma_timer->adjust(attotime(PIXEL_PS / 2 * clocks));
			m.m_dma_busy = 0;
			m.m_pic.set_irq_line(1, 0);
		}
	}
	else if (a >= 0xf9800 && a < 0xf9802)
		m.m_video->videocontrol_w(d, mask);
	else if (m.m_game->eeprom && a >= 0xf0000 && a < 0xf4000 && (mask & 0xff))
		m.m_eeprom[((a - 0xf0000) >> 1) & 0x1fff] = u8(d);
}

u16 m92_machine::main_io_read(void *ctx, offs_t a, u16 mask)
{
	m92_machine &m = *static_cast<m92_machine *>(ctx);
	a &= 0xfe;
	switch (a)
	{
	case 0x00: return u16(~(m.m_player[0] | (m.m_player[1] << 8)));
	case 0x02:
	{
		// start1, start2, coin1, coin2, service1, test, (unused), DMA busy (active high); SW3
		const u8 lo = u8((~m.m_system & 0x7f) | (m.m_dma_busy ? 0x80 : 0));
		return u16(lo | (m.m_dsw3 << 8));
	}
	case 0x04: return m.m_dsw;
	case 0x06: return u16(~(m.m_player[2] | (m.m_player[3] << 8)));
	case 0x08:
		if (mask & 0x00ff)
		{
			// answer from the sound CPU; reading it clears the request
			const u8 v = m.m_soundlatch2;
			m.set_soundlatch2_pending(false);
			return v;
		}
		return 0;
	case 0x40: return m.m_pic.read(0);
	case 0x42: return m.m_pic.read(1);
	default: return 0;
	}
}

void m92_machine::main_io_write(void *ctx, offs_t a, u16 d, u16 mask)
{
	m92_machine &m = *static_cast<m92_machine *>(ctx);
	a &= 0xfe;
	if (a >= 0x80 && a < 0x98)
	{
		const int layer = (a - 0x80) >> 3;
		m.m_video->pf_control_w(layer, ((a - 0x80) >> 1) & 3, d, mask);
		return;
	}
	if (a >= 0x98 && a < 0xa0)
	{
		const int off = (a - 0x98) >> 1;
		m.m_video->master_control_w(off, d, mask);
		if (off == 3)
		{
			m.m_raster_irq_position = (m.m_video->master_control(3) & 0x1ff) - 128;
			m.m_pic.set_irq_line(2, 0);
		}
		return;
	}
	switch (a)
	{
	case 0x00:
		if (mask & 0x00ff)
			m.soundlatch_write(u8(d));
		break;
	case 0x02:
		break;                       // coin counters
	case 0x20:
		if ((mask & 0x00ff) && (m.m_game->map == M92_MAP_M92_BANKED || m.m_game->map == M92_MAP_MAJTITL2))
		{
			m.m_bank = (d & 0x06) >> 1;
			m.m_maincpu->space(AS_PROGRAM).map_read(0xa0000, 0xbffff,
					m.m_region[M92_REG_MAINCPU].data() + 0x80000 + m.m_bank * 0x20000);
		}
		break;
	case 0x40:
		if (mask & 0x00ff) m.m_pic.write(0, u8(d));
		break;
	case 0x42:
		if (mask & 0x00ff) m.m_pic.write(1, u8(d));
		break;
	case 0xc0:
		m.sound_reset_w(d);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------- sound CPU memory

u16 m92_machine::sound_read(void *ctx, offs_t a, u16 mask)
{
	m92_machine &m = *static_cast<m92_machine *>(ctx);
	if (a >= 0xa8000 && a < 0xa8040)
		return (mask & 0xff) ? m.m_sound->ga20_read((a - 0xa8000) >> 1) : 0;
	if (a >= 0xa8040 && a < 0xa8044)
		return (mask & 0xff) ? m.m_sound->ym_read((a - 0xa8040) >> 1) : 0;
	if (a == 0xa8044)
		return m.m_soundlatch;       // separate acknowledge: reading does not clear it
	if (a >= 0xffff0)
	{
		const u8 *rom = m.m_region[M92_REG_SOUNDCPU].data() + 0x1fff0 + (a & 0xe);
		return rom[0] | (rom[1] << 8);
	}
	return 0;
}

void m92_machine::sound_write(void *ctx, offs_t a, u16 d, u16 mask)
{
	m92_machine &m = *static_cast<m92_machine *>(ctx);
	if (!(mask & 0xff))
		return;                      // everything here is on the low byte
	if (a >= 0xa8000 && a < 0xa8040)
		m.m_sound->ga20_write((a - 0xa8000) >> 1, u8(d));
	else if (a >= 0xa8040 && a < 0xa8044)
		m.m_sound->ym_write((a - 0xa8040) >> 1, u8(d));
	else if (a == 0xa8044)
		m.set_soundlatch_pending(false);   // acknowledge
	else if (a == 0xa8046)
		m.soundlatch2_write(u8(d));
}

u16 m92_machine::sound_io_read(void *, offs_t, u16) { return 0; }
void m92_machine::sound_io_write(void *, offs_t, u16, u16) {}

// ---------------------------------------------------------------- audio out

int m92_machine::audio_available() const { return m_sound->available(); }
int m92_machine::audio_read(int16_t *dst, int max_frames) { return m_sound->read(dst, max_frames); }

// ---------------------------------------------------------------- hiscores

size_t m92_machine::hi_size() const
{
	size_t n = 0;
	if (m_hi)
		for (int i = 0; i < m_hi->count; i++)
			n += m_hi->rows[i].size;
	return n;
}

bool m92_machine::hi_ready()
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

void m92_machine::hi_get(uint8_t *dst)
{
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	for (int i = 0; m_hi && i < m_hi->count; i++)
		for (u32 a = 0; a < m_hi->rows[i].size; a++)
			*dst++ = mp.read_byte(m_hi->rows[i].addr + a);
}

void m92_machine::hi_put(const uint8_t *src)
{
	// through the address space: VRAM rows (R-Type Leo) go through the video's sync
	address_space &mp = m_maincpu->space(AS_PROGRAM);
	for (int i = 0; m_hi && i < m_hi->count; i++)
		for (u32 a = 0; a < m_hi->rows[i].size; a++)
			mp.write_byte(m_hi->rows[i].addr + a, *src++);
}

// ---------------------------------------------------------------- save states

size_t m92_machine::state_size() const
{
	return 16 + m_save.total() + 8192;
}

bool m92_machine::save_state(std::vector<uint8_t> &out)
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

bool m92_machine::load_state(const std::vector<uint8_t> &in)
{
	if (!m_game || in.size() < 16)
		return false;
	u32 hdr[4];
	memcpy(hdr, in.data(), 16);
	if (hdr[0] != STATE_MAGIC || hdr[1] != STATE_VERSION || hdr[2] != m_save.total()
		|| in.size() != 20 + size_t(hdr[2]) + hdr[3])
		return false;
	m92_machine *self = this;
	m_video->sprites_changed();      // before the buffer changes under a line being drawn
	m_save.load(in.data() + 16);
	m_video->sprites_changed();
	m_pic.m = self;
	std::vector<u8> opm(in.begin() + 20 + hdr[2], in.end());
	m_sound->load_opm(opm);
	map_main();                      // bank
	update_cpu_rates();
	return true;
}
