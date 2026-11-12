/*
 * m92-pi - menus: the game list, OPTIONS, DIP SWITCHES, CONTROLS, power off,
 * credits
 */
#include "kernel.h"
#include "frame.h"
#include "m92_api.h"
#include <circle/machineinfo.h>

void CKernel::ShowUIFrame (boolean bWait, const char *pTopMessage)
{
	FrameDrawPicture (m_pUI->GetBuffer ());
	if (pTopMessage && pTopMessage[0])
	{
		// in the black band above the picture, like the in-game messages
		const CScreen::TTextSpan Span = {pTopMessage, UI_ERR};
		m_Video.DrawBandText (TRUE, &Span, 1);
	}
	else
	{
		m_Video.DrawBandText (TRUE, 0, 0);	// no status line or message over menus
	}
	m_Video.DrawBandText (FALSE, 0, 0);
	m_Video.DrawSideText (FALSE, 0, 0);	// no-op without side columns
	m_Video.DrawSideText (TRUE, 0, 0);
	m_Video.Flip ();
	if (!bWait)
	{
		return;
	}
	if (m_bVSync)
	{
		m_Video.WaitForVSync ();
	}
	else
	{
		CTimer::SimpleusDelay (16667);
	}
}

unsigned CKernel::RunList (unsigned nSel)
{
	u32 nPrev = ~0u;		// require a release before acting
	unsigned nRepeat = 0;
	unsigned nFrame = 0;

	while (1)
	{
		if (nFrame++ % 15 == 0)
		{
			m_pInput->UpdateDevices ();
			m_pInput->FlushDiagnostics ();

			// a pad never seen before: set up its controls first
			int nPad = m_pInput->GetUnmappedPad ();
			if (nPad >= 0)
			{
				RunMapping ((unsigned) nPad, m_nLoaded >= 0);
				nPrev = ~0u;
				continue;
			}
		}

		u32 n = m_pInput->GetAny ();
		u32 nNew = n & ~nPrev;

		// up/down with auto-repeat, left/right = page
		u32 nMove = nNew;
		if (n & (BTN_UP | BTN_DOWN))
		{
			if (++nRepeat > 20 && nRepeat % 4 == 0)
			{
				nMove |= n & (BTN_UP | BTN_DOWN);
			}
		}
		else
		{
			nRepeat = 0;
		}
		if ((nMove & BTN_UP) && nSel > 0)			nSel--;
		if ((nMove & BTN_DOWN) && nSel + 1 < m_nGames)		nSel++;
		if (nNew & BTN_LEFT)	nSel = nSel >= 8 ? nSel - 8 : 0;
		if (nNew & BTN_RIGHT)	nSel = nSel + 8 < m_nGames ? nSel + 8 : m_nGames - 1;

		if (nNew & (BTN_A | BTN_START))
		{
			return nSel;
		}
		if (nNew & BTN_D)
		{
			RunSystemMenu ();
			n = ~0u;		// wait for release
		}
		nPrev = n;

		const char *pMsg = m_pInput->GetPadCount () == 0 ? "CONNECT A USB GAMEPAD" : "";
		m_pUI->DrawList (m_pGames, m_nGames, nSel, "", m_nLoaded >= 0);
		ShowUIFrame (TRUE, pMsg);
	}
}

