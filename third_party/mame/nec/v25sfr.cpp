// license:BSD-3-Clause
// copyright-holders:Bryan McPhail, Alex W. Jackson
/****************************************************************************

    NEC V25/V35 special function registers and internal data area access

****************************************************************************/

#include "emu.h"
#include "v25.h"
#include "v25priv.ipp"

uint8_t v25_common_device::read_irqcontrol(int /*INTSOURCES*/ source, uint8_t priority)
{
	return  (((m_pending_irq & source)     ? 0x80 : 0x00)
			| ((m_unmasked_irq & source)   ? 0x00 : 0x40)
			| ((m_macro_service & source)  ? 0x20 : 0x00)
			| ((m_bankswitch_irq & source) ? 0x10 : 0x00)
			| priority);
}

void v25_common_device::write_irqcontrol(int /*INTSOURCES*/ source, uint8_t d)
{
	if (BIT(d, 7))
		m_pending_irq |= source;
	else
		m_pending_irq &= ~source;

	if (BIT(d, 6))
		m_unmasked_irq &= ~source;
	else
		m_unmasked_irq |= source;

	if (BIT(d, 5))
	{
		if ((m_macro_service & source) == 0)
			logerror("%06x: Warning: macro service function not implemented\n",PC());
		m_macro_service |= source;
	}
	else
		m_macro_service &= ~source;

	if (BIT(d, 4))
		m_bankswitch_irq |= source;
	else
		m_bankswitch_irq &= ~source;
}

uint8_t v25_common_device::p0_r()
{
	return m_p0_in();
}

void v25_common_device::p0_w(uint8_t d)
{
	m_p0_out(d);
}

void v25_common_device::pm0_w(uint8_t d)
{
	logerror("%06x: PM0 set to %02x\n", PC(), d);
}

void v25_common_device::pmc0_w(uint8_t d)
{
	logerror("%06x: PMC0 set to %02x\n", PC(), d);
}

uint8_t v25_common_device::p1_r()
{
	// P1 is combined with the interrupt lines
	return ((m_p1_in() & 0xf0)
			| (m_nmi_state     ? 0x00 : 0x01)
			| (m_intp_state[0] ? 0x00 : 0x02)
			| (m_intp_state[1] ? 0x00 : 0x04)
			| (m_intp_state[2] ? 0x00 : 0x08));
}

void v25_common_device::p1_w(uint8_t d)
{
	// only the upper four bits of P1 can be used as output
	m_p1_out(d & 0xf0);
}

void v25_common_device::pm1_w(uint8_t d)
{
	logerror("%06x: PM1 set to %02x\n", PC(), d);
}

void v25_common_device::pmc1_w(uint8_t d)
{
	logerror("%06x: PMC1 set to %02x\n", PC(), d);
}

uint8_t v25_common_device::p2_r()
{
	return m_p2_in();
}

void v25_common_device::p2_w(uint8_t d)
{
	m_p2_out(d);
}

void v25_common_device::pm2_w(uint8_t d)
{
	logerror("%06x: PM2 set to %02x\n", PC(), d);
}

void v25_common_device::pmc2_w(uint8_t d)
{
	logerror("%06x: PMC2 set to %02x\n", PC(), d);
}

uint8_t v25_common_device::pt_r()
{
	return m_pt_in();
}

void v25_common_device::pmt_w(uint8_t d)
{
	logerror("%06x: PMT set to %02x\n", PC(), d);
}

uint8_t v25_common_device::intm_r()
{
	return m_intm;
}

void v25_common_device::intm_w(uint8_t d)
{
	logerror("%06x: INTM set to %02x\n", PC(), d & 0x55);
	m_intm = d & 0x55;
}

uint8_t v25_common_device::ems_r(offs_t a)
{
	return m_ems[a];
}

void v25_common_device::ems_w(offs_t a, uint8_t d)
{
	logerror("%06x: EMS%d set to %02x\n", PC(), a, d & 0xf7);
	m_ems[a] = d & 0xf7;
}

