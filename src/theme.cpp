/*
 * m92-pi - its look: colours, fonts, logo, credits and the grid (theme.h)
 */
#include "theme.h"
#include "fonts.h"

// the logo, 88x15: # outline, o letters, t / g the i (cyan over green)
static const char *const s_Logo[] =
{
	"###################################################..........###########################",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooooooooooooo#ttttttt#",
	"#ooooooooooooooooo#ooooo#####ooooo###########ooooo#.########.#ooooooo#####ooooo#########",
	"#ooooooooooooooooo#ooooooooooooooo#ooooooooooooooo#.#oooooo#.#ooooooooooooooooo#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#.#oooooo#.#ooooooooooooooooo#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#.#oooooo#.#ooooooooooooooooo#ggggggg#",
	"#ooooo#ooooo#ooooo###########ooooo#ooooo###########.########.#ooooooo###########ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"#ooooo#ooooo#ooooo#ooooooooooooooo#ooooooooooooooo#..........#ooooooo#.........#ggggggg#",
	"###################################################..........#########.........#########",
};

static const char s_LogoKeys[] = "#otg";
static const u32 s_LogoColors[] = {0x000000, 0xEEF2F3, 0x37D6D6, 0x83FA27};

static const char *const s_Credits[][2] =
{
	{"CPUS",   "MAME NEC - B.MCPHAIL"},
	{"Z80",    "N.ALLEMAND, R.CARMICHAEL"},
	{"BOARDS", "MAME M92 M90 - B.MCPHAIL"},
	{"VIDEO",  "MISTER - M.DONLON"},
	{"SOUND",  "YMFM - A.GILES"},
	{"GA20",   "MAME - A.TANG,R.BELMONT"},
	{"ZIP",    "MINIZ - R.GELDREICH"},
	{"SYSTEM", "CIRCLE - R.STANGE"},
	{"NTSC",   "BLARGG - S.GREEN"},
	{"FONT",   "ZX ORIGINS - D.GUARD"}
};

// dark petrol panels, a teal band
const TUITheme g_Theme =
{
	"M92-PI",
	0xC8D6D8, 0x4E6468, 0x05161A,		// frame, outline, panel
	0x13525A, 0xFF5050,			// band, cursor
	0xF8F8F8, 0x37D6D6, 0x96A8AB,		// title, value, dim
	0xFF5050,				// POWER OFF
	g_FontYouSquared, g_FontCyberwire, FALSE,
	g_FontLander,				// around the picture
	0x37D6D6, TRUE,				// the list's bottom row: value, list font
	s_Logo, sizeof s_Logo / sizeof s_Logo[0], 1, s_LogoKeys, s_LogoColors,
	s_Credits, sizeof s_Credits / sizeof s_Credits[0],
	"IREM IS A TRADEMARK", "OF ITS OWNER.",
	{0, 0}, 0, 0,				// no frame: the grid's dither
	{4, 3}					// the columns' text a little further out
};

// the grid: a monitor test crosshatch at 75%, lines every 20 game pixels and
// a dot in the middle of each square, one framebuffer pixel wide (two at 720p)
u32 ThemeGrid (int x, int y, int M)
{
	const int P = 20 * M, t = (M + 1) / 2;
	const int mx = (x + P * 64) % P, my = (y + P * 64) % P;
	if ((mx - 10 * M + P) % P < t || (my - 10 * M + P) % P < t)
	{
		return 0x103034;
	}
	return mx < t && my < t ? 0x164545 : 0x041015;
}

u32 ThemeGround (void)
{
	return 0x041015;
}
