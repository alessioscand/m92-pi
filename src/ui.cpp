/*
 * m92-pi - menus (neo-pi's layout, 304x224, in the middle of the 320x240 picture)
 */
#include "fontdata.h"
#include "ui.h"
#include <circle/font.h>
#include <circle/util.h>
#include <circle/timer.h>

// "..." in one character (UI_ELLIPSIS), for names cut in the menus
static const u8 s_Ellipsis[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0x00};

#define OX	UI_OX
#define OY	UI_OY
#define ARGB(c)	(0xFF000000u | (c))
#define BUF_PX	(GAME_SRC_PITCH * GAME_SRC_LINES)

CUI::CUI (void)
{
	memset (m_Buf, 0, sizeof m_Buf);
	memset (m_Snapshot, 0, sizeof m_Snapshot);
}

void CUI::Pixel (int x, int y, u32 c)
{
	if (x >= 0 && x < UI_W && y >= 0 && y < UI_H)
	{
		m_Buf[(y + OY) * GAME_SRC_PITCH + x + OX] = ARGB (c);
	}
}

void CUI::Fill (int x, int y, int w, int h, u32 c)
{
	for (int j = y; j < y + h; j++)
	{
		for (int i = x; i < x + w; i++)
		{
			Pixel (i, j, c);
		}
	}
}

// m92-pi logo, 88x15, drawn at 1x above the game list: # outline (black),
// o letters (white), t / g the i (cyan over green), . transparent
static const char *const s_Logo[] =
{
	"###################################################..........###########################",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooo#####ooooo###########ooooo#.########.#ooooooo#####ooooo#########",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#.#oooooo#.#ooooooooooooooooo#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#.#oooooo#.#ooooooooooooooooo#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#.#oooooo#.#ooooooooooooooooo#ggggggg#",
	"#ooooo#ooooo#ooooo###########ooooo#ooooo###########.########.#ooooooo###########ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"###################################################..........#########.........#########",
};
#define LOGO_W		88
#define LOGO_H		15
#define LOGO_Y		6		// above the list panel (y 26)
#define LOGO_OUTLINE	0x000000u
#define LOGO_LETTERS	0xEEF2F3u	// off-white: the selected game stays the brightest thing
#define LOGO_I_TOP	0x37D6D6u	// = UI_VALUE
#define LOGO_I_BOTTOM	0x83FA27u

// Background: a monitor test grid (crosshatch) on dark petrol, lines every
// 20 pixels and a dot in the middle of each square, still like a service screen
#define BG_CELL		20
#define BG_DARK		0x06161Cu	// = UI_BG1
#define BG_LINE		0x164046u
#define BG_DOT		0x1E5C5Cu	// dark cyan, the logo's i

// Screens without a game behind them (bFreeze: kept for the callers; the grid
// does not move)
void CUI::Background (boolean bFreeze)
{
	(void) bFreeze;
	for (int y = 0; y < GAME_H; y++)
	{
		u32 *p = &m_Buf[y * GAME_SRC_PITCH];
		for (int x = 0; x < GAME_W; x++)
		{
			u32 c = BG_DARK;
			// closed on all four sides: the last column and row are 1 pixel
			// narrower (320 x 240 is not a multiple of the cell plus one line)
			if (x % BG_CELL == 0 || y % BG_CELL == 0 || x == GAME_W - 1 || y == GAME_H - 1)
			{
				c = BG_LINE;
			}
			else if (x % BG_CELL == BG_CELL / 2 && y % BG_CELL == BG_CELL / 2)
			{
				c = BG_DOT;
			}
			p[x] = ARGB (c);
		}
	}
}

void CUI::Logo (void)
{
	const int x0 = (UI_W - LOGO_W) / 2;
	for (int ly = 0; ly < LOGO_H; ly++)
	{
		for (int lx = 0; lx < LOGO_W; lx++)
		{
			u32 c;
			switch (s_Logo[ly][lx])
			{
			case '#': c = LOGO_OUTLINE; break;
			case 'o': c = LOGO_LETTERS; break;
			case 't': c = LOGO_I_TOP; break;
			case 'g': c = LOGO_I_BOTTOM; break;
			default:  continue;
			}
			Pixel (x0 + lx, LOGO_Y + ly, c);
		}
	}
}

