// m92-pi - Irem M90 sound chips: YM2151 (ymfm) and the 8-bit sample DAC, mixed to 48 kHz
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_M90_SOUND_H
#define M92PI_M90_SOUND_H

#include "emu.h"
#include "ymfm/ymfm_opm.h"
#include <vector>

class m90_machine;

class m90_sound : public ymfm::ymfm_interface
{
public:
	static constexpr double CLOCK = 3579545.0;            // YM2151
	static constexpr int OUT_RATE = 48000;

	explicit m90_sound(m90_machine &m);
	void start(save_registry &save);
	void reset();

	// bring the output up to the machine's current time
	void update();

	u8 ym_read(offs_t offset);
	void ym_write(offs_t offset, u8 data);
	// the DAC's new value (unsigned, 0x80 = silence)
	void dac_write(u8 data) { update(); m_dac = data; }

	int read(s16 *dst, int max_frames);

	void save_opm(std::vector<u8> &buf) { ymfm::ymfm_saved_state st(buf, true); m_opm.save_restore(st); }
	void load_opm(std::vector<u8> &buf) { ymfm::ymfm_saved_state st(buf, false); m_opm.save_restore(st); }

	// ymfm_interface
	void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
	void ymfm_set_busy_end(uint32_t clocks) override;
	bool ymfm_is_busy() override;
	void ymfm_update_irq(bool asserted) override;

private:
	s32 opm_next();
	void push(s32 v);

	m90_machine &m;
	ymfm::ym2151 m_opm;
	emu_timer *m_opm_timer[2] = { nullptr, nullptr };
	attotime m_busy_end;

	u8 m_dac = 0x80;
	// the board's 3.5 kHz low-pass on the samples (MiSTer sound.sv), at 48 kHz
	double m_c[5];                   // b0 b1 b2 a1 a2
	double m_lp[4] = { 0, 0, 0, 0 };

	// resampling: output n is at n / 48000 s; OPM sample k at k / (CLOCK / 64)
	u64 m_out_done = 0;
	u64 m_opm_done = 0;
	s32 m_opm_prev = 0, m_opm_cur = 0;
	attotime m_origin;

	static constexpr int FIFO = 16384;
	s16 m_fifo[FIFO * 2];
	u64 m_wr = 0, m_rd = 0;
};

#endif
