// m92-pi - Irem M90 sound chips (see m90_sound.h)
//
// YM2151 by ymfm (BSD-3-Clause, Aaron Giles). The mix follows the board as the MiSTer
// core's sound.sv models it: only the YM2151's left output is wired; the samples
// go through a 3.5 kHz low-pass filter and 10 dB of attenuation. Overall level as
// the M92's, so that the two machines sound alike.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m90_sound.h"
#include "m90.h"
#include <cmath>

// second-order Butterworth low-pass, 3.5 kHz at 48 kHz
static void lowpass(double fc, double fs, double c[5])
{
	const double k = std::tan(M_PI * fc / fs), q = M_SQRT1_2;
	const double n = 1 / (1 + k / q + k * k);
	c[0] = k * k * n;                // b0
	c[1] = 2 * c[0];                 // b1
	c[2] = c[0];                     // b2
	c[3] = 2 * (k * k - 1) * n;      // a1
	c[4] = (1 - k / q + k * k) * n;  // a2
}

m90_sound::m90_sound(m90_machine &mach)
	: m(mach)
	, m_opm(*this)
{
	lowpass(3500, OUT_RATE, m_c);
}

void m90_sound::start(save_registry &save)
{
	for (int i = 0; i < 2; i++)
		m_opm_timer[i] = m.new_timer(save, [this, i](s32) { update(); m_engine->engine_timer_expired(i); });
	save.add(&m_busy_end, sizeof(m_busy_end));
	save.add(&m_dac, sizeof(m_dac));
	save.add(m_lp, sizeof(m_lp));
	save.add(&m_out_done, sizeof(m_out_done));
	save.add(&m_opm_done, sizeof(m_opm_done));
	save.add(&m_opm_prev, sizeof(m_opm_prev));
	save.add(&m_opm_cur, sizeof(m_opm_cur));
	save.add(&m_origin, sizeof(m_origin));
}

void m90_sound::reset()
{
	m_opm.reset();
	m_opm_timer[0]->reset();
	m_opm_timer[1]->reset();
	m_busy_end = attotime::zero;
	m_dac = 0x80;
	for (double &v : m_lp)
		v = 0;
	m_out_done = m_opm_done = 0;
	m_opm_prev = m_opm_cur = 0;
	m_origin = m.current_time();
}

// ---------------------------------------------------------------- ymfm hooks

void m90_sound::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks)
{
	if (duration_in_clocks >= 0)
		m_opm_timer[tnum]->adjust(attotime(s64(double(duration_in_clocks) * 1e12 / CLOCK)));
	else
		m_opm_timer[tnum]->reset();
}

void m90_sound::ymfm_set_busy_end(uint32_t clocks)
{
	m_busy_end = m.current_time() + attotime(s64(double(clocks) * 1e12 / CLOCK));
}

bool m90_sound::ymfm_is_busy()
{
	return m.current_time() < m_busy_end;
}

void m90_sound::ymfm_update_irq(bool asserted)
{
	m.set_ym_irq(asserted);
}

u8 m90_sound::ym_read(offs_t offset)
{
	return m_opm.read(offset & 1);
}

void m90_sound::ym_write(offs_t offset, u8 data)
{
	update();
	m_opm.write(offset & 1, data);
}

// ---------------------------------------------------------------- mixing

s32 m90_sound::opm_next()
{
	ymfm::ym2151::output_data o;
	m_opm.generate(&o);
	return o.data[0];                // left only
}

void m90_sound::push(s32 v)
{
	if (v > 32767) v = 32767;
	if (v < -32768) v = -32768;
	if (m_wr - m_rd >= FIFO)
		m_rd++;                      // nobody is reading: drop the oldest
	const int i = int(m_wr % FIFO) * 2;
	m_fifo[i] = m_fifo[i + 1] = s16(v);
	m_wr++;
}

void m90_sound::update()
{
	const attotime now = m.current_time();
	if (now < m_origin)
		return;
	const unsigned __int128 elapsed = (unsigned __int128)(now.ps - m_origin.ps);
	const u64 target = u64(elapsed * OUT_RATE / 1000000000000ULL);

	constexpr double opm_rate = CLOCK / 64;
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
		const double opm = m_opm_prev + (m_opm_cur - m_opm_prev) * (p - double(i));

		// DAC: -10 dB, low-pass (x1 x2 y1 y2 in m_lp)
		const double x = (int(m_dac) - 0x80) * 256 * 0.316;
		const double *c = m_c;
		const double y = c[0] * x + c[1] * m_lp[0] + c[2] * m_lp[1] - c[3] * m_lp[2] - c[4] * m_lp[3];
		m_lp[1] = m_lp[0]; m_lp[0] = x;
		m_lp[3] = m_lp[2]; m_lp[2] = y;

		push(s32(lrint((opm + y) * 0.8)));
		m_out_done++;
	}
}

int m90_sound::read(s16 *dst, int max_frames)
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