void CKernel::RunSystemMenu (void)
{
	enum { SmCpu, SmFilter, SmControls, SmPower, SmCount };

	unsigned nSel = 0;
	u32 nPrev = ~0u;
	unsigned nOldCpu = m_nCpuSpeed, nOldFilter = m_nFilter;
	const char *pMsg = "";
	unsigned nFrame = 0;

	// "Raspberry Pi 3 Model B+" -> "PI 3 B+"
	char Model[32];
	const char *pName = CMachineInfo::Get ()->GetMachineName ();
	if (strncmp (pName, "Raspberry ", 10) == 0) pName += 10;
	unsigned nLen = 0;
	for (; *pName && nLen < sizeof Model - 1; pName++)
	{
		if (strncmp (pName, "Model ", 6) == 0) pName += 6;
		char c = *pName;
		Model[nLen++] = c >= 'a' && c <= 'z' ? (char) (c - 32) : c;
	}
	Model[nLen] = '\0';
	char Footer[48];
	snprintf (Footer, sizeof Footer, "M92-PI " PROJECT_VERSION "  %s", Model);

	while (1)
	{
		if (nFrame++ % 15 == 0)
		{
			m_pInput->UpdateDevices ();
		}

		u32 n = m_pInput->GetAny ();
		u32 nNew = nPrev == ~0u ? 0 : (n & ~nPrev);
		nPrev = n;

		if (nNew & BTN_UP)	{ nSel = (nSel + SmCount - 1) % SmCount; pMsg = ""; }
		if (nNew & BTN_DOWN)	{ nSel = (nSel + 1) % SmCount; pMsg = ""; }
		int nDir = (nNew & (BTN_A | BTN_RIGHT)) ? 1 : ((nNew & BTN_LEFT) ? -1 : 0);

		if (nDir != 0)
		{
			switch (nSel)
			{
			case SmCpu:
				// the main CPU as on the board, or faster against the slowdowns
				// (applies at once, as the MiSTer core's CPU Speed)
				m_nCpuSpeed = m_nCpuSpeed == CPU_TURBO ? CPU_NORMAL : CPU_TURBO;
				if (m_nLoaded >= 0)
				{
					m92_set_cpu_speed ((int) m_nCpuSpeed);
				}
				break;

			case SmFilter:
				if (m_bFilterAllowed)
				{
					m_nFilter = (m_nFilter + FILTER_COUNT + (unsigned) nDir) % FILTER_COUNT;
					ApplyVideoMode ();
				}
				else if (nNew & BTN_A)
				{
					pMsg = "NEEDS MULTICORE";
				}
				break;

			case SmControls:
				if (nNew & BTN_A)
				{
					int nPad = ChoosePad ();
					if (nPad >= 0)
					{
						RunMapping ((unsigned) nPad, m_nLoaded >= 0);
					}
					else if (m_pInput->GetPadCount () == 0)
					{
						pMsg = "CONNECT A USB GAMEPAD";
					}
					nPrev = ~0u;
				}
				break;

			case SmPower:
				if ((nNew & BTN_A) && PowerOffCountdown ())
				{
					PowerOff ();
				}
				nPrev = ~0u;
				break;
			}
		}

		if (nNew & BTN_B)
		{
			break;
		}

		static const char *Labels[SmCount] = {"CPU SPEED", "SCANLINES", "CONTROLS", "POWER OFF"};
		TMenuItem Items[SmCount];
		for (unsigned i = 0; i < SmCount; i++)
		{
			Items[i].pLabel = Labels[i];
			Items[i].pValue = 0;
			Items[i].bDisabled = FALSE;
		}
		Items[SmCpu].pValue = m_nCpuSpeed == CPU_TURBO ? "TURBO" : "NORMAL";
		static const char *FilterLabels[FILTER_COUNT] = {"OFF", "LIGHT", "MEDIUM", "STRONG", "COMPOSITE"};
		// not allowed only on a Pi 3 with multicore=off
		Items[SmFilter].pValue = m_bFilterAllowed ? FilterLabels[m_nFilter] : "MULTICORE OFF";
		Items[SmFilter].bDisabled = !m_bFilterAllowed;

		const char *pShow = pMsg;
		u32 nShowColor = UI_VALUE;
		if (!pShow[0] && nSel == SmCpu)
		{
			pShow = m_nCpuSpeed == CPU_TURBO ? "FEWER SLOWDOWNS" : "AS THE ARCADE BOARD";
		}
		else if (pShow[0])
		{
			nShowColor = UI_ERR;
		}
		static const TButtonHint Hints[2] = {{'A', "CHANGE"}, {'B', "BACK"}};
		m_pUI->DrawPanelMenu (m_nLoaded >= 0, "OPTIONS", Items, SmCount, nSel, pShow, nShowColor,
				      Hints, 2, Footer);
		ShowUIFrame ();
	}

	if (m_nCpuSpeed != nOldCpu || m_nFilter != nOldFilter)
	{
		SaveConfig ();
		char Line[120];
		snprintf (Line, sizeof Line, "New settings: CPU speed %s, filter %s",
			  m_nCpuSpeed == CPU_TURBO ? "turbo" : "normal", FilterName (m_nFilter));
		Log (Line, COLOR_TEXT);
	}
}

