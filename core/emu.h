// m92-pi - minimal stand-in for MAME's emu.h
//
// Just enough of MAME's device model for the NEC V33 and V35 cores to compile almost
// unchanged: integer types, bit helpers, a tiny time type, timers driven by the m92
// scheduler, address spaces made of 2 KB pages, and a save-state registry.
//
// SPDX-License-Identifier: BSD-3-Clause
#ifndef M92PI_EMU_H
#define M92PI_EMU_H

#include <cstdint>
#include <cstring>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <functional>
#include <vector>
#include <string>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef uint32_t offs_t;

#define ATTR_COLD
#define NAME(x) x, #x
#define LOGMASKED(...) do {} while (0)
#define LOG_BUSLOCK 0
#define FUNC(x) &x

enum { CLEAR_LINE = 0, ASSERT_LINE = 1 };
enum { INPUT_LINE_IRQ0 = 0, INPUT_LINE_NMI = 0x7e, INPUT_LINE_RESET = 0x7f };
enum { AS_PROGRAM = 0, AS_DATA = 1, AS_IO = 2 };

template <typename T> constexpr T BIT(T x, int n) { return (x >> n) & T(1); }
template <typename T> constexpr T BIT(T x, int n, int w) { return (x >> n) & ((T(1) << w) - 1); }
template <typename T, typename U> constexpr T make_bitmask(U n)
{
	return T((n < (8 * sizeof(T))) ? ((std::make_unsigned_t<T>(1) << n) - 1) : ~std::make_unsigned_t<T>(0));
}

#define COMBINE_DATA(varptr) (*(varptr) = (*(varptr) & ~mem_mask) | (data & mem_mask))

// ---------------------------------------------------------------------------
// time: picoseconds since power on (2^63 ps is about 106 days)

struct attotime
{
	s64 ps;
	constexpr attotime() : ps(0) {}
	constexpr explicit attotime(s64 p) : ps(p) {}
	static const attotime never;
	static const attotime zero;
	bool is_never() const { return ps == INT64_MAX; }
	attotime operator*(u32 n) const { return is_never() ? *this : attotime(ps * n); }
	attotime operator/(u32 n) const { return is_never() ? *this : attotime(ps / n); }
	attotime operator+(const attotime &o) const { return (is_never() || o.is_never()) ? never : attotime(ps + o.ps); }
	bool operator<(const attotime &o) const { return ps < o.ps; }
	bool operator<=(const attotime &o) const { return ps <= o.ps; }
	static attotime from_hz(double hz) { return attotime(s64(1e12 / hz + 0.5)); }
};
inline const attotime attotime::never = attotime(INT64_MAX);
inline const attotime attotime::zero = attotime(0);

// ---------------------------------------------------------------------------
// save states: every save_item() call records an address and a size; the m92 machine
// copies the whole list in registration order

struct save_registry
{
	struct item { void *ptr; size_t size; };
	std::vector<item> items;
	void add(void *p, size_t n) { items.push_back({ p, n }); }
	size_t total() const { size_t t = 0; for (auto &i : items) t += i.size; return t; }
	void save(u8 *dst) const { for (auto &i : items) { memcpy(dst, i.ptr, i.size); dst += i.size; } }
	void load(const u8 *src) const { for (auto &i : items) { memcpy(i.ptr, src, i.size); src += i.size; } }
};

// ---------------------------------------------------------------------------
// timers, run by the scheduler in m92.cpp

class emu_timer;

struct scheduler_base
{
	std::vector<emu_timer *> timers;
	virtual ~scheduler_base() {}
	virtual attotime current_time() const = 0;      // includes the cycles run so far by the running CPU
	virtual void timer_changed(emu_timer *t) = 0;  // a timer moved: may shorten the current slice
};

class emu_timer
{
public:
	scheduler_base *sched = nullptr;
	std::function<void(s32)> cb;
	attotime expire = attotime::never;
	attotime period = attotime::never;
	s32 param = 0;