uint8_t v25_common_device::exic0_r()
{
	return read_irqcontrol(INTP0, m_priority_intp);
}

void v25_common_device::exic0_w(uint8_t d)
{
	write_irqcontrol(INTP0, d);
	m_priority_intp = d & 0x7;
}

uint8_t v25_common_device::exic1_r()
{
	return read_irqcontrol(INTP1, 7);
}

void v25_common_device::exic1_w(uint8_t d)
{
	write_irqcontrol(INTP1, d);
}

uint8_t v25_common_device::exic2_r()
{
	return read_irqcontrol(INTP2, 7);
}

void v25_common_device::exic2_w(uint8_t d)
{
	write_irqcontrol(INTP2, d);
}

uint8_t v25_common_device::srms0_r()
{
	return m_srms[0];
}

void v25_common_device::srms0_w(uint8_t d)
{
	logerror("%06x: SRMS0 set to %02x\n", PC(), d & 0xf7);
	m_srms[0] = d & 0xf7;
}

uint8_t v25_common_device::stms0_r()
{
	return m_stms[0];
}

void v25_common_device::stms0_w(uint8_t d)
{
	logerror("%06x: STMS0 set to %02x\n", PC(), d & 0xf7);
	m_stms[0] = d & 0xf7;
}

uint8_t v25_common_device::scm0_r()
{
	return m_scm[0];
}

void v25_common_device::scm0_w(uint8_t d)
{
	scm_w(0, d);
}

uint8_t v25_common_device::rxb0_r()
{
	return rxb_r(0);
}

void v25_common_device::txb0_w(uint8_t d)
{
	txb_w(0, d);
}

uint8_t v25_common_device::scc0_r()
{
	return m_scc[0];
}

void v25_common_device::scc0_w(uint8_t d)
{
	logerror("%06x: SCC0 prescaler set to %d\n", PC(), 2 << (d & 0x0f));
	m_scc[0] = d & 0x0f;
}

uint8_t v25_common_device::brg0_r()
{
	return m_brg[0];
}

void v25_common_device::brg0_w(uint8_t d)
{
	logerror("%06x: BRG0 divider set to %u\n", PC(), d);
	m_brg[0] = d;
}

uint8_t v25_common_device::sce0_r()
{
	return m_sce[0] | (m_rxd_state[0] ? 0x80 : 0x00);
}

uint8_t v25_common_device::seic0_r()
{
	return read_irqcontrol(INTSER0, m_priority_ints0);
}

void v25_common_device::seic0_w(uint8_t d)
{
	// no macro service for error interrupt
	write_irqcontrol(INTSER0, d & 0xd0);
	m_priority_ints0 = d & 0x7;
}

uint8_t v25_common_device::sric0_r()
{
	return read_irqcontrol(INTSR0, 7);
}

void v25_common_device::sric0_w(uint8_t d)
{
	write_irqcontrol(INTSR0, d);
}

uint8_t v25_common_device::stic0_r()
{
	return read_irqcontrol(INTST0, 7);
}

void v25_common_device::stic0_w(uint8_t d)
{
	write_irqcontrol(INTST0, d);
}

uint8_t v25_common_device::srms1_r()
{
	return m_srms[1];
}

void v25_common_device::srms1_w(uint8_t d)
{
	logerror("%06x: SRMS1 set to %02x\n", PC(), d & 0xf7);
	m_srms[1] = d & 0xf7;
}

uint8_t v25_common_device::stms1_r()
{
	return m_stms[1];
}

void v25_common_device::stms1_w(uint8_t d)
{
	logerror("%06x: STMS1 set to %02x\n", PC(), d & 0xf7);
	m_stms[1] = d & 0xf7;
}

uint8_t v25_common_device::scm1_r()
{
	return m_scm[1];
}

void v25_common_device::scm1_w(uint8_t d)
{
	scm_w(1, d);
}

