// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2018 Christoph Oelckers
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
** hw_renderstate.cpp
** hardware independent part of render state.
**
*/

#include "hw_renderstate.h"
#include "hw_drawstructs.h"
#include "hw_portal.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "hwrenderer/utility/hw_cvars.h"


//==========================================================================
//
// set current light color
//
//==========================================================================
void HWDrawInfo::SetColor(FRenderState &state, int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon)
{
	if (fullbright)
	{
		state.SetColorAlpha(0xffffff, alpha, 0);
		if (isSoftwareLighting()) state.SetSoftLightLevel(255);
		else state.SetNoSoftLightLevel();
	}
	else
	{
		int hwlightlevel = CalcLightLevel(sectorlightlevel, rellight, weapon, cm.BlendFactor);
		PalEntry pe = CalcLightColor(hwlightlevel, cm.LightColor, cm.BlendFactor);
		state.SetColorAlpha(pe, alpha, cm.Desaturation);
		if (isSoftwareLighting()) state.SetSoftLightLevel(hw_ClampLight(sectorlightlevel + rellight), cm.BlendFactor);
		else state.SetNoSoftLightLevel();
	}
}

//==========================================================================
//
// Lighting stuff 
//
//==========================================================================

void HWDrawInfo::SetShaderLight(FRenderState &state, float level, float olight)
{
	const float MAXDIST = 256.f;
	const float THRESHOLD = 96.f;
	const float FACTOR = 0.75f;

	if (level > 0)
	{
		float lightdist, lightfactor;

		if (olight < THRESHOLD)
		{
			lightdist = (MAXDIST / 2) + (olight * MAXDIST / THRESHOLD / 2);
			olight = THRESHOLD;
		}
		else lightdist = MAXDIST;

		lightfactor = 1.f + ((olight / level) - 1.f) * FACTOR;
		if (lightfactor == 1.f) lightdist = 0.f;	// save some code in the shader
		state.SetLightParms(lightfactor, lightdist);
	}
	else
	{
		state.SetLightParms(1.f, 0.f);
	}
}


//==========================================================================
//
// Sets the fog for the current polygon
//
//==========================================================================

void HWDrawInfo::SetFog(FRenderState &state, int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive)
{
	PalEntry fogcolor;
	float fogdensity;

	if (Level->flags&LEVEL_HASFADETABLE)
	{
		fogdensity = 70;
		fogcolor = 0x808080;
	}
	else if (cmap != NULL && !fullbright)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = GetFogDensity(lightlevel, fogcolor, cmap->FogDensity, cmap->BlendFactor);
		fogcolor.a = 0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	if (screen->mPortalState->inskybox) fogdensity += fogdensity / 2;


	// no fog in enhanced vision modes!
	if (gl_fogmode == 0)
	{
		state.EnableFog(false);
		state.SetFog(0, 0);
	}
	else
	{
		if (cmap && (lightmode == ELightMode::Doom || (isSoftwareLighting() && cmap->BlendFactor > 0)) && fogcolor == 0)
		{
			float light = (float)CalcLightLevel(lightlevel, rellight, false, cmap->BlendFactor);
			SetShaderLight(state, light, lightlevel);
		}
		else
		{
			state.SetLightParms(1.f, 0.f);
		}

		// For additive rendering using the regular fog color here would mean applying it twice
		// so always use black
		if (isadditive)
		{
			fogcolor = 0;
		}

		state.EnableFog(true);
		state.SetFog(fogcolor, fogdensity);

		// Korshun: fullbright fog like in software renderer.
		if (isSoftwareLighting() && cmap && cmap->BlendFactor == 0 && Level->brightfog && fogdensity != 0 && fogcolor != 0)
		{
			state.SetSoftLightLevel(255);
		}
	}
}


void FRenderState::CheckTimer(uint64_t ShaderStartTime)
{
	// if firstFrame is not yet initialized, initialize it to current time
	// if we're going to overflow a float (after ~4.6 hours, or 24 bits), re-init to regain precision
	if ((firstFrame == 0) || (screen->FrameTime - firstFrame >= 1 << 24) || ShaderStartTime >= firstFrame)
		firstFrame = screen->FrameTime;
}

void FRenderState::ResetFadeColor()
{
	mFadeColor = gl_global_fade_color;
	mStreamData.uGlobalFadeColor = mFadeColor;
}

void FRenderState::InitSceneClearColor()
{
	float r, g, b;
	if (gl_global_fade)
	{
		mSceneColor = mFadeColor;
	}
	r = g = b = 1.f;
	screen->mSceneClearColor[0] = mSceneColor.r * r / 255.f;
	screen->mSceneClearColor[1] = mSceneColor.g * g / 255.f;
	screen->mSceneClearColor[2] = mSceneColor.b * b / 255.f;
}