void CUI::Panel (int x0, int y0, int x1, int y1)
{
	Fill (x0, y0, x1 - x0 + 1, y1 - y0 + 1, UI_FRAME2);
	Fill (x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, UI_FRAME1);
	Fill (x0 + 3, y0 + 3, x1 - x0 - 5, y1 - y0 - 5, UI_FRAME2);
	Fill (x0 + 4, y0 + 4, x1 - x0 - 7, y1 - y0 - 7, UI_PANEL);
}

// Menu font (You Squared); bListFont: the game list's font (Cyberwire). Both
// in capitals only
void CUI::Text (int x, int y, const char *s, u32 c, unsigned nTall, boolean bListFont)
{
	for (unsigned i = 0; s[i] != '\0'; i++)
	{
		unsigned ch = (unsigned char) s[i];
		const boolean bEllipsis = ch == (unsigned char) UI_ELLIPSIS[0];
		if (!bEllipsis && (ch < 32 || ch >= 128))
		{
			ch = '?';
		}
		if (ch >= 'a' && ch <= 'z')
		{
			ch -= 32;		// capitals only
		}
		for (int pass = 0; pass < 2; pass++)		// shadow, then glyph
		{
			int dx = pass == 0 ? 1 : 0;
			u32 col = pass == 0 ? 0x000000 : c;
			for (unsigned r = 0; r < 8; r++)
			{
				u8 nRow = bEllipsis ? s_Ellipsis[r] : (bListFont ? g_ListFont[ch - 32][r] : g_MenuFont[ch - 32][r]);
				for (unsigned k = 0; k < 8; k++)
				{
					if (nRow & (0x80 >> k))
					{
						for (unsigned t = 0; t < nTall; t++)
						{
							Pixel (x + i * 8 + k + dx, y + r * nTall + t + dx, col);
						}
					}
				}
			}
		}
	}
}

void CUI::CText (int y, const char *s, u32 c, unsigned nTall, boolean bListFont)
{
	Text ((UI_W - (int) strlen (s) * 8) / 2, y, s, c, nTall, bListFont);
}

// Buttons: 13 px disc in the button colour with a 5x7 letter drawn for it
// (exactly centred), neo-pi's style
// A red B yellow C green D blue, E violet F orange (the 6-button games)
static const u32 ButtonColor[6] = {0xDC2828, 0xF0C81E, 0x28B446, 0x325AE6, 0x9646D2, 0xF07828};
static const u32 ButtonInk[6]   = {0xFFFFFF, 0x281E00, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF};
static const char *ButtonGlyph[6][7] =
{
	{".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"},
	{"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."},
	{".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."},
	{"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."},
	{"#####", "#....", "#....", "####.", "#....", "#....", "#####"},
	{"#####", "#....", "#....", "####.", "#....", "#....", "#...."}
};

void CUI::ButtonIcon (int x, int y, char cButton)
{
	unsigned b = (unsigned) (cButton - 'A');
	if (b > 5)
	{
		return;
	}
	// a 13 x 13 disc drawn by hand, with an even 1-pixel black rim
	static const char *const Disc[13] =
	{
		"....#####....",
		"..##ooooo##..",
		".#ooooooooo#.",
		".#ooooooooo#.",
		"#ooooooooooo#",
		"#ooooooooooo#",
		"#ooooooooooo#",
		"#ooooooooooo#",
		"#ooooooooooo#",
		".#ooooooooo#.",
		".#ooooooooo#.",
		"..##ooooo##..",
		"....#####....",
	};
	for (int j = 0; j < 13; j++)
	{
		for (int i = 0; i < 13; i++)
		{
			char c = Disc[j][i];
			if (c != '.')
			{
				Pixel (x + i, y + j, c == '#' ? 0x000000 : ButtonColor[b]);
			}
		}
	}
	for (int r = 0; r < 7; r++)
	{
		for (int k = 0; k < 5; k++)
		{
			if (ButtonGlyph[b][r][k] == '#')
			{
				Pixel (x + 4 + k, y + 3 + r, ButtonInk[b]);
			}
		}
	}
}

u32 CUI::ButtonColorOf (char cButton)
{
	unsigned b = (unsigned) (cButton - 'A');
	return b <= 5 ? ButtonColor[b] : UI_WHITE;
}

u32 CUI::Dim (u32 c, unsigned nPercent)
{
	return ((((c >> 16) & 0xFF) * nPercent / 100) << 16)
	     | ((((c >> 8) & 0xFF) * nPercent / 100) << 8)
	     | (((c & 0xFF) * nPercent / 100));
}