uint8_t v25_common_device::rxb1_r()
{
	return rxb_r(1);
}

void v25_common_device::txb1_w(uint8_t d)
{
	txb_w(1, d);
}

uint8_t v25_common_device::scc1_r()
{
	return m_scc[1];
}

void v25_common_device::scc1_w(uint8_t d)
{
	logerror("%06x: SCC1 prescaler set to %d\n", PC(), 2 << (d & 0x0f));
	m_scc[1] = d & 0x0f;
}

uint8_t v25_common_device::brg1_r()
{
	return m_brg[1];
}

void v25_common_device::brg1_w(uint8_t d)
{
	logerror("%06x: BRG1 divider set to %u\n", PC(), d);
	m_brg[1] = d;
}

uint8_t v25_common_device::sce1_r()
{
	return m_sce[1] | (m_rxd_state[1] ? 0x80 : 0x00);
}

uint8_t v25_common_device::seic1_r()
{
	return read_irqcontrol(INTSER1, m_priority_ints1);
}

void v25_common_device::seic1_w(uint8_t d)
{
	// no macro service for error interrupt
	write_irqcontrol(INTSER1, d & 0xd0);
	m_priority_ints1 = d & 0x7;
}

uint8_t v25_common_device::sric1_r()
{
	return read_irqcontrol(INTSR1, 7);
}

void v25_common_device::sric1_w(uint8_t d)
{
	write_irqcontrol(INTSR1, d);
}

uint8_t v25_common_device::stic1_r()
{
	return read_irqcontrol(INTST1, 7);
}

void v25_common_device::stic1_w(uint8_t d)
{
	write_irqcontrol(INTST1, d);
}

uint16_t v25_common_device::tm0_r()
{
	if (!machine().side_effects_disabled())
		logerror("%06x: Warning: read back TM0\n",PC());
	return m_TM0;
}

void v25_common_device::tm0_w(uint16_t d)
{
	m_TM0 = d;
}

uint16_t v25_common_device::md0_r()
{
	if (!machine().side_effects_disabled())
		logerror("%06x: Warning: read back MD0\n",PC());
	return m_MD0;
}

void v25_common_device::md0_w(uint16_t d)
{
	m_MD0 = d;
}

uint16_t v25_common_device::tm1_r()
{
	if (!machine().side_effects_disabled())
		logerror("%06x: Warning: read back TM1\n",PC());
	return m_TM1;
}

void v25_common_device::tm1_w(uint16_t d)
{
	m_TM1 = d;
}

uint16_t v25_common_device::md1_r()
{
	if (!machine().side_effects_disabled())
		logerror("%06x: Warning: read back MD1\n",PC());
	return m_MD1;
}

void v25_common_device::md1_w(uint16_t d)
{
	if (m_MD1 == 0 && d != 0)
	{
		m_MD1 = d;
		tmc1_w(m_TMC1); // HACK: start timer if necessary
	}
	else
		m_MD1 = d;
}

uint8_t v25_common_device::tmc0_r()
{
	return m_TMC0;
}

void v25_common_device::tmc0_w(uint8_t d)
{
	m_TMC0 = d;
	if (BIT(d, 0))   // oneshot mode
	{
		if (BIT(d, 7) && m_TM0 != 0)
		{
			unsigned tmp = m_PCK * m_TM0 * (BIT(d, 6) ? 128 : 12);
			attotime time = clocks_to_attotime(tmp);
			m_timers[0]->adjust(time, INTTU0);
		}
		else
			m_timers[0]->adjust(attotime::never);

		if (BIT(d, 5) && m_MD0 != 0)
		{
			unsigned tmp = m_PCK * m_MD0 * (BIT(d, 4) ? 128 : 12);
			attotime time = clocks_to_attotime(tmp);
			m_timers[1]->adjust(time, INTTU1);
		}
		else
			m_timers[1]->adjust(attotime::never);
	}
	else    // interval mode
	{
		if (BIT(d, 7) && m_MD0 != 0)
		{
			unsigned tmp = m_PCK * m_MD0 * (BIT(d, 6) ? 128 : 6);
			attotime time = clocks_to_attotime(tmp);
			m_timers[0]->adjust(time, INTTU0, time);
			m_timers[1]->adjust(attotime::never);
			m_TM0 = m_MD0;
		}
		else
		{
			m_timers[0]->adjust(attotime::never);
			m_timers[1]->adjust(attotime::never);
		}
	}
}