// DIP SWITCHES (in-game menu): the game's own switches, with MAME's names and
// settings. A changes the selected one (left/right too), B goes back. They
// reach the board at once; many games only read them when they start, hence
// the note. Saved per game in saves/<game>.dip.
void CKernel::RunDipMenu (void)
{
	if (m_nLoaded < 0)
	{
		return;
	}
	const int nGame = m_pGames[m_nLoaded].nGame;
	enum { MaxItems = 24, Window = 10 };	// up to 10 on screen, the list scrolls
	unsigned nSel = 0, nFirst = 0;
	u32 nPrev = ~0u;
	boolean bChanged = FALSE;
	const u16 nOldDsw = m_nDsw;
	const u8 nOldDsw3 = m_nDsw3;

	while (1)
	{
		// the switches shown now (coin A/B or coinage, as the coin mode says);
		// the unknown ones, the screen flip and the service mode are left out
		int Dips[MaxItems];
		unsigned nItems = 0;
		const int nCount = m92_dip_count (nGame);
		for (int d = 0; d < nCount && nItems < MaxItems; d++)
		{
			TM92Dip D;
			m92_dip_get (nGame, d, &D);
			if (IsHiddenDip (D.pName))
			{
				continue;
			}
			if (D.nCondOp)
			{
				boolean bEq = (m_nDsw & D.nCondMask) == D.nCondValue;
				if ((D.nCondOp == 1) != bEq)
				{
					continue;
				}
			}
			Dips[nItems++] = d;
		}
		if (nItems == 0)
		{
			break;
		}
		if (nSel >= nItems) nSel = nItems - 1;

		m_pInput->UpdateDevices ();
		u32 n = m_pInput->GetAny ();
		u32 nNew = nPrev == ~0u ? 0 : (n & ~nPrev);
		nPrev = n;
		if (nNew & BTN_UP)	nSel = (nSel + nItems - 1) % nItems;
		if (nNew & BTN_DOWN)	nSel = (nSel + 1) % nItems;
		int nDir = (nNew & (BTN_A | BTN_RIGHT)) ? 1 : ((nNew & BTN_LEFT) ? -1 : 0);
		if (nNew & BTN_B)
		{
			break;
		}

		// value of each switch, as text
		static char Values[MaxItems][32];
		TMenuItem Items[MaxItems];
		static char Labels[MaxItems][32];
		for (unsigned i = 0; i < nItems; i++)
		{
			TM92Dip D;
			m92_dip_get (nGame, Dips[i], &D);
			u16 nCur = D.bBank3 ? (u16) (m_nDsw3 << 8) : m_nDsw;
			nCur &= D.nMask;
			int nIdx = 0;
			for (int s = 0; s < D.nSettings; s++)
			{
				u16 v;
				m92_dip_setting_name (nGame, Dips[i], s, &v);
				if (v == nCur) nIdx = s;
			}
			if (i == nSel && nDir)
			{
				nIdx = (nIdx + D.nSettings + nDir) % D.nSettings;
				u16 v;
				m92_dip_setting_name (nGame, Dips[i], nIdx, &v);
				if (D.bBank3)
				{
					m_nDsw3 = (u8) ((m_nDsw3 & ~(D.nMask >> 8)) | (v >> 8));
				}
				else
				{
					m_nDsw = (u16) ((m_nDsw & ~D.nMask) | v);
				}
				m92_set_dsw (m_nDsw, m_nDsw3);
				bChanged = TRUE;
			}
			strncpy (Labels[i], D.pName, sizeof Labels[i] - 1);
			Labels[i][sizeof Labels[i] - 1] = '\0';
			const char *pVal = m92_dip_setting_name (nGame, Dips[i], nIdx, 0);
			// label + value must fit in the wide panel (29 characters with the gap)
			size_t nLabel = strlen (Labels[i]);
			size_t nRoom = nLabel < 28 ? 29 - nLabel : 1;
			strncpy (Values[i], pVal ? pVal : "?", sizeof Values[i] - 1);
			Values[i][sizeof Values[i] - 1] = '\0';
			if (strlen (Values[i]) > nRoom) Values[i][nRoom] = '\0';
			Items[i].pLabel = Labels[i];
			Items[i].pValue = Values[i];
			Items[i].bDisabled = FALSE;
		}

		TM92Dip S;
		m92_dip_get (nGame, Dips[nSel], &S);
		const char *pMsg = bChanged ? "SOME APPLY AFTER RESET" : "";
		if (nSel < nFirst) nFirst = nSel;
		if (nSel >= nFirst + Window) nFirst = nSel - Window + 1;
		if (nFirst + Window > nItems) nFirst = nItems > Window ? nItems - Window : 0;
		unsigned nShown = nItems - nFirst < Window ? nItems - nFirst : (unsigned) Window;
		// the panel keeps the same size for every game and every list length;
		// the arrows say that there are more switches above or below
		static const TButtonHint Hints[2] = {{'A', "CHANGE"}, {'B', "BACK"}};
		m_pUI->DrawPanelMenu (TRUE, "DIP SWITCHES", Items + nFirst, nShown, nSel - nFirst, pMsg, UI_VALUE,
				      Hints, 2, S.pLocation, TRUE, Window, nFirst > 0, nFirst + nShown < nItems);
		ShowUIFrame ();
	}

	if (m_nDsw != nOldDsw || m_nDsw3 != nOldDsw3)
	{
		SaveDips ();
		char Line[80];
		snprintf (Line, sizeof Line, "  DIP switches: DSW %04X, SW3 %02X", m_nDsw, m_nDsw3);
		Log (Line, COLOR_TEXT);
	}
}