	void adjust(attotime delay, s32 p = 0, attotime per = attotime::never)
	{
		param = p;
		period = per;
		expire = delay.is_never() ? attotime::never : sched->current_time() + delay;
		sched->timer_changed(this);
	}
	bool enabled() const { return !expire.is_never(); }
	void reset() { expire = attotime::never; }
};

#define TIMER_CALLBACK_MEMBER(name) void name(s32 param)

// ---------------------------------------------------------------------------
// address spaces: 2 KB pages, each either plain memory (read and/or write pointer)
// or handled by functions with MAME's (byte offset, data, mem_mask) shape

class address_space
{
public:
	static constexpr int PAGE_BITS = 11;
	typedef u16 (*rhandler)(void *ctx, offs_t addr, u16 mem_mask);   // addr: byte address, even
	typedef void (*whandler)(void *ctx, offs_t addr, u16 data, u16 mem_mask);

	int m_addr_width = 20;
	int m_data_width = 16;
	offs_t m_mask = 0xfffff;
	std::vector<u8 *> m_rp, m_wp;     // memory pages, base already moved back by the page address
	rhandler m_rh = nullptr;
	whandler m_wh = nullptr;
	void *m_ctx = nullptr;

	void configure(int addr_width, int data_width, rhandler rh, whandler wh, void *ctx)
	{
		m_addr_width = addr_width;
		m_data_width = data_width;
		m_mask = (offs_t(1) << addr_width) - 1;
		size_t pages = (size_t(1) << addr_width) >> PAGE_BITS;
		if (pages == 0)
			pages = 1;
		m_rp.assign(pages, nullptr);
		m_wp.assign(pages, nullptr);
		m_rh = rh; m_wh = wh; m_ctx = ctx;
	}
	// direct memory for [start, end]; both ends on page boundaries
	void map_read(offs_t start, offs_t end, u8 *mem)
	{
		for (offs_t a = start; a <= end; a += (1 << PAGE_BITS))
			m_rp[a >> PAGE_BITS] = mem ? mem + (a - start) - a : nullptr;
	}
	void map_write(offs_t start, offs_t end, u8 *mem)
	{
		for (offs_t a = start; a <= end; a += (1 << PAGE_BITS))
			m_wp[a >> PAGE_BITS] = mem ? mem + (a - start) - a : nullptr;
	}

	int data_width() const { return m_data_width; }
	int addr_width() const { return m_addr_width; }

	u8 read_byte(offs_t a)
	{
		a &= m_mask;
		if (u8 *p = m_rp[a >> PAGE_BITS])
			return p[a];
		if (m_data_width == 8)
			return u8(m_rh(m_ctx, a, 0xff));
		const int sh = (a & 1) * 8;
		return u8(m_rh(m_ctx, a & ~1, u16(0xff << sh)) >> sh);
	}
	u16 read_word(offs_t a)
	{
		a &= m_mask;
		if (m_data_width == 8)
			return read_byte(a) | (read_byte(a + 1) << 8);
		a &= ~1;
		if (u8 *p = m_rp[a >> PAGE_BITS])
			return p[a] | (p[a + 1] << 8);
		return m_rh(m_ctx, a, 0xffff);
	}
	u16 read_word_unaligned(offs_t a)
	{
		if (!(a & 1) && m_data_width == 16)
			return read_word(a);
		return read_byte(a) | (read_byte(a + 1) << 8);
	}
	void write_byte(offs_t a, u8 d)
	{
		a &= m_mask;
		if (u8 *p = m_wp[a >> PAGE_BITS]) { p[a] = d; return; }
		if (m_data_width == 8) { m_wh(m_ctx, a, d, 0xff); return; }
		const int sh = (a & 1) * 8;
		m_wh(m_ctx, a & ~1, u16(d << sh), u16(0xff << sh));
	}
	void write_word(offs_t a, u16 d)
	{
		a &= m_mask;
		if (m_data_width == 8) { write_byte(a, d); write_byte(a + 1, d >> 8); return; }
		a &= ~1;
		if (u8 *p = m_wp[a >> PAGE_BITS]) { p[a] = u8(d); p[a + 1] = u8(d >> 8); return; }
		m_wh(m_ctx, a, d, 0xffff);
	}
	void write_word_unaligned(offs_t a, u16 d)
	{
		if (!(a & 1) && m_data_width == 16) { write_word(a, d); return; }
		write_byte(a, u8(d));
		write_byte(a + 1, u8(d >> 8));
	}