uint8_t v25_common_device::tmc1_r()
{
	return m_TMC1;
}

void v25_common_device::tmc1_w(uint8_t d)
{
	m_TMC1 = d & 0xC0;
	if (BIT(d, 7) && m_MD1 != 0)
	{
		unsigned tmp = m_PCK * m_MD1 * (BIT(d, 6) ? 128 : 6);
		attotime time = clocks_to_attotime(tmp);
		m_timers[2]->adjust(time, INTTU1 | INTTU2, time);
		m_TM1 = m_MD1;
	}
	else
		m_timers[2]->adjust(attotime::never);
}

uint8_t v25_common_device::tmms_r(offs_t a)
{
	return m_tmms[a];
}

void v25_common_device::tmms_w(offs_t a, uint8_t d)
{
	logerror("%06x: TMMS%d set to %02x\n", PC(), a, d & 0xf7);
	m_tmms[a] = d & 0xf7;
}

uint8_t v25_common_device::tmic0_r()
{
	return read_irqcontrol(INTTU0, m_priority_inttu);
}

void v25_common_device::tmic0_w(uint8_t d)
{
	write_irqcontrol(INTTU0, d);
	m_priority_inttu = d & 0x7;
}

uint8_t v25_common_device::tmic1_r()
{
	return read_irqcontrol(INTTU1, 7);
}

void v25_common_device::tmic1_w(uint8_t d)
{
	write_irqcontrol(INTTU1, d);
}

uint8_t v25_common_device::tmic2_r()
{
	return read_irqcontrol(INTTU2, 7);
}

void v25_common_device::tmic2_w(uint8_t d)
{
	write_irqcontrol(INTTU2, d);
}

uint8_t v25_common_device::dmac0_r()
{
	return m_dmac[0];
}

void v25_common_device::dmac0_w(uint8_t d)
{
	logerror("%06x: DMAC0 set to %02x\n", PC(), d);
	logerror("        SAR0 %s after each transfer\n", (d & 0x03) == 0x01 ? "incremented" : (d & 0x03) == 0x02 ? "decremented" : "unmodified");
	logerror("        DAR0 %s after each transfer\n", (d & 0x30) == 0x10 ? "incremented" : (d & 0x30) == 0x20 ? "decremented" : "unmodified");
	m_dmac[0] = d & 0x33;
}

uint8_t v25_common_device::dmam0_r()
{
	return m_dmam[0];
}

void v25_common_device::dmam0_w(uint8_t d)
{
	logerror("%06x: DMAM0 set to %02x\n", PC(), d);
	if ((d & 0x60) == 0)
		logerror("        %s mode, memory to memory, %s\n", BIT(d, 7) ? "Single step" : "Burst", BIT(d, 4) ? "words" : "bytes");
	else if ((d & 0x60) != 0x60)
		logerror("        %s mode, %s to %s, %s\n", BIT(d, 7) ? "Single transfer" : "Demand release",
			BIT(d, 5) ? "I/O" : "memory",
			BIT(d, 6) ? "I/O" : "memory",
			BIT(d, 4) ? "words" : "bytes");
	if (BIT(d, 3))
	{
		uint16_t sar = m_internal_ram[0];
		uint16_t dar = m_internal_ram[1];
		uint16_t sarh_darh = m_internal_ram[2];
		uint16_t tc = m_internal_ram[3];
		logerror("        DMA enabled%s (%04x:%04x -> %04x:%04x, %u %s)\n", BIT(d, 2) ? " and triggered" : "",
			sarh_darh & 0xff00, sar,
			(sarh_darh & 0x00ff) << 8, dar,
			tc, BIT(d, 4) ? "words" : "bytes");
	}
	else
		logerror("        DMA not enabled\n");
	m_dmam[0] = d & 0xfc;
}

