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
#include "stats.h"
#include "version.h"
#include "c_console.h"

#include "glvideo.h"
#include "gl_sysfb.h"
#include "r_defs.h"

#include "gl_framebuffer.h"

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
EXTERN_CVAR (Int, vid_defwidth)
EXTERN_CVAR (Int, vid_defheight)
EXTERN_CVAR (Int, vid_refreshrate)
EXTERN_CVAR (Int, vid_preferbackend)
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
}

int TBXR_GetRefresh();

DFrameBuffer *NoSDLGLVideo::CreateFrameBuffer ()
{
	vid_preferbackend = 0;
	SystemGLFrameBuffer *fb = new OpenGLRenderer::OpenGLFrameBuffer(0, true);

	return fb;
}

void NoSDLGLVideo::SetWindowedScale (float scale)
{
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

SystemGLFrameBuffer::SystemGLFrameBuffer (void *, bool fullscreen)
	: DFrameBuffer (vid_defwidth, vid_defheight)
{
}

SystemGLFrameBuffer::~SystemGLFrameBuffer ()
{
}


void SystemGLFrameBuffer::InitializeState() 
{
}

bool SystemGLFrameBuffer::IsFullscreen ()
{
	return true;
}

void SystemGLFrameBuffer::SetVSync( bool vsync )
{
}

int QzDoom_SetRefreshRate(int refreshRate);

void SystemGLFrameBuffer::NewRefreshRate ()
{
	if (QzDoom_SetRefreshRate(vid_refreshrate) != 0) {
		Printf("Failed to set refresh rate to %dHz.\n", *vid_refreshrate);
	}
}

void SystemGLFrameBuffer::SwapBuffers()
{
	//No swapping required
}

int SystemGLFrameBuffer::GetClientWidth()
{
	uint32_t w, h;
    QzDoom_GetScreenRes(&w, &h);
	int width = w;
	return width;
}

int SystemGLFrameBuffer::GetClientHeight()
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