// Hint bar: button followed by its action
static int HintIconWidth (char c)	{ (void) c; return 13; }

void CUI::Buttons (int y, const TButtonHint *pHints, unsigned nHints)
{
	const int nGap = 24;
	int nW = 0;
	for (unsigned i = 0; i < nHints; i++)
	{
		nW += HintIconWidth (pHints[i].cButton) + 4 + (int) strlen (pHints[i].pText) * 8 + (i ? nGap : 0);
	}
	int x = (UI_W - nW) / 2;
	for (unsigned i = 0; i < nHints; i++)
	{
		char c = pHints[i].cButton;
		ButtonIcon (x, y, c);
		int w = HintIconWidth (c);
		Text (x + w + 4, y + 3, pHints[i].pText, 0xD2D2D2);
		x += w + 4 + (int) strlen (pHints[i].pText) * 8 + nGap;
	}
}

// The paused game (darkened snapshot) or, with no game loaded, the pattern
void CUI::Backdrop (boolean bOverGame)
{
	if (bOverGame)
	{
		memcpy (m_Buf, m_Snapshot, sizeof m_Buf);
	}
	else
	{
		Background ();
	}
}

void CUI::DrawList (const TGameEntry *pGames, unsigned nCount, unsigned nSel, const char *pMessage,
		    boolean bOverGame)
{
	Backdrop (bOverGame);
	// the logo on top, then the list: 4 titles above and below the selected one
	Logo ();
	const int nTop = 26, nBottom = 190;
	const int C = 99;				// centre of the band
	const int nSide = 4;
	Panel (12, nTop, UI_W - 13, nBottom);

	Fill (17, C - 11, UI_W - 34, 23, UI_BAND);
	Fill (17, C - 11, UI_W - 34, 1, UI_FRAME2);
	Fill (17, C + 11, UI_W - 34, 1, UI_FRAME2);

	for (int k = -nSide; k <= nSide; k++)
	{
		int i = (int) nSel + k;
		if (i < 0 || i >= (int) nCount)
		{
			continue;
		}
		char Name[40];
		strncpy (Name, pGames[i].Name, 34);
		Name[34] = '\0';
		if (k == 0)
		{
			CText (C - 8, Name, UI_WHITE, 2, TRUE);
		}
		else
		{
			int y = C + k * 12 + (k > 0 ? 8 : -8) - 4;
			int nFade = 100 - (k < 0 ? -k : k) * 15;
			if (nFade < 30) nFade = 30;
			CText (y, Name, Dim (UI_DIM, (unsigned) nFade), 1, TRUE);
		}
	}

	// bottom row: set and region of the selected game on the left, counter on the right
	Fill (17, 172, UI_W - 34, 1, UI_FRAME2);
	char Count[16];
	{
		unsigned a = nSel + 1, b = nCount;
		char *p = Count;
		if (a >= 100) *p++ = '0' + (a / 100) % 10;
		if (a >= 10) *p++ = '0' + (a / 10) % 10;
		*p++ = '0' + a % 10;
		*p++ = '/';
		if (b >= 100) *p++ = '0' + (b / 100) % 10;
		if (b >= 10) *p++ = '0' + (b / 10) % 10;
		*p++ = '0' + b % 10;
		*p = '\0';
	}
	Text (UI_W - 18 - (int) strlen (Count) * 8, 177, Count, 0x7A8290);
	if (pMessage && pMessage[0])
	{
		Text (18, 177, pMessage, UI_ERR);
	}
	else if (nCount)
	{
		char Info[40];
		strncpy (Info, pGames[nSel].Info, sizeof Info - 1);
		Info[sizeof Info - 1] = '\0';
		size_t nRoom = (size_t) ((UI_W - 36 - (int) strlen (Count) * 8) / 8) - 1;
		if (strlen (Info) > nRoom) Info[nRoom] = '\0';
		Text (18, 177, Info, UI_VALUE);
	}
	static const TButtonHint Hints[2] = {{'A', "START GAME"}, {'D', "OPTIONS"}};
	Buttons (198, Hints, 2);
}

void CUI::SnapshotGame (const u32 *pGameBuf)
{
	for (unsigned i = 0; i < BUF_PX; i++)
	{
		u32 p = pGameBuf[i];
		u32 r = ((p >> 16) & 0xFF) * 30 / 100;
		u32 g = ((p >> 8) & 0xFF) * 30 / 100;
		u32 b = (p & 0xFF) * 30 / 100;
		m_Snapshot[i] = ARGB ((r << 16) | (g << 8) | b);
	}
}

