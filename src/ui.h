/*
 * m92-pi - menus with neo-pi's layout (304x224), centred in a buffer of the
 * game's size and format (320x240, XRGB8888 with R high), so they go through
 * the same scaling and filters as the game.
 */
#ifndef M92PI_UI_H
#define M92PI_UI_H

#include <circle/types.h>
#include "video.h"

#define UI_W	304		// neo-pi's layout
#define UI_H	224
#define UI_ELLIPSIS	"\x1F"	// "..." in one character (Text, CText)
#define UI_OX	((GAME_W - UI_W) / 2)	// where it sits in the 320x240 buffer
#define UI_OY	((GAME_H - UI_H) / 2)

// Palette "petrol / white / cyan": white like the logo's letters, cyan like its i (0xRRGGBB)
#define UI_BG1		0x06161Cu
#define UI_BG2		0x0B2F36u
#define UI_FRAME1	0xC8D6D8u
#define UI_FRAME2	0x4E6468u
#define UI_PANEL	0x05161Au
#define UI_BAND		0x13525Au
#define UI_TITLE	0xF8F8F8u	// white (logo letters)
#define UI_VALUE	0x37D6D6u	// cyan (logo i)
#define UI_DIM		0x96A8ABu
#define UI_WHITE	0xF8F8F8u
#define UI_CURSOR	0xFF5050u
#define UI_OK		0x6EBE6Eu
#define UI_ERR		0xFF6E6Eu
#define UI_WARN		0xF0C828u
#define UI_GREY		0xA0A0AAu

struct TMenuItem
{
	const char	*pLabel;
	const char	*pValue;	// 0 = none
	boolean		 bDisabled;
};

struct TButtonHint
{
	char		 cButton;	// 'A' 'B' 'C' 'D'
	const char	*pText;
};

struct TGameEntry
{
	char	Name[34];	// title shown in the list
	char	File[64];	// zip file name in the m92pi folder
	int	nGame;		// index in the core's game table
	char	Info[40];	// "UCCOPS  WORLD": set and region, under the list
};

class CUI
{
public:
	CUI (void);

	u32 *GetBuffer (void)	{ return m_Buf; }

	// bOverGame: a game is loaded, show it paused behind the list (SnapshotGame)
	void DrawList (const TGameEntry *pGames, unsigned nCount, unsigned nSel, const char *pMessage,
		       boolean bOverGame);
	// Menu over a darkened copy of the game frame (call SnapshotGame when it opens)
	void SnapshotGame (const u32 *pGameBuf);
	// Generic panel menu: bOverGame = draw over the snapshot, else over the pattern
	void DrawPanelMenu (boolean bOverGame, const char *pTitle,
			    const TMenuItem *pItems, unsigned nItems, unsigned nSel,
			    const char *pMessage, u32 nMessageColor,
			    const TButtonHint *pHints, unsigned nHints, const char *pFooter,
			    boolean bWide = FALSE);
	// Small box drawn over the current buffer contents
	void DrawBox (const char *pLine1, const char *pLine2, const TButtonHint *pHint);
	// Controls set-up: nStep 0-7 = A B C D START SELECT E F being asked, 8 = saved
	void DrawMapping (boolean bOverGame, const char *pTitle, int nStep,
			  const char *const *pValues, const char *pFooter);
	void DrawLoading (const char *pName, unsigned nPercent);
	// Credits over the darkened game (call SnapshotGame first)
	void DrawCredits (const char *pVersion);
	void DrawMessage (const char *pTitle, const char *pLine1, const char *pLine2);
	void DrawBlack (void);

	static u32 Dim (u32 c, unsigned nPercent);

private:
	void Pixel (int x, int y, u32 c);
	void Fill (int x, int y, int w, int h, u32 c);
	void Background (boolean bFreeze = FALSE);
	void Backdrop (boolean bOverGame);
	void Logo (void);
	void Panel (int x0, int y0, int x1, int y1);
	void Text (int x, int y, const char *s, u32 c, unsigned nTall = 1, boolean bListFont = FALSE);
	void CText (int y, const char *s, u32 c, unsigned nTall = 1, boolean bListFont = FALSE);
	void Buttons (int y, const TButtonHint *pHints, unsigned nHints);
	void ButtonIcon (int x, int y, char cButton);
	static u32 ButtonColorOf (char cButton);

private:
	u32	m_Buf[GAME_SRC_PITCH * GAME_SRC_LINES] __attribute__ ((aligned (64)));
	u32	m_Snapshot[GAME_SRC_PITCH * GAME_SRC_LINES];	// the whole paused picture
};

#endif
