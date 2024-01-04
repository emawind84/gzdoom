// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2010-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_framebuffer.cpp
** Implementation of the non-hardware specific parts of the
** OpenGL frame buffer
**
*/

#include "gl/system/gl_system.h"
#include "v_video.h"
#include "m_png.h"
#include "templates.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/textures/gl_samplers.h"
#include "hwrenderer/utility/hw_clock.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/models/gl_models.h"
#include "gl_debug.h"
#include "r_videoscale.h"

EXTERN_CVAR (Bool, vid_vsync)

CVAR(Bool, gl_aalines, false, CVAR_ARCHIVE)

FGLRenderer *GLRenderer;

void gl_LoadExtensions();
void gl_PrintStartupLog();

extern bool vid_hdr_active;

CUSTOM_CVAR(Int, vid_hwgamma, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL) //Defaulted off for VR
{
	if (self < 0 || self > 2) self = 2;
	if (screen != nullptr) screen->SetGamma();
}

//==========================================================================
//
//
//
//==========================================================================

OpenGLFrameBuffer::OpenGLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen) : 
	Super(hMonitor, width, height, bits, refreshHz, fullscreen, false) 
{
	// SetVSync needs to be at the very top to workaround a bug in Nvidia's OpenGL driver.
	// If wglSwapIntervalEXT is called after glBindFramebuffer in a frame the setting is not changed!
	Super::SetVSync(vid_vsync);

	// Make sure all global variables tracking OpenGL context state are reset..
	FHardwareTexture::InitGlobalState();
	FMaterial::InitGlobalState();
	gl_RenderState.Reset();

	GLRenderer = new FGLRenderer(this);
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();
	ScreenshotBuffer = NULL;

	InitializeState();
	mDebug = std::make_shared<FGLDebug>();
	mDebug->Update();
	SetGamma();
	hwcaps = gl.flags;
	if (gl.legacyMode) hwcaps |= RFL_NO_SHADERS;
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
	delete GLRenderer;
	GLRenderer = NULL;
}

//==========================================================================
//
// Initializes the GL renderer
//
//==========================================================================

void OpenGLFrameBuffer::InitializeState()
{
	static bool first=true;

	if (first)
	{
		if (ogl_LoadFunctions() == ogl_LOAD_FAILED)
		{
			I_FatalError("Failed to load OpenGL functions.");
		}
	}

	gl_LoadExtensions();
	Super::InitializeState();

	if (first)
	{
		first=false;
		gl_PrintStartupLog();
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glDepthFunc(GL_LESS);

	glEnable(GL_DITHER);
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
#ifndef __MOBILE__
	glEnable(GL_POLYGON_OFFSET_LINE);
#endif
	glEnable(GL_BLEND);
#ifndef __MOBILE__
	glEnable(GL_DEPTH_CLAMP);
#endif
	glDisable(GL_DEPTH_TEST);
	if (gl.legacyMode) glEnable(GL_TEXTURE_2D);
#ifndef __MOBILE__
	glDisable(GL_LINE_SMOOTH);
#endif
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GLRenderer->Initialize(GetWidth(), GetHeight());
	GLRenderer->SetOutputViewport(nullptr);
	Begin2D(false);
}

//==========================================================================
//
// Updates the screen
//
//==========================================================================

void OpenGLFrameBuffer::Update()
{
	Begin2D(false);

	DrawRateStuff();
	DrawVersionString();
	GLRenderer->Flush();

	Swap();
	CheckBench();

	int initialWidth = IsFullscreen() ? VideoWidth : GetClientWidth();
	int initialHeight = IsFullscreen() ? VideoHeight : GetClientHeight();
	int clientWidth = ViewportScaledWidth(initialWidth, initialHeight);
	int clientHeight = ViewportScaledHeight(initialWidth, initialHeight);
	if (clientWidth < 160) clientWidth = 160;
	if (clientHeight < 100) clientHeight = 100;
	if (clientWidth > 0 && clientHeight > 0 && (Width != clientWidth || Height != clientHeight))
	{
		// Do not call Resize here because it's only for software canvases
		Width = clientWidth;
		Height = clientHeight;
		V_OutputResized(Width, Height);
		GLRenderer->mVBO->OutputResized(Width, Height);
	}

	GLRenderer->SetOutputViewport(nullptr);
}

//===========================================================================
//
// 
//
//===========================================================================

void OpenGLFrameBuffer::RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV)
{
	if (!V_IsHardwareRenderer())
	{
		Super::RenderTextureView(tex, Viewpoint, FOV);
	}
	else if (GLRenderer != nullptr)
	{
		GLRenderer->RenderTextureView(tex, Viewpoint, FOV);
		camtexcount++;
	}
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

void OpenGLFrameBuffer::WriteSavePic(player_t *player, FileWriter *file, int width, int height)
{
	if (!V_IsHardwareRenderer())
		Super::WriteSavePic(player, file, width, height);

	if (GLRenderer != nullptr)
		GLRenderer->WriteSavePic(player, file, width, height);
}

//===========================================================================
//
// 
//
//===========================================================================

void OpenGLFrameBuffer::RenderView(player_t *player)
{
	if (GLRenderer != nullptr)
		GLRenderer->RenderView(player);
}



//===========================================================================
//
// 
//
//===========================================================================

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)

