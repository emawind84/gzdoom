/*
** Video basics and init code.
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


#include <stdio.h>

#include "i_system.h"
#include "x86.h"
#include "i_video.h"
#include "r_state.h"
#include "am_map.h"

#include "doomstat.h"

#include "c_console.h"
#include "hu_stuff.h"

#include "m_argv.h"

#include "v_video.h"
#include "v_text.h"
#include "sc_man.h"

#include "w_wad.h"

#include "c_dispatch.h"
#include "cmdlib.h"
#include "sbar.h"
#include "hardware.h"
#include "m_png.h"
#include "r_utility.h"
#include "r_renderer.h"
#include "menu/menu.h"
#include "vm.h"
#include "r_videoscale.h"
#include "i_time.h"
#include "version.h"

EXTERN_CVAR(Bool, cl_capfps)

CVAR(Int, win_x, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_y, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_w, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, win_h, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, win_maximized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)

CUSTOM_CVAR(Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
	else if (cl_capfps == 0)
	{
		I_SetFPSLimit(vid_maxfps);
	}
}

CUSTOM_CVAR(Int, vid_rendermode, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (usergame)
	{
		// [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
	}
	screen->SetTextureFilterMode();

	// No further checks needed. All this changes now is which scene drawer the render backend calls.
}



EXTERN_CVAR(Bool, r_blendmethod)

int active_con_scale();

FRenderer *SWRenderer;

#define DBGBREAK assert(0)

class DDummyFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	DDummyFrameBuffer (int width, int height)
		: DFrameBuffer (0, 0)
	{
		SetVirtualSize(width, height);
	}
	// These methods should never be called.
	void Update() { DBGBREAK; }
	bool IsFullscreen() { DBGBREAK; return 0; }
	int GetClientWidth() { DBGBREAK; return 0; }
	int GetClientHeight() { DBGBREAK; return 0; }

	float Gamma;
};

int DisplayWidth, DisplayHeight;

FFont *SmallFont, *SmallFont2, *BigFont, *ConFont, *IntermissionFont;

uint32_t Col2RGB8[65][256];
uint32_t *Col2RGB8_LessPrecision[65];
uint32_t Col2RGB8_Inverse[65][256];
ColorTable32k RGB32k;
ColorTable256k RGB256k;


static uint32_t Col2RGB8_2[63][256];

// [RH] The framebuffer is no longer a mere byte array.
// There's also only one, not four.
DFrameBuffer *screen;

CVAR (Int, vid_defwidth, 640, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, vid_defheight, 480, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, ticker, false, 0)

CUSTOM_CVAR (Bool, vid_vsync, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetVSync (*self);
	}
}

// [RH] Set true when vid_setmode command has been executed
bool	setmodeneeded = false;

//==========================================================================
//
// DCanvas Constructor
//
//==========================================================================

DCanvas::DCanvas (int _width, int _height, bool _bgra)
{
	// Init member vars
	Width = _width;
	Height = _height;
	Bgra = _bgra;
}

//==========================================================================
//
// DCanvas Destructor
//
//==========================================================================

DCanvas::~DCanvas ()
{
}

//==========================================================================
//
// V_GetColorFromString
//
// Passed a string of the form "#RGB", "#RRGGBB", "R G B", or "RR GG BB",
// returns a number representing that color. If palette is non-NULL, the
// index of the best match in the palette is returned, otherwise the
// RRGGBB value is returned directly.
//
//==========================================================================

int V_GetColorFromString (const uint32_t *palette, const char *cstr, FScriptPosition *sc)
{
	int c[3], i, p;
	char val[3];

	val[2] = '\0';

	// Check for HTML-style #RRGGBB or #RGB color string
	if (cstr[0] == '#')
	{
		size_t len = strlen (cstr);

		if (len == 7)
		{
			// Extract each eight-bit component into c[].
			for (i = 0; i < 3; ++i)
			{
				val[0] = cstr[1 + i*2];
				val[1] = cstr[2 + i*2];
				c[i] = ParseHex (val, sc);
			}
		}
		else if (len == 4)
		{
			// Extract each four-bit component into c[], expanding to eight bits.
			for (i = 0; i < 3; ++i)
			{
				val[1] = val[0] = cstr[1 + i];
				c[i] = ParseHex (val, sc);
			}
		}
		else
		{
			// Bad HTML-style; pretend it's black.
			c[2] = c[1] = c[0] = 0;
		}
	}
	else
	{
		if (strlen(cstr) == 6)
		{
			char *p;
			int color = strtol(cstr, &p, 16);
			if (*p == 0)
			{
				// RRGGBB string
				c[0] = (color & 0xff0000) >> 16;
				c[1] = (color & 0xff00) >> 8;
				c[2] = (color & 0xff);
			}
			else goto normal;
		}
		else
		{
normal:
			// Treat it as a space-delimited hexadecimal string
			for (i = 0; i < 3; ++i)
			{
				// Skip leading whitespace
				while (*cstr <= ' ' && *cstr != '\0')
				{
					cstr++;
				}
				// Extract a component and convert it to eight-bit
				for (p = 0; *cstr > ' '; ++p, ++cstr)
				{
					if (p < 2)
					{
						val[p] = *cstr;
					}
				}
				if (p == 0)
				{
					c[i] = 0;
				}
				else
				{
					if (p == 1)
					{
						val[1] = val[0];
					}
					c[i] = ParseHex (val, sc);
				}
			}
		}
	}
	if (palette)
		return ColorMatcher.Pick (c[0], c[1], c[2]);
	else
		return MAKERGB(c[0], c[1], c[2]);
}

//==========================================================================
//
// V_GetColorStringByName
//
// Searches for the given color name in x11r6rgb.txt and returns an
// HTML-ish "#RRGGBB" string for it if found or the empty string if not.
//
//==========================================================================

FString V_GetColorStringByName (const char *name, FScriptPosition *sc)
{
	FMemLump rgbNames;
	char *rgbEnd;
	char *rgb, *endp;
	int rgblump;
	int c[3], step;
	size_t namelen;

	if (Wads.GetNumLumps()==0) return FString();

	rgblump = Wads.CheckNumForName ("X11R6RGB");
	if (rgblump == -1)
	{
		if (!sc) Printf ("X11R6RGB lump not found\n");
		else sc->Message(MSG_WARNING, "X11R6RGB lump not found");
		return FString();
	}

	rgbNames = Wads.ReadLump (rgblump);
	rgb = (char *)rgbNames.GetMem();
	rgbEnd = rgb + Wads.LumpLength (rgblump);
	step = 0;
	namelen = strlen (name);

	while (rgb < rgbEnd)
	{
		// Skip white space
		if (*rgb <= ' ')
		{
			do
			{
				rgb++;
			} while (rgb < rgbEnd && *rgb <= ' ');
		}
		else if (step == 0 && *rgb == '!')
		{ // skip comment lines
			do
			{
				rgb++;
			} while (rgb < rgbEnd && *rgb != '\n');
		}
		else if (step < 3)
		{ // collect RGB values
			c[step++] = strtoul (rgb, &endp, 10);
			if (endp == rgb)
			{
				break;
			}
			rgb = endp;
		}
		else
		{ // Check color name
			endp = rgb;
			// Find the end of the line
			while (endp < rgbEnd && *endp != '\n')
				endp++;
			// Back up over any whitespace
			while (endp > rgb && *endp <= ' ')
				endp--;
			if (endp == rgb)
			{
				break;
			}
			size_t checklen = ++endp - rgb;
			if (checklen == namelen && strnicmp (rgb, name, checklen) == 0)
			{
				FString descr;
				descr.Format ("#%02x%02x%02x", c[0], c[1], c[2]);
				return descr;
			}
			rgb = endp;
			step = 0;
		}
	}
	if (rgb < rgbEnd)
	{
		if (!sc) Printf ("X11R6RGB lump is corrupt\n");
		else sc->Message(MSG_WARNING, "X11R6RGB lump is corrupt");
	}
	return FString();
}

//==========================================================================
//
// V_GetColor
//
// Works like V_GetColorFromString(), but also understands X11 color names.
//
//==========================================================================

int V_GetColor (const uint32_t *palette, const char *str, FScriptPosition *sc)
{
	FString string = V_GetColorStringByName (str, sc);
	int res;

	if (!string.IsEmpty())
	{
		res = V_GetColorFromString (palette, string, sc);
	}
	else
	{
		res = V_GetColorFromString (palette, str, sc);
	}
	return res;
}

int V_GetColor(const uint32_t *palette, FScanner &sc)
{
	FScriptPosition scc = sc;
	return V_GetColor(palette, sc.String, &scc);
}

//==========================================================================
//
// BuildTransTable
//
// Build the tables necessary for blending
//
//==========================================================================

static void BuildTransTable (const PalEntry *palette)
{
	int r, g, b;

	// create the RGB555 lookup table
	for (r = 0; r < 32; r++)
		for (g = 0; g < 32; g++)
			for (b = 0; b < 32; b++)
				RGB32k.RGB[r][g][b] = ColorMatcher.Pick ((r<<3)|(r>>2), (g<<3)|(g>>2), (b<<3)|(b>>2));
	// create the RGB666 lookup table
	for (r = 0; r < 64; r++)
		for (g = 0; g < 64; g++)
			for (b = 0; b < 64; b++)
				RGB256k.RGB[r][g][b] = ColorMatcher.Pick ((r<<2)|(r>>4), (g<<2)|(g>>4), (b<<2)|(b>>4));

	int x, y;

	// create the swizzled palette
	for (x = 0; x < 65; x++)
		for (y = 0; y < 256; y++)
			Col2RGB8[x][y] = (((palette[y].r*x)>>4)<<20) |
							  ((palette[y].g*x)>>4) |
							 (((palette[y].b*x)>>4)<<10);

	// create the swizzled palette with the lsb of red and blue forced to 0
	// (for green, a 1 is okay since it never gets added into)
	for (x = 1; x < 64; x++)
	{
		Col2RGB8_LessPrecision[x] = Col2RGB8_2[x-1];
		for (y = 0; y < 256; y++)
		{
			Col2RGB8_2[x-1][y] = Col2RGB8[x][y] & 0x3feffbff;
		}
	}
	Col2RGB8_LessPrecision[0] = Col2RGB8[0];
	Col2RGB8_LessPrecision[64] = Col2RGB8[64];

	// create the inverse swizzled palette
	for (x = 0; x < 65; x++)
		for (y = 0; y < 256; y++)
		{
			Col2RGB8_Inverse[x][y] = (((((255-palette[y].r)*x)>>4)<<20) |
									  (((255-palette[y].g)*x)>>4) |
									  ((((255-palette[y].b)*x)>>4)<<10)) & 0x3feffbff;
		}
}

CCMD(clean)
{
	Printf ("CleanXfac: %d\nCleanYfac: %d\n", CleanXfac, CleanYfac);
}


void V_UpdateModeSize (int width, int height)
{
	int cx1, cx2;
	V_CalcCleanFacs(320, 200, width, height, &CleanXfac, &CleanYfac, &cx1, &cx2);

	CleanWidth = width / CleanXfac;
	CleanHeight = height / CleanYfac;
	assert(CleanWidth >= 320 && CleanHeight >= 200);

	if (width < 800 || width >= 960)
	{
		if (cx1 < cx2)
		{
			// Special case in which we don't need to scale down.
			CleanXfac_1 = 
			CleanYfac_1 = cx1;
		}
		else
		{
			CleanXfac_1 = MAX(CleanXfac - 1, 1);
			CleanYfac_1 = MAX(CleanYfac - 1, 1);
			// On larger screens this is not enough so make sure it's at most 3/4 of the screen's width
			while (CleanXfac_1 * 320 > screen->GetWidth()*3/4 && CleanXfac_1 > 2)
			{
				CleanXfac_1--;
				CleanYfac_1--;
			}
		}
		CleanWidth_1 = width / CleanXfac_1;
		CleanHeight_1 = height / CleanYfac_1;
	}
	else // if the width is between 800 and 960 the ratio between the screensize and CleanXFac-1 becomes too large.
	{
		CleanXfac_1 = CleanXfac;
		CleanYfac_1 = CleanYfac;
		CleanWidth_1 = CleanWidth;
		CleanHeight_1 = CleanHeight;
	}


	DisplayWidth = width;
	DisplayHeight = height;

	R_OldBlend = ~0;
}

void V_OutputResized (int width, int height)
{
	V_UpdateModeSize(width, height);
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
	C_NewModeAdjust();
	// Reload crosshair if transitioned to a different size
	ST_LoadCrosshair(true);
	AM_NewResolution();
}

void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *_cx1, int *_cx2)
{
	float ratio;
	int cwidth;
	int cheight;
	int cx1, cy1, cx2, cy2;

	// For larger screems always use at least a 16:9 ratio for clean factor calculation, even if the actual ratio is narrower.
	if (realwidth > 1280 && (double)realwidth / realheight < 16./9)
	{
		realheight = realwidth * 9 / 16;
	}

	ratio = ActiveRatio(realwidth, realheight);
	if (AspectTallerThanWide(ratio))
	{
		cwidth = realwidth;
		cheight = realheight * AspectMultiplier(ratio) / 48;
	}
	else
	{
		cwidth = realwidth * AspectMultiplier(ratio) / 48;
		cheight = realheight;
	}
	// Use whichever pair of cwidth/cheight or width/height that produces less difference
	// between CleanXfac and CleanYfac.
	cx1 = MAX(cwidth / designwidth, 1);
	cy1 = MAX(cheight / designheight, 1);
	cx2 = MAX(realwidth / designwidth, 1);
	cy2 = MAX(realheight / designheight, 1);
	if (abs(cx1 - cy1) <= abs(cx2 - cy2) || MAX(cx1, cx2) >= 4)
	{ // e.g. 640x360 looks better with this.
		*cleanx = cx1;
		*cleany = cy1;
	}
	else
	{ // e.g. 720x480 looks better with this.
		*cleanx = cx2;
		*cleany = cy2;
	}

	if (*cleanx < *cleany)
		*cleany = *cleanx;
	else
		*cleanx = *cleany;

	if (_cx1 != NULL)	*_cx1 = cx1;
	if (_cx2 != NULL)	*_cx2 = cx2;
}

bool IVideo::SetResolution ()
{
	DFrameBuffer *buff = CreateFrameBuffer();

	if (buff == NULL)	// this cannot really happen
	{
		return false;
	}

	screen = buff;
	screen->SetGamma();

	V_UpdateModeSize(screen->GetWidth(), screen->GetHeight());

	return true;
}

//
// V_Init
//

void V_Init (bool restart) 
{ 
	const char *i;
	int width, height, bits;

	atterm (V_Shutdown);

	// [RH] Initialize palette management
	InitPalette ();

	if (!restart)
	{
		width = height = bits = 0;

		if ( (i = Args->CheckValue ("-width")) )
			width = atoi (i);

		if ( (i = Args->CheckValue ("-height")) )
			height = atoi (i);

		if (width == 0)
		{
			if (height == 0)
			{
				width = vid_defwidth;
				height = vid_defheight;
			}
			else
			{
				width = (height * 8) / 6;
			}
		}
		else if (height == 0)
		{
			height = (width * 6) / 8;
		}
		// Remember the passed arguments for the next time the game starts up windowed.
		vid_defwidth = width;
		vid_defheight = height;

		screen = new DDummyFrameBuffer (width, height);
	}
	// Update screen palette when restarting
	else
	{
		PalEntry *palette = screen->GetPalette ();
		for (int i = 0; i < 256; ++i)
			*palette++ = GPalette.BaseColors[i];
		screen->UpdatePalette();
	}

	BuildTransTable (GPalette.BaseColors);
}

void V_Init2()
{
	float gamma = static_cast<DDummyFrameBuffer *>(screen)->Gamma;

	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}

	UCVarValue val;

	val.Bool = !!Args->CheckParm("-devparm");
	ticker.SetGenericRepDefault(val, CVAR_Bool);


	I_InitGraphics();

	Video->SetResolution();	// this only fails via exceptions.
	Printf ("Resolution: %d x %d\n", SCREENWIDTH, SCREENHEIGHT);

	screen->SetGamma ();
	FBaseCVar::ResetColors ();
	C_NewModeAdjust();
	setsizeneeded = true;
}

void V_Shutdown()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}
	V_ClearFonts();
}

CUSTOM_CVAR (Int, vid_aspect, 0, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->CallScreenSizeChanged();
	}
}

// Helper for ActiveRatio and CheckRatio. Returns the forced ratio type, or -1 if none.
int ActiveFakeRatio(int width, int height)
{
	int fakeratio = -1;
	if ((vid_aspect >= 1) && (vid_aspect <= 6))
	{
		// [SP] User wants to force aspect ratio; let them.
		fakeratio = int(vid_aspect);
		if (fakeratio == 3)
		{
			fakeratio = 0;
		}
		else if (fakeratio == 5)
		{
			fakeratio = 3;
		}
	}
	else if (vid_aspect == 0 && ViewportIsScaled43())
	{
		fakeratio = 0;
	}
	return fakeratio;
}

// Active screen ratio based on cvars and size
float ActiveRatio(int width, int height, float *trueratio)
{
	static float forcedRatioTypes[] =
	{
		4 / 3.0f,
		16 / 9.0f,
		16 / 10.0f,
		17 / 10.0f,
		5 / 4.0f,
		17 / 10.0f,
		21 / 9.0f
	};

	float ratio = width / (float)height;
	int fakeratio = ActiveFakeRatio(width, height);

	if (trueratio)
		*trueratio = ratio;
	return (fakeratio != -1) ? forcedRatioTypes[fakeratio] : ratio;
}

DEFINE_ACTION_FUNCTION(_Screen, GetAspectRatio)
{
	ACTION_RETURN_FLOAT(ActiveRatio(screen->GetWidth(), screen->GetHeight(), nullptr));
}

// Tries to guess the physical dimensions of the screen based on the
// screen's pixel dimensions. Can return:
// 0: 4:3
// 1: 16:9
// 2: 16:10
// 3: 17:10
// 4: 5:4
// 5: 17:10 (redundant, never returned)
// 6: 21:9
int CheckRatio (int width, int height, int *trueratio)
{
	float aspect = width / (float)height;

	static std::pair<float, int> ratioTypes[] =
	{
		{ 21 / 9.0f , 6 },
		{ 16 / 9.0f , 1 },
		{ 17 / 10.0f , 3 },
		{ 16 / 10.0f , 2 },
		{ 4 / 3.0f , 0 },
		{ 5 / 4.0f , 4 },
		{ 0.0f, 0 }
	};

	int ratio = ratioTypes[0].second;
	float distance = fabs(ratioTypes[0].first - aspect);
	for (int i = 1; ratioTypes[i].first != 0.0f; i++)
	{
		float d = fabs(ratioTypes[i].first - aspect);
		if (d < distance)
		{
			ratio = ratioTypes[i].second;
			distance = d;
		}
	}

	int fakeratio = ActiveFakeRatio(width, height);
	if (fakeratio == -1)
		fakeratio = ratio;

	if (trueratio)
		*trueratio = ratio;
	return fakeratio;
}

int AspectBaseWidth(float aspect)
{
	return (int)round(240.0f * aspect * 3.0f);
}

int AspectBaseHeight(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(200.0f * (320.0f / (AspectBaseWidth(aspect) / 3.0f)) * 3.0f);
	else
		return (int)round((200.0f * (4.0f / 3.0f)) / aspect * 3.0f);
}

double AspectPspriteOffset(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return 0.0;
	else
		return ((4.0 / 3.0) / aspect - 1.0) * 97.5;
}

int AspectMultiplier(float aspect)
{
	if (!AspectTallerThanWide(aspect))
		return (int)round(320.0f / (AspectBaseWidth(aspect) / 3.0f) * 48.0f);
	else
		return (int)round(200.0f / (AspectBaseHeight(aspect) / 3.0f) * 48.0f);
}

bool AspectTallerThanWide(float aspect)
{
	return aspect < 1.333f;
}

void ScaleWithAspect (int &w, int &h, int Width, int Height)
{
	int resRatio = CheckRatio (Width, Height);
	int screenRatio;
	CheckRatio (w, h, &screenRatio);
	if (resRatio == screenRatio)
		return;

	double yratio;
	switch(resRatio)
	{
		case 0: yratio = 4./3.; break;
		case 1: yratio = 16./9.; break;
		case 2: yratio = 16./10.; break;
		case 3: yratio = 17./10.; break;
		case 4: yratio = 5./4.; break;
		case 6: yratio = 21./9.; break;
		default: return;
	}
	double y = w/yratio;
	if (y > h)
		w = static_cast<int>(h * yratio);
	else
		h = static_cast<int>(y);
}

CCMD(vid_setsize)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: vid_setsize width height\n");
	}
	else
	{
		screen->SetWindowSize((int)strtol(argv[1], nullptr, 0), (int)strtol(argv[2], nullptr, 0));
		V_OutputResized(screen->GetClientWidth(), screen->GetClientHeight());
	}
}


void IVideo::DumpAdapters ()
{
	Printf("Multi-monitor support unavailable.\n");
}

CUSTOM_CVAR(Bool, fullscreen, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	setmodeneeded = true;
}

CUSTOM_CVAR(Bool, vid_hdr, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CCMD(vid_listadapters)
{
	if (Video != NULL)
		Video->DumpAdapters();
}

bool vid_hdr_active = false;

DEFINE_GLOBAL(SmallFont)
DEFINE_GLOBAL(SmallFont2)
DEFINE_GLOBAL(BigFont)
DEFINE_GLOBAL(ConFont)
DEFINE_GLOBAL(IntermissionFont)
DEFINE_GLOBAL(CleanXfac)
DEFINE_GLOBAL(CleanYfac)
DEFINE_GLOBAL(CleanWidth)
DEFINE_GLOBAL(CleanHeight)
DEFINE_GLOBAL(CleanXfac_1)
DEFINE_GLOBAL(CleanYfac_1)
DEFINE_GLOBAL(CleanWidth_1)
DEFINE_GLOBAL(CleanHeight_1)