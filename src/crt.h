/*
 * m92-pi - CRT output (composite jack, 240p)
 *
 * Chosen at start-up when the firmware drives a standard-definition screen
 * (config.txt: enable_tvout=1, sdtv_mode=16, see dist/sd/config.txt).
 * The firmware reports it as 720x480 (NTSC) and scales our framebuffer to
 * it with the HVS.
 *
 * Game mode: a 360x240 framebuffer, one game line per CRT line, the 320
 * pixels in the middle. The HVS doubles it to 720 wide: 640 pixels of the
 * 720 a CRT line holds, the width of the M92 picture on the arcade monitor
 * (its pixel clock, 6.67 MHz, is about half of the 13.5 MHz of the 720).
 * No filters, no black bands: the whole picture is the game.
 * Console mode: 720x240, the boot text 8x16.
 */
#ifndef CRT_H
#define CRT_H

#include "screen.h"
#include "smp.h"
#include <circle/bcmframebuffer.h>
#include <circle/chargenerator.h>

class CCrtOut final : public CScreen
{
public:
	CCrtOut (void);
	~CCrtOut (void);

	// TRUE if the firmware drives a standard-definition screen (composite)
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

	// beam racing: lines of the HVS channel, measured (240 on a CRT; 480 if
	// a 480p HDMI screen is used instead)
	void SetOutputLines (unsigned nLines);

	void SetRacing (boolean bOn) override		{ SharedStore (m_bRacing, bOn); }

	unsigned GetRows (void) const override;
	void Clear (unsigned nBuffer, u32 nColor) override;
	void DrawText (unsigned nBuffer, unsigned nCol, unsigned nRow, const char *pText,
		       u32 nFG, u32 nBG) override;
	void ClearRows (unsigned nBuffer, unsigned nFirstRow, unsigned nRows, u32 nColor) override;

	void DrawGame (const u32 *pSource) override;
	void DrawGameLine (const u32 *pSource, unsigned y, u32 *, u32 *) override
	{
		DrawGameLineTo (SharedLoad (m_bRacing) ? m_nBack ^ 1 : m_nBack, pSource, y);
	}
	void DrawNtscLine (unsigned, const u32 *, unsigned, u32 *) override {}	// no filters

	// no black bands on a CRT: nowhere to write
	void DrawBandText (boolean, const TTextSpan *, unsigned) override {}
	boolean HasSideColumns (void) const override	{ return FALSE; }
	void DrawSideText (boolean, const TTextSpan *, unsigned) override {}

	unsigned GetPixelOrder (void) const override	{ return m_nPixelOrder; }
	unsigned GetWCPages (void) const override	{ return m_nWCPages; }
	boolean IsRedLow (void) const override		{ return m_bRLow; }

	void ShowBuffer (unsigned nBuffer) override;
	boolean Flip (void) override;
	boolean WaitForVSync (void) override;

private:
	boolean Allocate (unsigned nWidth, unsigned nHeight);
	void DrawGameLineTo (unsigned nBuffer, const u32 *pSource, unsigned y);
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
	CCharGenerator	 m_Font;		// console font (8x16)

	unsigned m_nDispW, m_nDispH;		// as the firmware reports the screen
	unsigned m_nLines;			// picture lines: 240 (NTSC) or 288 (PAL)
	unsigned m_nWidth, m_nHeight;		// current framebuffer
	unsigned m_nPitchPx;
	u32	*m_pBase;
	unsigned m_nBack;
	boolean	 m_bGameMode;
	boolean	 m_bRacing;			// read by core 1
	unsigned m_nK;				// output lines per framebuffer line
	unsigned m_nGameX, m_nGameY;

	unsigned m_nPixelOrder;
	unsigned m_nWCPages;
	boolean	 m_bRLow;
};

#endif