uint32_t OpenGLFrameBuffer::GetCaps()
{
	if (!V_IsHardwareRenderer())
		return Super::GetCaps();

	// describe our basic feature set
	ActorRenderFeatureFlags FlagSet = RFF_FLATSPRITES | RFF_MODELS | RFF_SLOPE3DFLOORS |
		RFF_TILTPITCH | RFF_ROLLSPRITES | RFF_POLYGONAL;
	if (r_drawvoxels)
		FlagSet |= RFF_VOXELS;
	if (gl.legacyMode)
	{
		// legacy mode always has truecolor because palette tonemap is not available
		FlagSet |= RFF_TRUECOLOR;
	}
	else if (!(FGLRenderBuffers::IsEnabled()))
	{
		// truecolor is always available when renderbuffers are unavailable because palette tonemap is not possible
		FlagSet |= RFF_TRUECOLOR | RFF_MATSHADER | RFF_BRIGHTMAP;
	}
	else
	{
		if (gl_tonemap != 5) // not running palette tonemap shader
			FlagSet |= RFF_TRUECOLOR;
		FlagSet |= RFF_MATSHADER | RFF_POSTSHADER | RFF_BRIGHTMAP;
	}
	return (uint32_t)FlagSet;
}

//==========================================================================
//
// Swap the buffers
//
//==========================================================================

CVAR(Bool, gl_finishbeforeswap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

void OpenGLFrameBuffer::Swap()
{
	bool swapbefore = gl_finishbeforeswap && camtexcount == 0;
	Finish.Reset();
	Finish.Clock();
	if (swapbefore) glFinish();
	SwapBuffers();
	if (!swapbefore) glFinish();
	Finish.Unclock();
	camtexcount = 0;
	FHardwareTexture::UnbindAll();
	mDebug->Update();
}

//==========================================================================
//
// Enable/disable vertical sync
//
//==========================================================================

void OpenGLFrameBuffer::SetVSync(bool vsync)
{
	// Switch to the default frame buffer because some drivers associate the vsync state with the bound FB object.
	GLint oldDrawFramebufferBinding = 0, oldReadFramebufferBinding = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebufferBinding);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebufferBinding);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	Super::SetVSync(vsync);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFramebufferBinding);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFramebufferBinding);
}

//===========================================================================
//
// DoSetGamma
//
// (Unfortunately Windows has some safety precautions that block gamma ramps
//  that are considered too extreme. As a result this doesn't work flawlessly)
//
//===========================================================================

