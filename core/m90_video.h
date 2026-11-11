// m92-pi - Irem M90 video: the GA25 (two tile layers and sprites), drawn one line at a time
//
// Registers, scroll offsets and sprite format from MAME's m90_v.cpp; the mix of the
// two layers and the sprites (tile priority by pen, sprite colours over priority
// tiles) as the MiSTer core's ga25.sv, which follows a real board.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_M90_VIDEO_H
#define M92PI_M90_VIDEO_H

#include "emu.h"
#include <vector>

class m90_video
{
public:
	// the eight control registers (I/O 0x80-0x8f) a line is drawn with
	struct line_regs { u16 ctrl[8]; };

	void decode_gfx(const u8 *gfx, size_t size);
	void set_area(int x0, int w) { m_x0 = x0; m_w = w; }
	void reset();
	void register_save(save_registry &s);

	void capture(line_regs &r) const { memcpy(r.ctrl, m_ctrl, sizeof(m_ctrl)); }
	// draw line y of MAME's bitmap (136..375 for most games) into the visible pixels
	void render_line(const line_regs &r, int y, u32 *out);

	// called before anything the drawing reads (VRAM, palette) changes
	void set_sync(void (*fn)(void *), void *ctx) { m_sync = fn; m_sync_ctx = ctx; }
	void sync() { if (m_sync) m_sync(m_sync_ctx); }

	// CPU access
	void vram_w(offs_t offset, u16 data, u16 mem_mask) { sync(); COMBINE_DATA(&m_vram[offset & 0x7fff]); }
	u16 palette_r(offs_t offset) const { return m_palette[offset & 0x1ff]; }
	void palette_w(offs_t offset, u16 data, u16 mem_mask);
	void ctrl_w(offs_t offset, u16 data, u16 mem_mask) { COMBINE_DATA(&m_ctrl[offset & 7]); }

	u8 *vram_bytes() { return reinterpret_cast<u8 *>(m_vram); }
	const u16 *vram() const { return m_vram; }
	const u16 *ctrl() const { return m_ctrl; }

private:
	void palette_set(int index, u16 data);
	void draw_layer(const line_regs &r, int layer, int y);
	void draw_sprites(int y);

	// state
	u16 m_vram[0x8000];              // 0xd0000-0xdffff: tiles, sprites at 0xee00, tables at 0xf000
	u16 m_palette[0x200];            // 0xe0000-0xe03ff, xBGR 555: tiles 0-255, sprites 256-511
	u16 m_ctrl[8];

	// derived
	u32 m_rgb[0x200];
	void (*m_sync)(void *) = nullptr;
	void *m_sync_ctx = nullptr;
	int m_x0 = 80, m_w = 320;

	// decoded graphics, one byte per pixel; tiles and sprites come from the same ROMs
	std::vector<u8> m_chars;         // 8x8
	std::vector<u8> m_sprites;       // 16x16
	u32 m_char_count = 1, m_sprite_count = 1;
	u32 m_char_mask = 0, m_sprite_mask = 0;
	u32 char_index(u32 n) const { return m_char_mask ? n & m_char_mask : n % m_char_count; }
	u32 sprite_index(u32 n) const { return m_sprite_mask ? n & m_sprite_mask : n % m_sprite_count; }

	// line work buffers (with room for sprites hanging off either side)
	static constexpr int MAX_W = 384;
	u16 m_color[2][MAX_W];           // each layer's palette index
	u8 m_prio[2][MAX_W];             // the layer's pixel is over the sprites
	u8 m_obj[512 + 32];              // sprite pixel: colour << 4 | pen, 0 = none
};

#endif