	template <typename C> void cache(C &c) { c.m_space = this; }
};

template <int Width, int AddrShift, int Level, int Endian>
struct memory_access
{
	struct cache
	{
		address_space *m_space = nullptr;
		u8 read_byte(offs_t a) { return m_space->read_byte(a); }
		u16 read_word(offs_t a) { return m_space->read_word(a); }
	};
};
enum { ENDIANNESS_LITTLE = 0, ENDIANNESS_BIG = 1 };

// ---------------------------------------------------------------------------
// the parts of device_t / cpu_device the NEC cores use

struct machine_stub
{
	bool side_effects_disabled() const { return false; }
};

class cpu_device
{
public:
	virtual ~cpu_device() {}

	// set by the machine
	u32 m_clock = 0;
	scheduler_base *m_sched = nullptr;
	save_registry *m_save = nullptr;
	std::function<int(int)> m_irq_ack;   // returns the interrupt vector (pic8259 INTA)
	address_space m_spaces[3];
	attotime m_local_time;               // time reached by this CPU
	bool m_in_reset = false;

	u32 clock() const { return m_clock; }
	address_space &space(int n) { return m_spaces[n]; }
	machine_stub machine() const { return machine_stub(); }

	// device interface used by the m92 machine
	virtual void device_start() = 0;
	virtual void device_reset() = 0;
	virtual void execute_run() = 0;
	virtual void execute_set_input(int line, int state) = 0;
	virtual u64 execute_clocks_to_cycles(u64 clocks) const { return clocks; }
	virtual u64 execute_cycles_to_clocks(u64 cycles) const { return cycles; }
	int *m_icountptr = nullptr;
	void set_icountptr(int &ic) { m_icountptr = &ic; }
	void set_input_line(int line, int state) { execute_set_input(line, state); }

	// helpers for the cores
	attotime clocks_to_attotime(u64 clocks) const { return attotime(s64((clocks * 1000000000000ULL) / m_clock)); }
	template <typename... A> void logerror(const char *, A...) const {}
	int standard_irq_callback(int line, offs_t) { return m_irq_ack ? m_irq_ack(line) : 0xff; }
	void debugger_exception_hook(int) {}
	void debugger_instruction_hook(offs_t) {}
	void debugger_wait_hook() {}
	bool access_to_be_redone() { return false; }
	bool access_to_be_redone_noclear() { return false; }
	void notify_clock_changed() {}

	template <typename T> void save_item(T &v, const char *) { if (m_save) m_save->add(&v, sizeof(v)); }
	template <typename T> void save_pointer(T *v, const char *, size_t n) { if (m_save) m_save->add(v, sizeof(T) * n); }

	template <typename T> emu_timer *timer_alloc(void (T::*fn)(s32), T *obj)
	{
		emu_timer *t = new emu_timer;
		t->sched = m_sched;
		t->cb = [obj, fn](s32 p) { (obj->*fn)(p); };
		m_sched->timers.push_back(t);
		if (m_save)
		{
			m_save->add(&t->expire, sizeof(t->expire));
			m_save->add(&t->period, sizeof(t->period));
			m_save->add(&t->param, sizeof(t->param));
		}
		return t;
	}
};

#endif