uint8_t v25_common_device::dmac1_r()
{
	return m_dmac[1];
}

void v25_common_device::dmac1_w(uint8_t d)
{
	logerror("%06x: DMAC1 set to %02x\n", PC(), d);
	logerror("        SAR1 %s after each transfer\n", (d & 0x03) == 0x01 ? "incremented" : (d & 0x03) == 0x02 ? "decremented" : "unmodified");
	logerror("        DAR1 %s after each transfer\n", (d & 0x30) == 0x10 ? "incremented" : (d & 0x30) == 0x20 ? "decremented" : "unmodified");
	m_dmac[1] = d & 0x33;
}

uint8_t v25_common_device::dmam1_r()
{
	return m_dmam[1];
}

void v25_common_device::dmam1_w(uint8_t d)
{
	logerror("%06x: DMAM1 set to %02x\n", PC(), d);
	if ((d & 0x60) == 0)
		logerror("        %s mode, memory to memory, %s\n", BIT(d, 7) ? "Single step" : "Burst", BIT(d, 4) ? "words" : "bytes");
	else if ((d & 0x60) != 0x60)
		logerror("        %s mode, %s to %s, %s\n", BIT(d, 7) ? "Single transfer" : "Demand release",
			BIT(d, 5) ? "I/O" : "memory",
			BIT(d, 6) ? "I/O" : "memory",
			BIT(d, 4) ? "words" : "bytes");
	if (BIT(d, 3))
	{
		uint16_t sar = m_internal_ram[4];
		uint16_t dar = m_internal_ram[5];
		uint16_t sarh_darh = m_internal_ram[6];
		uint16_t tc = m_internal_ram[7];
		logerror("        DMA enabled%s (%04x:%04x -> %04x:%04x, %u %s)\n", BIT(d, 2) ? " and triggered" : "",
			sarh_darh & 0xff00, sar,
			(sarh_darh & 0x00ff) << 8, dar,
			tc, BIT(d, 4) ? "words" : "bytes");
	}
	else
		logerror("        DMA not enabled\n");
	m_dmam[1] = d & 0xfc;
}

uint8_t v25_common_device::dic0_r()
{
	return read_irqcontrol(INTD0, m_priority_intd);
}

void v25_common_device::dic0_w(uint8_t d)
{
	write_irqcontrol(INTD0, d);
	m_priority_intd = d & 0x7;
}

uint8_t v25_common_device::dic1_r()
{
	return read_irqcontrol(INTD1, 7);
}

void v25_common_device::dic1_w(uint8_t d)
{
	write_irqcontrol(INTD1, d);
}

uint8_t v25_common_device::rfm_r()
{
	return m_RFM;
}

void v25_common_device::rfm_w(uint8_t d)
{
	m_RFM = d;
}

uint16_t v25_common_device::wtc_r()
{
	return m_WTC;
}

void v25_common_device::wtc_w(offs_t a, uint16_t d, uint16_t m)
{
	m_WTC = (m_WTC & ~m) | (d & m);
}

uint8_t v25_common_device::flag_r()
{
	return (m_F0 << 3) | (m_F1 << 5);
}

void v25_common_device::flag_w(uint8_t d)
{
	m_F0 = BIT(d, 3);
	m_F1 = BIT(d, 5);
}

