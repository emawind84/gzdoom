// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Refresh of things, i.e. objects represented by sprites.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "templates.h"
#include "m_alloc.h"
#include "doomdef.h"
#include "m_swap.h"
#include "m_argv.h"
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "r_local.h"
#include "p_effect.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"

extern fixed_t globaluclip, globaldclip;


#define MINZ			(2048*4)
#define BASEYCENTER 	(100)

EXTERN_CVAR (Bool, st_scale)
CVAR (Bool, r_drawfuzz, true, CVAR_ARCHIVE)


//
// Sprite rotation 0 is facing the viewer,
//	rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//	which increases counter clockwise (protractor).
//
fixed_t 		pspritexscale;
fixed_t			pspriteyscale;
fixed_t 		pspritexiscale;
fixed_t			sky1scale;			// [RH] Sky 1 scale factor
fixed_t			sky2scale;			// [RH] Sky 2 scale factor

static int		spriteshade;

// constant arrays
//	used for psprite clipping and initializing clipping
short			negonearray[MAXWIDTH];
short			screenheightarray[MAXWIDTH];

#define MAX_SPRITE_FRAMES	29		// [RH] Macro-ized as in BOOM.


CVAR (Bool, r_drawplayersprites, true, 0)	// [RH] Draw player sprites?

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//	and range check thing_t sprites patches
TArray<spritedef_t> sprites;

spriteframe_t	sprtemp[MAX_SPRITE_FRAMES];
int 			maxframe;
char*			spritename;

// [RH] skin globals
FPlayerSkin		*skins;
size_t			numskins;

// [RH] particle globals
int				NumParticles;
int				ActiveParticles;
int				InactiveParticles;
particle_t		*Particles;

