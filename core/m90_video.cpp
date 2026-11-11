// m92-pi - Irem M90 video (see m90_video.h)
//
// Control registers (I/O 0x80-0x8f, one word each):
//   0, 2: layer 0 y / x scroll     4, 6: layer 1 y / x scroll
//   10, 12: layer 0 / 1 control: bits 0-1 VRAM page, 2 wide (128 tiles), 4 off,
//           5 row scroll (table at 0xf000 / 0xf400), 6 row select (0xf800 / 0xfc00)
//   14: bit 2 picture off; bits 0-1 which sprite colours go over priority tiles
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m90_video.h"
#include <cstring>

// ---------------------------------------------------------------- graphics decoding

void m90_video::decode_gfx(const u8 *gfx, size_t size)
{
	// MAME: one plane in each quarter of the region, the first quarter the least
	// significant (planes RGN_FRAC(3,4)..RGN_FRAC(0,4), the first listed the MSB)
	const size_t q = size / 4;

	m_char_count = u32(q / 8);
	m_chars.assign(size_t(m_char_count) * 64, 0);
	for (u32 c = 0; c < m_char_count; c++)
		for (int y = 0; y < 8; y++)
		{
			const size_t o = size_t(c) * 8 + y;
			const u8 p3 = gfx[3 * q + o], p2 = gfx[2 * q + o], p1 = gfx[q + o], p0 = gfx[o];
			u8 *dst = &m_chars[size_t(c) * 64 + y * 8];
			for (int x = 0; x < 8; x++)
			{
				const int b = 7 - x;
				dst[x] = u8((((p3 >> b) & 1) << 3) | (((p2 >> b) & 1) << 2) | (((p1 >> b) & 1) << 1) | ((p0 >> b) & 1));
			}
		}

	// sprites: 16x16, 32 bytes per plane: rows of the left half, then of the right
	m_sprite_count = u32(q / 32);
	m_sprites.assign(size_t(m_sprite_count) * 256, 0);
	for (u32 c = 0; c < m_sprite_count; c++)
		for (int y = 0; y < 16; y++)
			for (int x = 0; x < 16; x++)
			{
				const size_t byte = size_t(c) * 32 + (x < 8 ? y : 16 + y);
				const int b = 7 - (x & 7);
				m_sprites[size_t(c) * 256 + y * 16 + x] = u8((((gfx[3 * q + byte] >> b) & 1) << 3)
						| (((gfx[2 * q + byte] >> b) & 1) << 2) | (((gfx[q + byte] >> b) & 1) << 1) | ((gfx[byte] >> b) & 1));
			}

	if (!m_char_count) { m_char_count = 1; m_chars.assign(64, 0); }
	if (!m_sprite_count) { m_sprite_count = 1; m_sprites.assign(256, 0); }
	auto pow2_mask = [](u32 n) { return (n && !(n & (n - 1))) ? n - 1 : 0u; };
	m_char_mask = pow2_mask(m_char_count);
	m_sprite_mask = pow2_mask(m_sprite_count);
}

void m90_video::reset()
{
	memset(m_vram, 0, sizeof(m_vram));
	memset(m_palette, 0, sizeof(m_palette));
	memset(m_rgb, 0, sizeof(m_rgb));
	memset(m_ctrl, 0, sizeof(m_ctrl));
}

void m90_video::register_save(save_registry &s)
{
	s.add(m_vram, sizeof(m_vram));
	s.add(m_palette, sizeof(m_palette));
	s.add(m_rgb, sizeof(m_rgb));
	s.add(m_ctrl, sizeof(m_ctrl));
}

