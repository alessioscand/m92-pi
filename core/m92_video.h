// m92-pi - Irem M92 video: GA21 (sprite and palette DMA), GA22 (sprites), GA23 (tiles)
//
// Drawn one line at a time. Tile layers follow MAME's m92_v.cpp (scroll offsets, row
// scroll, the three tile priority groups); the final mix of sprites and tiles follows
// the hardware as the MiSTer core models it: one sprite line buffer in which the last
// sprite written wins, then a choice per pixel between that and the tile colour.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef M92PI_M92_VIDEO_H
#define M92PI_M92_VIDEO_H

#include "emu.h"
#include "m92.h"
#include <vector>

class m92_video
{
public:
	// bitmap coordinates as in MAME: visible x 80..399, visible y 8..247
	static constexpr int VIS_X0 = 80, VIS_W = 320;
	static constexpr int VIS_Y0 = 8, VIS_H = 240;

	void decode_gfx(const u8 *gfx1, size_t size1, const u8 *gfx2, size_t size2, bool psoldier_layout);
	void reset();
	void register_save(save_registry &s);

	// the registers a line is drawn with; captured when the line starts, so that the line
	// can be drawn later on another core (VRAM, palette and sprites must not change
	// meanwhile: see m92_machine::set_video_sync)
	typedef m92_machine::line_regs line_regs;
	void capture(line_regs &r) const;
	void debug_pixel(int y, int x);

	// draw bitmap line y (VIS_Y0 .. VIS_Y0 + VIS_H - 1) into 320 pixels 0x00RRGGBB
	void render_line(const line_regs &r, int y, u32 *out);

	// called before anything the drawing reads (VRAM, palette, sprite buffer) changes
	void set_sync(void (*fn)(void *), void *ctx) { m_sync = fn; m_sync_ctx = ctx; }
	void sync() { if (m_sync) m_sync(m_sync_ctx); }
	// the sprite buffer was replaced as a whole (save state loaded)
	void sprites_changed() { sync(); m_obj_dirty = true; }
	void vram_w(offs_t offset, u16 data, u16 mem_mask) { sync(); COMBINE_DATA(&m_vram[offset & 0x7fff]); }

	// CPU access
	u16 spriteram_r(offs_t offset);
	void spriteram_w(offs_t offset, u16 data, u16 mem_mask);
	u16 paletteram_r(offs_t offset);
	void paletteram_w(offs_t offset, u16 data, u16 mem_mask);
	void videocontrol_w(u16 data, u16 mem_mask) { COMBINE_DATA(&m_videocontrol); }
	void pf_control_w(int layer, offs_t offset, u16 data, u16 mem_mask) { COMBINE_DATA(&m_pf_control[layer][offset & 3]); }
	// returns the length of the transfer in pixel clocks / 2, or 0 when nothing starts
	int dmacontrol_w(offs_t offset, u16 data, u16 mem_mask);
	void master_control_w(offs_t offset, u16 data, u16 mem_mask) { COMBINE_DATA(&m_master_control[offset & 3]); }
	u16 master_control(int n) const { return m_master_control[n]; }

	u8 *vram_bytes() { return reinterpret_cast<u8 *>(m_vram); }
	const u16 *sprite_buffer() const { return m_spriteram_buffer; }
	const u16 *dmacontrol() const { return m_dmacontrol; }
	u16 videocontrol() const { return m_videocontrol; }
	const u16 *pf_control(int l) const { return m_pf_control[l]; }
	const u16 *vram() const { return m_vram; }

private:
	void sprite_dma(int amount);
	void palette_set(int index, u16 data);
	void draw_layer(const line_regs &r, int layer, int y, u16 *color, u8 *prio, bool opaque);
	void draw_sprites(const line_regs &r, int y);
	void build_obj_lists();

	// state
	u16 m_vram[0x8000];              // 0xd0000-0xdffff
	u16 m_spriteram[0x800];          // CPU side: two banks of 0x400 words
	u16 m_spriteram_buffer[0x400];   // what GA22 draws
	u16 m_paletteram[0x1000];        // CPU side: four banks of 0x400 colours
	u16 m_palette[0x2000];           // the colours on screen (xBGR 555)
	u16 m_dmacontrol[8];
	u16 m_videocontrol;
	u16 m_pf_control[3][4];
	u16 m_master_control[4];

	// derived
	u32 m_rgb[0x2000];               // m_palette as 0x00RRGGBB
	void (*m_sync)(void *) = nullptr;
	void *m_sync_ctx = nullptr;

	// decoded graphics: one byte per pixel
	std::vector<u8> m_chars;         // 8x8
	std::vector<u8> m_sprites;       // 16x16
	u32 m_char_count = 0, m_sprite_count = 0;
	u32 m_char_mask = 0, m_sprite_mask = 0;  // count - 1 when the count is a power of 2, else 0
	u32 char_index(u32 n) const { return m_char_mask ? n & m_char_mask : n % m_char_count; }
	u32 sprite_index(u32 n) const { return m_sprite_mask ? n & m_sprite_mask : n % m_sprite_count; }

	// line work buffers (with room for sprites hanging off either side)
	u16 m_tile_color[VIS_W];
	u8 m_tile_prio[VIS_W];
	u16 m_obj_line[512 + 64];        // 0 = no sprite pixel; else colour index + 1, bit 15 = over everything

	// the sprites crossing each visible line, as offsets into m_spriteram_buffer in buffer
	// order (drawing order). Built from the buffer when it has changed (DMA or CPU write),
	// by the core that draws, so a line never scans all 256 slots. Not saved: rebuilt.
	bool m_obj_dirty = true;
	u16 m_obj_first[VIS_H + 1];      // line y: m_obj_list[m_obj_first[y] .. m_obj_first[y + 1])
	u16 m_obj_list[256 * 128];       // at most 256 slots, each crossing at most 128 lines
};

#endif
