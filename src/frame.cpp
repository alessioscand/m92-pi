/*
 * m92-pi - the emulated frame (see frame.h)
 */
#include "frame.h"
#include "mcline.h"
#include "racing.h"
#include "smp.h"
#include "tvcore.h"
#include "composite.h"
#include <circle/new.h>
#include "m92_api.h"

u32 g_VideoBuf[GAME_SRC_PITCH * GAME_SRC_LINES] __attribute__ ((aligned (64)));
int16_t g_AudioBuf[8192] __attribute__ ((aligned (64)));
unsigned g_nAudioFrames;

static CScreen *s_pVideo = 0;
static u32 s_Line[GAME_W * 8], s_Dark[GAME_W * 8];	// core 1's scanline buffers
static u32 s_TvLine[2][GAME_W * 8];		// core 2's, and the caller's without it

// read by core 1 for each line; changed only while no line is queued
static boolean s_bLinesToScreen = FALSE;
static boolean s_bComposite = FALSE;
static boolean s_bRaced = FALSE;

static unsigned s_nTvBurst = 0;			// colour burst phase of the frame (0-2)
static unsigned s_nCoreFrames = 0, s_nMcFailFrame = 0;

// Cabinet voicing, always on: the board's amplifier and a cabinet speaker
// after MAME's flat mix, so that the YM2151 is less tinny. Low shelf +4 dB
// below 120 Hz, 2nd-order low-pass at 8 kHz, -0.5 dB to keep the loudness.
// One sample at a time, no buffering: no added latency (about 0.1% of core 0).
static float s_Voice[4];			// biquad states (transposed direct form II)

static void CabinetVoicing (int16_t *pBuf, unsigned nFrames)
{
	static const float B0 = 1.00256636f, B1 = -1.98014518f, B2 = 0.97788640f,
			   A1 = -1.98020194f, A2 = 0.98039601f;		// low shelf
	static const float L0 = 0.15504213f, L1 = 0.31008426f, L2 = 0.15504213f,
			   M1 = -0.62016853f, M2 = 0.24033706f;		// low-pass
	static const float Gain = 0.944f;				// -0.5 dB
	for (unsigned i = 0; i < nFrames; i++)
	{
		float x = (float) pBuf[i * 2];				// mono: L = R
		float y = B0 * x + s_Voice[0];
		s_Voice[0] = B1 * x - A1 * y + s_Voice[1];
		s_Voice[1] = B2 * x - A2 * y;
		float z = L0 * y + s_Voice[2];
		s_Voice[2] = L1 * y - M1 * z + s_Voice[3];
		s_Voice[3] = L2 * y - M2 * z;
		z *= Gain;
		int v = (int) (z < 0 ? z - 0.5f : z + 0.5f);
		if (v > 32767) v = 32767;
		if (v < -32768) v = -32768;
		pBuf[i * 2] = pBuf[i * 2 + 1] = (int16_t) v;
	}
}

void FrameTakeAudio (void)
{
	const int nMax = (int) (sizeof g_AudioBuf / sizeof g_AudioBuf[0] / 2);
	g_nAudioFrames = (unsigned) m92_audio_read (g_AudioBuf, nMax);
	CabinetVoicing (g_AudioBuf, g_nAudioFrames);
}

// core 1 (or core 0 single core): a finished line
static void LineDone (unsigned nRow)
{
	if (!SharedLoad (s_bLinesToScreen))
	{
		return;
	}
	if (SharedLoad (s_bComposite))
	{
		TvPost (g_VideoBuf + nRow * GAME_SRC_PITCH,
			(s_nTvBurst + nRow) % COMP_PHASES, nRow, SharedLoad (s_bRaced));
	}
	else
	{
		s_pVideo->DrawGameLine (g_VideoBuf, nRow, s_Line, s_Dark);
		if (SharedLoad (s_bRaced))
		{
			RaceRowDone (nRow);
		}
	}
}

// core 2 (or the caller without it, nSide 1): a filtered line
static void TvLineOut (unsigned nRow, const u32 *pNtsc, boolean bRace, unsigned nSide)
{
	s_pVideo->DrawNtscLine (nRow, pNtsc, COMP_OUT_W, s_TvLine[nSide]);
	if (bRace)
	{
		RaceRowDone (nRow);
	}
}

// COMPOSITE: blargg's own composite look, on the game's lines
static_assert (COMP_OUT_W == PROJECT_TV_OUT_W, "project.h: PROJECT_TV_OUT_W");
static void *s_pTables = 0;

static void CompositeLine (const void *pSource, unsigned nPhase, u32 *pOut)
{
	composite_line (s_pTables, (const unsigned *) pSource, GAME_W, nPhase, pOut);
}

boolean FrameTvInit (void)
{
	if (s_pTables == 0)
	{
		void *p = new u8[composite_size ()];
		if (p == 0)
		{
			return FALSE;
		}
		composite_init (p, 0);
		s_pTables = p;
		TvSetFilter (CompositeLine);
	}
	return TRUE;
}

void FrameSetup (CScreen *pVideo)
{
	s_pVideo = pVideo;
	TvSetOutput (TvLineOut);
	McSetup (g_VideoBuf, GAME_SRC_PITCH, LineDone);
}

void FrameSync (void)
{
	McSync ();
	if (s_bComposite)
	{
		TvSync ();
		// the phase steps through all three values: the colour artifacts crawl
		// along the edges even on a still picture (the classic dot crawl)
		s_nTvBurst = (s_nTvBurst + 1) % COMP_PHASES;
	}
	s_nCoreFrames++;
	if (McFailed () && s_nMcFailFrame == 0)
	{
		s_nMcFailFrame = s_nCoreFrames;
	}
}

void FrameLinesToScreen (boolean bOn)
{
	SharedStore (s_bLinesToScreen, bOn);
}

void FrameSetComposite (boolean bOn)
{
	TvSync ();
	SharedStore (s_bComposite, bOn);
	__asm__ volatile ("dsb ish" ::: "memory");
}

boolean FrameIsComposite (void)
{
	return s_bComposite;
}

void FrameSetRaced (boolean bOn)
{
	SharedStore (s_bRaced, bOn);
}

void FrameDraw (void)
{
	if (s_bLinesToScreen)
	{
		return;				// each line went to the screen as it was drawn
	}
	if (s_bComposite)
	{
		for (unsigned y = 0; y < GAME_H; y++)
		{
			TvPost (g_VideoBuf + (y + GAME_SRC_Y) * GAME_SRC_PITCH + GAME_SRC_X,
				(s_nTvBurst + y) % COMP_PHASES, y, FALSE);
		}
		TvSync ();
		s_nTvBurst = (s_nTvBurst + 1) % COMP_PHASES;
		return;
	}
	s_pVideo->DrawGame (g_VideoBuf);
}

// a fixed colour burst phase, so the dots do not crawl on still text
void FrameDrawPicture (u32 *pBuffer)
{
	if (s_bComposite)
	{
		for (unsigned y = 0; y < GAME_H; y++)
		{
			TvPost (pBuffer + (y + GAME_SRC_Y) * GAME_SRC_PITCH + GAME_SRC_X, y % COMP_PHASES, y, FALSE);
		}
		TvSync ();
		return;
	}
	s_pVideo->DrawGame (pBuffer);
}

unsigned FrameMcFailFrame (void)
{
	return s_nMcFailFrame;
}
