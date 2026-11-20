/*
 * CRT output (see crt.h)
 */
#include "crt.h"
#include "theme.h"
#include "fbmap.h"
#include <circle/bcmpropertytags.h>
#include <circle/util.h>

#define PROPTAG_RELEASE_BUFFER	0x00048001
#define PROPTAG_GET_PIXEL_ORDER	0x00040006	// 0 = BGR, 1 = RGB

#define FB_W		(GAME_W + 40)		// composite, 360: x2 by the HVS = 720
#define FB_H		240			// NTSC, progressive

CCrtOut::CCrtOut (void)
:	m_pFrameBuffer (0),
	m_nDispW (0),
	m_nDispH (0),
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
	m_pGridPic (0),
	m_nPixelOrder (1),
	m_nWCPages (0),
	m_bRLow (TRUE)
{
}

CCrtOut::~CCrtOut (void)
{
	delete [] m_pGridPic;
	delete m_pFrameBuffer;
}

// composite: 720x480 (NTSC; 720x576 PAL is not supported yet);
// VGA666: N x 240, N a multiple of GAME_W
unsigned CCrtOut::Detect (void)
{
#ifdef PROJECT_FORCE_CRT
	return CRT_COMPOSITE;		// QEMU tests
#endif
	CBcmPropertyTags Tags;
	TPropertyTagDisplayDimensions Dim;
	if (!Tags.GetTag (PROPTAG_GET_DISPLAY_DIMENSIONS, &Dim, sizeof Dim))
	{
		return CRT_NONE;
	}
	if (Dim.nWidth == 720 && Dim.nHeight == 480)
	{
		return CRT_COMPOSITE;
	}
	if (Dim.nHeight == FB_H && Dim.nWidth >= GAME_W && Dim.nWidth % GAME_W == 0)
	{
		return CRT_DPI;
	}
	return CRT_NONE;
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

	if (!Allocate (m_nDispH == FB_H ? GAME_W : FB_W, FB_H))	// VGA666 : composite
	{
		return FALSE;
	}
	m_nGameX = (m_nWidth - GAME_W) / 2;
	m_nGameY = (m_nHeight - GAME_H) / 2;	// 224-line boards: 8 black lines above and below

	// the menus' grid where the picture goes, from the screen's centre as on
	// HDMI, one point per game pixel
	m_pGridPic = new u32[GAME_W * GAME_H];
	if (m_pGridPic == 0)
	{
		return FALSE;
	}
	const int W2 = (int) (m_nWidth / 2), H2 = (int) (m_nHeight / 2);
	for (unsigned y = 0; y < GAME_H; y++)
	{
		for (unsigned x = 0; x < GAME_W; x++)
		{
			const u32 c = ThemeGrid ((int) (m_nGameX + x) - W2, (int) (m_nGameY + y) - H2, 1, W2, H2);
			m_pGridPic[y * GAME_W + x] = Native (RGB ((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF));
		}
	}

	return SetConsoleMode ();
}

// one framebuffer for both modes: nothing to set up, only to clear
boolean CCrtOut::SetConsoleMode (void)
{
	m_bGameMode = FALSE;
	Clear (0, RGB (0, 0, 0));
	Clear (1, RGB (0, 0, 0));
	return TRUE;
}

boolean CCrtOut::SetGameMode (unsigned, boolean)
{
	m_bGameMode = TRUE;
	Clear (0, RGB (0, 0, 0));
	Clear (1, RGB (0, 0, 0));
	return TRUE;
}

void CCrtOut::Clear (unsigned nBuffer, u32 nColor)
{
	nColor = Native (nColor);
	for (unsigned y = 0; y < m_nHeight; y++)
	{
		u32 *p = Row (nBuffer, y);
		for (unsigned x = 0; x < m_nWidth; x++)
		{
			p[x] = nColor;
		}
	}
}

void CCrtOut::DrawGame (const u32 *pSource, boolean bFilter)
{
	for (unsigned y = 0; y < GAME_H; y++)
	{
		DrawGameLineTo (m_nBack, pSource, y, !bFilter);
	}
}

// one game line, 1:1, converting the core's XRGB8888 (R high) if needed;
// bMenu: where the menus leave a hole, the grid
void CCrtOut::DrawGameLineTo (unsigned nBuffer, const u32 *pSource, unsigned y, boolean bMenu)
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
	if (bMenu)
	{
		const u32 *pGrid = m_pGridPic + y * GAME_W;
		for (unsigned x = 0; x < GAME_W; x++)
		{
			if (pSrc[x] == GRID_HOLE)
			{
				pDst[x] = pGrid[x];
			}
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