CVAR (Bool, r_particles, true, 0);


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
// [RH] Removed checks for coexistance of rotation 0 with other
//		rotations and made it look more like BOOM's version.
//
static void R_InstallSpriteLump (int lump, unsigned frame, char rot, BOOL flipped)
{
	unsigned rotation;

	if (rot >= '0' && rot <= '9')
	{
		rotation = rot - '0';
	}
	else if (rot >= 'A')
	{
		rotation = rot - 'A' + 10;
	}
	else
	{
		rotation = 17;
	}

	if (frame >= MAX_SPRITE_FRAMES || rotation > 16)
		I_FatalError ("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

	if ((int)frame > maxframe)
		maxframe = frame;
				
	if (rotation == 0)
	{
		// the lump should be used for all rotations
        // false=0, true=1, but array initialised to -1
        // allows doom to have a "no value set yet" boolean value!
		int r;

		for (r = 14; r >= 0; r -= 2)
		{
			if (sprtemp[frame].lump[r] == -1)
			{
				sprtemp[frame].lump[r] = (short)(lump);
				if (flipped)
				{
					sprtemp[frame].flip |= 1 << r;
				}
				sprtemp[frame].rotate = false;
			}
		}
	}
	else
	{
		if (rotation <= 8)
		{
			rotation = (rotation - 1) * 2;
		}
		else
		{
			rotation = (rotation - 9) * 2 + 1;
		}

		if (sprtemp[frame].lump[rotation] == -1)
		{
			// the lump is only used for one rotation
			sprtemp[frame].lump[rotation] = (short)(lump);
			if (flipped)
			{
				sprtemp[frame].flip |= 1 << rotation;
			}
			sprtemp[frame].rotate = true;
		}
	}
}


// [RH] Seperated out of R_InitSpriteDefs()
static void R_InstallSprite (int num)
{
	int frame;

	if (maxframe == -1)
	{
		sprites[num].numframes = 0;
		return;
	}

	maxframe++;
	
	for (frame = 0; frame < maxframe; frame++)
	{
		switch ((int)sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			I_FatalError ("R_InstallSprite: No patches found for %s frame %c", sprites[num].name, frame+'A');
			break;
			
		case 0:
			// only the first rotation is needed
			break;
					
		case 1:
			// must have all 8 frame pairs
			{
				int rot;

				for (rot = 0; rot < 8; ++rot)
				{
					if (sprtemp[frame].lump[rot*2+1] == -1)
					{
						sprtemp[frame].lump[rot*2+1] = sprtemp[frame].lump[rot*2];
						if (sprtemp[frame].flip & (1 << (rot*2)))
						{
							sprtemp[frame].flip |= 1 << (rot*2+1);
						}
					}
					if (sprtemp[frame].lump[rot*2] == -1)
					{
						sprtemp[frame].lump[rot*2] = sprtemp[frame].lump[rot*2+1];
						if (sprtemp[frame].flip & (1 << (rot*2+1)))
						{
							sprtemp[frame].flip |= 1 << (rot*2);
						}
					}

				}
				for (rot = 0; rot < 16; ++rot)
				{
					if (sprtemp[frame].lump[rot] == -1)
						I_FatalError ("R_InstallSprite: Sprite %c%c%c%c frame %c is missing rotations",
									  sprites[num].name[0],
									  sprites[num].name[1],
									  sprites[num].name[2],
									  sprites[num].name[3], frame+'A');
				}
			}
			break;
		}
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[num].numframes = maxframe;
	sprites[num].spriteframes = (spriteframe_t *)
		Z_Malloc (maxframe * sizeof(spriteframe_t), PU_STATIC, NULL);
	memcpy (sprites[num].spriteframes, sprtemp, maxframe * sizeof(spriteframe_t));
}


//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//	(4 chars exactly) to be used.
// Builds the sprite rotation matrices to account
//	for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//	a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//	letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs () 
{ 
	size_t i;
	int l;
	int intname;
	int start;
	int end;
				
	start = firstspritelump - 1;
	end = lastspritelump + 1;
		
	// scan all the lump names for each of the names,
	//	noting the highest frame letter.
	// Just compare 4 characters as ints
	for (i = 0; i < sprites.Size (); i++)
	{
		spritename = sprites[i].name;
		memset (sprtemp, -1, sizeof(sprtemp));
		for (int j = 0; j < MAX_SPRITE_FRAMES; ++j)
		{
			sprtemp[j].flip = 0;
		}
				
		maxframe = -1;
		intname = *(int *)spritename;
		
		// scan the lumps,
		//	filling in the frames for whatever is found
		for (l = lastspritelump; l >= firstspritelump; l--)
		{
			if (*(int *)lumpinfo[l].name == intname)
			{
				R_InstallSpriteLump (l, 
									 lumpinfo[l].name[4] - 'A',
									 lumpinfo[l].name[5],
									 false);

				if (lumpinfo[l].name[6])
					R_InstallSpriteLump (l,
									 lumpinfo[l].name[6] - 'A',
									 lumpinfo[l].name[7],
									 true);
			}
		}
		
		R_InstallSprite (i);
	}
}

// [RH]
// R_InitSkins
// Reads in everything applicable to a skin. The skins should have already
// been counted and had their identifiers assigned to namespaces.
//
#define NUMSKINSOUNDS 9
static const char *skinsoundnames[NUMSKINSOUNDS][2] = {
	{ "dsplpain",	"*pain100" },
	{ "dspldeth",	"*death" },
	{ "dspdiehi",	"*xdeath" },
	{ "dsoof",		"*land" },
	{ "dsoof",		"*grunt" },
	{ "dsnoway",	"*usefail" },
	{ "dsslop",		"*gibbed" },
	{ "dspunch",	"*fist" },
	{ "dsjump",		"*jump" }
};
static const char painname[3][8] = { "*pain25", "*pain50", "*pain75" };
static WORD playersoundrefs[NUMSKINSOUNDS+3];

static int STACK_ARGS skinsorter (const void *a, const void *b)
{
	return stricmp (((FPlayerSkin *)a)->name, ((FPlayerSkin *)b)->name);
}

void R_InitSkins (void)
{
	spritedef_t temp;
	int sndlumps[NUMSKINSOUNDS];
	char key[10];
	int intname;
	size_t i;
	int j, k, base;
	int lastlump;

	key[9] = 0;
	i = 0;
	lastlump = 0;

	for (j = 0; j < NUMSKINSOUNDS; ++j)
	{
		playersoundrefs[j] = S_FindSound (skinsoundnames[j][1]);
	}
	for (j = 0; j < 3; ++j)
	{
		playersoundrefs[j+NUMSKINSOUNDS] = S_FindSound (painname[j]);
	}

	while ((base = W_FindLump ("S_SKIN", &lastlump)) != -1)
	{
		// The player sprite has 23 frames. This means that the S_SKIN
		// marker needs a minimum of 23 lumps after it.
		if (base >= numlumps - 23 || base == -1)
			continue;

		i++;
		for (j = 0; j < NUMSKINSOUNDS; j++)
			sndlumps[j] = -1;
		skins[i].namespc = lumpinfo[base].namespc;

		SC_OpenLumpNum (base, "S_SKIN");
		intname = 0;

		// Data is stored as "key = data".
		while (SC_GetString ())
		{
			strncpy (key, sc_String, 9);
			if (!SC_GetString() || sc_String[0] != '=')
			{
				Printf_Bold ("Bad format for skin %d: %s\n", i, key);
				break;
			}
			SC_GetString ();
			if (0 == stricmp (key, "name"))
			{
				strncpy (skins[i].name, sc_String, 16);
				for (j = 0; (size_t)j < i; j++)
				{
					if (stricmp (skins[i].name, skins[j].name) == 0)
					{
						sprintf (skins[i].name, "skin%d", i);
						Printf_Bold ("Skin %s duplicated as %s\n",
							skins[j].name, skins[i].name);
						break;
					}
				}
			}
			else if (0 == stricmp (key, "sprite"))
			{
				for (j = 3; j >= 0; j--)
					sc_String[j] = toupper (sc_String[j]);
				intname = *((int *)sc_String);
			}
			else if (0 == stricmp (key, "face"))
			{
				for (j = 2; j >= 0; j--)
					skins[i].face[j] = toupper (sc_String[j]);
			}
			else if (0 == stricmp (key, "gender"))
			{
				skins[i].gender = D_GenderToInt (sc_String);
			}
			else
			{
				for (j = 0; j < NUMSKINSOUNDS; j++)
				{
					if (stricmp (key, skinsoundnames[j][0]) == 0)
					{
						sndlumps[j] = W_CheckNumForName (sc_String, skins[i].namespc);
						if (sndlumps[j] == -1)
						{ // Replacement not found, try finding it in the global namespace
							sndlumps[j] = W_CheckNumForName (sc_String);
						}
					}
				}
				//if (j == 8)
				//	Printf ("Funny info for skin %i: %s = %s\n", i, key, sc_String);
			}
		}

		if (skins[i].name[0] == 0)
			sprintf (skins[i].name, "skin%d", i);

		// Now collect the sprite frames for this skin. If the sprite name was not
		// specified, use whatever immediately follows the specifier lump.
		if (intname == 0)
		{
			intname = *(int *)(lumpinfo[base+1].name);
		}

		memset (sprtemp, -1, sizeof(sprtemp));
		for (k = 0; k < MAX_SPRITE_FRAMES; ++k)
		{
			sprtemp[k].flip = 0;
		}
		maxframe = -1;

		for (k = base + 1; lumpinfo[k].wadnum == lumpinfo[base].wadnum; k++)
		{
			if (*(int *)lumpinfo[k].name == intname)
			{
				R_InstallSpriteLump (k, 
									 lumpinfo[k].name[4] - 'A',
									 lumpinfo[k].name[5],
									 false);

				if (lumpinfo[k].name[6])
					R_InstallSpriteLump (k,
									 lumpinfo[k].name[6] - 'A',
									 lumpinfo[k].name[7],
									 true);
			}
		}

		if (maxframe <= 0)
		{
			Printf_Bold ("Skin %s (#%d) has no frames. Removing.\n", skins[i].name, i);
			if (i < numskins-1)
				memmove (&skins[i], &skins[i+1], sizeof(skins[0])*(numskins-i-1));
			i--;
			continue;
		}

		strncpy (temp.name, lumpinfo[base+1].name, 4);
		skins[i].sprite = sprites.Push (temp);
		R_InstallSprite (skins[i].sprite);

		// Register any sounds this skin provides
		for (j = 0; j < NUMSKINSOUNDS; j++)
		{
			if (sndlumps[j] != -1)
			{
				int aliasid;

				aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
					playersoundrefs[j], sndlumps[j]);
				if (j == 0)
				{
					for (int l = 3; l > 0; --l)
					{
						S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
							playersoundrefs[NUMSKINSOUNDS-1+l], aliasid);
					}
				}
			}
		}

		SC_Close ();

		// Make sure face prefix is a full 3 chars
		if (skins[i].face[1] == 0 || skins[i].face[2] == 0)
		{
			skins[i].face[0] = 0;
		}
	}

	if (numskins > 1)
	{ // The sound table may have changed, so rehash it.
		S_HashSounds ();
		S_ShrinkPlayerSoundLists ();
	}
}

