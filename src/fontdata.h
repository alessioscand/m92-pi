/*
 * m92-pi - built-in menu font
 */
#ifndef M92PI_FONTDATA_H
#define M92PI_FONTDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_DEGREE	"\x7F"		// degree sign in g_MenuFont

extern const unsigned char g_MenuFont[96][8];
extern const unsigned char g_ListFont[96][8];	// the game list (fontlist.c)

#ifdef __cplusplus
}
#endif

#endif
