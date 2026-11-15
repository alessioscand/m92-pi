/*
 * What the game list and the screen around the game show of each set:
 * the same structure in every sibling project, its own table in gameinfo.cpp
 */
#ifndef _gameinfo_h
#define _gameinfo_h

#define GAMEINFO_BUTTONS	6

struct TGameInfo
{
	const char *pSet;			// MAME's set name
	const char *pTitle;			// short title: "Undercover Cops"
	const char *pRegion;			// under the list; "" = from MAME's name
	const char *pYear;
	const char *pMaker;
	const char *pButtons[GAMEINFO_BUTTONS];	// buttons A..F, 0 after the last;
						// all 0 = not known, nothing is shown
};

// 0 if the set is not in the table: then MAME's name, year and maker
const TGameInfo *GameInfoFind (const char *pSet);

#endif
