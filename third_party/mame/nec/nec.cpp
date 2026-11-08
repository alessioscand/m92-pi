// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
// m92-pi: MAME's nec.cpp reduced to the V33 (device glue, debugger and the
// state interface removed; memory goes through m92-pi's address_space).

#include "emu.h"
#include "nec.h"

typedef uint8_t BOOLEAN;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

#include "necpriv.ipp"

nec_common_device::nec_common_device(uint32_t clk)
	: m_prefetch_size(6)
	, m_prefetch_cycles(1)
	, m_chip_type(V33_TYPE)
	, m_has_v30_quirks(false)
	, m_has_div_quirk(false)
{
	m_clock = clk;
	memset(m_v33_transtable, 0, sizeof(m_v33_transtable));
}

offs_t nec_common_device::v33_translate(offs_t addr)
{
	if (m_xa)
		return uint32_t(m_v33_transtable[(addr >> 14) & 63]) << 14 | (addr & 0x03fff);
	else
		return addr & 0xfffff;
}

void nec_common_device::prefetch()
{
	m_prefetch_count--;
}

void nec_common_device::do_prefetch()
{
	/* The implementation is not accurate, but comes close.
	 * It does not respect that the V30 will fetch two bytes
	 * at once directly, but instead uses only 2 cycles instead
	 * of 4. There are however only very few sources publicly
	 * available and they are vague.
	 */
	while (m_prefetch_count < 0)
	{
		m_prefetch_count++;
		if (m_cur_cycles > m_prefetch_cycles)
			m_cur_cycles -= m_prefetch_cycles;
		else
			m_icount -= m_prefetch_cycles;
	}

	if (m_prefetch_reset)
	{
		m_prefetch_count = 0;
		m_prefetch_reset = 0;
		return;
	}

	while (m_cur_cycles >= m_prefetch_cycles && m_prefetch_count < m_prefetch_size)
	{
		m_cur_cycles -= m_prefetch_cycles;
		m_prefetch_count++;
	}
}

uint8_t nec_common_device::fetch()
{
	prefetch();
	return m_dr8((Sreg(PS)<<4) + m_ip++);
}

uint16_t nec_common_device::fetchword()
{
	uint16_t r = fetch();
	r |= (fetch()<<8);
	return r;
}

#include "necinstr.h"
#include "necmacro.h"
#include "necea.h"
#include "necmodrm.h"

static uint8_t parity_table[256];

uint8_t nec_common_device::fetchop()
{
	prefetch();
	return m_dr8((Sreg(PS)<<4) + m_ip++);
}



/***************************************************************************/

void nec_common_device::device_reset()
{
	// the V30 leaves its general registers undefined on reset; unverified on the other variants
	if (!has_v30_quirks())
		std::fill(std::begin(m_regs.w), std::end(m_regs.w), 0);

	m_ip = 0;
	m_prev_ip = 0;
	m_rep_ip = 0;
	m_TF = 0;
	m_IF = 0;
	m_DF = 0;
	m_MF = 1;
	m_em = 1;
	m_SignVal = 0;
	m_AuxVal = 0;
	m_OverVal = 0;
	m_ZeroVal = 1;
	m_CarryVal = 0;
	m_ParityVal = 1;
	m_pending_irq = 0;
	m_nmi_state = 0;
	m_irq_state = 0;
	m_poll_state = 1;
	m_halted = 0;
	m_rep_params = 0;
	m_io_retry = false;
	m_io_word_pending = false;
	m_io_word_latch = 0;

	if (m_chip_type == V33_TYPE)
		m_xa = false;

	Sreg(PS) = 0xffff;
	Sreg(SS) = 0;
	Sreg(DS0) = 0;
	Sreg(DS1) = 0;

	CHANGE_PC;
}