// [RH] Find a skin by name
int R_FindSkin (const char *name)
{
	int min, max, mid;
	int lexx;

	if (stricmp ("base", name) == 0)
	{
		return 0;
	}

	min = 1;
	max = numskins-1;

	while (min <= max)
	{
		mid = (min + max)/2;
		lexx = strnicmp (skins[mid].name, name, 16);
		if (lexx == 0)
			return mid;
		else if (lexx < 0)
			min = mid + 1;
		else
			max = mid - 1;
	}
	return 0;
}

// [RH] List the names of all installed skins
CCMD (skins)
{
	int i;

	for (i = 0; i < (int)numskins; i++)
		Printf ("% 3d %s\n", i, skins[i].name);
}

//
// GAME FUNCTIONS
//
int				MaxVisSprites;
vissprite_t 	*vissprites;
vissprite_t		*firstvissprite;
vissprite_t		*vissprite_p;
vissprite_t		*lastvissprite;
int 			newvissprite;



//
// R_InitSprites
// Called at program start.
//
void R_InitSprites ()
{
	byte rangestart, rangeend;
	int lump, lastlump;
	size_t i;

	for (i = 0; i < MAXWIDTH; i++)
	{
		negonearray[i] = 0;
	}

	MaxVisSprites = 128;	// [RH] This is the initial default value. It grows as needed.
	firstvissprite = vissprites = (vissprite_t *)Malloc (MaxVisSprites * sizeof(vissprite_t));
	lastvissprite = &vissprites[MaxVisSprites];

	if (gameinfo.gametype == GAME_Doom)
	{
		rangestart = 112;
		rangeend = 127;
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		rangestart = 225;
		rangeend = 240;
	}
	else // Hexen
	{
		rangestart = 146;
		rangeend = 163;
	}

	// [RH] Count the number of skins.
	numskins = 1;
	lastlump = 0;
	while ((lump = W_FindLump ("S_SKIN", &lastlump)) != -1)
	{
		numskins++;
	}

	// [RH] Do some preliminary setup
	skins = (FPlayerSkin *)Z_Malloc (sizeof(*skins) * numskins, PU_STATIC, 0);
	memset (skins, 0, sizeof(*skins) * numskins);
	for (i = 0; i < numskins; i++)
	{
		skins[i].range0start = rangestart;
		skins[i].range0end = rangeend;
	}

	R_InitSpriteDefs ();
	R_InitSkins ();		// [RH] Finish loading skin data

	// [RH] Set up base skin
	strcpy (skins[0].name, "Base");
	skins[0].face[0] = 'S';
	skins[0].face[1] = 'T';
	skins[0].face[2] = 'F';
	skins[0].namespc = ns_global;

	for (i = 0; i < sprites.Size (); i++)
	{
		if (memcmp (sprites[i].name, deh.PlayerSprite, 4) == 0)
		{
			skins[0].sprite = i;
			break;
		}
	}

	// [RH] Sort the skins, but leave base as skin 0
	qsort (&skins[1], numskins-1, sizeof(FPlayerSkin), skinsorter);
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
	vissprite_p = firstvissprite;
}


//
// R_NewVisSprite
//
vissprite_t *R_NewVisSprite (void)
{
	if (vissprite_p == lastvissprite)
	{
		ptrdiff_t firstvisspritenum = firstvissprite - vissprites;
		ptrdiff_t prevvisspritenum = vissprite_p - vissprites;

		MaxVisSprites *= 2;
		vissprites = (vissprite_t *)Realloc (vissprites, MaxVisSprites * sizeof(vissprite_t));
		lastvissprite = &vissprites[MaxVisSprites];
		firstvissprite = &vissprites[firstvisspritenum];
		vissprite_p = &vissprites[prevvisspritenum];
		DPrintf ("MaxVisSprites increased to %d\n", MaxVisSprites);
	}
	
	vissprite_p++;
	return vissprite_p-1;
}


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//	in posts/runs of opaque pixels.
//
short*			mfloorclip;
short*			mceilingclip;

fixed_t 		spryscale;
fixed_t 		sprtopscreen;

bool			sprflipvert;

