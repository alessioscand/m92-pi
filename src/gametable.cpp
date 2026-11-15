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
	{"atompunk", "Atomic Punk", "US", "1991", "Irem / Hudson Soft", {"Bomb", "Detonate"}},
	{"bbmanw", "Bomber Man World", "World", "1992", "Irem", {"Bomb", "Detonate"}},
	{"bbmanwj", "Bomber Man World", "Japan Set 1", "1992", "Irem", {"Bomb", "Detonate"}},
	{"bbmanwja", "Bomber Man World", "Japan Set 2", "1992", "Irem", {"Bomb", "Detonate"}},
	{"bmaster", "Blade Master", "World", "1991", "Irem", {"Attack", "Jump"}},
	{"bombrman", "Bomber Man", "Japan", "1991", "Irem / Hudson Soft", {"Bomb", "Detonate"}},
	{"crossbld", "Cross Blades!", "Japan", "1991", "Irem", {"Attack", "Jump"}},
	{"dsoccr94j", "Dream Soccer '94", "Japan", "1994", "Irem", {"Shoot", "Pass"}},
	{"dynablst", "Dynablaster", "World", "1991", "Irem / Hudson Soft", {"Bomb", "Detonate"}},
	{"geostorm", "Geo Storm", "Japan 014", "1994", "Irem", {"Fire", "Jump"}},
	{"geostorma", "Geo Storm", "Japan 026", "1994", "Irem", {"Fire", "Jump"}},
	{"gunforc2", "Gun Force II", "US", "1994", "Irem", {"Fire", "Jump"}},
	{"gunforce", "Gunforce", "World", "1991", "Irem", {"Fire", "Jump"}},
	{"gunforcej", "Gunforce", "Japan", "1991", "Irem", {"Fire", "Jump"}},
	{"gunforceu", "Gunforce", "US", "1991", "Irem", {"Fire", "Jump"}},
	{"gunhohki", "Mahou Keibitai Gun Hohki", "Japan", "1992", "Irem", {"Shot", "Broom"}},
	{"gussun", "Gussun Oyoyo", "Japan", "1993", "Irem", {"Rotate Counter", "Rotate Clockwise"}},
	{"hasamu", "Hasamu", "Japan", "1991", "Irem", {"Select", "Cancel"}},
	{"hook", "Hook", "World", "1992", "Irem", {"Attack", "Jump"}},
	{"hookj", "Hook", "Japan", "1992", "Irem", {"Attack", "Jump"}},
	{"hooku", "Hook", "US", "1992", "Irem", {"Attack", "Jump"}},
	{"inthunt", "In The Hunt", "World", "1993", "Irem", {"Torpedo", "Missile"}},
	{"inthuntk", "In The Hunt", "Korea", "1993", "Irem", {"Torpedo", "Missile"}},
	{"inthuntu", "In The Hunt", "US", "1993", "Irem", {"Torpedo", "Missile"}},
	{"kaiteids", "Kaitei Daisensou", "Japan", "1993", "Irem", {"Torpedo", "Missile"}},
	{"leaguemn", "Yakyuu Kakutou League-Man", "Japan Set 1", "1993", "Irem", {"Attack", "Jump"}},
	{"leaguemna", "Yakyuu Kakutou League-Man", "Japan Set 2", "1993", "Irem", {"Attack", "Jump"}},
	{"lethalth", "Lethal Thunder", "World", "1991", "Irem", {"Fire", "Bomb"}},
	{"majtitl2", "Major Title 2", "World Set 1", "1992", "Irem", {"Shot", "Option"}},
	{"majtitl2a", "Major Title 2", "World Set 2", "1992", "Irem", {"Shot", "Option"}},
	{"majtitl2b", "Major Title 2", "World Set 3", "1992", "Irem", {"Shot", "Option"}},
	{"majtitl2j", "Major Title 2", "Japan", "1992", "Irem", {"Shot", "Option"}},
	{"matchit2", "Match It II", "World", "1993", "Tamtex", {0}},
	{"mysticri", "Mystic Riders", "World", "1992", "Irem", {"Shot", "Broom"}},
	{"mysticrib", "Mystic Riders", "Bootleg", "1992", "Irem", {"Shot", "Broom"}},
	{"nbbatman", "Ninja Baseball Bat Man", "World", "1993", "Irem", {"Attack", "Jump"}},
	{"nbbatmanu", "Ninja Baseball Bat Man", "US", "1993", "Irem", {"Attack", "Jump"}},
	{"newapunk", "New Atomic Punk", "US", "1992", "Irem", {"Bomb", "Detonate"}},
	{"psoldier", "Perfect Soldiers", "Japan", "1993", "Irem", {"Punch", "Punch", "Punch", "Kick", "Kick", "Kick"}},
	{"quizf1", "Quiz F1 1-2 Finish", "Japan", "1992", "Irem", {"Answer", "Answer", "Answer", "Answer"}},
	{"riskchal", "Risky Challenge", "World", "1993", "Irem", {"Rotate Counter", "Rotate Clockwise"}},
	{"rtypeleo", "R-Type Leo", "World", "1992", "Irem", {"Fire", "Psy-Bit"}},
	{"rtypeleoj", "R-Type Leo", "Japan", "1992", "Irem", {"Fire", "Psy-Bit"}},
	{"shisen2", "Shisensho II", "Japan", "1993", "Tamtex", {0}},
	{"skingame", "The Irem Skins Game", "US Set 1", "1992", "Irem", {"Shot", "Option"}},
	{"skingame2", "The Irem Skins Game", "US Set 2", "1992", "Irem", {"Shot", "Option"}},
	{"ssoldier", "Superior Soldiers", "US", "1993", "Irem", {"Punch", "Punch", "Punch", "Kick", "Kick", "Kick"}},
	{"thndblst", "Thunder Blaster", "Japan", "1991", "Irem", {"Fire", "Bomb"}},
	{"uccops", "Undercover Cops", "World", "1992", "Irem", {"Attack", "Jump"}},
	{"uccopsar", "Undercover Cops Alpha", "World", "1992", "Irem", {"Attack", "Jump"}},
	{"uccopsaru", "Undercover Cops Alpha", "US", "1992", "Irem", {"Attack", "Jump"}},
	{"uccopsj", "Undercover Cops", "Japan", "1992", "Irem", {"Attack", "Jump"}},
	{"uccopsu", "Undercover Cops", "US", "1992", "Irem", {"Attack", "Jump"}},
};

const unsigned g_nGameInfo = sizeof g_GameInfo / sizeof g_GameInfo[0];
