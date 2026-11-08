/*
 * m92-pi - what the shared files in common/ need to know about the project
 */
#ifndef PROJECT_H
#define PROJECT_H

#define PROJECT_NAME	"m92-pi"
#define PROJECT_VERSION	"1.0.0"

/* The emulated picture */
#define PROJECT_WIDTH	320
#define PROJECT_HEIGHT	240

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
