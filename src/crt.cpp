/*
 * m92-pi - CRT output (see crt.h)
 */
#include "crt.h"
#include "fbmap.h"
#include <circle/bcmpropertytags.h>
#include <circle/util.h>

#define PROPTAG_RELEASE_BUFFER	0x00048001
#define PROPTAG_GET_PIXEL_ORDER	0x00040006	// 0 = BGR, 1 = RGB

#define FB_GAME_W	(GAME_W + 40)	// 360: x2 by the HVS = the 720 of a CRT line

CCrtOut::CCrtOut (void)
:	m_pFrameBuffer (0),
	m_nDispW (0),
	m_nDispH (0),
	m_nLines (240),
	m_nWidth (0),
	m_nHeight (0),
	m_nPitchPx (0),
	m_pBase (0),
	m_nBack (1),
	m_bGameMode (FALSE),
	m_bRacing (FALSE),
	m_nK (1),
	m_nGameX (0),
	m_nGameY (0),
	m_nPixelOrder (1),
	m_nWCPages (0),
	m_bRLow (TRUE)
{
}

CCrtOut::~CCrtOut (void)
{
	delete m_pFrameBuffer;
}

// composite: 720x480 (NTSC) or 720x576 (PAL), maybe 720x240 / 720x288; an
// HDMI 480p or 576p mode has the same size and works the same way
boolean CCrtOut::Detect (void)
{
#ifdef PROJECT_FORCE_CRT
	return TRUE;			// QEMU tests
#endif
	CBcmPropertyTags Tags;
	TPropertyTagDisplayDimensions Dim;
	return    Tags.GetTag (PROPTAG_GET_DISPLAY_DIMENSIONS, &Dim, sizeof Dim)
	       && Dim.nWidth == 720
	       && (   Dim.nHeight == 480 || Dim.nHeight == 576
		   || Dim.nHeight == 240 || Dim.nHeight == 288);
}

boolean CCrtOut::Allocate (unsigned nWidth, unsigned nHeight)
{
	if (m_pFrameBuffer != 0)
	{
		CBcmPropertyTags Tags;
		TPropertyTagSimple Release;
		Tags.GetTag (PROPTAG_RELEASE_BUFFER, &Release, sizeof Release);	// may be unsupported
		delete m_pFrameBuffer;
	}

	m_pFrameBuffer = new CBcmFrameBuffer (nWidth, nHeight, 32, nWidth, nHeight * 2, 0, TRUE);
	if (   m_pFrameBuffer == 0
	    || !m_pFrameBuffer->Initialize ())
	{
		return FALSE;
	}

	m_nWidth   = m_pFrameBuffer->GetWidth ();
	m_nHeight  = m_pFrameBuffer->GetHeight ();
	m_nPitchPx = m_pFrameBuffer->GetPitch () / 4;
	m_pBase    = (u32 *) (uintptr) m_pFrameBuffer->GetBuffer ();
	if (   m_pBase == 0
	    || m_nWidth != nWidth || m_nHeight != nHeight
	    || m_pFrameBuffer->GetVirtHeight () < 2 * m_nHeight)
	{
		return FALSE;
	}

	CBcmPropertyTags Tags;
	TPropertyTagSimple Order;
	if (Tags.GetTag (PROPTAG_GET_PIXEL_ORDER, &Order, sizeof Order))
	{
		m_nPixelOrder = Order.nValue;
	}
	m_bRLow = m_nPixelOrder != 0;

	m_nWCPages = FbWriteCombine ((uintptr) m_pBase, (size_t) m_nPitchPx * 4 * m_pFrameBuffer->GetVirtHeight ());

	Clear (0, RGB (0, 0, 0));
	Clear (1, RGB (0, 0, 0));
	ShowBuffer (0);
	return TRUE;
}

boolean CCrtOut::Initialize (void)
{
	CBcmPropertyTags Tags;
	TPropertyTagDisplayDimensions Dim;
	if (!Tags.GetTag (PROPTAG_GET_DISPLAY_DIMENSIONS, &Dim, sizeof Dim))
	{
		return FALSE;
	}
	m_nDispW = Dim.nWidth;
	m_nDispH = Dim.nHeight;

	// a 480 or 576 screen is interlaced in the firmware's terms: our
	// progressive picture is half of it
	m_nLines = m_nDispH > 288 ? m_nDispH / 2 : m_nDispH;
	if (m_nLines < GAME_H)
	{
		m_nLines = GAME_H;
	}

	return SetConsoleMode ();
}

