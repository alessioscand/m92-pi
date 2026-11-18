/*
 * m92-pi - hiscores (saves/<game>.hi) and Major Title 2's EEPROM
 * (saves/<game>.nv) through common/hiscore.cpp
 */
#include "kernel.h"
#include "mcline.h"
#include "m92_api.h"
#include "hiscore.h"

// the EEPROM, as battery RAM: only the games that have one
static size_t NvSize (void)
{
	size_t nSize = 0;
	return m92_eeprom (&nSize) ? nSize : 0;
}

static u8 *NvData (void)
{
	size_t nSize;
	return m92_eeprom (&nSize);
}

static const THiCore s_HiCore = {m92_hi_size, m92_hi_ready, m92_hi_get, m92_hi_put};
static const TNvCore s_NvCore = {NvSize, NvData};
static CHiscore s_Hiscore;
static CBatteryRam s_Battery;

void CKernel::HiStart (void)
{
	char Path[160];
	GamePath (Path, sizeof Path, "hi");
	s_Hiscore.Start (&s_HiCore, Path);
}

void CKernel::HiStop (void)
{
	s_Hiscore.Stop ();
}

// after each frame: the saved table goes in once the game has set its own up
void CKernel::HiTick (void)
{
	switch (s_Hiscore.Tick ())
	{
	case CardLoaded:	Log ("  hiscores: loaded from the card", COLOR_DIM);	break;
	case CardBlank:		Log ("  hiscores: none saved yet", COLOR_DIM);		break;
	default:		break;
	}
}

void CKernel::HiSave (void)
{
	McSync ();
	switch (s_Hiscore.Save ())
	{
	case CardSaved:		Log ("  hiscores: saved", COLOR_DIM);		break;
	case CardFailed:	Log ("  hiscores: NOT saved", COLOR_ERR);	break;
	default:		break;
	}
}

// before the reset: the board reads its EEPROM when it starts
void CKernel::NvStart (void)
{
	char Path[160];
	GamePath (Path, sizeof Path, "nv");
	switch (s_Battery.Start (&s_NvCore, Path))
	{
	case CardLoaded:	Log ("  EEPROM: loaded from the card", COLOR_DIM);	break;
	case CardBlank:		Log ("  EEPROM: none saved yet", COLOR_DIM);	break;
	default:		break;
	}
}

// CLEAR MEMORY: the EEPROM back to its factory contents, the file deleted
void CKernel::NvClear (void)
{
	s_Battery.Clear ();
}

void CKernel::NvSave (void)
{
	McSync ();
	switch (s_Battery.Save ())
	{
	case CardSaved:		Log ("  EEPROM: saved", COLOR_DIM);		break;
	case CardFailed:	Log ("  EEPROM: NOT saved", COLOR_ERR);	break;
	default:		break;
	}
}