void OpenGLFrameBuffer::SetGamma()
{
	bool useHWGamma = m_supportsGamma && ((vid_hwgamma == 0) || (vid_hwgamma == 2 && IsFullscreen()));
	if (useHWGamma)
	{
		uint16_t gammaTable[768];

		// This formula is taken from Doomsday
		BuildGammaTable(gammaTable);
		SetGammaTable(gammaTable);

		HWGammaActive = true;
	}
	else if (HWGammaActive)
	{
		ResetGammaTable();
		HWGammaActive = false;
	}
}

//===========================================================================
//
//
//===========================================================================

void OpenGLFrameBuffer::CleanForRestart()
{
	if (GLRenderer)
		GLRenderer->ResetSWScene();
}

void OpenGLFrameBuffer::SetTextureFilterMode()
{
	if (GLRenderer != nullptr && GLRenderer->mSamplerManager != nullptr) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

IHardwareTexture *OpenGLFrameBuffer::CreateHardwareTexture(FTexture *tex) 
{ 
	return new FHardwareTexture(tex->bNoCompress);
}

FModelRenderer *OpenGLFrameBuffer::CreateModelRenderer(int mli) 
{
	return new FGLModelRenderer(mli);
}


void OpenGLFrameBuffer::UnbindTexUnit(int no)
{
	FHardwareTexture::Unbind(no);
}

void OpenGLFrameBuffer::FlushTextures()
{
	if (GLRenderer) GLRenderer->FlushTextures();
}

void OpenGLFrameBuffer::TextureFilterChanged()
{
	if (GLRenderer != NULL && GLRenderer->mSamplerManager != NULL) GLRenderer->mSamplerManager->SetTextureFilterMode();
}

void OpenGLFrameBuffer::ResetFixedColormap()
{
	if (GLRenderer != nullptr && GLRenderer->mShaderManager != nullptr)
	{
		GLRenderer->mShaderManager->ResetFixedColormap();
	}
}


void OpenGLFrameBuffer::UpdatePalette()
{
	if (GLRenderer)
		GLRenderer->ClearTonemapPalette();
}

void OpenGLFrameBuffer::GetFlashedPalette (PalEntry pal[256])
{
	memcpy(pal, SourcePalette, 256*sizeof(PalEntry));
}

PalEntry *OpenGLFrameBuffer::GetPalette ()
{
	return SourcePalette;
}

bool OpenGLFrameBuffer::SetFlash(PalEntry rgb, int amount)
{
	Flash = PalEntry(amount, rgb.r, rgb.g, rgb.b);
	return true;
}

void OpenGLFrameBuffer::GetFlash(PalEntry &rgb, int &amount)
{
	rgb = Flash;
	rgb.a = 0;
	amount = Flash.a;
}

void OpenGLFrameBuffer::InitForLevel()
{
	if (GLRenderer != NULL)
	{
		GLRenderer->SetupLevel();
	}
}

//===========================================================================
//
// 
//
//===========================================================================

void OpenGLFrameBuffer::SetClearColor(int color)
{
	PalEntry pe = GPalette.BaseColors[color];
	GLRenderer->mSceneClearColor[0] = pe.r / 255.f;
	GLRenderer->mSceneClearColor[1] = pe.g / 255.f;
	GLRenderer->mSceneClearColor[2] = pe.b / 255.f;
}


//==========================================================================
//
//
//
//==========================================================================
void OpenGLFrameBuffer::Begin2D(bool copy3d)
{
	Super::Begin2D(copy3d);
	gl_RenderState.mViewMatrix.loadIdentity();
	gl_RenderState.mProjectionMatrix.ortho(0, GetWidth(), GetHeight(), 0, -1.0f, 1.0f);
	gl_RenderState.ApplyMatrices();

	glDisable(GL_DEPTH_TEST);

	// Korshun: ENABLE AUTOMAP ANTIALIASING!!!
	if (gl_aalines)
		glEnable(GL_LINE_SMOOTH);
	else
	{
		glDisable(GL_MULTISAMPLE);
		glDisable(GL_LINE_SMOOTH);
		glLineWidth(1.0);
	}

	if (GLRenderer != NULL)
			GLRenderer->Begin2D();
}

//===========================================================================
// 
//	Takes a screenshot
//
//===========================================================================

#ifdef __ANDROID__
extern uint8_t * gles_convertRGB(uint8_t * data, int width, int height);
#endif

void OpenGLFrameBuffer::GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma)
{
	const auto &viewport = GLRenderer->mOutputLetterbox;

	// Grab what is in the back buffer.
	// We cannot rely on SCREENWIDTH/HEIGHT here because the output may have been scaled.
	TArray<uint8_t> pixels;
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
#ifdef __ANDROID__ //Some androids do not like GL_RGB
	pixels.Resize(viewport.width * viewport.height * 4);
	glReadPixels(viewport.left, viewport.top, viewport.width, viewport.height, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
	gles_convertRGB(&pixels[0], viewport.width, viewport.height);
#else
	pixels.Resize(viewport.width * viewport.height * 3);
	glReadPixels(viewport.left, viewport.top, viewport.width, viewport.height, GL_RGB, GL_UNSIGNED_BYTE, &pixels[0]);
#endif
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	// Copy to screenshot buffer:
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	ReleaseScreenshotBuffer();
	ScreenshotBuffer = new uint8_t[w * h * 3];

	float rcpWidth = 1.0f / w;
	float rcpHeight = 1.0f / h;
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			float u = (x + 0.5f) * rcpWidth;
			float v = (y + 0.5f) * rcpHeight;
			int sx = u * viewport.width;
			int sy = v * viewport.height;
			int sindex = (sx + sy * viewport.width) * 3;
			int dindex = (x + y * w) * 3;
			ScreenshotBuffer[dindex] = pixels[sindex];
			ScreenshotBuffer[dindex + 1] = pixels[sindex + 1];
			ScreenshotBuffer[dindex + 2] = pixels[sindex + 2];
		}
	}

	pitch = -w*3;
	color_type = SS_RGB;
	buffer = ScreenshotBuffer + w * 3 * (h - 1);

	// Screenshot should not use gamma correction if it was already applied to rendered image
	EXTERN_CVAR(Bool, fullscreen);
	gamma = 1 == vid_hwgamma || (2 == vid_hwgamma && !fullscreen) ? 1.0f : Gamma;
	if (vid_hdr_active && fullscreen)
		gamma *= 2.2f;
}

//===========================================================================
// 
// Releases the screenshot buffer.
//
//===========================================================================

void OpenGLFrameBuffer::ReleaseScreenshotBuffer()
{
	if (ScreenshotBuffer != NULL) delete [] ScreenshotBuffer;
	ScreenshotBuffer = NULL;
}


void OpenGLFrameBuffer::GameRestart()
{
	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdatePalette ();
	ScreenshotBuffer = NULL;
}


void OpenGLFrameBuffer::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	int letterboxX = GLRenderer->mOutputLetterbox.left;
	int letterboxY = GLRenderer->mOutputLetterbox.top;
	int letterboxWidth = GLRenderer->mOutputLetterbox.width;
	int letterboxHeight = GLRenderer->mOutputLetterbox.height;

	// Subtract the LB video mode letterboxing
	if (IsFullscreen())
		y -= (GetTrueHeight() - VideoHeight) / 2;

	x = int16_t((x - letterboxX) * Width / letterboxWidth);
	y = int16_t((y - letterboxY) * Height / letterboxHeight);
}

//===========================================================================
// 
// 2D drawing
//
//===========================================================================

void OpenGLFrameBuffer::Draw2D()
{
	if (GLRenderer != nullptr) GLRenderer->Draw2D(&m2DDrawer);
}
