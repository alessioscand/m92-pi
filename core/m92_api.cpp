// m92-pi - C interface to the M92 core (see m92_api.h)
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m92_api.h"
#include "m92.h"
#include "m90.h"
#include <cstring>
#include <new>

static irem_machine *s_m;

// the M92 sets first, then the M90 ones
struct game_info
{
	const char *name, *parent, *desc, *year, *maker;
	const m92_inputs *inputs;
	int width, height;
};

static bool info(int n, game_info &gi)
{
	if (const m92_game *g = m92_game_at(n))
	{
		gi = { g->name, g->parent, g->desc, g->year, g->manufacturer, g->inputs,
		       m92_machine::SCREEN_W, m92_machine::SCREEN_H };
		return true;
	}
	if (const m90_game *g = m90_game_at(n - m92_game_count()))
	{
		gi = { g->name, g->parent, g->desc, g->year, g->manufacturer, g->inputs, g->vis_w, g->vis_h };
		return true;
	}
	return false;
}

static const m92_inputs *inputs(int n)
{
	game_info gi;
	return info(n, gi) ? gi.inputs : nullptr;
}

int m92_num_games(void) { return m92_game_count() + m90_game_count(); }

int m92_find(const char *name)
{
	game_info gi;
	for (int i = 0; info(i, gi); i++)
		if (!strcmp(gi.name, name))
			return i;
	return -1;
}

#define FIELD(f, none) { game_info gi; return info(n, gi) ? gi.f : none; }
const char *m92_game_name(int n) FIELD(name, "")
const char *m92_game_parent(int n) FIELD(parent, "")
const char *m92_game_desc(int n) FIELD(desc, "")
const char *m92_game_year(int n) FIELD(year, "")
const char *m92_game_maker(int n) FIELD(maker, "")
int m92_game_width(int n) FIELD(width, 320)
int m92_game_height(int n) FIELD(height, 240)
#undef FIELD
int m92_game_players(int n) { return inputs(n) ? inputs(n)->players : 2; }
int m92_game_buttons(int n) { return inputs(n) ? inputs(n)->buttons : 2; }

int m92_dip_count(int n)
{
	if (!inputs(n))
		return 0;
	int c = 0;
	for (const m92_dip *d = inputs(n)->dips; d->name; d++)
		c++;
	return c;
}

int m92_dip_get(int n, int i, TM92Dip *p)
{
	if (i < 0 || i >= m92_dip_count(n))
		return 0;
	const m92_dip &d = inputs(n)->dips[i];
	p->pName = d.name;
	p->pLocation = d.loc;
	p->nMask = d.mask;
	p->nDefault = d.def;
	p->bBank3 = d.bank3;
	p->nSettings = d.count;
	p->nCondMask = d.cond_mask;
	p->nCondValue = d.cond_value;
	p->nCondOp = d.cond_op;
	return 1;
}

const char *m92_dip_setting_name(int n, int i, int s, uint16_t *value)
{
	if (i < 0 || i >= m92_dip_count(n))
		return nullptr;
	const m92_dip &d = inputs(n)->dips[i];
	if (s < 0 || s >= d.count)
		return nullptr;
	const m92_dip_setting &set = inputs(n)->settings[d.first + s];
	if (value)
		*value = set.value;
	return set.name;
}

uint16_t m92_dsw_default(int n) { return inputs(n) ? inputs(n)->dsw_default : 0xffff; }
uint8_t m92_dsw3_default(int n) { return inputs(n) ? inputs(n)->dsw3_default : 0xff; }

int m92_load(int n, TM92Fetch *fetch, void *ctx, char *missing, size_t nmissing)
{
	m92_unload();
	const m92_inputs *in = inputs(n);
	if (!in)
		return 0;
	auto rom = [fetch, ctx](const char *name, uint32_t crc, uint32_t size, uint8_t *dst) {
		return fetch(ctx, name, crc, size, dst) != 0;
	};
	std::string miss;
	bool ok;
	if (const m92_game *g = m92_game_at(n))
	{
		m92_machine *m = new (std::nothrow) m92_machine;
		if (!m)
			return 0;
		s_m = m;
		ok = m->load(g, rom, miss);
	}
	else
	{
		m90_machine *m = new (std::nothrow) m90_machine;
		if (!m)
			return 0;
		s_m = m;
		ok = m->load(m90_game_at(n - m92_game_count()), rom, miss);
	}
	if (!ok)
	{
		if (missing && nmissing)
		{
			strncpy(missing, miss.c_str(), nmissing - 1);
			missing[nmissing - 1] = '\0';
		}
		m92_unload();
		return 0;
	}
	s_m->set_dsw(in->dsw_default, in->dsw3_default);
	return 1;
}