void m90_video::palette_set(int index, u16 data)
{
	index &= 0x1ff;
	m_palette[index] = data;
	// xBGR 555 -> 0x00RRGGBB (MAME's pal5bit)
	const unsigned r = data & 0x1f, g = (data >> 5) & 0x1f, b = (data >> 10) & 0x1f;
	m_rgb[index] = ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

void m90_video::palette_w(offs_t offset, u16 data, u16 mem_mask)
{
	sync();
	u16 v = m_palette[offset & 0x1ff];
	COMBINE_DATA(&v);
	palette_set(int(offset), v);
}

// ---------------------------------------------------------------- drawing

// One layer of bitmap line y into m_color[layer] / m_prio[layer]. MAME's tilemaps:
// x scroll + 2 (layer 0) or - 2 (layer 1), + 256 when wide; row scroll indexed by
// the tilemap row, row select by the screen line. A layer that is off gives pen 0
// of colour 0, transparent.
void m90_video::draw_layer(const line_regs &r, int layer, int y)
{
	u16 *color = m_color[layer];
	u8 *prio = m_prio[layer];
	const u16 ctl = r.ctrl[5 + layer];
	if (ctl & 0x10)
	{
		memset(color, 0, sizeof(m_color[0]));
		memset(prio, 0, sizeof(m_prio[0]));
		return;
	}

	const bool wide = ctl & 0x04;
	const u32 base = (wide ? (ctl & 2) : (ctl & 3)) << 13;     // in words
	const int cols = wide ? 128 : 64;
	const int wmask = wide ? 1023 : 511;

	int ty;
	if (ctl & 0x40)
		ty = (y + m_vram[(0xf800 / 2 + 0x200 * layer + y) & 0x7fff]) & 511;
	else
		ty = (y + r.ctrl[2 * layer]) & 511;
	int scrollx;
	if (ctl & 0x20)
		scrollx = m_vram[(0xf000 / 2 + 0x200 * layer + ty) & 0x7fff];
	else
		scrollx = r.ctrl[2 * layer + 1];
	const int xoff = scrollx + (layer ? -2 : 2) + (wide ? 256 : 0);

	const int row = ty >> 3, py = ty & 7;
	int tx = (m_x0 + xoff) & wmask;
	int x = 0;
	while (x < m_w)
	{
		const u32 a = (base + 2 * (row * cols + (tx >> 3))) & 0x7fff;
		const u16 attr = m_vram[(a + 1) & 0x7fff];
		const u32 tile = char_index(m_vram[a]);
		const u16 pal = u16((attr & 0x0f) << 4);
		const bool flipx = attr & 0x40, flipy = attr & 0x80;
		// over the sprites: pens 8-15 with bit 4, every pen but 0 with bit 5
		const u8 pmask = (attr & 0x20) ? 0x0f : (attr & 0x10) ? 0x08 : 0x00;
		const u8 *src = &m_chars[size_t(tile) * 64 + (flipy ? 7 - py : py) * 8];
		for (int px = tx & 7; px < 8 && x < m_w; px++, x++, tx++)
		{
			const u8 pen = src[flipx ? 7 - px : px];
			color[x] = u16(pal | pen);
			prio[x] = (pen & pmask) ? 1 : 0;
		}
		tx &= wmask;
	}
}

// The 84 sprite entries at VRAM 0xee00, three words each; a later entry is drawn
// over an earlier one (MAME draws them backwards, the first drawn pixel winning).
//   word 0: f hh cccc yyyyyyyyy - flip y, height 16 << h, colour, y
//   word 1: code
//   word 2: ...... x . xxxxxxxxx - flip x (bit 9), x
void m90_video::draw_sprites(int y)
{
	const u16 *spr = &m_vram[0xee00 / 2];
	u8 *line = m_obj + 16;
	for (int n = 0; n < 84; n++, spr += 3)
	{
		const u16 w0 = spr[0], w2 = spr[2];
		const int multi = 1 << ((w0 >> 13) & 3);
		const int top = 512 - (w0 & 0x1ff) - 16 * multi;
		const int k = y - top;
		if (k < 0 || k >= 16 * multi)
			continue;
		const bool fy = w0 & 0x8000, fx = w2 & 0x200;
		const int i = k >> 4;
		const u32 code = sprite_index(u32(spr[1]) + u32(fy ? multi - 1 - i : i));
		const u8 *src = &m_sprites[size_t(code) * 256 + (fy ? 15 - (k & 15) : (k & 15)) * 16];
		const u8 colour = u8(((w0 >> 9) & 0x0f) << 4);
		const int bx = (w2 & 0x1ff) - 16 - m_x0;
		if (bx <= -16 || bx >= m_w)
			continue;
		u8 *dst = line + bx;
		for (int px = 0; px < 16; px++)
			if (const u8 pen = src[fx ? 15 - px : px])
				dst[px] = u8(colour | pen);
	}
}

void m90_video::render_line(const line_regs &r, int y, u32 *out)
{
	if (r.ctrl[7] & 0x04)
	{
		for (int x = 0; x < m_w; x++)        // picture off: colour 0
			out[x] = m_rgb[0];
		return;
	}

	draw_layer(r, 0, y);
	draw_layer(r, 1, y);
	memset(m_obj, 0, sizeof(m_obj));
	draw_sprites(y);

	const u16 vc = r.ctrl[7];
	const u8 *obj = m_obj + 16;
	for (int x = 0; x < m_w; x++)
	{
		// layer 0 where it has a pen, else layer 1 (pen 0 included)
		u16 c = (m_color[0][x] & 0x0f) ? m_color[0][x] : m_color[1][x];
		if (const u8 o = obj[x])
		{
			bool show = !m_prio[0][x] && !m_prio[1][x];
			if (!show && (vc & 1) && !(o & 0x80))
				show = true;
			else if (!show && (vc & 2) && (o & 0xc0) != 0xc0)
				show = true;
			if (show)
				c = u16(0x100 | o);
		}
		out[x] = m_rgb[c & 0x1ff];
	}
}
