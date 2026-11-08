// m92-pi - Irem M92 sound chips (see m92_sound.h)
//
// GA20 channel logic from MAME's iremga20.cpp (BSD-3-Clause, Acho A. Tang, R. Belmont,
// Valley Bell); YM2151 by ymfm (BSD-3-Clause, Aaron Giles). Levels as MAME's m92 driver
// mixes them: YM2151 left and right at 0.40 each, both GA20 outputs at 1.0, into one
// speaker.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m92_sound.h"
#include "m92.h"
#include "nec/v25.h"
#include <cmath>

m92_sound::m92_sound(m92_machine &mach)
	: m(mach)
	, m_opm(*this)
{
	memset(m_ga20_regs, 0, sizeof(m_ga20_regs));
	memset(m_ch, 0, sizeof(m_ch));
}

void m92_sound::start(const u8 *rom, size_t size, save_registry &save)
{
	m_ga20_rom = rom;
	// the GA20 sees a 1 MB space; smaller sample ROMs repeat in it
	u32 p = 1;
	while (p < size)
		p <<= 1;
	m_ga20_mask = p - 1;

	for (int i = 0; i < 2; i++)
	{
		emu_timer *t = new emu_timer;
		t->sched = &m;
		t->cb = [this, i](s32) { update(); m_engine->engine_timer_expired(i); };
		m.timers.push_back(t);
		save.add(&t->expire, sizeof(t->expire));
		save.add(&t->period, sizeof(t->period));
		save.add(&t->param, sizeof(t->param));
		m_opm_timer[i] = t;
	}
	save.add(&m_busy_end, sizeof(m_busy_end));
	save.add(m_ga20_regs, sizeof(m_ga20_regs));
	save.add(m_ch, sizeof(m_ch));
	save.add(&m_out_done, sizeof(m_out_done));
	save.add(&m_opm_done, sizeof(m_opm_done));
	save.add(&m_ga_done, sizeof(m_ga_done));
	save.add(&m_opm_prev, sizeof(m_opm_prev));
	save.add(&m_opm_cur, sizeof(m_opm_cur));
	save.add(&m_origin, sizeof(m_origin));
}

void m92_sound::reset()
{
	m_opm.reset();
	m_opm_timer[0]->reset();
	m_opm_timer[1]->reset();
	m_busy_end = attotime::zero;
	memset(m_ga20_regs, 0, sizeof(m_ga20_regs));
	memset(m_ch, 0, sizeof(m_ch));
	m_out_done = m_opm_done = m_ga_done = 0;
	m_opm_prev = m_opm_cur = 0;
	m_origin = m.current_time();
}

// ---------------------------------------------------------------- ymfm hooks

void m92_sound::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks)
{
	if (duration_in_clocks >= 0)
		m_opm_timer[tnum]->adjust(attotime(s64(double(duration_in_clocks) * 1e12 / CLOCK)));
	else
		m_opm_timer[tnum]->reset();
}

void m92_sound::ymfm_set_busy_end(uint32_t clocks)
{
	m_busy_end = m.current_time() + attotime(s64(double(clocks) * 1e12 / CLOCK));
}

bool m92_sound::ymfm_is_busy()
{
	return m.current_time() < m_busy_end;
}

void m92_sound::ymfm_update_irq(bool asserted)
{
	m.m_soundcpu->set_input_line(NEC_INPUT_LINE_INTP0, asserted ? ASSERT_LINE : CLEAR_LINE);
}

u8 m92_sound::ym_read(offs_t offset)
{
	return m_opm.read(offset & 1);
}

void m92_sound::ym_write(offs_t offset, u8 data)
{
	update();
	m_opm.write(offset & 1, data);
}

// ---------------------------------------------------------------- GA20

u8 m92_sound::ga20_read(offs_t offset)
{
	update();
	offset &= 0x1f;
	if ((offset & 7) == 7)           // voice status: bit 0 set while playing
		return u8(m_ch[offset >> 3].play);
	return 0;
}

