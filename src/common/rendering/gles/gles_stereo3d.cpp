/*
** gles_stereo3d.cpp
** Stereoscopic 3D API (OpenGL ES backend)
**
**---------------------------------------------------------------------------
** Copyright 2015 Christopher Bruns
** Copyright 2016-2021 Christoph Oelckers
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
** Only anaglyph, side-by-side and top-bottom are implemented here.
** Row/column/checker interleaved modes need a dedicated shader that has no
** GLES equivalent (wadsrc/static/shaders_gles/ has no FPresent3DColumn/Row/
** CheckerShader), and quad-buffered stereo (GL_BACK_LEFT/RIGHT, GL_STEREO)
** is a desktop-GL-context concept with no GLES equivalent at all. Both fall
** through to VRMode's default (no-op) case below.
*/

#include "gles_system.h"
#include "gles_renderer.h"
#include "gles_renderbuffers.h"
#include "hw_vrmodes.h"
#include "gles_framebuffer.h"

EXTERN_CVAR(Int, vr_mode)

namespace OpenGLESRenderer
{

//==========================================================================
//
//
//
//==========================================================================

void FGLRenderer::PresentAnaglyph(bool r, bool g, bool b)
{
	mBuffers->BindOutputFB();
	ClearBorders();

	glColorMask(r, g, b, 1);
	mBuffers->BindEyeTexture(0, 0);
	DrawPresentTexture(screen->mOutputLetterbox, true);

	glColorMask(!r, !g, !b, 1);
	mBuffers->BindEyeTexture(1, 0);
	DrawPresentTexture(screen->mOutputLetterbox, true);

	glColorMask(1, 1, 1, 1);
}

//==========================================================================
//
//
//
//==========================================================================

void FGLRenderer::PresentSideBySide(int vrmode)
{
	if (vrmode == VR_SIDEBYSIDEFULL || vrmode == VR_SIDEBYSIDESQUISHED)
	{
		mBuffers->BindOutputFB();
		ClearBorders();

		// Compute screen regions to use for left and right eye views
		int leftWidth = screen->mOutputLetterbox.width / 2;
		int rightWidth = screen->mOutputLetterbox.width - leftWidth;
		IntRect leftHalfScreen = screen->mOutputLetterbox;
		leftHalfScreen.width = leftWidth;
		IntRect rightHalfScreen = screen->mOutputLetterbox;
		rightHalfScreen.width = rightWidth;
		rightHalfScreen.left += leftWidth;

		mBuffers->BindEyeTexture(0, 0);
		DrawPresentTexture(leftHalfScreen, true);

		mBuffers->BindEyeTexture(1, 0);
		DrawPresentTexture(rightHalfScreen, true);
	}
	else if (vrmode == VR_SIDEBYSIDELETTERBOX)
	{
		mBuffers->BindOutputFB();
		screen->mOutputLetterbox.top = screen->mOutputLetterbox.height;

		ClearBorders();
		screen->mOutputLetterbox.top = 0;  //reset so screenshots can be taken

		// Compute screen regions to use for left and right eye views
		int leftWidth = screen->mOutputLetterbox.width / 2;
		int rightWidth = screen->mOutputLetterbox.width - leftWidth;
		//cut letterbox height in half
		int height = screen->mOutputLetterbox.height / 2;
		int top = height * .5;
		IntRect leftHalfScreen = screen->mOutputLetterbox;
		leftHalfScreen.width = leftWidth;
		leftHalfScreen.height = height;
		leftHalfScreen.top = top;
		IntRect rightHalfScreen = screen->mOutputLetterbox;
		rightHalfScreen.width = rightWidth;
		rightHalfScreen.left += leftWidth;
		//give it those cinematic black bars on top and bottom
		rightHalfScreen.height = height;
		rightHalfScreen.top = top;

		mBuffers->BindEyeTexture(0, 0);
		DrawPresentTexture(leftHalfScreen, true);

		mBuffers->BindEyeTexture(1, 0);
		DrawPresentTexture(rightHalfScreen, true);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FGLRenderer::PresentTopBottom()
{
	mBuffers->BindOutputFB();
	ClearBorders();

	// Compute screen regions to use for left and right eye views
	int topHeight = screen->mOutputLetterbox.height / 2;
	int bottomHeight = screen->mOutputLetterbox.height - topHeight;
	IntRect topHalfScreen = screen->mOutputLetterbox;
	topHalfScreen.height = topHeight;
	topHalfScreen.top = topHeight;
	IntRect bottomHalfScreen = screen->mOutputLetterbox;
	bottomHalfScreen.height = bottomHeight;
	bottomHalfScreen.top = 0;

	mBuffers->BindEyeTexture(0, 0);
	DrawPresentTexture(topHalfScreen, true);

	mBuffers->BindEyeTexture(1, 0);
	DrawPresentTexture(bottomHalfScreen, true);
}

void FGLRenderer::PresentStereo()
{
	auto vrmode = VRMode::GetVRMode(true);
	if (vrmode->mEyeCount > 1)
		mBuffers->BlitToEyeTexture(mBuffers->CurrentEye());

	switch (vr_mode)
	{
	default:
		return;

	case VR_GREENMAGENTA:
		PresentAnaglyph(false, true, false);
		break;

	case VR_REDCYAN:
		PresentAnaglyph(true, false, false);
		break;

	case VR_AMBERBLUE:
		PresentAnaglyph(true, true, false);
		break;

	case VR_SIDEBYSIDEFULL:
	case VR_SIDEBYSIDESQUISHED:
	case VR_SIDEBYSIDELETTERBOX:
		PresentSideBySide(vr_mode);
		break;

	case VR_TOPBOTTOM:
		PresentTopBottom();
		break;
	}
}

}
