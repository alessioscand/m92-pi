// m92-pi - Irem M92 sound chips: YM2151 (ymfm) and Irem GA20, mixed to 48 kHz
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_M92_SOUND_H
#define M92PI_M92_SOUND_H

#include "emu.h"
#include "ymfm/ymfm_opm.h"
#include <vector>

class m92_machine;

class m92_sound : public ymfm::ymfm_interface
{
public:
	static constexpr double CLOCK = 14318181.0 / 4;      // both chips: 3.579545 MHz
	static constexpr int OUT_RATE = 48000;

	explicit m92_sound(m92_machine &m);
	void start(const u8 *ga20_rom, size_t ga20_size, save_registry &save);
	void reset();

	// bring the output up to the machine's current time
	void update();

	u8 ym_read(offs_t offset);
	void ym_write(offs_t offset, u8 data);
	u8 ga20_read(offs_t offset);
	void ga20_write(offs_t offset, u8 data);

	// output FIFO (stereo frames)
	int available() const { return int(m_wr - m_rd); }
	int read(s16 *dst, int max_frames);

	// ymfm state for save states
	void save_opm(std::vector<u8> &buf) { ymfm::ymfm_saved_state st(buf, true); m_opm.save_restore(st); }
	void load_opm(std::vector<u8> &buf) { ymfm::ymfm_saved_state st(buf, false); m_opm.save_restore(st); }

	// ymfm_interface
	void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
	void ymfm_set_busy_end(uint32_t clocks) override;
	bool ymfm_is_busy() override;
	void ymfm_update_irq(bool asserted) override;

private:
	struct ga20_channel { u32 rate, pos, counter, end, volume, play; };

	s32 opm_next();
	s32 ga20_next();
	void push(s32 v);

	m92_machine &m;
	ymfm::ym2151 m_opm;
	emu_timer *m_opm_timer[2] = { nullptr, nullptr };
	attotime m_busy_end;

	// GA20
	const u8 *m_ga20_rom = nullptr;
	u32 m_ga20_mask = 0;
	u8 m_ga20_regs[0x20];
	ga20_channel m_ch[4];

	// resampling: output n is at n / 48000 s; OPM sample k at k / (CLOCK / 64);
	// GA20 sample k at k / (CLOCK / 4)
	u64 m_out_done = 0;              // output samples made since reset
	u64 m_opm_done = 0;              // OPM samples made
	u64 m_ga_done = 0;               // GA20 samples made
	s32 m_opm_prev = 0, m_opm_cur = 0;
	attotime m_origin;               // machine time of output sample 0

	// FIFO
	static constexpr int FIFO = 16384;
	s16 m_fifo[FIFO * 2];
	u64 m_wr = 0, m_rd = 0;
};

#endif