// CONTROLS from the SYSTEM menu: which pad? (the only one, or the one that is pressed)
int CKernel::ChoosePad (void)
{
	unsigned nCount = m_pInput->GetPadCount ();
	if (nCount == 0)
	{
		return -1;
	}
	if (nCount == 1)
	{
		for (unsigned i = 0; i < MAX_PADS; i++)
		{
			if (m_pInput->IsPadConnected (i))
			{
				return (int) i;
			}
		}
	}

	boolean bReleased = FALSE;
	for (unsigned f = 0; f < 600; f++)		// 10 seconds
	{
		int nActive = m_pInput->GetActivePad ();
		if (!bReleased)
		{
			bReleased = nActive < 0;
		}
		else if (nActive >= 0)
		{
			return nActive;
		}
		if (m_pInput->GetKeyboard () & BTN_MENU)
		{
			break;
		}
		m_pUI->DrawMessage ("CONTROLS", "PRESS A BUTTON ON THE PAD", "YOU WANT TO SET UP");
		ShowUIFrame ();
	}
	return -1;
}

// Asks for A B C D E F SELECT START in turn on one pad (directions are
// automatic), then saves on its own. A button already used skips the function
// being asked for; Esc on the keyboard cancels.
boolean CKernel::RunMapping (unsigned nSlot, boolean bOverGame)
{
	// slots in the order of the logical bits: A B C D START SELECT E F;
	// asked in the order A B C D E F SELECT START
	static const char *Names[8] = {"A", "B", "C", "D", "START", "SELECT / COIN", "E", "F"};
	static const unsigned SlotOfStep[8] = {0, 1, 2, 3, 6, 7, 5, 4};
	const unsigned nSteps = MAP_COUNT - MAP_FIRST_BUTTON;

	TBind Binds[MAP_COUNT];
	memset (Binds, 0, sizeof Binds);
	static char Values[8][16];
	const char *pValues[8];
	for (unsigned i = 0; i < 8; i++)
	{
		Values[i][0] = '\0';
		pValues[i] = Values[i];
	}

	unsigned nStep = 0;
	boolean bWaitRelease = TRUE;		// e.g. A still held from the SYSTEM menu
	u32 nKeyPrev = ~0u;
	unsigned nFrame = 0;

	char Line[120];
	snprintf (Line, sizeof Line, "Controls: setting up pad %u (%04x:%04x)",
		  nSlot + 1, m_pInput->GetVendor (nSlot), m_pInput->GetProduct (nSlot));
	Log (Line, COLOR_TEXT);

	char Title[32], Footer[40];
	snprintf (Title, sizeof Title, "CONTROLS  PLAYER %u", m_pInput->GetPlayerOfPad (nSlot) + 1);
	snprintf (Footer, sizeof Footer, "PAD %04X:%04X", m_pInput->GetVendor (nSlot),
		  m_pInput->GetProduct (nSlot));

	while (nStep < nSteps)
	{
		if (nFrame++ % 15 == 0)
		{
			m_pInput->UpdateDevices ();
			if (!m_pInput->IsPadConnected (nSlot))
			{
				Log ("  pad unplugged: set-up cancelled", COLOR_TEXT);
				return FALSE;
			}
		}

		u32 nKey = m_pInput->GetKeyboard ();
		u32 nKeyNew = nKeyPrev == ~0u ? 0 : (nKey & ~nKeyPrev);
		nKeyPrev = nKey;
		if (nKeyNew & BTN_MENU)
		{
			Log ("  set-up cancelled", COLOR_TEXT);
			return FALSE;
		}

		if (bWaitRelease)
		{
			bWaitRelease = !m_pInput->IsReleased (nSlot);
		}
		else
		{
			int nButton = m_pInput->CaptureButton (nSlot);
			if (nButton >= 0)
			{
				const unsigned nCur = SlotOfStep[nStep];
				TBind &B = Binds[MAP_FIRST_BUTTON + nCur];
				if (!CInput::IsButtonBound (Binds, MAP_COUNT, nButton))
				{
					B.nType = BindButton;
					B.nIndex = (u8) nButton;
					B.nValue = 0;
				}
				// a button already assigned skips this function: it stays unassigned
				CInput::DescribeBind (B, Values[nCur], sizeof Values[nCur]);
				nStep++;
				bWaitRelease = TRUE;
			}
		}

		m_pUI->DrawMapping (bOverGame, Title, nStep < nSteps ? (int) SlotOfStep[nStep] : 8,
				    pValues, Footer);
		ShowUIFrame ();
	}

	m_pInput->SetMap (nSlot, Binds);
	SavePads ();
	for (unsigned i = 0; i < nSteps; i++)
	{
		unsigned k = SlotOfStep[i];
		snprintf (Line, sizeof Line, "  %s = %s", Names[k], Values[k]);
		Log (Line, COLOR_TEXT);
	}
	Log ("  controls saved to m92pi/pads.cfg", COLOR_TEXT);
	SaveLog ();

	// "SAVED" for one second
	for (unsigned f = 0; f < 60; f++)
	{
		m_pUI->DrawMapping (bOverGame, Title, 8, pValues, Footer);
		ShowUIFrame ();
	}
	return TRUE;
}

