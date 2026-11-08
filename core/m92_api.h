/*
 * m92-pi - C interface to the M92 core
 *
 * The core is C++ with the standard library; the Circle front-end is built
 * without it. This header is all the front-end sees (as nes-pi's nes_core.h).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef M92_API_H
#define M92_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- games (MAME romsets) --------------------------------------------- */
int m92_num_games (void);
int m92_find (const char *pName);			/* -1 = not an M92 set */
const char *m92_game_name (int nGame);			/* "uccops" */
const char *m92_game_parent (int nGame);		/* "" or the parent's name */
const char *m92_game_desc (int nGame);			/* "Undercover Cops (World)" */
const char *m92_game_year (int nGame);
const char *m92_game_maker (int nGame);
int m92_game_players (int nGame);			/* 2, 3 or 4 */
int m92_game_buttons (int nGame);

/* ---- DIP switches -------------------------------------------------------
 * The value of each switch is (dsw & mask) for the DSW port, or the SW3 bank
 * shifted into bits 8-15 when bank3 is set (as MAME's COINS_DSW3 port). */
typedef struct
{
	const char *pName;		/* "Lives" */
	const char *pLocation;		/* "SW1:1,2" */
	uint16_t nMask, nDefault;
	int bBank3;
	int nSettings;
	uint16_t nCondMask, nCondValue;	/* shown only when (dsw & mask) ==/!= value */
	int nCondOp;			/* 0 none, 1 equals, 2 not equals */
} TM92Dip;
int m92_dip_count (int nGame);
int m92_dip_get (int nGame, int nDip, TM92Dip *pDip);
const char *m92_dip_setting_name (int nGame, int nDip, int nSetting, uint16_t *pValue);
uint16_t m92_dsw_default (int nGame);
uint8_t m92_dsw3_default (int nGame);

/* ---- the machine ---------------------------------------------------------- */

/* reads one ROM file into pDst (exactly nSize bytes); returns non-zero if found */
typedef int TM92Fetch (void *pCtx, const char *pName, uint32_t nCRC, uint32_t nSize, uint8_t *pDst);

/* builds the machine for a game; 0 on failure (pMissing gets the ROM's name) */
int m92_load (int nGame, TM92Fetch *pFetch, void *pCtx, char *pMissing, size_t nMissing);
void m92_unload (void);
void m92_reset (void);

/* one frame: from the start of the vertical blank (line 248) to the next */
void m92_run_frame (void);

/* players 0-3, active high: bit 0 right, 1 left, 2 down, 3 up, 4 button 4,
 * 5 button 3, 6 button 2, 7 button 1. Players 3 and 4: bit 4 = start, 5 = coin. */
void m92_set_player (int nPlayer, uint8_t nBits);
/* bit 0 start 1, 1 start 2, 2 coin 1, 3 coin 2, 4 service, 5 test (active high) */
void m92_set_system (uint8_t nBits);
void m92_set_dsw (uint16_t nDSW, uint8_t nDSW3);
/* main CPU speed in percent: 100 = 9 MHz as the board, 222 = 20 MHz (MiSTer Turbo) */
void m92_set_cpu_speed (int nPercent);

/* sound: 48 kHz stereo frames made so far */
int m92_audio_read (int16_t *pDst, int nMaxFrames);

/* video: 320x240 pixels 0x00RRGGBB, one line at a time */
typedef void TM92LineCB (void *pCtx, int y, const uint32_t *pRGB);
void m92_set_line_callback (TM92LineCB *pCB, void *pCtx);
/* called at the start of each of the 262 lines (8-247 visible), before it is drawn */
typedef void TM92LineHook (void *pCtx, int nLine);
void m92_set_line_hook (TM92LineHook *pHook, void *pCtx);

/* multicore: lines handed to a sink with the registers to draw them with;
 * the sync function must wait until every line handed over has been drawn */
typedef struct { uint16_t pf_control[3][4]; uint16_t master_control[4]; uint16_t videocontrol; } TM92LineRegs;
typedef void TM92LineSink (void *pCtx, int y, const TM92LineRegs *pRegs);
void m92_set_line_sink (TM92LineSink *pSink, void *pCtx);	/* 0 = draw on this core */
void m92_set_video_sync (void (*pSync) (void *), void *pCtx);
void m92_render_line (const TM92LineRegs *pRegs, int y, uint32_t *pOut);

/* save states */
size_t m92_state_size (void);
int m92_state_save (void *pBuffer, size_t nSize);	/* returns the size written, 0 = failed */
int m92_state_load (const void *pBuffer, size_t nSize);

/* Major Title 2: the EEPROM (8 KB), to keep between runs */
uint8_t *m92_eeprom (size_t *pSize);

/* hiscores (MAME's hiscore.dat): size of the .hi file, 0 = none for this game;
 * ready once the game has set its table up; get / put read and write it */
size_t m92_hi_size (void);
int m92_hi_ready (void);
void m92_hi_get (uint8_t *pDst);
void m92_hi_put (const uint8_t *pSrc);

/* diagnostics */
uint32_t m92_main_pc (void);
uint32_t m92_sound_pc (void);

#ifdef __cplusplus
}
#endif

#endif
