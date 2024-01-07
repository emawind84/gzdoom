/*
** sdlglvideo.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Christoph Oelckers et.al.
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

// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "version.h"
#include "c_console.h"

#include "videomodes.h"
#include "glvideo.h"
#include "gl_sysfb.h"
//#include "gl/system/gl_system.h"
#include "r_defs.h"
//#include "gl/gl_intern.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_shader.h"

//#include <QzDoom/VrCommon.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;
// extern int vid_renderer;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_adapter)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Int, vid_renderer)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Int, vid_refreshrate)
EXTERN_CVAR (Bool, cl_capfps)


DFrameBuffer *CreateGLSWFrameBuffer(int width, int height, bool bgra, bool fullscreen);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Bool, gl_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
#ifdef __arm__
CUSTOM_CVAR(Bool, gl_es, false, CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
#else
CUSTOM_CVAR(Bool, gl_es, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

NoSDLGLVideo::NoSDLGLVideo (int parm)
{
	IteratorBits = 0;
}

NoSDLGLVideo::~NoSDLGLVideo ()
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

void NoSDLGLVideo::StartModeIterator (int bits, bool fs)
{
	IteratorMode = 0;
	IteratorBits = bits;
}

bool NoSDLGLVideo::NextMode (int *width, int *height, bool *letterbox)
{
	if (IteratorBits != 8)
		return false;

	if ((unsigned)IteratorMode < sizeof(VideoModes)/sizeof(VideoModes[0]))
	{
		*width = VideoModes[IteratorMode].width;
		*height = VideoModes[IteratorMode].height;
		++IteratorMode;
		return true;
	}
	return false;
}

int TBXR_GetRefresh();

DFrameBuffer *NoSDLGLVideo::CreateFrameBuffer (int width, int height, bool bgra, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;
	
	PalEntry flashColor;
//	int flashAmount;

	if (old != NULL)
	{
		delete old;
	}
	else
	{
		flashColor = 0;
//		flashAmount = 0;
	}
	
	SystemFrameBuffer *fb;

	fb = new OpenGLFrameBuffer(0, width, height, 32, TBXR_GetRefresh(), true);

	retry = 0;
	return fb;
}

void NoSDLGLVideo::SetWindowedScale (float scale)
{
}

bool NoSDLGLVideo::SetResolution (int width, int height, int bits)
{
	// FIXME: Is it possible to do this without completely destroying the old
	// interface?
#ifndef NO_GL

	if (GLRenderer != NULL) GLRenderer->FlushTextures();
	I_ShutdownGraphics();

	Video = new NoSDLGLVideo(0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");

#if (defined(WINDOWS)) || defined(WIN32)
	bits=32;
#else
	bits=24;
#endif
	
	V_DoModeSetup(width, height, bits);
#endif
	return true;	// We must return true because the old video context no longer exists.
}

//==========================================================================
//
// 
//
//==========================================================================
#ifdef __MOBILE__
extern "C" int glesLoad;
#endif

void NoSDLGLVideo::SetupPixelFormat(bool allowsoftware, int multisample, const int *glver)
{
		
#ifdef __MOBILE__

	int major,min;

	const char *version = Args->CheckValue("-glversion");
	if( !strcmp(version, "gles1") )
	{
		glesLoad = 1;
		major = 1;
		min = 0;
	}
	else if ( !strcmp(version, "gles2") )
	{
		glesLoad = 2;
        major = 2;
        min = 0;
	}
    else if ( !strcmp(version, "gles3") )
	{
		glesLoad = 3;
		major = 3;
		min = 1;
	}
#endif

}


IVideo *gl_CreateVideo()
{
	return new NoSDLGLVideo(0);
}


// FrameBuffer implementation -----------------------------------------------

SystemFrameBuffer::SystemFrameBuffer (void *, int width, int height, int, int, bool fullscreen, bool bgra)
	: DFrameBuffer (width, height, bgra)
{
}

SystemFrameBuffer::~SystemFrameBuffer ()
{
}


void SystemFrameBuffer::InitializeState() 
{
}

void SystemFrameBuffer::SetGammaTable(uint16_t *tbl)
{
}

void SystemFrameBuffer::ResetGammaTable()
{
}

bool SystemFrameBuffer::IsFullscreen ()
{
	return true;
}

void SystemFrameBuffer::SetVSync( bool vsync )
{
}

int QzDoom_SetRefreshRate(int refreshRate);

void SystemFrameBuffer::NewRefreshRate ()
{
	if (QzDoom_SetRefreshRate(vid_refreshrate) != 0) {
		Printf("Failed to set refresh rate to %dHz.\n", *vid_refreshrate);
	}
}

void SystemFrameBuffer::SwapBuffers()
{
	//No swapping required
}

int SystemFrameBuffer::GetClientWidth()
{
	uint32_t w, h;
    QzDoom_GetScreenRes(&w, &h);
	int width = w;
	return width;
}

int SystemFrameBuffer::GetClientHeight()
{
	uint32_t w, h;
    QzDoom_GetScreenRes(&w, &h);
	int height = h;
	return height;
}


// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* caption)
{
}

