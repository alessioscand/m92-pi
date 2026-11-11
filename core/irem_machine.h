// m92-pi - what the Irem M92 and M90 machines share: the scheduler that runs
// two CPUs and the timers in step, the inputs, and the interface the front-end
// uses (m92_api.cpp)
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_IREM_MACHINE_H
#define M92PI_IREM_MACHINE_H

#include "emu.h"
#include <cstdint>
#include <functional>
#include <vector>

class irem_machine : public scheduler_base
{
public:
	static constexpr int AUDIO_RATE = 48000;

	// a frame is 262 lines of 424 pixel clocks at 26.666 MHz / 4 (about 60 Hz)
	static constexpr int TOTAL_LINES = 262;
	static constexpr int FIRST_VISIBLE_LINE = 8;
	static constexpr int VBLANK_LINE = 248;
	static constexpr double FRAME_HZ = 26666666.0 / 4 / 424 / 262;

	virtual ~irem_machine();

	virtual void reset() = 0;
	// one frame: from the start of the vertical blank (line 248) to the next
	virtual void run_frame() = 0;

	// the picture, as handed to the line callback
	virtual int screen_w() const = 0;
	virtual int screen_h() const = 0;

	// inputs: one byte per player, active high:
	// bit 0 right, 1 left, 2 down, 3 up, 4 button 4, 5 button 3, 6 button 2, 7 button 1
	void set_player(int n, uint8_t bits) { m_player[n & 3] = bits; }
	// bit 0 start1, 1 start2, 2 coin1, 3 coin2, 4 service, 5 test (active high)
	void set_system(uint8_t bits) { m_system = bits; }
	uint16_t dsw() const { return m_dsw; }
	uint8_t dsw3() const { return m_dsw3; }
	void set_dsw(uint16_t dsw, uint8_t dsw3) { m_dsw = dsw; m_dsw3 = dsw3; }
	// main CPU speed in percent of the board's
	virtual void set_cpu_speed(int percent) = 0;

	// sound: 48 kHz stereo pairs made during the frames run so far
	virtual int audio_read(int16_t *dst, int max_frames) = 0;

	// video: each visible line is drawn as it starts, by default here, and handed to
	// the line callback as screen_w() pixels 0x00RRGGBB
	typedef std::function<void(int y, const uint32_t *rgb)> line_cb;
	void set_line_callback(line_cb cb) { m_line_cb = std::move(cb); }
	// multicore: with a sink set, lines are not drawn here; the sink gets the registers
	// the line must be drawn with (LINE_REGS_BYTES at most) and draws it, on another
	// core, with render_line(). The sync function is called before anything the drawing
	// reads changes: it must wait until every line handed to the sink has been drawn.
	static constexpr size_t LINE_REGS_BYTES = 34;
	typedef std::function<void(int y, const void *regs)> line_sink;
	void set_line_sink(line_sink sink) { m_line_sink = std::move(sink); }
	virtual void set_video_sync(void (*fn)(void *), void *ctx) = 0;
	virtual void render_line(const void *regs, int y, uint32_t *out) = 0;
	// called at the start of each of the 262 lines (8 = first visible line), before it
	// is drawn: beam racing
	typedef std::function<void(int line)> line_hook;
	void set_line_hook(line_hook hook) { m_line_hook = std::move(hook); }

	// save states
	virtual size_t state_size() const = 0;
	virtual bool save_state(std::vector<uint8_t> &out) = 0;
	virtual bool load_state(const std::vector<uint8_t> &in) = 0;

	// Major Title 2's EEPROM; nullptr for the others
	virtual uint8_t *eeprom(size_t &size) { size = 0; return nullptr; }

	// hiscores (MAME's hiscore.dat rows in the main CPU's program space)
	virtual size_t hi_size() const = 0;
	virtual bool hi_ready() = 0;
	virtual void hi_get(uint8_t *dst) = 0;
	virtual void hi_put(const uint8_t *src) = 0;

	// diagnostics
	virtual uint32_t main_pc() const = 0;
	virtual uint32_t sound_pc() const = 0;
	uint64_t frame_count() const { return m_frame; }

	// scheduler_base
	attotime current_time() const override;
	void timer_changed(emu_timer *t) override;

protected:
	struct cpu_slot
	{
		cpu_device *cpu = nullptr;
		u64 cycles_per_sec = 0;      // cycles, not clocks (V35: clock / PCK)
		s64 ps_frac = 0;             // carried picoseconds * cycles_per_sec, for exact time
		int requested = 0;           // cycles asked for in the current run
	};

	// the machine's hooks into the scheduler: a CPU held in reset does not run; a CPU
	// whose speed can change (V35 PCK) updates cycles_per_sec before it runs
	virtual bool cpu_held(int /*slot*/) const { return false; }
	virtual void cpu_rate(int /*slot*/) {}

	void run_until(attotime target);
	void run_cpu(int slot, attotime until);
	// run fn after the running CPU's current instruction, once the other CPU has caught
	// up to the same time (as MAME's synchronize())
	void defer(std::function<void()> fn);
	// timers made by the machine, registered for save states
	emu_timer *new_timer(save_registry &save, std::function<void(s32)> cb);
	void delete_timers();
	// the scheduler's own state, for save states
	void register_save(save_registry &save);
	void reset_scheduler();

	cpu_slot m_slot[2];
	cpu_slot *m_running = nullptr;
	attotime m_now;                  // machine time between slices
	attotime m_slice_end;
	std::vector<std::function<void()>> m_deferred;

	// inputs
	u8 m_player[4] = { 0, 0, 0, 0 };
	u8 m_system = 0;
	u16 m_dsw = 0xffff;
	u8 m_dsw3 = 0xff;

	line_cb m_line_cb;
	line_sink m_line_sink;
	line_hook m_line_hook;
	u64 m_frame = 0;
};

#endif
