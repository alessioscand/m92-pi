// m92-pi - Irem M92 video (see m92_video.h)
//
// Register meanings and DMA from MAME's m92_v.cpp (BSD-3-Clause, Bryan McPhail);
// sprite/tile mixing from the MiSTer Irem M92 core (palram.sv, ga22.sv, ga23.sv).
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "m92_video.h"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------- graphics decoding

void m92_video::decode_gfx(const u8 *gfx1, size_t size1, const u8 *gfx2, size_t size2, bool psoldier_layout)
{
	// characters: 8x8, 4 planes, one plane in each quarter of the region (plane 3 = MSB first quarter... see below)
	// MAME charlayout: planes { RGN_FRAC(3,4), RGN_FRAC(2,4), RGN_FRAC(1,4), RGN_FRAC(0,4) }, the first
	// listed plane being the most significant bit of the pen
	const size_t q1 = size1 / 4;
	m_char_count = u32(q1 / 8);
	m_chars.assign(size_t(m_char_count) * 64, 0);
	for (u32 c = 0; c < m_char_count; c++)
		for (int y = 0; y < 8; y++)
		{
			const size_t o = size_t(c) * 8 + y;
			const u8 p3 = gfx1[3 * q1 + o], p2 = gfx1[2 * q1 + o], p1 = gfx1[q1 + o], p0 = gfx1[o];
			u8 *dst = &m_chars[size_t(c) * 64 + y * 8];
			for (int x = 0; x < 8; x++)
			{
				const int b = 7 - x;
				dst[x] = u8((((p3 >> b) & 1) << 3) | (((p2 >> b) & 1) << 2) | (((p1 >> b) & 1) << 1) | ((p0 >> b) & 1));
			}
		}

	// sprites: 16x16, 32 bytes per plane per sprite
	const size_t q2 = size2 / 4;
	m_sprite_count = u32(q2 / 32);
	m_sprites.assign(size_t(m_sprite_count) * 256, 0);
	for (u32 c = 0; c < m_sprite_count; c++)
		for (int y = 0; y < 16; y++)
			for (int x = 0; x < 16; x++)
			{
				size_t byte;
				if (psoldier_layout)
					byte = size_t(c) * 32 + y * 2 + (x >> 3);              // spritelayout2: STEP16(0,1), STEP16(0,16)
				else
					byte = size_t(c) * 32 + (x < 8 ? y : 16 + y);        // spritelayout: STEP8(0,1),STEP8(128,1) / STEP16(0,8)
				const int b = 7 - (x & 7);
				const u8 pen = u8((((gfx2[3 * q2 + byte] >> b) & 1) << 3) | (((gfx2[2 * q2 + byte] >> b) & 1) << 2)
						| (((gfx2[q2 + byte] >> b) & 1) << 1) | ((gfx2[byte] >> b) & 1));
				m_sprites[size_t(c) * 256 + y * 16 + x] = pen;
			}

	// a % by a power of 2 is a mask (the usual case): no division per tile
	auto pow2_mask = [](u32 n) { return (n && !(n & (n - 1))) ? n - 1 : 0u; };
	m_char_mask = pow2_mask(m_char_count);
	m_sprite_mask = pow2_mask(m_sprite_count);
}

void m92_video::reset()
{
	// MAME clears the layer state at video_start and leaves RAM as it is; start from zero
	memset(m_vram, 0, sizeof(m_vram));
	memset(m_spriteram, 0, sizeof(m_spriteram));
	memset(m_spriteram_buffer, 0, sizeof(m_spriteram_buffer));
	memset(m_paletteram, 0, sizeof(m_paletteram));
	memset(m_palette, 0, sizeof(m_palette));
	memset(m_rgb, 0, sizeof(m_rgb));
	memset(m_dmacontrol, 0, sizeof(m_dmacontrol));
	m_videocontrol = 0;
	memset(m_pf_control, 0, sizeof(m_pf_control));
	memset(m_master_control, 0, sizeof(m_master_control));
	m_obj_dirty = true;
}