void nec_common_device::nec_interrupt(unsigned int_num, int/*INTSOURCES*/ source)
{
	uint32_t dest_seg, dest_off;

	m_rep_params = 0;
	i_pushf();
	m_TF = m_IF = 0;
	m_MF = 1;

	if (source == INT_IRQ)  /* get vector */
		int_num = standard_irq_callback(0, PC());
	debugger_exception_hook(int_num);

	dest_off = read_mem_word(int_num*4);
	dest_seg = read_mem_word(int_num*4+2);

	PUSH(Sreg(PS));
	PUSH(m_ip);
	m_prev_ip = m_ip = (WORD)dest_off;
	Sreg(PS) = (WORD)dest_seg;
	CHANGE_PC;
}

void nec_common_device::nec_trap()
{
	(this->*s_nec_instruction[fetchop()])();
	nec_interrupt(NEC_TRAP_VECTOR, BRK);
}

void nec_common_device::nec_brk(unsigned int_num)
{
	if (m_chip_type != V33_TYPE)
	{
		m_em = 0;
		m_MF = 0;
		i_pushf();
		PUSH(Sreg(PS));
		PUSH(m_ip);
	}
	m_prev_ip = m_ip = read_mem_word(int_num*4);
	Sreg(PS) = read_mem_word(int_num*4+2);
	CHANGE_PC;
}

void nec_common_device::external_int()
{
	if (m_pending_irq & NMI_IRQ)
	{
		nec_interrupt(NEC_NMI_VECTOR, NMI_IRQ);
		m_pending_irq &= ~NMI_IRQ;
	}
	else if (m_pending_irq)
	{
		/* the actual vector is retrieved after pushing flags */
		/* and clearing the IF */
		nec_interrupt((uint32_t)-1, INT_IRQ);
		m_irq_state = CLEAR_LINE;
		m_pending_irq &= ~INT_IRQ;
	}
}

/****************************************************************************/
/*                             OPCODES                                      */
/****************************************************************************/

#include "necinstr.hxx"
#include "nec80inst.hxx"

/*****************************************************************************/

void nec_common_device::set_int_line(int state)
{
	m_irq_state = state;
	if (state == CLEAR_LINE)
		m_pending_irq &= ~INT_IRQ;
	else
	{
		m_pending_irq |= INT_IRQ;
		m_halted = 0;
	}
}

void nec_common_device::set_nmi_line(int state)
{
	if (m_nmi_state == state)
		return;
	m_nmi_state = state;
	if (state != CLEAR_LINE)
	{
		m_pending_irq |= NMI_IRQ;
		m_halted = 0;
	}
}

void nec_common_device::set_poll_line(int state)
{
	m_poll_state = state;
}

void nec_common_device::execute_set_input(int irqline, int state)
{
	switch (irqline)
	{
	case 0:
		set_int_line(state);
		break;
	case INPUT_LINE_NMI:
		set_nmi_line(state);
		break;
	case NEC_INPUT_LINE_POLL:
		set_poll_line(state);
		break;
	}
}