void CUI::DrawPanelMenu (boolean bOverGame, const char *pTitle,
			 const TMenuItem *pItems, unsigned nItems, unsigned nSel,
			 const char *pMessage, u32 nMessageColor,
			 const TButtonHint *pHints, unsigned nHints, const char *pFooter,
			 boolean bWide, unsigned nRows, boolean bMoreAbove, boolean bMoreBelow)
{
	if (bOverGame)
	{
		memcpy (m_Buf, m_Snapshot, sizeof m_Buf);
	}
	else
	{
		Background ();
	}

	const unsigned nSpace = nRows ? nRows : nItems;	// rows the panel is sized for
	const int nStep = nSpace > 8 ? 12 : 15;		// long lists (controls) a bit tighter
	int nInner = 22 + (int) nSpace * nStep + 22;		// title + items + message line
	int y0 = (190 - nInner) / 2 + 4;
	if (y0 < 4) y0 = 4;
	// DIP SWITCHES uses the width of the game list: MAME's names fit in full
	int x0 = bWide ? 12 : 44, x1 = bWide ? UI_W - 13 : UI_W - 45;
	Panel (x0, y0, x1, y0 + nInner + 8);

	// 25 characters fit inside the frame (200 px); a longer name is cut at a
	// word, within 24 characters, and ends with the one-character ellipsis
	char Title[34];
	strncpy (Title, pTitle, 33);
	Title[33] = '\0';
	if (strlen (Title) > 25)
	{
		int nCut = 24;
		for (int i = 24; i > 8; i--)
		{
			if (Title[i] == ' ')
			{
				nCut = i;
				break;
			}
		}
		while (nCut > 0 && (Title[nCut - 1] == ' ' || Title[nCut - 1] == '-' || Title[nCut - 1] == ':'))
		{
			nCut--;
		}
		Title[nCut] = UI_ELLIPSIS[0];
		Title[nCut + 1] = '\0';
	}
	CText (y0 + 10, Title, UI_TITLE);
	Fill (x0 + 8, y0 + 22, x1 - x0 - 15, 1, UI_FRAME2);

	for (unsigned n = 0; n < nItems; n++)
	{
		int y = y0 + 32 + (int) n * nStep;
		boolean bSel = n == nSel;
		if (bSel)
		{
			Fill (x0 + 8, y - 3, x1 - x0 - 15, 13, UI_BAND);
			for (int t = 0; t < 4; t++)		// cursor triangle
			{
				Fill (x0 + 12 + t, y + t, 1, 8 - 2 * t, UI_CURSOR);
			}
		}
		u32 nColor = pItems[n].bDisabled ? Dim (UI_DIM, 55) : (bSel ? UI_WHITE : UI_DIM);
		if (!pItems[n].bDisabled && strcmp (pItems[n].pLabel, "POWER OFF") == 0)
		{
			nColor = UI_CURSOR;		// same red as the cursor, selected or not
		}
		Text (x0 + 22, y, pItems[n].pLabel, nColor);
		if (pItems[n].pValue)
		{
			int nLen = (int) strlen (pItems[n].pValue);
			u32 nValColor = pItems[n].bDisabled ? Dim (UI_DIM, 45) : (bSel ? UI_VALUE : Dim (UI_DIM, 75));
			Text (x1 - 10 - nLen * 8, y, pItems[n].pValue, nValColor);
		}
	}

	if (pMessage && pMessage[0])
	{
		Text (x0 + 22, y0 + 32 + (int) nSpace * nStep + 4, pMessage, nMessageColor);
	}

	// more rows out of view: a small arrow in the right corner, level with the
	// title (above) or the message line (below)
	for (int a = 0; a < 2; a++)
	{
		if (a == 0 ? !bMoreAbove : !bMoreBelow)
		{
			continue;
		}
		const int cx = x1 - 13;
		const int cy = a == 0 ? y0 + 7 : y0 + nInner - 3;
		for (int pass = 0; pass < 2; pass++)		// shadow, then arrow
		{
			for (int r = 0; r < 5; r++)
			{
				int hw = a == 0 ? r : 4 - r;
				Fill (cx - hw + 1 - pass, cy + r + 1 - pass, 2 * hw + 1, 1, pass == 0 ? 0x000000u : UI_VALUE);
			}
		}
	}

	if (pFooter && pFooter[0])
	{
		char Footer[40];
		strncpy (Footer, pFooter, 37);
		Footer[37] = '\0';
		CText (y0 + nInner + 14, Footer, Dim (UI_DIM, 60));
	}

	Buttons (209, pHints, nHints);
}