void m92_sound::ga20_write(offs_t offset, u8 data)
{
	update();
	offset &= 0x1f;
	m_ga20_regs[offset] = data;
	const int ch = offset >> 3;

	// channel registers: 0,1 start address; 2,3 end address; 4 rate; 5 volume; 6 control
	switch (offset & 7)
	{
	case 4:
		m_ch[ch].rate = data;
		break;
	case 5:
		m_ch[ch].volume = (data * 256) / (data + 10);
		break;
	case 6:
		if (data & 2)                // key on
		{
			m_ch[ch].play = 1;
			m_ch[ch].pos = (m_ga20_regs[ch << 3 | 0] | m_ga20_regs[ch << 3 | 1] << 8) << 4;
			m_ch[ch].end = (m_ga20_regs[ch << 3 | 2] | m_ga20_regs[ch << 3 | 3] << 8) << 4;
			m_ch[ch].counter = 0x100;
		}
		else
			m_ch[ch].play = 0;
		break;
	}
}

s32 m92_sound::ga20_next()
{
	s32 out = 0;
	for (auto &ch : m_ch)
	{
		if (!ch.play)
			continue;
		const int sample = m_ga20_rom[ch.pos & m_ga20_mask];
		if (sample == 0x00)          // end marker
			ch.play = 0;
		else
		{
			out += (sample - 0x80) * s32(ch.volume);
			ch.counter--;
			if (ch.counter <= ch.rate)
			{
				ch.pos++;
				ch.counter = 0x100;
			}
		}
	}
	return out;
}

s32 m92_sound::opm_next()
{
	ymfm::ym2151::output_data o;
	m_opm.generate(&o);
	return (o.data[0] + o.data[1]) * 2 / 5;
}

// ---------------------------------------------------------------- mixing

void m92_sound::push(s32 v)
{
	if (v > 32767) v = 32767;
	if (v < -32768) v = -32768;
	if (m_wr - m_rd >= FIFO)
		m_rd++;                      // nobody is reading: drop the oldest
	const int i = int(m_wr % FIFO) * 2;
	m_fifo[i] = m_fifo[i + 1] = s16(v);
	m_wr++;
}

void m92_sound::update()
{
	const attotime now = m.current_time();
	if (now < m_origin)
		return;
	const unsigned __int128 elapsed = (unsigned __int128)(now.ps - m_origin.ps);
	const u64 target = u64(elapsed * OUT_RATE / 1000000000000ULL);

	constexpr double opm_rate = CLOCK / 64;
	constexpr double ga_rate = CLOCK / 4;
	while (m_out_done < target)
	{
		const u64 n = m_out_done;

		// YM2151: linear interpolation between its samples
		const double p = double(n) * opm_rate / OUT_RATE;
		const u64 i = u64(p);
		while (m_opm_done < i + 2)
		{
			m_opm_prev = m_opm_cur;
			m_opm_cur = opm_next();
			m_opm_done++;
		}
		const double frac = p - double(i);
		const double opm = m_opm_prev + (m_opm_cur - m_opm_prev) * frac;

		// GA20: average of its samples in this output period
		const u64 e = u64(double(n + 1) * ga_rate / OUT_RATE);
		s64 sum = 0;
		u32 cnt = 0;
		while (m_ga_done < e)
		{
			sum += ga20_next();
			m_ga_done++;
			cnt++;
		}
		const double ga = cnt ? double(sum) / cnt / 2 : 0;

		push(s32(lrint(opm + ga)));
		m_out_done++;
	}
}

int m92_sound::read(s16 *dst, int max_frames)
{
	int n = 0;
	while (n < max_frames && m_rd < m_wr)
	{
		const int i = int(m_rd % FIFO) * 2;
		dst[n * 2] = m_fifo[i];
		dst[n * 2 + 1] = m_fifo[i + 1];
		m_rd++;
		n++;
	}
	return n;
}