uint8_t v25_common_device::prc_r()
{
	uint8_t ret = m_RAMEN ? 0x40 : 0;

	switch (m_TB)
	{
	case 10:
		break;
	case 13:
		ret |= 0x04;
		break;
	case 16:
		ret |= 0x08;
		break;
	case 20:
		ret |= 0x0C;
		break;
	}

	switch (m_PCK)
	{
	case 2:
		break;
	case 4:
		ret |= 0x01;
		break;
	case 8:
		ret |= 0x02;
		break;
	}

	return ret;
}

void v25_common_device::prc_w(uint8_t d)
{
	static const int timebases[4] = { 10, 13, 16, 20 };
	static const int clocks[4] = { 2, 4, 8, 0 };

	logerror("%06x: PRC set to %02x\n", PC(), d);
	m_RAMEN = ((d & 0x40) == 0x40);
	m_TB = timebases[(d & 0x0C) >> 2];
	m_PCK = clocks[d & 0x03];
	if (m_PCK == 0)
	{
		logerror("        Warning: invalid clock divider\n");
		m_PCK = 8;
	}

	unsigned tmp = m_PCK << m_TB;
	attotime time = clocks_to_attotime(tmp);
	m_timers[3]->adjust(time, INTTB, time);
	notify_clock_changed(); // make device_execute_interface pick up the new clocks_to_cycles()

	logerror("        Internal RAM %sabled\n", (m_RAMEN ? "en" : "dis"));
	logerror("        Time base set to 2^%d\n", m_TB);
	logerror("        Clock divider set to %d\n", m_PCK);
}

uint8_t v25_common_device::tbic_r()
{
	return read_irqcontrol(INTTB, 7);
}

void v25_common_device::tbic_w(uint8_t d)
{
	// time base interrupt doesn't support macro service, bank switching or priority control
	write_irqcontrol(INTTB, d & 0xC0);
}

uint8_t v25_common_device::irqs_r()
{
	return m_IRQS;
}

uint8_t v25_common_device::ispr_r()
{
	return m_ISPR;
}

uint8_t v25_common_device::idb_r()
{
	return m_IDB >> 12;
}

void v25_common_device::idb_w(uint8_t d)
{
	m_IDB = (d << 12) | 0xe00;
}

uint8_t v25_common_device::v25_read_byte(unsigned a)
{
	if (((a & 0xffe00) == m_IDB && (m_RAMEN || BIT(a, 8))) || a == 0xfffff)
		return m_data.read_byte(a & 0x1ff);
	else
		return m_program->read_byte(a);
}

uint16_t v25_common_device::v25_read_word(unsigned a)
{
	if (BIT(a, 0))
		return (v25_read_byte(a) | (v25_read_byte(a + 1) << 8));

	// not sure about this - manual says FFFFC-FFFFE are "reserved"
	if (a == 0xffffe)
		return (m_program->read_byte(a) | (m_data.read_byte(0x1ff) << 8));
	else if ((a & 0xffe00) == m_IDB && (m_RAMEN || BIT(a, 8)))
		return m_data.read_word(a & 0x1ff);
	else
		return m_program->read_word(a);
}

void v25_common_device::v25_write_byte(unsigned a, uint8_t d)
{
	if (((a & 0xffe00) == m_IDB && (m_RAMEN || BIT(a, 8))) || a == 0xfffff)
		m_data.write_byte(a & 0x1ff, d);
	else
		m_program->write_byte(a, d);
}

void v25_common_device::v25_write_word(unsigned a, uint16_t d)
{
	if (BIT(a, 0))
	{
		v25_write_byte(a, d);
		v25_write_byte(a + 1, d >> 8);
		return;
	}

	// not sure about this - manual says FFFFC-FFFFE are "reserved"
	if (a == 0xffffe)
	{
		m_program->write_byte(a, d);
		m_data.write_byte(0x1ff, d >> 8);
	}
	else if ((a & 0xffe00) == m_IDB && (m_RAMEN || BIT(a, 8)))
		m_data.write_word(a & 0x1ff, d);
	else
		m_program->write_word(a, d);
}