void CUI::DrawBox (const char *pLine1, const char *pLine2, const TButtonHint *pHint)
{
	// darken what is underneath, so the box stands out from the menu
	for (unsigned i = 0; i < BUF_PX; i++)
	{
		m_Buf[i] = ARGB (Dim (m_Buf[i] & 0xFFFFFF, 40));
	}
	Panel (64, 80, UI_W - 65, 150);
	if (pLine2 && pLine2[0])
	{
		if (pLine1) CText (92, pLine1, UI_WHITE);
		CText (110, pLine2, UI_DIM);
		if (pHint) Buttons (126, pHint, 1);
	}
	else
	{
		// one line and the button: centred in the box
		if (pLine1) CText (pHint ? 99 : 111, pLine1, UI_WHITE);
		if (pHint) Buttons (117, pHint, 1);
	}
}

void CUI::DrawBlack (void)
{
	for (unsigned i = 0; i < BUF_PX; i++)
	{
		m_Buf[i] = 0xFF000000u;
	}
}

void CUI::DrawLoading (const char *pName, unsigned nPercent)
{
	Background (TRUE);

	// long titles go on two lines of up to 26 characters, split at a space
	const unsigned nMax = 26;
	char Line1[34], Line2[34];
	size_t nLen = strlen (pName);
	Line2[0] = '\0';
	if (nLen <= nMax)
	{
		strncpy (Line1, pName, sizeof Line1 - 1);
		Line1[sizeof Line1 - 1] = '\0';
	}
	else
	{
		unsigned nCut = nMax;
		while (nCut > 0 && pName[nCut] != ' ')
		{
			nCut--;
		}
		if (nCut == 0)
		{
			nCut = nMax;			// no space: split inside the word
		}
		memcpy (Line1, pName, nCut);
		Line1[nCut] = '\0';
		const char *pRest = pName + nCut;
		while (*pRest == ' ')
		{
			pRest++;
		}
		strncpy (Line2, pRest, nMax);
		Line2[nMax] = '\0';
	}

	int dy = Line2[0] ? 6 : 0;			// the box grows by 6 lines up and down
	Panel (32, 84 - dy, UI_W - 33, 150 + dy);
	CText (98 - dy, "LOADING", UI_TITLE);
	CText (112 - dy, Line1, UI_WHITE);
	if (Line2[0])
	{
		CText (124 - dy, Line2, UI_WHITE);
	}
	int nBarW = UI_W - 96;
	int yBar = 130 + dy;
	Fill (48, yBar, nBarW, 8, UI_FRAME2);
	Fill (49, yBar + 1, nBarW - 2, 6, UI_PANEL);
	Fill (49, yBar + 1, (nBarW - 2) * (int) (nPercent > 100 ? 100 : nPercent) / 100, 6, UI_TITLE);
}

void CUI::DrawMessage (const char *pTitle, const char *pLine1, const char *pLine2)
{
	Background ();
	Panel (16, 70, UI_W - 17, 160);

	// the lines centred vertically in the panel (inside 74-156): 22 px from
	// the title to the first line below it, 16 px between the other two
	const int nY0 = 70, nY1 = 160, nGlyph = 8;
	int nH = nGlyph;
	if (pLine1) nH += 22;
	if (pLine2) nH += pLine1 ? 16 : 22;
	int y = (nY0 + nY1) / 2 - nH / 2;
	CText (y, pTitle, UI_ERR);
	y += 22;
	if (pLine1)
	{
		CText (y, pLine1, UI_WHITE);
		y += 16;
	}
	if (pLine2) CText (y, pLine2, UI_DIM);
}

// ---- controls set-up -------------------------------------------------------

