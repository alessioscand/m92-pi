// m92-pi - the Z80 (third_party/z80, MIT) as a cpu_device the scheduler can run
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_Z80_DEVICE_H
#define M92PI_Z80_DEVICE_H

#include "emu.h"
#include "z80/z80.h"

class z80_device : public cpu_device
{
public:
	typedef u8 (*read_fn)(void *ctx, u16 addr);
	typedef void (*write_fn)(void *ctx, u16 addr, u8 data);

	z80_device(u32 clock, void *ctx, read_fn mem_r, write_fn mem_w, read_fn io_r, write_fn io_w)
		: m_ctx(ctx), m_mem_r(mem_r), m_mem_w(mem_w), m_io_r(io_r), m_io_w(io_w)
	{
		m_clock = clock;
		set_icountptr(m_icount);
		z80_init(&m_z);
		hook();
	}

	void device_start() override
	{
		// registers and interrupt state; the callbacks inside are put back after a load
		if (m_save)
			m_save->add(&m_z, sizeof(m_z));
	}
	void device_reset() override { z80_init(&m_z); hook(); z80_reset(&m_z); }
	void after_load() { hook(); }

	void execute_run() override
	{
		while (m_icount > 0)
			m_icount -= int(z80_step(&m_z));
	}
	void execute_set_input(int line, int state) override
	{
		if (line == INPUT_LINE_NMI)
		{
			if (state)
				z80_pulse_nmi(&m_z);
		}
		else if (state)
			z80_assert_irq(&m_z, m_irq_vector);
		else
			z80_clr_irq(&m_z);
	}
	// the byte put on the bus when the interrupt is acknowledged (IM 0: an RST)
	void set_irq_vector(u8 v) { m_irq_vector = v; m_z.irq_data = v; }

	u32 pc() const { return m_z.pc; }
	int m_icount = 0;

private:
	void hook()
	{
		m_z.userdata = this;
		m_z.read_byte = [](void *u, uint16_t a) { auto *d = static_cast<z80_device *>(u); return d->m_mem_r(d->m_ctx, a); };
		m_z.write_byte = [](void *u, uint16_t a, uint8_t v) { auto *d = static_cast<z80_device *>(u); d->m_mem_w(d->m_ctx, a, v); };
		m_z.port_in = [](z80 *z, uint16_t p) { auto *d = static_cast<z80_device *>(z->userdata); return d->m_io_r(d->m_ctx, p); };
		m_z.port_out = [](z80 *z, uint16_t p, uint8_t v) { auto *d = static_cast<z80_device *>(z->userdata); d->m_io_w(d->m_ctx, p, v); };
	}

	z80 m_z;
	u8 m_irq_vector = 0xff;
	void *m_ctx;
	read_fn m_mem_r;
	write_fn m_mem_w;
	read_fn m_io_r;
	write_fn m_io_w;
};

#endif