boolean CKernel::PowerOffCountdown (void)
{
	// 3 seconds, B cancels; drawn over whatever the UI buffer shows
	static u32 Saved[GAME_SRC_PITCH * GAME_SRC_LINES];
	memcpy (Saved, m_pUI->GetBuffer (), sizeof Saved);
	u32 nPrev = ~0u;
	for (unsigned f = 0; f < 180; f++)
	{
		u32 n = m_pInput->GetAny ();
		u32 nNew = nPrev == ~0u ? 0 : (n & ~nPrev);
		nPrev = n;
		if (nNew & BTN_B)
		{
			memcpy (m_pUI->GetBuffer (), Saved, sizeof Saved);
			Log ("  power off cancelled", COLOR_TEXT);
			return FALSE;
		}
		char Line[24];
		snprintf (Line, sizeof Line, "POWER OFF IN %u", 3 - f / 60);
		memcpy (m_pUI->GetBuffer (), Saved, sizeof Saved);
		static const TButtonHint Cancel = {'B', "CANCEL"};
		m_pUI->DrawBox (Line, "", &Cancel);
		ShowUIFrame ();
	}
	return TRUE;
}

// Credits over the paused game; B (or Esc) goes back to the game
void CKernel::RunCredits (void)
{
	Log ("  credits", COLOR_TEXT);
	u32 nPrev = ~0u;
	for (unsigned nFrame = 0; ; nFrame++)
	{
		if (nFrame % 15 == 0)
		{
			m_pInput->UpdateDevices ();	// a new pad is set up after the credits
		}
		u32 n = m_pInput->GetAny ();
		u32 nNew = nPrev == ~0u ? 0 : (n & ~nPrev);
		if (nPrev == ~0u)
		{
			if (n == 0) nPrev = 0;	// wait for Select and D to be released
		}
		else
		{
			nPrev = n;
		}
		if (nNew & (BTN_B | BTN_MENU))
		{
			return;
		}
		m_pUI->DrawCredits (PROJECT_VERSION);
		ShowUIFrame ();
	}
}