boolean CCrtOut::SetConsoleMode (void)
{
	m_bGameMode = FALSE;
	return Allocate (720, m_nLines);
}

boolean CCrtOut::SetGameMode (unsigned, boolean)
{
	if (!Allocate (FB_GAME_W, m_nLines))
	{
		return FALSE;
	}
	m_nGameX = (m_nWidth - GAME_W) / 2;
	m_nGameY = (m_nHeight - GAME_H) / 2;	// PAL: 24 black lines above and below
	m_bGameMode = TRUE;
	return TRUE;
}

void CCrtOut::SetOutputLines (unsigned nLines)
{
	m_nK = nLines >= 2 * m_nLines ? 2 : 1;
}

unsigned CCrtOut::GetRows (void) const
{
	return m_nHeight / m_Font.GetCharHeight ();
}

void CCrtOut::Clear (unsigned nBuffer, u32 nColor)
{
	ClearRows (nBuffer, 0, GetRows () + 1, nColor);
}

void CCrtOut::ClearRows (unsigned nBuffer, unsigned nFirstRow, unsigned nRows, u32 nColor)
{
	nColor = Native (nColor);
	unsigned nCharH = m_Font.GetCharHeight ();
	unsigned y1 = (nFirstRow + nRows) * nCharH;
	if (y1 > m_nHeight)
	{
		y1 = m_nHeight;
	}
	for (unsigned y = nFirstRow * nCharH; y < y1; y++)
	{
		u32 *p = Row (nBuffer, y);
		for (unsigned x = 0; x < m_nWidth; x++)
		{
			p[x] = nColor;
		}
	}
}

void CCrtOut::DrawText (unsigned nBuffer, unsigned nCol, unsigned nRow, const char *pText,
			u32 nFG, u32 nBG)
{
	nFG = Native (nFG);
	nBG = Native (nBG);
	const unsigned nCharW = m_Font.GetCharWidth ();
	const unsigned nCharH = m_Font.GetCharHeight ();
	if ((nRow + 1) * nCharH > m_nHeight)
	{
		return;
	}
	for (unsigned i = 0; pText[i] != '\0' && (nCol + i + 1) * nCharW <= m_nWidth; i++)
	{
		for (unsigned cy = 0; cy < nCharH; cy++)
		{
			CCharGenerator::TPixelLine Line = m_Font.GetPixelLine (pText[i], cy);
			u32 *p = Row (nBuffer, nRow * nCharH + cy) + (nCol + i) * nCharW;
			for (unsigned cx = 0; cx < nCharW; cx++)
			{
				p[cx] = m_Font.GetPixel (cx, Line) ? nFG : nBG;
			}
		}
	}
}

void CCrtOut::DrawGame (const u32 *pSource)
{
	for (unsigned y = 0; y < GAME_H; y++)
	{
		DrawGameLineTo (m_nBack, pSource, y);
	}
}

// one game line, 1:1, converting the core's XRGB8888 (R high) if needed
void CCrtOut::DrawGameLineTo (unsigned nBuffer, const u32 *pSource, unsigned y)
{
	const u32 *pSrc = pSource + (GAME_SRC_Y + y) * GAME_SRC_PITCH + GAME_SRC_X;
	u32 *pDst = Row (nBuffer, m_nGameY + y) + m_nGameX;
	if (m_bRLow)
	{
		for (unsigned x = 0; x < GAME_W; x++)
		{
			u32 p = pSrc[x];
			pDst[x] = 0xFF000000u | (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
		}
	}
	else
	{
		for (unsigned x = 0; x < GAME_W; x++)
		{
			pDst[x] = 0xFF000000u | pSrc[x];
		}
	}
	FbFlush ();			// the line is in memory before the HVS reads it
}

void CCrtOut::ShowBuffer (unsigned nBuffer)
{
	m_pFrameBuffer->SetVirtualOffset (0, nBuffer * m_nHeight);
	m_nBack = nBuffer ^ 1;
}

boolean CCrtOut::Flip (void)
{
	boolean bOK = m_pFrameBuffer->SetVirtualOffset (0, m_nBack * m_nHeight);
	m_nBack ^= 1;
	return bOK;
}

boolean CCrtOut::WaitForVSync (void)
{
	return m_pFrameBuffer->WaitForVerticalSync ();
}
