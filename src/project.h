/*
 * m92-pi - what the shared files in common/ need to know about the project
 */
#ifndef PROJECT_H
#define PROJECT_H

#define PROJECT_NAME	"m92-pi"
#define PROJECT_VERSION	"1.2.0"

/* The emulated picture: the core hands over 320x240 lines, pitch 320 */
#define PROJECT_WIDTH	320
#define PROJECT_HEIGHT	240
#define PROJECT_SRC_PITCH	320		/* the game's buffer */
#define PROJECT_SRC_LINES	240
#define PROJECT_SRC_X	0			/* the picture in it */
#define PROJECT_SRC_Y	0

/* TV filter (common/tvcore, common/composite): one line in (320 pixels
   0x00RRGGBB), the filtered line out (COMP_OUT_W) */
#define PROJECT_TV_IN_BYTES	(320 * 4)
#define PROJECT_TV_OUT_W	749

/* Players: one pad each (common/input) */
#define PROJECT_PLAYERS	4

/* Buttons besides the directions, START and SELECT: A B C D E F */
#define PROJECT_BUTTONS	6

/* Each emulated frame starts here: the first line after the picture */
#define PROJECT_PHASE_LINE	248

/* Audio queue (common/audio.cpp): ~1 video frame (~17 ms) instead of 2,
   less audio lag; the sound is made a line at a time (core/m92.cpp) */
#define PROJECT_AUDIO_TARGET	800
#define PROJECT_AUDIO_BAND	200

/* Error codes = number of green LED blinks when the screen is unusable */
#define PROJECT_ERR_FB    2   /* framebuffer / HDMI */
#define PROJECT_ERR_SD    3   /* SD card or FAT filesystem */
#define PROJECT_ERR_GAME  5   /* no M92 game (MAME .zip) in the m92pi folder */
#define PROJECT_ERR_CORE  6   /* emulator core error */
#define PROJECT_ERR_MEM   7   /* out of memory */

#ifdef __cplusplus
extern "C" {
#endif

void project_log (const char *pFormat, ...);
void project_fatal (int nCode, const char *pFormat, ...);

#ifdef __cplusplus
}
#endif

#endif