// m92-pi: the internal data area (MAME's ida_sfr_map) as plain functions.
// 0x000-0x0ff is the register-bank RAM, 0x100-0x1ff the special function registers.
u8 v25_common_device::sfr_r(offs_t a)
{
	switch (a)
	{
	case 0x100: return p0_r();
	case 0x108: return p1_r();
	case 0x110: return p2_r();
	case 0x138: return pt_r();
	case 0x140: return intm_r();
	case 0x144: case 0x145: case 0x146: return ems_r(a - 0x144);
	case 0x14c: return exic0_r();
	case 0x14d: return exic1_r();
	case 0x14e: return exic2_r();
	case 0x160: return rxb0_r();
	case 0x165: return srms0_r();
	case 0x166: return stms0_r();
	case 0x168: return scm0_r();
	case 0x169: return scc0_r();
	case 0x16a: return brg0_r();
	case 0x16b: return sce0_r();
	case 0x16c: return seic0_r();
	case 0x16d: return sric0_r();
	case 0x16e: return stic0_r();
	case 0x170: return rxb1_r();
	case 0x175: return srms1_r();
	case 0x176: return stms1_r();
	case 0x178: return scm1_r();
	case 0x179: return scc1_r();
	case 0x17a: return brg1_r();
	case 0x17b: return sce1_r();
	case 0x17c: return seic1_r();
	case 0x17d: return sric1_r();
	case 0x17e: return stic1_r();
	case 0x180: return u8(tm0_r());  case 0x181: return u8(tm0_r() >> 8);
	case 0x182: return u8(md0_r());  case 0x183: return u8(md0_r() >> 8);
	case 0x188: return u8(tm1_r());  case 0x189: return u8(tm1_r() >> 8);
	case 0x18a: return u8(md1_r());  case 0x18b: return u8(md1_r() >> 8);
	case 0x190: return tmc0_r();
	case 0x191: return tmc1_r();
	case 0x194: case 0x195: case 0x196: return tmms_r(a - 0x194);
	case 0x19c: return tmic0_r();
	case 0x19d: return tmic1_r();
	case 0x19e: return tmic2_r();
	case 0x1a0: return dmac0_r();
	case 0x1a1: return dmam0_r();
	case 0x1a2: return dmac1_r();
	case 0x1a3: return dmam1_r();
	case 0x1ac: return dic0_r();
	case 0x1ad: return dic1_r();
	case 0x1e1: return rfm_r();
	case 0x1e8: return u8(wtc_r());  case 0x1e9: return u8(wtc_r() >> 8);
	case 0x1ea: return flag_r();
	case 0x1eb: return prc_r();
	case 0x1ec: return tbic_r();
	case 0x1ef: return irqs_r();
	case 0x1fc: return ispr_r();
	case 0x1ff: return idb_r();
	default: return 0xff;   // unmapped (MAME's default for an unmapped read)
	}
}