void m92_video::register_save(save_registry &s)
{
	s.add(m_vram, sizeof(m_vram));
	s.add(m_spriteram, sizeof(m_spriteram));
	s.add(m_spriteram_buffer, sizeof(m_spriteram_buffer));
	s.add(m_paletteram, sizeof(m_paletteram));
	s.add(m_palette, sizeof(m_palette));
	s.add(m_rgb, sizeof(m_rgb));
	s.add(m_dmacontrol, sizeof(m_dmacontrol));
	s.add(&m_videocontrol, sizeof(m_videocontrol));
	s.add(m_pf_control, sizeof(m_pf_control));
	s.add(m_master_control, sizeof(m_master_control));
}

// ---------------------------------------------------------------- CPU access

void m92_video::palette_set(int index, u16 data)
{
	index &= 0x1fff;
	m_palette[index] = data;
	// xBGR 555 -> 0x00RRGGBB (MAME's pal5bit)
	const unsigned r = data & 0x1f, g = (data >> 5) & 0x1f, b = (data >> 10) & 0x1f;
	m_rgb[index] = ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

u16 m92_video::spriteram_r(offs_t offset)
{
	if (BIT(m_dmacontrol[1], 0))
		return m_spriteram_buffer[offset & 0x3ff];
	return m_spriteram[(offset | (m_videocontrol << 10 & 0x400)) & 0x7ff];
}

void m92_video::spriteram_w(offs_t offset, u16 data, u16 mem_mask)
{
	if (BIT(m_dmacontrol[1], 0))
	{
		sync();
		COMBINE_DATA(&m_spriteram_buffer[offset & 0x3ff]);
		m_obj_dirty = true;
	}
	else
		COMBINE_DATA(&m_spriteram[(offset | (m_videocontrol << 10 & 0x400)) & 0x7ff]);
}

u16 m92_video::paletteram_r(offs_t offset)
{
	if (BIT(m_dmacontrol[1], 1))
		return m_palette[(offset | (m_videocontrol << 2 & 0x1c00)) & 0x1fff];
	return m_paletteram[(offset | (m_videocontrol << 9 & 0xc00)) & 0xfff];
}

void m92_video::paletteram_w(offs_t offset, u16 data, u16 mem_mask)
{

	if (BIT(m_dmacontrol[1], 1))
	{
		sync();
		const int i = (offset | (m_videocontrol << 2 & 0x1c00)) & 0x1fff;
		u16 v = m_palette[i];
		COMBINE_DATA(&v);
		palette_set(i, v);
	}
	else
		COMBINE_DATA(&m_paletteram[(offset | (m_videocontrol << 9 & 0xc00)) & 0xfff]);
}

// The GA21 copy as the MiSTer core does it (ga21.sv): the objects are written
// into the GA22's RAM from slot 0x100 - pointer downwards (each one takes as many
// slots as it has columns), and the GA22 draws the slots upwards, the last one
// drawn winning. So the first object copied is drawn last, on top: with the copy
// sorted by layer, layer 0 is the top one. (MAME's m92_v.cpp fills the buffer
// upwards instead, which puts the layers the other way round: Undercover Cops'
// character portraits then vanish behind their pink frame.)
void m92_video::sprite_dma(int slots)
{
	const u16 src_objbank = m_videocontrol << 7 & 0x400;
	const u16 *source = &m_spriteram[src_objbank];
	const bool sort = BIT(m_dmacontrol[2], 0);
	const bool skip_layer7 = !BIT(m_dmacontrol[2], 3);
	int idx = slots;

	for (int layer = 0; layer < 8; layer++)
	{
		for (int offs = 0, numcols = 0; offs < 0x400; offs += 4 * numcols)
		{
			const int curlayer = (source[offs] >> 13) & 7;
			numcols = 1 << ((source[offs] >> 11) & 3);

			if ((!sort || layer == curlayer) && !(curlayer == 7 && skip_layer7))
			{
				if (idx - numcols < 0)
					return;          // no room left
				idx -= numcols;
				for (int i = 0; i < 4; i++)
					m_spriteram_buffer[(idx * 4 + i) & 0x3ff] = source[offs + i];
			}
		}
		if (!sort)
			return;
		if (layer == 6 && skip_layer7)
			return;
	}
}

int m92_video::dmacontrol_w(offs_t offset, u16 data, u16 mem_mask)
{
	offset &= 7;
	COMBINE_DATA(&m_dmacontrol[offset]);
	if (offset != 4)
		return 0;

	sync();
	int clocks = 0x400;

	// palette
	const int src_palbank = m_videocontrol << 6 & 0xc00;
	const int dest_palbank = m_dmacontrol[2] << 2 & 0x1c00;
	for (int i = 0; i < 0x400; i++)
		palette_set(i | dest_palbank, m_paletteram[i | src_palbank]);

	// sprites
	memset(m_spriteram_buffer, 0, sizeof(m_spriteram_buffer));
	m_obj_dirty = true;
	if (m_dmacontrol[0] & 0xff)
	{
		const int slots = 0x100 - (m_dmacontrol[0] & 0xff);
		clocks += slots * 4;
		sprite_dma(slots);
	}
	return clocks;
}

// ---------------------------------------------------------------- drawing

// One tile layer for bitmap line y. MAME's tilemaps: scrolldx(2 * layer) (wide: - 256),
// scrolldy(-128); row scroll from VRAM 0xf400 + 0x400 * layer, indexed by tilemap row.
void m92_video::draw_layer(const line_regs &r, int layer, int y, u16 *color, u8 *prio, bool opaque)
{
	const u16 mc = r.master_control[layer];
	if (mc & 0x10)
		return;                      // layer off

	const bool wide = mc & 0x04;
	const u32 vram_base = (mc & 3) * 0x2000;
	const int cols = wide ? 128 : 64;
	const int wmask = wide ? 1023 : 511;

	const int ty = (y + r.pf_control[layer][0] + 128) & 511;
	int scrollx;
	if (mc & 0x40)
		scrollx = m_vram[((0xf400 + 0x400 * layer) / 2 + ty) & 0x7fff];
	else
		scrollx = r.pf_control[layer][2];
	const int xoff = scrollx - 2 * layer + (wide ? 256 : 0);

	const u16 colhigh = r.videocontrol >> 7 & 0x180;
	const int row = ty >> 3, py = ty & 7;

	int tx = (VIS_X0 + xoff) & wmask;
	int x = 0;
	while (x < VIS_W)
	{
		const int col = tx >> 3;
		const u32 a = (vram_base + 2 * (row * cols + col)) & 0x7fff;
		const u16 attrib = m_vram[(a + 1) & 0x7fff];
		const u32 tile = char_index(m_vram[a] | ((attrib & 0x8000) << 1));
		const u16 pal = u16(((attrib & 0x7f) | colhigh) << 4);
		const bool flipx = attrib & 0x200, flipy = attrib & 0x400;
		const u8 *src = &m_chars[size_t(tile) * 64 + (flipy ? 7 - py : py) * 8];
		const int group = (attrib & 0x100) ? 2 : (attrib & 0x80) ? 1 : 0;

		if ((tx & 7) == 0 && x + 8 <= VIS_W)
		{
			// a whole tile: the same pixels as the loop below, without the per-pixel tests
			u8 pens[8];
			if (flipx)
				for (int i = 0; i < 8; i++) pens[i] = src[7 - i];
			else
				memcpy(pens, src, 8);
			u16 *c = color + x;
			u8 *p = prio + x;
			if (group == 1)
				for (int i = 0; i < 8; i++) { if (pens[i] & 8) p[i] = 1; }
			else if (group == 2)
				for (int i = 0; i < 8; i++) { if (pens[i]) p[i] = 1; }
			if (opaque)
				for (int i = 0; i < 8; i++) c[i] = pal | pens[i];
			else
				for (int i = 0; i < 8; i++) { if (pens[i]) c[i] = pal | pens[i]; }
			x += 8;
			tx = (tx + 8) & wmask;
			continue;
		}

		for (int px = tx & 7; px < 8 && x < VIS_W; px++, x++, tx++)
		{
			const u8 pen = src[flipx ? 7 - px : px];
			// this pixel goes over normal sprites: pens 8-15 of group 1, all but pen 0 of group 2
			if ((group == 1 && (pen & 8)) || (group == 2 && pen))
				prio[x] = 1;
			if (pen || opaque)
				color[x] = pal | pen;
		}
		tx &= wmask;
	}
}

// the sprites crossing each visible line, in buffer order (see m92_video.h)
void m92_video::build_obj_lists()
{
	u16 count[VIS_H];
	memset(count, 0, sizeof(count));
	const u16 *source = m_spriteram_buffer;
	for (int pass = 0; pass < 2; pass++)
	{
		if (pass)
		{
			int sum = 0;
			for (int y = 0; y < VIS_H; y++)
			{
				m_obj_first[y] = u16(sum);
				sum += count[y];
				count[y] = m_obj_first[y];   // fill position
			}
			m_obj_first[VIS_H] = u16(sum);
		}
		for (int offs = 0; offs < 0x400; )
		{
			const u16 s0 = source[offs];
			const int numcols = 1 << ((s0 >> 11) & 3);
			const int numrows = 1 << ((s0 >> 9) & 3);
			const int ybot = 384 - 16 - (s0 & 0x1ff);    // as draw_sprites
			const int top = ybot - (numrows - 1) * 16;
			int y0 = top - VIS_Y0, y1 = ybot + 15 - VIS_Y0;
			if (y0 < 0) y0 = 0;
			if (y1 > VIS_H - 1) y1 = VIS_H - 1;
			for (int y = y0; y <= y1; y++)
			{
				if (pass)
					m_obj_list[count[y]++] = u16(offs);
				else
					count[y]++;
			}
			offs += 4 * numcols;
		}
	}
	m_obj_dirty = false;
}

void m92_video::draw_sprites(const line_regs &r, int y)
{
	// offset 0: lllccrry yyyyyyyy - layer (used by the DMA), columns, rows, y
	// offset 1: cccccccc cccccccc - code
	// offset 2: ......yx pccccccc - flip y, flip x, priority / colour high bit, colour
	// offset 3: .......x xxxxxxxx - x
	const bool pri_enabled = BIT(r.videocontrol, 13);
	const u16 cmask = pri_enabled ? 0x7f : 0xff;
	const u16 chigh = BIT(r.videocontrol, 15) ? 0x100 : 0;
	const u16 *source = m_spriteram_buffer;
	u16 *line = m_obj_line;

	const u16 *list = &m_obj_list[m_obj_first[y - VIS_Y0]];
	const u16 *end = &m_obj_list[m_obj_first[y - VIS_Y0 + 1]];
	for (; list != end; list++)
	{
		const int offs = *list;
		const u16 s0 = source[offs], s1 = source[offs + 1], s2 = source[offs + 2], s3 = source[offs + 3];
		const int numcols = 1 << ((s0 >> 11) & 3);
		const int numrows = 1 << ((s0 >> 9) & 3);

		const int ybot = 384 - 16 - (s0 & 0x1ff);        // top line of the lowest row
		const int top = ybot - (numrows - 1) * 16;
		const int k = y - top;
		const int row = (numrows - 1) - (k >> 4);         // MAME's row number
		const bool flipx = (s2 >> 8) & 1, flipy = (s2 >> 9) & 1;
		const int line_in = flipy ? 15 - (k & 15) : (k & 15);
		const u16 color = u16(((s2 & cmask) | chigh) << 4);
		const u16 over = (pri_enabled && (s2 & 0x80)) ? 0x4000 : 0;

		int x = ((s3 & 0x1ff) - 16) & 0x1ff;
		if (flipx)
			x += 16 * (numcols - 1);
		for (int col = 0; col < numcols; col++)
		{
			const int s_ptr = flipy ? 8 * col + row : 8 * col + numrows - 1 - row;
			const u32 code = sprite_index(u32(s1) + s_ptr);
			const u8 *src = &m_sprites[size_t(code) * 256 + line_in * 16];
			for (int pass = 0; pass < 2; pass++)          // at x, and wrapped at x - 512
			{
				const int bx = (pass ? x - 512 : x) - VIS_X0;
				if (bx <= -16 || bx >= VIS_W)
					continue;
				const int p0 = bx < 0 ? -bx : 0;
				const int p1 = bx > VIS_W - 16 ? VIS_W - bx : 16;
				const u16 base = u16(0x2000 | over | color);
				u16 *dst = line + bx;
				if (flipx)
				{
					for (int px = p0; px < p1; px++)
						if (const u8 pen = src[15 - px])
							dst[px] = u16(base | pen);
				}
				else
				{
					for (int px = p0; px < p1; px++)
						if (const u8 pen = src[px])
							dst[px] = u16(base | pen);
				}
			}
			x += flipx ? -16 : 16;
		}
	}
}

void m92_video::capture(line_regs &r) const
{
	memcpy(r.pf_control, m_pf_control, sizeof(r.pf_control));
	memcpy(r.master_control, m_master_control, sizeof(r.master_control));
	r.videocontrol = m_videocontrol;
}

void m92_video::render_line(const line_regs &r, int y, u32 *out)
{
	// tiles: bottom layer opaque, then the two above with pen 0 transparent
	memset(m_tile_prio, 0, sizeof(m_tile_prio));
	if (r.master_control[2] & 0x10)
		for (int x = 0; x < VIS_W; x++)
			m_tile_color[x] = 0;
	draw_layer(r, 2, y, m_tile_color, m_tile_prio, true);
	draw_layer(r, 1, y, m_tile_color, m_tile_prio, false);
	draw_layer(r, 0, y, m_tile_color, m_tile_prio, false);

	if (r.videocontrol & 0x80)
	{
		for (int x = 0; x < VIS_W; x++)
			out[x] = m_rgb[m_tile_color[x] & 0x1fff];
		return;
	}

	if (m_obj_dirty)
		build_obj_lists();
	if (m_obj_first[y - VIS_Y0] == m_obj_first[y - VIS_Y0 + 1])
	{
		for (int x = 0; x < VIS_W; x++)          // no sprite on this line
			out[x] = m_rgb[m_tile_color[x] & 0x1fff];
		return;
	}
	memset(m_obj_line, 0, sizeof(u16) * VIS_W);
	draw_sprites(r, y);
	for (int x = 0; x < VIS_W; x++)
	{
		const u16 o = m_obj_line[x];
		u16 c = m_tile_color[x];
		if (o && ((o & 0x4000) || !m_tile_prio[x]))
			c = o & 0x1fff;
		out[x] = m_rgb[c & 0x1fff];
	}
}

// harness only: what each layer and the sprites give at one pixel
void m92_video::debug_pixel(int y, int x)
{
	line_regs r;
	capture(r);
	for (int l = 2; l >= 0; l--)
	{
		memset(m_tile_prio, 0, sizeof(m_tile_prio));
		for (int i = 0; i < VIS_W; i++) m_tile_color[i] = 0xffff;
		draw_layer(r, l, y, m_tile_color, m_tile_prio, true);
		fprintf(stderr, "layer %d: color %04x prio %d rgb %06x\n", l, m_tile_color[x], m_tile_prio[x], m_tile_color[x] == 0xffff ? 0 : m_rgb[m_tile_color[x] & 0x1fff]);
	}
	memset(m_obj_line, 0, sizeof(u16) * VIS_W);
	if (m_obj_dirty)
		build_obj_lists();
	draw_sprites(r, y);
	fprintf(stderr, "sprite: %04x\n", m_obj_line[x]);
	int ci = m_obj_line[x] & 0x1ff0;
	for (int i = 0; i < 16; i++) fprintf(stderr, "%04x/%04x ", m_palette[ci + i], m_paletteram[(ci + i) & 0xfff]);
	fprintf(stderr, "\n");
}
