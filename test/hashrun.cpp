// m92-pi - regression check: one line per frame with the picture and sound hashes, for
// the same scripted inputs every time (coins, start, pseudo-random joystick and buttons).
// Two builds giving the same file emulate identically.
//
//   g++ -std=c++20 -O2 -I core -I third_party/mame -I third_party -I third_party/miniz
//       -o hashrun test/hashrun.cpp core/m92.cpp core/m92_video.cpp core/m92_sound.cpp
//       third_party/mame/irem_cpu.cpp third_party/mame/nec/{nec,v25,v25sfr}.cpp
//       third_party/ymfm/ymfm_opm.cpp -x c third_party/miniz/miniz.c
//   ./hashrun <rom folder> <set> <frames> <out.txt>     MC=1: lines drawn late, as multicore
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m92.h"
#include "zipfile.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static uint32_t fnv(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static const uint32_t FNV_START = 2166136261u;

static uint32_t s_fb[320 * 240];
static m92_machine *s_machine;
static std::vector<std::pair<int, m92_machine::line_regs>> s_queue;	// MC=1: lines not drawn yet

static void draw_queued(void *)
{
	for (auto &job : s_queue)
		s_machine->render_line(job.second, job.first, s_fb + job.first * 320);
	s_queue.clear();
}

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		printf("usage: hashrun <rom folder> <set> <frames> <out.txt>\n");
		return 1;
	}
	const m92_game *g = m92_find_game(argv[2]);
	const int frames = atoi(argv[3]);
	FILE *out = fopen(argv[4], "w");
	zipfile zip(std::string(argv[1]) + "/" + g->name + ".zip");
	zipfile parent(g->parent[0] ? std::string(argv[1]) + "/" + g->parent + ".zip" : "");

	m92_machine m;
	std::string missing;
	auto fetch = [&](const char *n, uint32_t c, uint32_t s, uint8_t *d) { return zip.fetch(n, c, s, d) || parent.fetch(n, c, s, d); };
	if (!m.load(g, fetch, missing))
	{
		printf("%s missing %s\n", g->name, missing.c_str());
		return 2;
	}
	m.set_dsw(g->inputs->dsw_default, g->inputs->dsw3_default);
	s_machine = &m;

	const bool mc = getenv("MC") != nullptr;
	if (mc)
	{
		m.set_line_sink([](int y, const m92_machine::line_regs &r) { s_queue.push_back({y, r}); });
		m.set_video_sync(draw_queued, nullptr);
	}
	else
		m.set_line_callback([](int y, const uint32_t *p) { memcpy(s_fb + y * 320, p, 1280); });

	uint32_t rng = 12345, all = FNV_START;
	uint8_t pad = 0;
	double total_ms = 0;
	for (int f = 0; f < frames; f++)
	{
		// coin at 300 and 330, start at 400, every 30 s; a new pad state every 20 frames
		uint8_t sys = 0;
		const int c = f % 1800;
		if (c >= 300 && c < 306) sys |= 4;
		if (c >= 330 && c < 336) sys |= 4;
		if (c >= 400 && c < 406) sys |= 1;
		if (f % 20 == 0)
		{
			rng = rng * 1103515245 + 12345;
			pad = uint8_t(rng >> 16);
			if ((pad & 3) == 3) pad &= ~1;		// not left and right together
			if ((pad & 12) == 12) pad &= ~4;	// nor up and down
		}
		m.set_system(sys);
		m.set_player(0, pad);
		m.set_player(1, uint8_t(pad ^ 0x55) & 0xf0);

		auto t0 = std::chrono::steady_clock::now();
		m.run_frame();
		if (mc)
			draw_queued(nullptr);
		total_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

		uint32_t h = FNV_START;
		for (uint32_t p : s_fb)
			h = fnv(h, p);
		uint32_t ha = FNV_START;
		int16_t ab[4096];
		int n;
		while ((n = m.audio_read(ab, 2048)) > 0)
			for (int i = 0; i < n * 2; i++)
				ha = fnv(ha, uint16_t(ab[i]));
		fprintf(out, "%d %08x %08x\n", f, h, ha);
		all = fnv(all, h);
	}
	printf("%-10s %6.3f ms/frame  all %08x\n", g->name, total_ms / frames, all);
	fclose(out);
}