void CUI::DrawMapping (boolean bOverGame, const char *pTitle, int nStep,
		       const char *const *pValues, const char *pFooter)
{
	if (bOverGame)
	{
		memcpy (m_Buf, m_Snapshot, sizeof m_Buf);
	}
	else
	{
		Background ();
	}

	const int x0 = 44, x1 = UI_W - 45;
	Panel (x0, 12, x1, 196);
	CText (22, pTitle, UI_TITLE);
	Fill (x0 + 8, 34, x1 - x0 - 15, 1, UI_FRAME2);

	// nStep is the slot being asked for, in the order of the logical bits
	// (A B C D START SELECT E F), 8 = done; rows and steps: A B C D E F SELECT START
	static const char *Names[8] = {"A", "B", "C", "D", "START", "SELECT", "E", "F"};
	static const char cButton[8] = {'A', 'B', 'C', 'D', 0, 0, 'E', 'F'};
	static const int StepOfRow[8] = {0, 1, 2, 3, 6, 7, 5, 4};
	if (nStep >= 0 && nStep < 8)
	{
		const char *pName = Names[nStep];
		int nLen = 6 + (int) strlen (pName);
		int x = (UI_W - nLen * 8) / 2;
		Text (x, 42, "PRESS ", UI_WHITE, 2);
		Text (x + 48, 42, pName, cButton[nStep] ? ButtonColorOf (cButton[nStep]) : UI_VALUE, 2);
	}
	else
	{
		CText (42, "SAVED", UI_OK, 2);
	}

	for (int r = 0; r < 8; r++)
	{
		int y = 66 + r * 14;
		int i = StepOfRow[r];
		if (i == nStep)
		{
			Fill (x0 + 8, y - 1, x1 - x0 - 15, 14, UI_BAND);
		}
		if (cButton[i])
		{
			ButtonIcon (x0 + 20, y, cButton[i]);
		}
		else
		{
			Text (x0 + 20, y + 3, i == 4 ? "START" : "SELECT / COIN", i == nStep ? UI_WHITE : UI_DIM);
		}
		const char *pValue = pValues[i];
		if (i == nStep)
		{
			pValue = "?";
		}
		if (pValue && pValue[0])
		{
			Text (x1 - 10 - (int) strlen (pValue) * 8, y + 3, pValue,
			      i == nStep ? UI_VALUE : Dim (UI_DIM, 85));
		}
	}

	if (pFooter && pFooter[0])
	{
		CText (182, pFooter, Dim (UI_DIM, 60));
	}
}

// ---- credits -----------------------------------------------------------------

void CUI::DrawCredits (const char *pVersion)
{
	memcpy (m_Buf, m_Snapshot, sizeof m_Buf);

	const int x0 = 18, x1 = UI_W - 19, y0 = 6;
	Panel (x0, y0, x1, y0 + 187);
	CText (y0 + 10, "CREDITS", UI_TITLE);
	Fill (x0 + 8, y0 + 22, x1 - x0 - 15, 1, UI_FRAME2);

	char Title[32] = "M92-PI ";
	strncat (Title, pVersion, sizeof Title - 8);
	CText (y0 + 28, Title, UI_WHITE, 2, TRUE);
	CText (y0 + 48, "BY A.SCANDEREBECH", UI_VALUE);
	Fill (x0 + 8, y0 + 62, x1 - x0 - 15, 1, UI_FRAME2);

	static const char *Rows[10][2] =
	{
		{"CPUS",   "MAME NEC - B.MCPHAIL"},
		{"Z80",    "N.ALLEMAND, R.CARMICHAEL"},
		{"BOARDS", "MAME M92 M90 - B.MCPHAIL"},
		{"VIDEO",  "MISTER - M.DONLON"},
		{"SOUND",  "YMFM - A.GILES"},
		{"GA20",   "MAME - A.TANG,R.BELMONT"},
		{"ZIP",    "MINIZ - R.GELDREICH"},
		{"SYSTEM", "CIRCLE - R.STANGE"},
		{"NTSC",   "BLARGG - S.GREEN"},
		{"FONT",   "ZX ORIGINS - D.GUARD"}
	};
	for (int i = 0; i < 10; i++)
	{
		int y = y0 + 67 + i * 9;
		Text (x0 + 12, y, Rows[i][0], Dim (UI_DIM, 80));
		Text (x0 + 12 + 7 * 8, y, Rows[i][1], 0xEBEBEB);
	}
	Fill (x0 + 8, y0 + 157, x1 - x0 - 15, 1, UI_FRAME2);
	CText (y0 + 162, "IREM IS A TRADEMARK", 0x828796);
	CText (y0 + 173, "OF ITS OWNER.", 0x828796);

	static const TButtonHint Close = {'B', "CLOSE"};
	Buttons (201, &Close, 1);
}