// 16-bit registers written a byte at a time get MAME's treatment: the handler sees the
// byte in its lane and zero in the other
void v25_common_device::sfr_w(offs_t a, u8 d)
{
	const u16 lo = d, hi = u16(d << 8);
	switch (a)
	{
	case 0x100: p0_w(d); break;
	case 0x101: pm0_w(d); break;
	case 0x102: pmc0_w(d); break;
	case 0x108: p1_w(d); break;
	case 0x109: pm1_w(d); break;
	case 0x10a: pmc1_w(d); break;
	case 0x110: p2_w(d); break;
	case 0x111: pm2_w(d); break;
	case 0x112: pmc2_w(d); break;
	case 0x13b: pmt_w(d); break;
	case 0x140: intm_w(d); break;
	case 0x144: case 0x145: case 0x146: ems_w(a - 0x144, d); break;
	case 0x14c: exic0_w(d); break;
	case 0x14d: exic1_w(d); break;
	case 0x14e: exic2_w(d); break;
	case 0x162: txb0_w(d); break;
	case 0x165: srms0_w(d); break;
	case 0x166: stms0_w(d); break;
	case 0x168: scm0_w(d); break;
	case 0x169: scc0_w(d); break;
	case 0x16a: brg0_w(d); break;
	case 0x16c: seic0_w(d); break;
	case 0x16d: sric0_w(d); break;
	case 0x16e: stic0_w(d); break;
	case 0x172: txb1_w(d); break;
	case 0x175: srms1_w(d); break;
	case 0x176: stms1_w(d); break;
	case 0x178: scm1_w(d); break;
	case 0x179: scc1_w(d); break;
	case 0x17a: brg1_w(d); break;
	case 0x17c: seic1_w(d); break;
	case 0x17d: sric1_w(d); break;
	case 0x17e: stic1_w(d); break;
	case 0x180: tm0_w(lo); break;  case 0x181: tm0_w(hi); break;
	case 0x182: md0_w(lo); break;  case 0x183: md0_w(hi); break;
	case 0x188: tm1_w(lo); break;  case 0x189: tm1_w(hi); break;
	case 0x18a: md1_w(lo); break;  case 0x18b: md1_w(hi); break;
	case 0x190: tmc0_w(d); break;
	case 0x191: tmc1_w(d); break;
	case 0x194: case 0x195: case 0x196: tmms_w(a - 0x194, d); break;
	case 0x19c: tmic0_w(d); break;
	case 0x19d: tmic1_w(d); break;
	case 0x19e: tmic2_w(d); break;
	case 0x1a0: dmac0_w(d); break;
	case 0x1a1: dmam0_w(d); break;
	case 0x1a2: dmac1_w(d); break;
	case 0x1a3: dmam1_w(d); break;
	case 0x1ac: dic0_w(d); break;
	case 0x1ad: dic1_w(d); break;
	case 0x1e1: rfm_w(d); break;
	case 0x1e8: wtc_w(0, lo, 0x00ff); break;
	case 0x1e9: wtc_w(0, hi, 0xff00); break;
	case 0x1ea: flag_w(d); break;
	case 0x1eb: prc_w(d); break;
	case 0x1ec: tbic_w(d); break;
	case 0x1ff: idb_w(d); break;
	default: break;
	}
}

u8 v25_common_device::data_access::read_byte(offs_t a)
{
	a &= 0x1ff;
	if (a < 0x100)
		return reinterpret_cast<u8 *>(d->m_internal_ram)[a];
	return d->sfr_r(a);
}

u16 v25_common_device::data_access::read_word(offs_t a)
{
	a &= 0x1fe;
	if (a < 0x100)
		return d->m_internal_ram[a >> 1];
	switch (a)
	{
	case 0x180: return d->tm0_r();
	case 0x182: return d->md0_r();
	case 0x188: return d->tm1_r();
	case 0x18a: return d->md1_r();
	case 0x1e8: return d->wtc_r();
	default: return d->sfr_r(a) | (d->sfr_r(a + 1) << 8);
	}
}

void v25_common_device::data_access::write_byte(offs_t a, u8 v)
{
	a &= 0x1ff;
	if (a < 0x100)
		reinterpret_cast<u8 *>(d->m_internal_ram)[a] = v;
	else
		d->sfr_w(a, v);
}

void v25_common_device::data_access::write_word(offs_t a, u16 v)
{
	a &= 0x1fe;
	if (a < 0x100)
	{
		d->m_internal_ram[a >> 1] = v;
		return;
	}
	switch (a)
	{
	case 0x180: d->tm0_w(v); break;
	case 0x182: d->md0_w(v); break;
	case 0x188: d->tm1_w(v); break;
	case 0x18a: d->md1_w(v); break;
	case 0x1e8: d->wtc_w(0, v, 0xffff); break;
	default: d->sfr_w(a, u8(v)); d->sfr_w(a + 1, u8(v >> 8)); break;
	}
}
