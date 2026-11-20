/*
 * m92-pi - the emulated frame: picture, sound, where its lines go (to the
 * screen from core 1, or core 0 single core; COMPOSITE: core 2)
 */
#ifndef FRAME_H
#define FRAME_H

#include "screen.h"
#include <stdint.h>

extern u32 g_VideoBuf[GAME_SRC_PITCH * GAME_SRC_LINES];

// after each frame: the core's sound for it (FrameTakeAudio)
extern int16_t g_AudioBuf[8192];
extern unsigned g_nAudioFrames;		// stereo frames

void FrameSetup (CScreen *pVideo);	// once, before the first frame
void FrameTakeAudio (void);
void FrameSync (void);			// wait until cores 1 and 2 have finished the frame

// FALSE: the lines stay in g_VideoBuf (benchmark, MC test); FrameDraw shows them
void FrameLinesToScreen (boolean bOn);
boolean FrameTvInit (void);		// COMPOSITE's tables (about 4 MB, built once)
boolean FrameTvTables (void);		// FrameTvInit done
void FrameSetComposite (boolean bOn);
boolean FrameIsComposite (void);
void FrameSetRaced (boolean bOn);	// this frame is beam raced

void FrameDraw (void);			// g_VideoBuf to the screen, if not there yet
void FrameDrawPicture (u32 *pBuffer, boolean bFilter = TRUE);	// a UI picture, through the same filter (or none)

unsigned FrameMcFailFrame (void);	// frame at which core 1 stopped answering

#endif
