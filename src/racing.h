/*
 * m92-pi - beam racing state shared by the game loop (see race.h, raster.h)
 */
#ifndef RACING_H
#define RACING_H

#include "race.h"

#define M92_FIRST_LINE	8		// the core's first visible line

struct TRacing
{
	int		nCfg;		// beamracing= in m92pi.cfg: -1 default (on), 0 off, 1 on
	boolean		bReady;		// calibrated at start-up
	boolean		bGame;		// racing in this game (until it gives up)
	CRaster		*pRaster;
	CBeamRacer	*pRacer;
	u64		nWaitTicks;	// waits for the beam in this frame: pacing, not work
};

extern TRacing g_Race;

void RaceCreate (void);			// once: the raster reader and the racer

void RaceLineHook (void *, int nLine);	// core 0, start of each line of a raced frame
void RaceRowDone (unsigned nRow);	// the row is in the framebuffer being shown

#endif