void R_DrawMaskedColumn (column_t *column)
{
	int top = -1;

	while (column->topdelta != 0xff)
	{
		if (column->topdelta <= top)
		{
			top += column->topdelta;
		}
		else
		{
			top = column->topdelta;
		}

		if (column->length == 0)
		{
			goto nextpost;
		}

		// calculate unclipped screen coordinates for post
		dc_yl = (sprtopscreen + spryscale * top) >> FRACBITS;
		dc_yh = (sprtopscreen + spryscale * (top + column->length) - FRACUNIT) >> FRACBITS;

		if (sprflipvert)
		{
			swap (dc_yl, dc_yh);
		}

		if (dc_yh >= mfloorclip[dc_x])
		{
			dc_yh = mfloorclip[dc_x] - 1;
		}
		if (dc_yl < mceilingclip[dc_x])
		{
			dc_yl = mceilingclip[dc_x];
		}

		if (dc_yl <= dc_yh)
		{
			if (sprflipvert)
			{
				dc_texturefrac = (dc_yl*dc_iscale) - (top << FRACBITS)
					- FixedMul (centeryfrac, dc_iscale) - dc_texturemid;
				const fixed_t maxfrac = column->length << FRACBITS;
				while (dc_texturefrac >= maxfrac)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				while (endfrac < 0)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			else
			{
				dc_texturefrac = dc_texturemid - (top << FRACBITS)
					+ (dc_yl*dc_iscale) - FixedMul (centeryfrac-FRACUNIT, dc_iscale);
				while (dc_texturefrac < 0)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				const fixed_t maxfrac = column->length << FRACBITS;
				while (endfrac >= maxfrac)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			dc_source = (byte *)column + 3;
			colfunc ();
		}
nextpost:
		column = (column_t *)((byte *)column + column->length + 4);
	}
}

//
// R_DrawVisSprite
//	mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite (vissprite_t *vis, int x1, int x2)
{
#ifdef RANGECHECK
	unsigned int	patchwidth;
#endif
	fixed_t 		frac;
	patch_t*		patch;

	if (vis->picnum == -1)
	{ // [RH] It's a particle
		R_DrawParticle (vis, x1, x2);
		return;
	}

	patch = TileCache[R_CacheTileNum (vis->picnum, PU_CACHE)];

	dc_colormap = vis->colormap;
	spryscale = vis->yscale;
	sprflipvert = false;
	dc_iscale = 0xffffffffu / (unsigned)vis->yscale;
	dc_texturemid = vis->texturemid;
	frac = vis->startfrac;

	sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);

#ifdef RANGECHECK
	patchwidth = (unsigned)SHORT(patch->width);
#endif

	switch (R_SetPatchStyle (vis->RenderStyle, vis->alpha,
		vis->Translation, vis->AlphaColor))
	{
	case DontDraw:
		break;

	case DoDraw0:
		// One column at a time
		{
		int x1 = vis->x1, x2 = vis->x2;
		fixed_t xiscale = vis->xiscale;

		for (dc_x = x1; dc_x <= x2; dc_x++, frac += xiscale)
		{
			unsigned int texturecolumn = frac>>FRACBITS;

#ifdef RANGECHECK
			if (texturecolumn >= patchwidth)
			{
				DPrintf ("R_DrawSpriteRange: bad texturecolumn (%d)\n", texturecolumn);
				continue;
			}
#endif

			R_DrawMaskedColumn ((column_t *)((byte *)patch + LONG(patch->columnofs[texturecolumn])));
		}
		}
		break;

	case DoDraw1:
		// Up to four columns at a time
		{
		int x1 = vis->x1, x2 = vis->x2 + 1;
		fixed_t xiscale = vis->xiscale;
		int stop = x2 & ~3;

		if (x1 < x2)
		{
			dc_x = x1;

			while ((dc_x < stop) && (dc_x & 3))
			{
				R_DrawMaskedColumn ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < stop)
			{
				rt_initcols();
				R_DrawMaskedColumnHoriz ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
				R_DrawMaskedColumnHoriz ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
				R_DrawMaskedColumnHoriz ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
				R_DrawMaskedColumnHoriz ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				rt_draw4cols (dc_x - 3);
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < x2)
			{
				R_DrawMaskedColumn ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
			}
		}
		}
		break;
	}

	R_FinishSetPatchStyle ();
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//
void R_ProjectSprite (AActor *thing, int fakeside)
{
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	
	fixed_t				gzt;				// killough 3/27/98
	fixed_t				gzb;				// [RH] use bottom of sprite, not actor
	fixed_t 			tx, tx2;
	fixed_t 			tz;

	fixed_t 			xscale;
	
	int 				x1;
	int 				x2;

	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int 				lump;
	
	WORD 				flip;
	
	vissprite_t*		vis;
	
	fixed_t 			iscale;

	sector_t*			heightsec;			// killough 3/27/98

	if ((thing->renderflags & RF_INVISIBLE) ||
		thing->RenderStyle == STYLE_None ||
		(thing->RenderStyle >= STYLE_Translucent && thing->alpha == 0))
	{
		return;
	}

	// transform the origin point
	tr_x = thing->x - viewx;
	tr_y = thing->y - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	// thing is behind view plane?
	if (tz < MINZ)
		return;

	tx = DMulScale16 (tr_x, viewsin, -tr_y, viewcos);

	// [RH] Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = viewwidth - tx - 1;
	}
	tx2 = tx >> 4;

	// too far off the side?
	if ((abs (tx) >> 6) > tz)
	{
		return;
	}

	xscale = Scale (centerxfrac, (thing->xscale+1)<<6, tz);

	// decide which patch to use for sprite relative to player
#ifdef RANGECHECK
	if ((unsigned)thing->sprite >= (unsigned)sprites.Size ())
	{
		DPrintf ("R_ProjectSprite: invalid sprite number %i\n", thing->sprite);
		return;
	}
#endif
	sprdef = &sprites[thing->sprite];
	if (thing->frame >= sprdef->numframes)
	{
		DPrintf ("R_ProjectSprite: invalid sprite frame %c%c%c%c: %c (max %c)\n",
			sprdef->name[0], sprdef->name[1], sprdef->name[2], sprdef->name[3],
			thing->frame + 'A', sprdef->numframes + 'A' - 1);
		return;
	}
	sprframe = &sprdef->spriteframes[thing->frame];

	if (sprframe->rotate)
	{
		// choose a different rotation based on player view
		angle_t ang = R_PointToAngle (thing->x, thing->y);
		unsigned rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>28;
		lump = sprframe->lump[rot];
		flip = sprframe->flip & (1 << rot);
	}
	else
	{
		// use single rotation for all views
		lump = sprframe->lump[0];
		flip = sprframe->flip & 1;
	}

	if (TileSizes[lump].Width == 0xffff)
	{
		R_CacheTileNum (lump, PU_CACHE);	// [RH] get sprite's size
	}
	
	// [RH] Added scaling
	gzt = thing->z + (TileSizes[lump].TopOffset << (FRACBITS-6)) * (thing->yscale+1);
	gzb = thing->z + ((TileSizes[lump].TopOffset - TileSizes[lump].Height) << (FRACBITS-6)) * (thing->yscale+1);

	// [RH] Reject sprites that are off the top or bottom of the screen
	if (MulScale12 (globaluclip, tz) > viewz - gzb ||
		MulScale12 (globaldclip, tz) < viewz - gzt)
	{
		return;
	}

	// calculate edges of the shape
	tx -= TileSizes[lump].LeftOffset << 16;
	x1 = (centerxfrac + MulScale16 (tx, xscale)) >> FRACBITS;

	// off the right side?
	if (x1 > WindowRight)
		return;
	
	tx += TileSizes[lump].Width << 16;
	x2 = ((centerxfrac + MulScale16 (tx, xscale)) >> FRACBITS) - 1;

	// off the left side or too small?
	if (x2 < WindowLeft || x2 < x1)
		return;

	// killough 3/27/98: exclude things totally separated
	// from the viewer, by either water or fake ceilings
	// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

	heightsec = thing->Sector->heightsec;

	if (heightsec != NULL && heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)
	{
		heightsec = NULL;
	}

	if (heightsec)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			if (gzt < heightsec->ceilingplane.ZatPoint (thing->x, thing->y))
				return;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			if (gzb >= heightsec->floorplane.ZatPoint (thing->x, thing->y))
				return;
		}
		else
		{
			if (gzt < heightsec->floorplane.ZatPoint (thing->x, thing->y))
				return;
			if (gzb >= heightsec->ceilingplane.ZatPoint (thing->x, thing->y))
				return;
		}
	}

	// [RH] Flip for mirrors and renderflags
	if ((MirrorFlags ^ thing->renderflags) & RF_XFLIP)
	{
		flip = !flip;
	}

	// store information in a vissprite
	vis = R_NewVisSprite ();

	// killough 3/27/98: save sector for special clipping later
	vis->heightsec = heightsec;

	vis->renderflags = thing->renderflags;
	vis->RenderStyle = thing->RenderStyle;
	vis->AlphaColor = thing->alphacolor;
	vis->xscale = xscale;
	vis->yscale = Scale (InvZtoScale, (thing->yscale+1)<<6, tz);
	vis->depth = tz;
	vis->cx = tx2;
	vis->gx = thing->x;
	vis->gy = thing->y;
	vis->gz = gzb;		// [RH] use gzb, not thing->z
	vis->gzt = gzt;		// killough 3/27/98
	vis->floorclip = thing->floorclip;
	vis->texturemid = (TileSizes[lump].TopOffset << FRACBITS)
		- SafeDivScale6 (viewz-thing->z, thing->yscale+1) - vis->floorclip;
	vis->x1 = x1 < WindowLeft ? WindowLeft : x1;
	vis->x2 = x2 > WindowRight ? WindowRight : x2;
	vis->Translation = thing->Translation;		// [RH] thing translation table
	vis->FakeFlatStat = fakeside;
	vis->alpha = thing->alpha;
	vis->picnum = lump;
	iscale = DivScale32 (1, xscale);

	if (flip)
	{
		vis->startfrac = (TileSizes[lump].Width << FRACBITS) - 1;
		vis->xiscale = -iscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xiscale = iscale;
	}

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	
	// get light level
	if (fixedlightlev)
	{
		vis->colormap = basecolormap + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		// fixed map
		vis->colormap = fixedcolormap;
	}
	else if (!foggy && (thing->renderflags & RF_FULLBRIGHT))
	{
		// full bright
		vis->colormap = basecolormap;	// [RH] Use basecolormap
	}
	else
	{
		// diminished light
		vis->colormap = basecolormap + (GETPALOOKUP (
			(fixed_t)DivScale12 (r_SpriteVisibility, tz), spriteshade) << COLORMAPSHIFT);
	}
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
// [RH] Save which side of heightsec sprite is on here.
void R_AddSprites (sector_t *sec, int lightlevel, int fakeside)
{
	AActor *thing;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//	subsectors during BSP building.
	// Thus we check whether it was already added.
	if (sec->thinglist == NULL || sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	spriteshade = LIGHT2SHADE(lightlevel + r_actualextralight);

	// Handle all things in sector.
	for (thing = sec->thinglist; thing; thing = thing->snext)
	{
		R_ProjectSprite (thing, fakeside);
	}
}


//
// R_DrawPSprite
//
void R_DrawPSprite (pspdef_t* psp, AActor *owner)
{
	fixed_t 			tx;
	int 				x1;
	int 				x2;
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int 				lump;
	BOOL 				flip;
	vissprite_t*		vis;
	vissprite_t 		avis;
	
	// decide which patch to use
#ifdef RANGECHECK
	if ( (unsigned)psp->state->sprite.index >= (unsigned)sprites.Size ())
	{
		DPrintf ("R_DrawPSprite: invalid sprite number %i\n", psp->state->sprite.index);
		return;
	}
#endif
	sprdef = &sprites[psp->state->sprite.index];
#ifdef RANGECHECK
	if (psp->state->GetFrame() >= sprdef->numframes)
	{
		DPrintf ("R_DrawPSprite: invalid sprite frame %i : %i\n", psp->state->sprite.index, psp->state->GetFrame());
		return;
	}
#endif
	sprframe = &sprdef->spriteframes[psp->state->GetFrame()];

	lump = sprframe->lump[0];
	flip = (BOOL)sprframe->flip & 1;

	if (TileSizes[lump].Width == 0xffff)
	{
		R_CacheTileNum (lump, PU_CACHE);
	}

	// calculate edges of the shape
	tx = psp->sx-((320/2)<<FRACBITS);
		
	tx -= TileSizes[lump].LeftOffset << FRACBITS;
	x1 = (centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS;

	// off the right side
	if (x1 > viewwidth)
		return; 

	tx += TileSizes[lump].Width << FRACBITS;
	x2 = ((centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return;
	
	// store information in a vissprite
	vis = &avis;
	vis->renderflags = owner->renderflags;
	vis->floorclip = 0;
	vis->texturemid = (BASEYCENTER<<FRACBITS) -
		(psp->sy - (TileSizes[lump].TopOffset << FRACBITS));
	if (camera->player && (RenderTarget != screen ||
		realviewheight == RenderTarget->GetHeight() ||
		(RenderTarget->GetWidth() > 320 && !st_scale)))
	{	// Adjust PSprite for fullscreen views
		FWeaponInfo *weapon;
		if (camera->player->powers[pw_weaponlevel2])
			weapon = wpnlev2info[camera->player->readyweapon];
		else
			weapon = wpnlev1info[camera->player->readyweapon];
		if (weapon && weapon->yadjust)
		{
			if (RenderTarget != screen || realviewheight == RenderTarget->GetHeight())
			{
				vis->texturemid -= weapon->yadjust;
			}
			else
			{
				vis->texturemid -= FixedMul (StatusBar->GetDisplacement (),
					weapon->yadjust);
			}
		}
	}
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->xscale = pspritexscale;
	vis->yscale = pspriteyscale;
	vis->Translation = 0;		// [RH] Use default colors
	vis->alpha = owner->alpha;
	vis->RenderStyle = owner->RenderStyle;
	vis->picnum = lump;

	if (flip)
	{
		vis->xiscale = -pspritexiscale;
		vis->startfrac = (TileSizes[lump].Width << FRACBITS) - 1;
	}
	else
	{
		vis->xiscale = pspritexiscale;
		vis->startfrac = 0;
	}

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);

	if (fixedlightlev)
	{
		vis->colormap = basecolormap + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		// fixed color
		vis->colormap = fixedcolormap;
	}
	else if (psp->state->GetFullbright())
	{
		// full bright
		vis->colormap = basecolormap;	// [RH] use basecolormap
	}
	else
	{
		// local light
		vis->colormap = basecolormap + (GETPALOOKUP (0, spriteshade) << COLORMAPSHIFT);
	}
	if (camera->player &&
		camera->player->powers[pw_invisibility] < 4*32 &&
		!(camera->player->powers[pw_invisibility] & 8))
	{
		// shadow draw
		vis->RenderStyle = STYLE_Normal;
	}
		
	R_DrawVisSprite (vis, vis->x1, vis->x2);
}



//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites (void)
{
	int 		i;
	int 		lightnum;
	pspdef_t*	psp;
	sector_t*	sec;
	static sector_t tempsec;
	int			floorlight, ceilinglight;
	
	if (!r_drawplayersprites ||
		!camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM))
		return;

	sec = R_FakeFlat (camera->Sector, &tempsec, &floorlight,
		&ceilinglight, false);

	// [RH] set foggy flag
	foggy = (level.fadeto || sec->floorcolormap->Fade || sec->ceilingcolormap->Fade);
	r_actualextralight = foggy ? 0 : extralight << 4;

	// [RH] set basecolormap
	basecolormap = sec->floorcolormap->Maps;

	// get light level
	lightnum = ((floorlight + ceilinglight) >> 1) + r_actualextralight;
	spriteshade = LIGHT2SHADE(lightnum) - 24*FRACUNIT;

	// clip to screen bounds
	mfloorclip = screenheightarray;
	mceilingclip = negonearray;

	if (camera->player != NULL)
	{
		fixed_t centerhack = centeryfrac;

		centery = viewheight >> 1;
		centeryfrac = centery << FRACBITS;

		// add all active psprites
		for (i = 0, psp = camera->player->psprites;
			 i < NUMPSPRITES;
			 i++, psp++)
		{
			if (psp->state)
				R_DrawPSprite (psp, camera);
		}

		centeryfrac = centerhack;
		centery = centerhack >> FRACBITS;
	}
}




//
// R_SortVisSprites
//
// [RH] The old code for this function used a bubble sort, which was far less
//		than optimal with large numbers of sprites. I changed it to use the
//		stdlib qsort() function instead, and now it is a *lot* faster; the
//		more vissprites that need to be sorted, the better the performance
//		gain compared to the old function.
//
static vissprite_t **spritesorter;
static int spritesortersize = 0;
static int vsprcount;

static int STACK_ARGS sv_compare (const void *arg1, const void *arg2)
{
	int diff = (*(vissprite_t **)arg1)->depth - (*(vissprite_t **)arg2)->depth;
	if (diff == 0)
		return (*(vissprite_t **)arg2)->gzt - (*(vissprite_t **)arg1)->gzt;
	return diff;
}

void R_SortVisSprites (void)
{
	int i;
	vissprite_t *spr;

	vsprcount = vissprite_p - firstvissprite;

	if (!vsprcount)
		return;

	if (spritesortersize < MaxVisSprites)
	{
		if (spritesorter != NULL)
			delete[] spritesorter;
		spritesorter = new vissprite_t *[MaxVisSprites];
		spritesortersize = MaxVisSprites;
	}

	for (i = 0, spr = firstvissprite; i < vsprcount; i++, spr++)
	{
		spritesorter[i] = spr;
	}

	qsort (spritesorter, vsprcount, sizeof (vissprite_t *), sv_compare);
}


//
// R_DrawSprite
//
void R_DrawSprite (vissprite_t *spr)
{
	static short clipbot[MAXWIDTH];
	static short cliptop[MAXWIDTH];
	drawseg_t *ds;
	int i;
	int r1, r2;
	short topclip, botclip;
	short *clip1, *clip2;

	// [RH] Quickly reject sprites with bad x ranges.
	if (spr->x1 > spr->x2)
		return;

	// [RH] Initialize the clipping arrays to their largest possible range
	// instead of using a special "not clipped" value. This eliminates
	// visual anomalies when looking down and should be faster, too.
	topclip = 0;
	botclip = viewheight;

	// killough 3/27/98:
	// Clip the sprite against deep water and/or fake ceilings.
	// [RH] rewrote this to be based on which part of the sector is really visible

	if (spr->heightsec &&
		!(spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{ // only things in specially marked sectors
		if (spr->FakeFlatStat != FAKED_AboveCeiling)
		{
			fixed_t h = spr->heightsec->floorplane.ZatPoint (spr->gx, spr->gy);
			h = (centeryfrac - FixedMul (h-viewz, spr->yscale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_BelowFloor)
			{ // seen below floor: clip top
				if (h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
			}
			else
			{ // seen in the middle: clip bottom
				if (h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
			}
		}
		if (spr->FakeFlatStat != FAKED_BelowFloor)
		{
			fixed_t h = spr->heightsec->ceilingplane.ZatPoint (spr->gx, spr->gy);
			h = (centeryfrac - FixedMul (h-viewz, spr->yscale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_AboveCeiling)
			{ // seen above ceiling: clip bottom
				if (h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
			}
			else
			{ // seen in the middle: clip top
				if (h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
			}
		}
	}
	// killough 3/27/98: end special clipping for deep water / fake ceilings
	else if (spr->floorclip)
	{ // [RH] Move floorclip stuff from R_DrawVisSprite to here
		if (TileSizes[spr->picnum].Width == 0xffff)
		{
			R_CacheTileNum (spr->picnum, PU_CACHE);
		}
		int clip = ((centeryfrac - FixedMul (spr->texturemid -
			(TileSizes[spr->picnum].Height<<FRACBITS) +
			spr->floorclip, spr->yscale)) >> FRACBITS);
		if (clip < botclip)
		{
			botclip = MAX<short> (0, clip);
		}
	}

	i = spr->x2 - spr->x1 + 1;
	clip1 = clipbot + spr->x1;
	clip2 = cliptop + spr->x1;
	do
	{
		*clip1++ = botclip;
		*clip2++ = topclip;
	} while (--i);
	
	// Scan drawsegs from end to start for obscuring segs.
	// The first drawseg that is closer than the sprite is the clip seg.

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	for (ds = ds_p; ds-- > firstdrawseg ; )  // new -- killough
	{
		// determine if the drawseg obscures the sprite
		if (ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
			(!(ds->silhouette & SIL_BOTH) && ds->maskedtexturecol == -1) )
		{
			// does not cover sprite
			continue;
		}

		r1 = MAX<int> (ds->x1, spr->x1);
		r2 = MIN<int> (ds->x2, spr->x2);

		if (ds->neardepth > spr->depth || (ds->fardepth > spr->depth &&
			// Check if sprite is in front of draw seg:
			DMulScale24 (spr->depth - ds->cy, ds->cdx,
						 ds->cdy, ds->cx - spr->cx) < 0))
		{
			// seg is behind sprite, so draw the mid texture if it has one
			if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
				R_RenderMaskedSegRange (ds, r1, r2);
			continue;
		}

		// clip this piece of the sprite
		// killough 3/27/98: optimized and made much shorter
		// [RH] Optimized further (at least for VC++;
		// other compilers should be at least as good as before)

		if (ds->silhouette & SIL_BOTTOM) //bottom sil
		{
			clip1 = clipbot + r1;
			clip2 = openings + ds->sprbottomclip + r1 - ds->x1;
			i = r2 - r1 + 1;
			do
			{
				if (*clip1 > *clip2)
					*clip1 = *clip2;
				clip1++;
				clip2++;
			} while (--i);
		}

		if (ds->silhouette & SIL_TOP)   // top sil
		{
			clip1 = cliptop + r1;
			clip2 = openings + ds->sprtopclip + r1 - ds->x1;
			i = r2 - r1 + 1;
			do
			{
				if (*clip1 < *clip2)
					*clip1 = *clip2;
				clip1++;
				clip2++;
			} while (--i);
		}
	}

	// all clipping has been performed, so draw the sprite

	mfloorclip = clipbot;
	mceilingclip = cliptop;
	R_DrawVisSprite (spr, spr->x1, spr->x2);
}

//
// R_DrawMasked
//
void R_DrawMasked (void)
{
	drawseg_t *ds;
	int i;

	R_SortVisSprites ();

	for (i = vsprcount; i > 0; i--)
	{
		R_DrawSprite (spritesorter[i-1]);
	}

	// render any remaining masked mid textures

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	for (ds = ds_p; ds-- > firstdrawseg; )	// new -- killough
	{
		if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
		{
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);
		}
	}
	
	// draw the psprites on top of everything
	//	but does not draw on side views
	if (!viewangleoffset)
	{
		R_DrawPlayerSprites ();
	}
}


//
// [RH] Particle functions
//
#ifndef _MSC_VER
// inlined with VC++
particle_t *NewParticle (void)
{
	particle_t *result = NULL;
	if (InactiveParticles != -1)
	{
		result = Particles + InactiveParticles;
		InactiveParticles = result->next;
		result->next = ActiveParticles;
		ActiveParticles = result - Particles;
	}
	return result;
}
#endif

void R_InitParticles (void)
{
	char *i;

	if ((i = Args.CheckValue ("-numparticles")))
		NumParticles = atoi (i);
	if (NumParticles == 0)
		NumParticles = 4000;
	else if (NumParticles < 100)
		NumParticles = 100;

	Particles = new particle_t[NumParticles * sizeof(particle_t)];
	R_ClearParticles ();
}

void R_ClearParticles (void)
{
	int i;

	memset (Particles, 0, NumParticles * sizeof(particle_t));
	ActiveParticles = -1;
	InactiveParticles = 0;
	for (i = 0; i < NumParticles-1; i++)
		Particles[i].next = i + 1;
	Particles[i].next = -1;
}

void R_ProjectParticle (particle_t *particle)
{
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	fixed_t				gzt;				// killough 3/27/98
	fixed_t 			tx;
	fixed_t 			tz, tiz;
	fixed_t 			xscale;
	int 				x1;
	int 				x2;
	vissprite_t*		vis;
	sector_t			*sector = NULL;
	sector_t*			heightsec = NULL;	// killough 3/27/98

	// transform the origin point
	tr_x = particle->x - viewx;
	tr_y = particle->y - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	// particle is behind view plane?
	if (tz < MINZ)
		return;

	tx = DMulScale20 (tr_x, viewsin, -tr_y, viewcos);

	// Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = viewwidth - tx - 1;
	}

	// too far off the side?
	if (tz <= abs (tx))
		return;

	tiz = 268435456 / tz;
	xscale = centerx * tiz;

	// calculate edges of the shape
	int psize = particle->size << (12-3);

	x1 = MAX<int> (WindowLeft, (centerxfrac + MulScale12 (tx-psize, xscale)) >> FRACBITS);
	x2 = MIN<int> (WindowRight, (centerxfrac + MulScale12 (tx+psize, xscale)) >> FRACBITS);

	if (x1 > x2)
		return;

	gzt = particle->z+1;

	// killough 3/27/98: exclude things totally separated
	// from the viewer, by either water or fake ceilings
	// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

	{
		subsector_t *subsector = R_PointInSubsector (particle->x, particle->y);
		if (subsector)
		{
			sector = subsector->sector;
			heightsec = sector->heightsec;
			if (particle->z < sector->floorplane.ZatPoint (particle->x, particle->y) ||
				particle->z > sector->ceilingplane.ZatPoint (particle->y, particle->z))
				return;
		}
	}

	if (heightsec && !(heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{ // only clip particles which are in special sectors
		sector_t *phs = viewsector->heightsec;
		if (phs != NULL && phs->MoreFlags & SECF_IGNOREHEIGHTSEC)
		{
			phs = NULL;
		}
		if (phs && viewz < phs->floorplane.ZatPoint (viewx, viewy) ?
			particle->z >= heightsec->floorplane.ZatPoint (particle->x, particle->y):
			gzt < heightsec->floorplane.ZatPoint (particle->x, particle->y))
		  return;
		if (phs && viewz > phs->ceilingplane.ZatPoint (viewx, viewy) ?
			gzt < heightsec->ceilingplane.ZatPoint (particle->x, particle->y) &&
			viewz >= heightsec->ceilingplane.ZatPoint (viewx, viewy) :
			particle->z >= heightsec->ceilingplane.ZatPoint (particle->x, particle->y))
		  return;
	}

	// store information in a vissprite
	vis = R_NewVisSprite ();
	vis->heightsec = heightsec;
	vis->xscale = xscale;
//	vis->yscale = FixedMul (xscale, InvZtoScale);
	vis->depth = tz;
	vis->gx = particle->x;
	vis->gy = particle->y;
	vis->gz = particle->z;
	vis->gzt = gzt;
	vis->texturemid = FixedMul (yaspectmul, vis->gzt - viewz);
	vis->x1 = x1;
	vis->x2 = x2;
	vis->Translation = 0;
	vis->startfrac = particle->color;
	vis->picnum = -1;
	vis->renderflags = particle->trans;

	if (fixedcolormap)
	{
		vis->colormap = fixedcolormap;
	}
	else if (sector)
	{
		byte *map;

		if (sector->heightsec == NULL ||
			(sector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
		{
			map = sector->floorcolormap->Maps;
		}
		else
		{
			const sector_t *s = sector->heightsec;
			if (particle->z <= s->floorplane.ZatPoint (particle->x, particle->y) ||
				particle->z > s->ceilingplane.ZatPoint (particle->x, particle->y))
				map = s->floorcolormap->Maps;
			else
				map = sector->floorcolormap->Maps;
		}

		if (fixedlightlev)
		{
			vis->colormap = map + fixedlightlev;
		}
		else
		{
			vis->colormap = map + (GETPALOOKUP (FixedMul (tiz, r_ParticleVisibility),
				LIGHT2SHADE(sector->lightlevel + r_actualextralight)) << COLORMAPSHIFT);
		}
	}
	else
	{
		vis->colormap = realcolormaps;
	}
}

void R_DrawParticle (vissprite_t *vis, int x1, int x2)
{
	byte color = vis->colormap[vis->startfrac];
	int yl = (centeryfrac - FixedMul(vis->texturemid, vis->xscale) + FRACUNIT - 1) >> FRACBITS;
	int yh;
	x1 = vis->x1;
	x2 = vis->x2;

	yh = yl + (((x2 - x1)<<detailxshift)>>detailyshift);

	// Don't bother clipping each individual column
	if (yh >= mfloorclip[x1])
		yh = mfloorclip[x1]-1;
	if (yl < mceilingclip[x1])
		yl = mceilingclip[x1];
	if (yh >= mfloorclip[x2])
		yh = mfloorclip[x2]-1;
	if (yl < mceilingclip[x2])
		yl = mceilingclip[x2];

	// vis->renderflags holds translucency level (0-255)
	{
		DWORD *bg2rgb;
		int countbase = x2 - x1 + 1;
		int ycount;
		int spacing;
		byte *dest;
		DWORD fg;

		ycount = yh - yl;
		if (ycount < 0)
			return;
		ycount++;

		{
			fixed_t fglevel, bglevel;
			DWORD *fg2rgb;

			fglevel = ((vis->renderflags + 1) << 8) & ~0x3ff;
			bglevel = FRACUNIT-fglevel;
			fg2rgb = Col2RGB8[fglevel>>10];
			bg2rgb = Col2RGB8[bglevel>>10];
			fg = fg2rgb[color];
		}

		spacing = (RenderTarget->GetPitch()<<detailyshift) - countbase;
		dest = ylookup[yl] + x1;

		do
		{
			int count = countbase;
			do
			{
				DWORD bg = bg2rgb[*dest];
				bg = (fg+bg) | 0x1f07c1f;
				*dest++ = RGB32k[0][0][bg & (bg>>15)];
			} while (--count);
			dest += spacing;
		} while (--ycount);
	}
}
