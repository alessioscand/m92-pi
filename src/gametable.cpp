/*
 * m92-pi - what the game list and the screen around the game show of each set:
 * a short title, the region under the list, year, maker and the names of
 * buttons A..F (none = not known: nothing is shown). Written by hand, once:
 * titles and regions from the old m92pi/list.txt, buttons from controls.dat
 * (SirPoonga, Howard Casto) and the games themselves.
 */
#include "gameinfo.h"

const TGameInfo g_GameInfo[] =
{
	{"atompunk", "Atomic Punk", "US", "1991", "Irem / Hudson Soft", {"bomb", "detonate"}},
	{"bbmanw", "Bomber Man World", "World", "1992", "Irem", {"bomb", "detonate"}},
	{"bbmanwj", "Bomber Man World", "Japan Set 1", "1992", "Irem", {"bomb", "detonate"}},
	{"bbmanwja", "Bomber Man World", "Japan Set 2", "1992", "Irem", {"bomb", "detonate"}},
	{"bmaster", "Blade Master", "World", "1991", "Irem", {"attack", "jump"}},
	{"bombrman", "Bomber Man", "Japan", "1991", "Irem / Hudson Soft", {"bomb", "detonate"}},
	{"crossbld", "Cross Blades!", "Japan", "1991", "Irem", {"attack", "jump"}},
	{"dsoccr94j", "Dream Soccer '94", "Japan", "1994", "Irem", {"shoot", "pass"}},
	{"dynablst", "Dynablaster", "World", "1991", "Irem / Hudson Soft", {"bomb", "detonate"}},
	{"geostorm", "Geo Storm", "Japan 014", "1994", "Irem", {"fire", "jump"}},
	{"geostorma", "Geo Storm", "Japan 026", "1994", "Irem", {"fire", "jump"}},
	{"gunforc2", "Gun Force II", "US", "1994", "Irem", {"fire", "jump"}},
	{"gunforce", "Gunforce", "World", "1991", "Irem", {"fire", "jump"}},
	{"gunforcej", "Gunforce", "Japan", "1991", "Irem", {"fire", "jump"}},
	{"gunforceu", "Gunforce", "US", "1991", "Irem", {"fire", "jump"}},
	{"gunhohki", "Mahou Keibitai Gun Hohki", "Japan", "1992", "Irem", {"shot", "broom"}},
	{"gussun", "Gussun Oyoyo", "Japan", "1993", "Irem", {"rotate counter", "rotate clockwise"}},
	{"hasamu", "Hasamu", "Japan", "1991", "Irem", {"select", "cancel"}},
	{"hook", "Hook", "World", "1992", "Irem", {"attack", "jump"}},
	{"hookj", "Hook", "Japan", "1992", "Irem", {"attack", "jump"}},
	{"hooku", "Hook", "US", "1992", "Irem", {"attack", "jump"}},
	{"inthunt", "In The Hunt", "World", "1993", "Irem", {"torpedo", "missile"}},
	{"inthuntk", "In The Hunt", "Korea", "1993", "Irem", {"torpedo", "missile"}},
	{"inthuntu", "In The Hunt", "US", "1993", "Irem", {"torpedo", "missile"}},
	{"kaiteids", "Kaitei Daisensou", "Japan", "1993", "Irem", {"torpedo", "missile"}},
	{"leaguemn", "Yakyuu Kakutou League-Man", "Japan Set 1", "1993", "Irem", {"attack", "jump"}},
	{"leaguemna", "Yakyuu Kakutou League-Man", "Japan Set 2", "1993", "Irem", {"attack", "jump"}},
	{"lethalth", "Lethal Thunder", "World", "1991", "Irem", {"fire", "bomb"}},
	{"majtitl2", "Major Title 2", "World Set 1", "1992", "Irem", {"shot", "option"}},
	{"majtitl2a", "Major Title 2", "World Set 2", "1992", "Irem", {"shot", "option"}},
	{"majtitl2b", "Major Title 2", "World Set 3", "1992", "Irem", {"shot", "option"}},
	{"majtitl2j", "Major Title 2", "Japan", "1992", "Irem", {"shot", "option"}},
	{"matchit2", "Match It II", "World", "1993", "Tamtex", {0}},
	{"mysticri", "Mystic Riders", "World", "1992", "Irem", {"shot", "broom"}},
	{"mysticrib", "Mystic Riders", "Bootleg", "1992", "Irem", {"shot", "broom"}},
	{"nbbatman", "Ninja Baseball Bat Man", "World", "1993", "Irem", {"attack", "jump"}},
	{"nbbatmanu", "Ninja Baseball Bat Man", "US", "1993", "Irem", {"attack", "jump"}},
	{"newapunk", "New Atomic Punk", "US", "1992", "Irem", {"bomb", "detonate"}},
	{"psoldier", "Perfect Soldiers", "Japan", "1993", "Irem", {"punch", "punch", "punch", "kick", "kick", "kick"}},
	{"quizf1", "Quiz F1 1-2 Finish", "Japan", "1992", "Irem", {"answer", "answer", "answer", "answer"}},
	{"riskchal", "Risky Challenge", "World", "1993", "Irem", {"rotate counter", "rotate clockwise"}},
	{"rtypeleo", "R-Type Leo", "World", "1992", "Irem", {"fire", "psy-bit"}},
	{"rtypeleoj", "R-Type Leo", "Japan", "1992", "Irem", {"fire", "psy-bit"}},
	{"shisen2", "Shisensho II", "Japan", "1993", "Tamtex", {0}},
	{"skingame", "The Irem Skins Game", "US Set 1", "1992", "Irem", {"shot", "option"}},
	{"skingame2", "The Irem Skins Game", "US Set 2", "1992", "Irem", {"shot", "option"}},
	{"ssoldier", "Superior Soldiers", "US", "1993", "Irem", {"punch", "punch", "punch", "kick", "kick", "kick"}},
	{"thndblst", "Thunder Blaster", "Japan", "1991", "Irem", {"fire", "bomb"}},
	{"uccops", "Undercover Cops", "World", "1992", "Irem", {"attack", "jump"}},
	{"uccopsar", "Undercover Cops Alpha", "World", "1992", "Irem", {"attack", "jump"}},
	{"uccopsaru", "Undercover Cops Alpha", "US", "1992", "Irem", {"attack", "jump"}},
	{"uccopsj", "Undercover Cops", "Japan", "1992", "Irem", {"attack", "jump"}},
	{"uccopsu", "Undercover Cops", "US", "1992", "Irem", {"attack", "jump"}},
};

const unsigned g_nGameInfo = sizeof g_GameInfo / sizeof g_GameInfo[0];
