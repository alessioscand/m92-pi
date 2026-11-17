/*
 * m92-pi - the video lines drawn on core 1: core 0 (V33, V35, sound) queues each line
 * with its registers; before changing what they read (VRAM, palette, sprite buffer) McSync waits
 * for them (the same picture as single core). As neo-pi: DSB + SEV, core 1
 * stuck 0.5 s: single core.
 */
#ifndef MCLINE_H
#define MCLINE_H

#include <circle/types.h>
#include "m92_api.h"

// called on core 1 after each line it has drawn into the frame buffer
typedef void TMcLineDone (unsigned nRow);

void McSetup (u32 *pFrame, unsigned nPitch, TMcLineDone *pDone);
void McEnable (boolean bOn);		// on: the core's lines go through core 1
boolean McEnabled (void);
boolean McFailed (void);
boolean McAlive (void);
void McSync (void);			// core 0: wait for every queued line
void McWorker (void);			// core 1's loop, never returns

// diagnostics when core 1 was found stuck
void McFailText (char *pOut, size_t nSize, unsigned nFrame);

#endif
