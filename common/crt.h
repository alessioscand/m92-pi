/*
 * CRT output: the composite jack, 240p (see screen.h)
 *
 * Chosen at start-up when the firmware drives a standard-definition screen
 * (config.txt, the CRT block): it reports it as 720x480 and scales our
 * framebuffer to it with the HVS.
 *
 * One framebuffer for everything, 360x240: one game line per CRT line, the
 * GAME_W pixels in the middle, doubled to 720 wide by the HVS (320 -> 640,
 * the width of the arcade picture on a 720 line). Only the picture: no
 * filters, no border, no text beside it, no console text.
 */
#ifndef COMMON_CRT_H
#define COMMON_CRT_H

#include "screen.h"
#include "smp.h"
#include <circle/bcmframebuffer.h>

class CCrtOut final : public CScreen
{
public:
	CCrtOut (void);
	~CCrtOut (void);

	// TRUE if the firmware drives a standard-definition screen
	static boolean Detect (void);

	boolean Initialize (void) override;

	boolean SetGameMode (unsigned nFilter, boolean bAllowHVS) override;
	boolean SetConsoleMode (void) override;
	boolean IsGameMode (void) const override	{ return m_bGameMode; }

	unsigned GetWidth (void) const override		{ return m_nDispW; }
	unsigned GetHeight (void) const override	{ return m_nDispH; }
	unsigned GetScale (void) const override		{ return 1; }
	unsigned GetHVSFactor (void) const override	{ return m_nK; }	// vertical
	unsigned GetCPUScale (void) const override	{ return 1; }
	unsigned GetFilter (void) const override	{ return FILTER_NONE; }
	unsigned GetFBWidth (void) const override	{ return m_nWidth; }
	unsigned GetFBHeight (void) const override	{ return m_nHeight; }
	unsigned GetGameY (void) const override		{ return m_nGameY; }

	// beam racing: the lines of the HVS channel, as measured (480: each
	// framebuffer line is two of them)
	void SetOutputLines (unsigned nLines)		{ m_nK = nLines >= 2 * m_nHeight ? 2 : 1; }

	void SetRacing (boolean bOn) override		{ SharedStore (m_bRacing, bOn); }

	// no console text on the CRT
	unsigned GetRows (void) const override		{ return 15; }
	void Clear (unsigned nBuffer, u32 nColor) override;
	void DrawText (unsigned, unsigned, unsigned, const char *, u32, u32) override {}
	void ClearRows (unsigned nBuffer, unsigned, unsigned, u32 nColor) override { Clear (nBuffer, nColor); }

	void DrawGame (const u32 *pSource, boolean bFilter = TRUE) override;
	void DrawGameLine (const u32 *pSource, unsigned y, u32 *, u32 *) override
	{
		DrawGameLineTo (SharedLoad (m_bRacing) ? m_nBack ^ 1 : m_nBack, pSource, y, FALSE);
	}
	void DrawNtscLine (unsigned, const u32 *, unsigned, u32 *) override {}	// no filters
	void SetBezel (unsigned, const TBezelInfo *, unsigned) override {}	// no border
	void DrawSideText (boolean, const TTextSpan *, unsigned) override {}	// no text

	unsigned GetPixelOrder (void) const override	{ return m_nPixelOrder; }
	unsigned GetWCPages (void) const override	{ return m_nWCPages; }
	boolean IsRedLow (void) const override		{ return m_bRLow; }

	void ShowBuffer (unsigned nBuffer) override;
	boolean Flip (void) override;
	boolean WaitForVSync (void) override;

private:
	boolean Allocate (unsigned nWidth, unsigned nHeight);
	void DrawGameLineTo (unsigned nBuffer, const u32 *pSource, unsigned y, boolean bMenu);
	u32 *Row (unsigned nBuffer, unsigned nY) const
	{
		return m_pBase + (nBuffer * m_nHeight + nY) * m_nPitchPx;
	}
	u32 Native (u32 c) const		// RGB() (R low) -> framebuffer order
	{
		return m_bRLow ? c : (c & 0xFF00FF00u) | ((c >> 16) & 0xFFu) | ((c & 0xFFu) << 16);
	}

private:
	CBcmFrameBuffer *m_pFrameBuffer;

	unsigned m_nDispW, m_nDispH;		// as the firmware reports the screen
	unsigned m_nWidth, m_nHeight;		// the framebuffer
	unsigned m_nPitchPx;
	u32	*m_pBase;
	unsigned m_nBack;
	boolean	 m_bGameMode;
	boolean	 m_bRacing;			// read by core 1
	unsigned m_nK;				// HVS channel lines per framebuffer line
	unsigned m_nGameX, m_nGameY;
	u32	*m_pGridPic;			// the menus' grid, GAME_W x GAME_H

	unsigned m_nPixelOrder;
	unsigned m_nWCPages;
	boolean	 m_bRLow;
};

#endif