void m92_unload(void)
{
	delete s_m;
	s_m = nullptr;
}

void m92_reset(void) { if (s_m) s_m->reset(); }
void m92_run_frame(void) { if (s_m) s_m->run_frame(); }
void m92_set_player(int p, uint8_t bits) { if (s_m) s_m->set_player(p, bits); }
void m92_set_system(uint8_t bits) { if (s_m) s_m->set_system(bits); }
void m92_set_dsw(uint16_t dsw, uint8_t dsw3) { if (s_m) s_m->set_dsw(dsw, dsw3); }
void m92_set_cpu_speed(int pct) { if (s_m) s_m->set_cpu_speed(pct); }
int m92_audio_read(int16_t *dst, int max) { return s_m ? s_m->audio_read(dst, max) : 0; }

void m92_set_line_callback(TM92LineCB *cb, void *ctx)
{
	if (!s_m)
		return;
	if (cb)
		s_m->set_line_callback([cb, ctx](int y, const uint32_t *rgb) { cb(ctx, y, rgb); });
	else
		s_m->set_line_callback(nullptr);
}

void m92_set_line_hook(TM92LineHook *hook, void *ctx)
{
	if (!s_m)
		return;
	if (hook)
		s_m->set_line_hook([hook, ctx](int line) { hook(ctx, line); });
	else
		s_m->set_line_hook(nullptr);
}

void m92_set_line_sink(TM92LineSink *sink, void *ctx)
{
	if (!s_m)
		return;
	static_assert(sizeof(TM92LineRegs) >= irem_machine::LINE_REGS_BYTES, "line regs");
	if (sink)
		s_m->set_line_sink([sink, ctx](int y, const void *r) {
			TM92LineRegs regs;
			memcpy(&regs, r, irem_machine::LINE_REGS_BYTES);
			sink(ctx, y, &regs);
		});
	else
		s_m->set_line_sink(nullptr);
}

void m92_set_video_sync(void (*fn)(void *), void *ctx) { if (s_m) s_m->set_video_sync(fn, ctx); }

void m92_render_line(const TM92LineRegs *regs, int y, uint32_t *out)
{
	if (s_m)
		s_m->render_line(regs, y, out);
}

size_t m92_state_size(void) { return s_m ? s_m->state_size() : 0; }

int m92_state_save(void *buf, size_t size)
{
	if (!s_m)
		return 0;
	std::vector<uint8_t> v;
	if (!s_m->save_state(v) || v.size() > size)
		return 0;
	memcpy(buf, v.data(), v.size());
	return int(v.size());
}

int m92_state_load(const void *buf, size_t size)
{
	if (!s_m)
		return 0;
	std::vector<uint8_t> v(static_cast<const uint8_t *>(buf), static_cast<const uint8_t *>(buf) + size);
	return s_m->load_state(v) ? 1 : 0;
}

uint8_t *m92_eeprom(size_t *size)
{
	size_t n = 0;
	uint8_t *p = s_m ? s_m->eeprom(n) : nullptr;
	if (size)
		*size = p ? n : 0;
	return p;
}

size_t m92_hi_size(void) { return s_m ? s_m->hi_size() : 0; }
int m92_hi_ready(void) { return s_m && s_m->hi_ready() ? 1 : 0; }
void m92_hi_get(uint8_t *dst) { if (s_m) s_m->hi_get(dst); }
void m92_hi_put(const uint8_t *src) { if (s_m) s_m->hi_put(src); }

uint32_t m92_main_pc(void) { return s_m ? s_m->main_pc() : 0; }
uint32_t m92_sound_pc(void) { return s_m ? s_m->sound_pc() : 0; }
