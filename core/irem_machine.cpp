// m92-pi - the scheduler shared by the Irem M92 and M90 machines (see irem_machine.h)
//
// Two CPUs run in slices up to the next timer, the main CPU first; each keeps its own
// time in picoseconds with the fraction carried, so no time is lost or gained.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "irem_machine.h"

irem_machine::~irem_machine()
{
	delete_timers();
}

emu_timer *irem_machine::new_timer(save_registry &save, std::function<void(s32)> cb)
{
	emu_timer *t = new emu_timer;
	t->sched = this;
	t->cb = std::move(cb);
	timers.push_back(t);
	save.add(&t->expire, sizeof(t->expire));
	save.add(&t->period, sizeof(t->period));
	save.add(&t->param, sizeof(t->param));
	return t;
}

void irem_machine::delete_timers()
{
	for (emu_timer *t : timers)
		delete t;
	timers.clear();
}

void irem_machine::register_save(save_registry &save)
{
	for (auto &s : m_slot)
	{
		save.add(&s.cpu->m_local_time, sizeof(s.cpu->m_local_time));
		save.add(&s.ps_frac, sizeof(s.ps_frac));
	}
}

void irem_machine::reset_scheduler()
{
	m_now = attotime::zero;
	for (auto &s : m_slot)
	{
		s.cpu->m_local_time = attotime::zero;
		s.ps_frac = 0;
	}
	for (emu_timer *t : timers)
		t->reset();
	m_deferred.clear();
}

attotime irem_machine::current_time() const
{
	if (!m_running)
		return m_now;
	const cpu_slot &s = *m_running;
	const s64 done = s.requested - *s.cpu->m_icountptr;
	const unsigned __int128 t = (unsigned __int128)(done > 0 ? done : 0) * 1000000000000ULL + u64(s.ps_frac);
	return attotime(s.cpu->m_local_time.ps + s64(t / s.cycles_per_sec));
}

// cycles from the CPU's own time to "until", rounded up
static int cycles_until(const attotime &local, s64 frac, u64 cps, const attotime &until)
{
	if (until.ps <= local.ps)
		return 0;
	const unsigned __int128 need = (unsigned __int128)(until.ps - local.ps) * cps;
	const unsigned __int128 have = u64(frac);
	if (need <= have)
		return 0;
	const unsigned __int128 c = (need - have + 999999999999ULL) / 1000000000000ULL;
	return c > 0x7fffffff ? 0x7fffffff : int(c);
}

void irem_machine::timer_changed(emu_timer *t)
{
	if (!m_running || t->expire.is_never() || !(t->expire < m_slice_end))
		return;
	// a timer now falls inside the slice being run: stop the CPU there
	m_slice_end = t->expire;
	cpu_slot &s = *m_running;
	int &ic = *s.cpu->m_icountptr;
	const int done = s.requested - ic;
	const int need = cycles_until(s.cpu->m_local_time, s.ps_frac, s.cycles_per_sec, t->expire);
	if (need < s.requested)
	{
		ic = need > done ? need - done : 0;
		s.requested = done + ic;
	}
}

void irem_machine::defer(std::function<void()> fn)
{
	m_deferred.push_back(std::move(fn));
	if (m_running)
	{
		// end the running CPU's slice after this instruction: the other CPU catches up to
		// here before the action happens, as MAME's synchronize()
		cpu_slot &s = *m_running;
		int &ic = *s.cpu->m_icountptr;
		const int done = s.requested - ic;
		ic = 0;
		s.requested = done;
		m_slice_end = current_time();
	}
}

void irem_machine::run_cpu(int slot, attotime until)
{
	cpu_slot &s = m_slot[slot];
	cpu_device *c = s.cpu;
	if (!(c->m_local_time < until))
		return;
	if (cpu_held(slot))
	{
		c->m_local_time = until;
		s.ps_frac = 0;
		return;
	}
	cpu_rate(slot);
	const int cycles = cycles_until(c->m_local_time, s.ps_frac, s.cycles_per_sec, until);
	if (cycles <= 0)
		return;
	s.requested = cycles;
	*c->m_icountptr = cycles;
	m_running = &s;
	c->execute_run();
	m_running = nullptr;
	const s64 done = s.requested - *c->m_icountptr;
	const unsigned __int128 t = (unsigned __int128)(done > 0 ? done : 0) * 1000000000000ULL + u64(s.ps_frac);
	c->m_local_time.ps += s64(t / s.cycles_per_sec);
	s.ps_frac = s64(t % s.cycles_per_sec);
}

void irem_machine::run_until(attotime target)
{
	while (m_now < target || m_now.ps == target.ps)
	{
		// fire what is due
		bool fired = true;
		while (fired)
		{
			fired = false;
			emu_timer *first = nullptr;
			for (emu_timer *t : timers)
				if (!t->expire.is_never() && t->expire <= m_now && (!first || t->expire < first->expire))
					first = t;
			if (first)
			{
				const s32 p = first->param;
				if (!first->period.is_never())
					first->expire = first->expire + first->period;
				else
					first->expire = attotime::never;
				first->cb(p);
				fired = true;
			}
		}
		if (!(m_now < target))
			break;

		attotime next = target;
		for (emu_timer *t : timers)
			if (!t->expire.is_never() && t->expire < next)
				next = t->expire;
		m_slice_end = next;

		run_cpu(0, m_slice_end);
		run_cpu(1, m_slice_end);
		// a CPU may have stopped early (deferred action): the other one has been run to the
		// same point since m_slice_end only moves backwards
		m_now = m_slice_end;

		if (!m_deferred.empty())
		{
			std::vector<std::function<void()>> d;
			d.swap(m_deferred);
			for (auto &fn : d)
				fn();
		}
	}
}