void nec_common_device::device_start()
{
	unsigned int i, j, c;

	static const WREGS wreg_name[8]={ AW, CW, DW, BW, SP, BP, IX, IY };
	static const BREGS breg_name[8]={ AL, CL, DL, BL, AH, CH, DH, BH };

	for (i = 0; i < 256; i++)
	{
		for (j = i, c = 0; j > 0; j >>= 1)
			if (j & 1) c++;
		parity_table[i] = !(c & 1);
	}

	for (i = 0; i < 256; i++)
	{
		Mod_RM.reg.b[i] = breg_name[(i & 0x38) >> 3];
		Mod_RM.reg.w[i] = wreg_name[(i & 0x38) >> 3];
	}

	for (i = 0xc0; i < 0x100; i++)
	{
		Mod_RM.RM.w[i] = wreg_name[i & 7];
		Mod_RM.RM.b[i] = breg_name[i & 7];
	}

	m_no_interrupt = 0;
	m_cur_cycles = 0;
	m_prefetch_count = 0;
	m_prefetch_reset = 0;
	m_prefix_base = 0;
	m_seg_prefix = 0;
	m_EA = 0;
	m_EO = 0;
	m_E16 = 0;
	m_ip = 0;
	m_prev_ip = 0;
	m_rep_ip = 0;
	m_io_retry = false;
	m_io_word_pending = false;
	m_io_word_latch = 0;
	m_xa = false;

	memset(m_regs.w, 0x00, sizeof(m_regs.w));
	memset(m_sregs, 0x00, sizeof(m_sregs));

	save_item(NAME(m_regs.w));
	save_item(NAME(m_sregs));
	save_item(NAME(m_ip));
	save_item(NAME(m_prev_ip));
	save_item(NAME(m_rep_ip));
	save_item(NAME(m_TF));
	save_item(NAME(m_IF));
	save_item(NAME(m_DF));
	save_item(NAME(m_MF));
	save_item(NAME(m_SignVal));
	save_item(NAME(m_AuxVal));
	save_item(NAME(m_OverVal));
	save_item(NAME(m_ZeroVal));
	save_item(NAME(m_CarryVal));
	save_item(NAME(m_ParityVal));
	save_item(NAME(m_pending_irq));
	save_item(NAME(m_nmi_state));
	save_item(NAME(m_irq_state));
	save_item(NAME(m_poll_state));
	save_item(NAME(m_no_interrupt));
	save_item(NAME(m_halted));
	save_item(NAME(m_rep_params));
	save_item(NAME(m_prefix_base));
	save_item(NAME(m_seg_prefix));
	save_item(NAME(m_io_retry));
	save_item(NAME(m_io_word_pending));
	save_item(NAME(m_io_word_latch));
	save_item(NAME(m_prefetch_count));
	save_item(NAME(m_prefetch_reset));
	save_item(NAME(m_cur_cycles));
	save_item(NAME(m_em));
	save_item(NAME(m_xa));
	save_item(NAME(m_v33_transtable));

	m_program = &space(AS_PROGRAM);
	m_io = &space(AS_IO);

	set_icountptr(m_icount);
}

u16 nec_common_device::io_read_word(offs_t a)
{
	if (!(a & 1) && true)
		return m_io->read_word(a);

	// an odd word (or an 8-bit bus) takes two cycles; keep the first byte if the second is retried
	if (!m_io_word_pending)
	{
		m_io_word_latch = io_read_byte(a);
		if (access_to_be_redone_noclear())
			return 0;
		m_io_word_pending = true;
	}
	const u16 result = m_io_word_latch | (io_read_byte((a + 1) & 0xffff) << 8);
	if (!access_to_be_redone_noclear())
		m_io_word_pending = false;
	return result;
}

void nec_common_device::io_write_word(offs_t a, u16 v)
{
	if (!(a & 1) && true)
	{
		m_io->write_word(a, v);
		return;
	}

	if (!m_io_word_pending)
	{
		io_write_byte(a, v);
		if (access_to_be_redone_noclear())
			return;
		m_io_word_pending = true;
	}
	io_write_byte((a + 1) & 0xffff, v >> 8);
	if (!access_to_be_redone_noclear())
		m_io_word_pending = false;
}

void nec_common_device::execute_run()
{
	if (m_halted)
	{
		debugger_wait_hook();
		m_icount = 0;
		return;
	}

	while(m_icount>0)
	{
		m_prev_ip = m_ip;

		// Dispatch IRQ
		if (!m_io_retry && m_pending_irq && m_no_interrupt==0)
		{
			if (m_pending_irq & NMI_IRQ)
				external_int();
			else if (m_IF)
				external_int();
		}

		// No interrupt allowed between last instruction and this one
		if (!m_io_retry && m_no_interrupt)
			m_no_interrupt--;

		debugger_instruction_hook((Sreg(PS)<<4) + m_ip);
		m_cur_cycles = 0;

		if (m_rep_params)
			cont_rep();
		else
		{
			if (m_MF)
				(this->*s_nec_instruction[fetchop()])();
			else
				(this->*s_nec80_instruction[fetchop()])();
		}
		// only I/O accesses may be retried; other instructions would repeat their side effects
		m_io_retry = access_to_be_redone();
		if (m_io_retry)
		{
			m_ip = m_prev_ip;
			m_icount -= 4; // the attempted bus cycle
			m_prefetch_reset = 1;
		}
		else
		{
			do_prefetch();
		}
	}
}
