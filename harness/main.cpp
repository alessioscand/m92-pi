// m92-pi - Linux test harness
//
//   m92test <rom folder> <game> [options]
//     -f N          frames to run (default 600)
//     -s N          write a PPM screenshot every N frames (default: only the last frame)
//     -o DIR        output folder (default .)
//     -w            write the sound to <game>.wav
//     -t            save a state at the middle frame, run on, load it, run again and
//                   check both runs give the same pictures and sound
//     -turbo        main CPU at 20 MHz, like the MiSTer core's Turbo
//     -coin F       press coin 1 at frame F, start 1 a second later
//
// ROMs are looked up by CRC (then by name) in <game>.zip and its parent's zip, as MAME sets.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m92.h"
#include "m92_video.h"
#include "../test/zipfile.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void write_ppm(const std::string &path, const std::vector<uint32_t> &fb)
{
	FILE *f = fopen(path.c_str(), "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", m92_machine::SCREEN_W, m92_machine::SCREEN_H);
	for (uint32_t p : fb)
	{
		fputc(p >> 16, f); fputc(p >> 8, f); fputc(p, f);
	}
	fclose(f);
}

static void write_wav(const std::string &path, const std::vector<int16_t> &pcm)
{
	FILE *f = fopen(path.c_str(), "wb");
	if (!f)
		return;
	const uint32_t data = uint32_t(pcm.size() * 2), rate = m92_machine::AUDIO_RATE;
	auto put32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto put16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); put32(36 + data); fwrite("WAVEfmt ", 1, 8, f);
	put32(16); put16(1); put16(2); put32(rate); put32(rate * 4); put16(4); put16(16);
	fwrite("data", 1, 4, f); put32(data);
	fwrite(pcm.data(), 2, pcm.size(), f);
	fclose(f);
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: %s <rom folder> <game> [-f frames] [-s every] [-o dir] [-w] [-t] [-turbo] [-coin frame] [-hitest]\n", argv[0]);
		fprintf(stderr, "games:");
		for (int i = 0; i < m92_game_count(); i++)
			fprintf(stderr, " %s", m92_game_at(i)->name);
		fprintf(stderr, "\n");
		return 1;
	}
	const std::string dir = argv[1];
	const char *name = argv[2];
	int frames = 600, every = 0, coin = -1;
	bool wav = false, statetest = false, turbo = false;
	std::string out = ".";
	for (int i = 3; i < argc; i++)
	{
		if (!strcmp(argv[i], "-f") && i + 1 < argc) frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-s") && i + 1 < argc) every = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(argv[i], "-coin") && i + 1 < argc) coin = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-w")) wav = true;
		else if (!strcmp(argv[i], "-t")) statetest = true;
		else if (!strcmp(argv[i], "-turbo")) turbo = true;
	}

	const m92_game *game = m92_find_game(name);
	if (!game)
	{
		fprintf(stderr, "unknown game %s\n", name);
		return 1;
	}
	zipfile z(dir + "/" + game->name + ".zip");
	zipfile zp(game->parent[0] ? dir + "/" + game->parent + ".zip" : std::string());
	auto fetch = [&](const char *rn, uint32_t crc, uint32_t size, uint8_t *dst) {
		return z.fetch(rn, crc, size, dst) || zp.fetch(rn, crc, size, dst);
	};

	m92_machine m;
	std::string missing;
	if (!m.load(game, fetch, missing))
	{
		fprintf(stderr, "%s: missing ROM %s\n", game->name, missing.c_str());
		return 2;
	}
	m.set_dsw(game->inputs->dsw_default, game->inputs->dsw3_default);
	if (turbo)
		m.set_cpu_speed(222);
	printf("%s: %s (%s %s)\n", game->name, game->desc, game->manufacturer, game->year);
	for (int i = 3; i < argc; i++)
		if (!strcmp(argv[i], "-hitest"))
		{
			// hiscore rows: size, when the game sets its table up, put / get
			const size_t n = m.hi_size();
			printf("hiscore bytes: %zu\n", n);
			if (!n)
				return 0;
			const m92_hiscore *h = m92_find_hiscore(game->name);
			int f = 0;
			while (f < 3600 && !m.hi_ready()) { m.run_frame(); f++; }
			std::vector<uint8_t> t(n), back(n);
			if (m.hi_ready())
			{
				// a real game: its table, changed (as a .hi file would), must stay as put
				printf("table ready at frame %d\n", f);
				m.hi_get(t.data());
				for (size_t k = 1; k + 1 < n; k++)
					if (t[k] >= 'A' && t[k] <= 'Z')
						t[k] = t[k] == 'Z' ? 'A' : t[k] + 1;
				m.hi_put(t.data());
				for (int k = 0; k < 1800; k++) m.run_frame();
				m.hi_get(back.data());
				printf("after 30 s the table is still the one put: %s\n", t == back ? "yes" : "NO");
				return t == back ? 0 : 3;
			}
			// a synthetic game: no table; put / get through the address space
			printf("no table after 3600 frames (synthetic game)\n");
			size_t o = 0;
			for (int r = 0; r < h->count; r++)
				for (uint32_t k = 0; k < h->rows[r].size; k++, o++)
					t[o] = k == 0 ? h->rows[r].first : k == h->rows[r].size - 1u ? h->rows[r].last : uint8_t(0x41 + (o % 26));
			m.hi_put(t.data());
			m.hi_get(back.data());
			printf("put/get identical: %s, ready now: %s\n", t == back ? "yes" : "NO", m.hi_ready() ? "yes" : "NO");
			return t == back && m.hi_ready() ? 0 : 3;
		}

	std::vector<uint32_t> fb(m92_machine::SCREEN_W * m92_machine::SCREEN_H, 0);
	m.set_line_callback([&](int y, const uint32_t *px) {
		memcpy(&fb[size_t(y) * m92_machine::SCREEN_W], px, m92_machine::SCREEN_W * 4);
	});

	std::vector<int16_t> pcm;
	auto drain = [&]() {
		int16_t buf[2048];
		int n;
		while ((n = m.audio_read(buf, 1024)) > 0)
			pcm.insert(pcm.end(), buf, buf + n * 2);
	};

	std::vector<uint8_t> state;
	std::vector<uint32_t> hash_a, hash_b;
	auto fbhash = [&]() {
		uint32_t h = 2166136261u;
		for (uint32_t p : fb) { h ^= p; h *= 16777619u; }
		return h;
	};

	const auto t0 = std::chrono::steady_clock::now();
	for (int f = 0; f < frames; f++)
	{
		uint8_t sys = 0;
		if (coin >= 0 && f >= coin && f < coin + 6) sys |= 0x04;
		if (coin >= 0 && f >= coin + 60 && f < coin + 66) sys |= 0x01;
		m.set_system(sys);
		m.run_frame();
		drain();
		if (statetest && f == frames / 2)
		{
			m.save_state(state);
			printf("state saved at frame %d: %zu bytes\n", f, state.size());
		}
		if (statetest && f > frames / 2)
			hash_a.push_back(fbhash());
		if (every && (f + 1) % every == 0)
		{
			char p[512];
			snprintf(p, sizeof p, "%s/%s_%05d.ppm", out.c_str(), game->name, f + 1);
			write_ppm(p, fb);
		}
		if (getenv("CTLTRACE") && (f + 1) % 10 == 0)
			printf("f%d mc %04x %04x %04x %04x vc %04x pf0 %04x %04x pf1 %04x %04x pf2 %04x %04x dma %04x %04x %04x\n", f + 1,
				m.video()->master_control(0), m.video()->master_control(1), m.video()->master_control(2), m.video()->master_control(3),
				m.video()->videocontrol(), m.video()->pf_control(0)[0], m.video()->pf_control(0)[2], m.video()->pf_control(1)[0], m.video()->pf_control(1)[2],
				m.video()->pf_control(2)[0], m.video()->pf_control(2)[2], m.video()->dmacontrol()[0], m.video()->dmacontrol()[1], m.video()->dmacontrol()[2]);
		if (getenv("PIXDUMP") && f + 1 == atoi(getenv("PIXDUMP")))
		{
			int px = atoi(getenv("PX")), py = atoi(getenv("PY"));
			const_cast<m92_video *>(m.video())->debug_pixel(py + 8, px);
			printf("final %06x\n", fb[py * 320 + px]);
		}
		if (getenv("SPRDUMP") && f + 1 == atoi(getenv("SPRDUMP")))
		{
			const uint16_t *s = m.video()->sprite_buffer();
			for (int l = 0; l < 3; l++)
				printf("layer %d master %04x ctrl %04x %04x %04x %04x\n", l, m.video()->master_control(l), m.video()->pf_control(l)[0], m.video()->pf_control(l)[1], m.video()->pf_control(l)[2], m.video()->pf_control(l)[3]);
			printf("master3 %04x\n", m.video()->master_control(3));
			{
				unsigned nz = 0, sum = 0;
				for (int i = 0; i < 0x8000; i++) { if (m.video()->vram()[i]) nz++; sum += m.video()->vram()[i]; }
				printf("vram nonzero words %u sum %08x\n", nz, sum);
			}
			printf("dma %04x %04x %04x vc %04x\n", m.video()->dmacontrol()[0], m.video()->dmacontrol()[1], m.video()->dmacontrol()[2], m.video()->videocontrol());
			for (int o = 0; o < 0x400; o += 4)
				if (s[o] | s[o+1] | s[o+2] | s[o+3])
					printf("%03x: %04x %04x %04x %04x\n", o, s[o], s[o+1], s[o+2], s[o+3]);
		}
		if ((f + 1) % 60 == 0)
			printf("frame %5d  main pc %05x  sound pc %05x  ram %02x%02x %02x\n", f + 1, m.main_pc(), m.sound_pc(), m.main_ram()[1], m.main_ram()[0], m.main_ram()[2]);
	}
	const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	printf("%d frames in %.2f s: %.1f fps (%.0f%% of real time)\n", frames, secs, frames / secs, 100.0 * frames / 60.0 / secs);

	char p[512];
	snprintf(p, sizeof p, "%s/%s_last.ppm", out.c_str(), game->name);
	write_ppm(p, fb);
	if (wav)
	{
		snprintf(p, sizeof p, "%s/%s.wav", out.c_str(), game->name);
		write_wav(p, pcm);
		printf("sound: %zu frames (%.2f s)\n", pcm.size() / 2, pcm.size() / 2.0 / m92_machine::AUDIO_RATE);
	}

	if (statetest)
	{
		if (!m.load_state(state))
		{
			printf("STATE TEST: load failed\n");
			return 3;
		}
		for (int f = frames / 2 + 1; f < frames; f++)
		{
			m.run_frame();
			drain();
			hash_b.push_back(fbhash());
		}
		printf("STATE TEST: %s (%zu frames compared)\n", hash_a == hash_b ? "identical" : "DIFFERENT", hash_a.size());
	}
	return 0;
}
