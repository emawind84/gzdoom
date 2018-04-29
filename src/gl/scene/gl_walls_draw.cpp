// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
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

#include "gl/system/gl_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"

#include "gl/system/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"

EXTERN_CVAR(Int, gl_max_vertices)

// If we want to share the array to avoid constant allocations it needs to be thread local unless it'd be littered with expensive synchronization.
thread_local FDynLightData lightdata;

//==========================================================================
//
// General purpose wall rendering function
// everything goes through here
//
//==========================================================================

void FDrawInfo::RenderWall(GLWall *wall, int textured)
{
	assert(wall->vertcount > 0);
	gl_RenderState.Apply();
	gl_RenderState.ApplyLightIndex(wall->dynlightindex);
	
	if (gl_max_vertices > 0 && s3d::EyePose::wallVerticesPerEye >= gl_max_vertices) {
		return;
	}

	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, wall->vertindex, wall->vertcount);
	vertexcount += wall->vertcount;
	s3d::EyePose::wallVerticesPerEye += wall->vertcount;
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::RenderFogBoundary(GLWall *wall)
{
	if (gl_fogmode && mDrawer->FixedColormap == 0)
	{
		if (!gl.legacyMode)
		{
			int rel = wall->rellight + getExtraLight();
			mDrawer->SetFog(wall->lightlevel, rel, &wall->Colormap, false);
			gl_RenderState.EnableDrawBuffers(1);
			gl_RenderState.SetEffect(EFF_FOGBOUNDARY);
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1.0f, -128.0f);
			RenderWall(wall, GLWall::RWF_BLANK);
			glPolygonOffset(0.0f, 0.0f);
			glDisable(GL_POLYGON_OFFSET_FILL);
			gl_RenderState.SetEffect(EFF_NONE);
			gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		}
		else
		{
			RenderFogBoundaryCompat(wall);
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::RenderMirrorSurface(GLWall *wall)
{
	if (!TexMan.mirrorTexture.isValid()) return;

	if (!gl.legacyMode)
	{
		// we use texture coordinates and texture matrix to pass the normal stuff to the shader so that the default vertex buffer format can be used as is.
		gl_RenderState.EnableTextureMatrix(true);
		gl_RenderState.mTextureMatrix.computeNormalMatrix(gl_RenderState.mViewMatrix);
	}
	else
	{
		FVector3 v = wall->glseg.Normal();
		glNormal3fv(&v[0]);
	}

	// Use sphere mapping for this
	gl_RenderState.SetEffect(EFF_SPHEREMAP);

	mDrawer->SetColor(wall->lightlevel, 0, wall->Colormap ,0.1f);
	mDrawer->SetFog(wall->lightlevel, 0, &wall->Colormap, true);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA,GL_ONE);
	gl_RenderState.AlphaFunc(GL_GREATER,0);
	glDepthFunc(GL_LEQUAL);

	FMaterial * pat=FMaterial::ValidateTexture(TexMan.mirrorTexture, false, false);
	gl_RenderState.SetMaterial(pat, CLAMP_NONE, 0, -1, false);

	wall->flags &= ~GLWall::GLWF_GLOW;
	RenderWall(wall, GLWall::RWF_BLANK);

	gl_RenderState.EnableTextureMatrix(false);
	gl_RenderState.SetEffect(EFF_NONE);

	// Restore the defaults for the translucent pass
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	glDepthFunc(GL_LESS);

	// This is drawn in the translucent pass which is done after the decal pass
	// As a result the decals have to be drawn here, right after the wall they are on,
	// because the depth buffer won't get set by translucent items.
	if (wall->seg->sidedef->AttachedDecals)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -128.0f);
		glDepthMask(false);
		gl_drawinfo->DrawDecalsForMirror(wall);
		glDepthMask(true);
		glPolygonOffset(0.0f, 0.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		gl_RenderState.SetTextureMode(TM_MODULATE);
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

static const uint8_t renderwalltotier[] =
{
	side_t::none,
	side_t::top,
	side_t::mid,
	side_t::mid,
	side_t::bottom,
	side_t::none,
	side_t::none,
	side_t::mid,
	side_t::none,
	side_t::mid,
};

void FDrawInfo::RenderTexturedWall(GLWall *wall, int rflags)
{
	int tmode = gl_RenderState.GetTextureMode();
	int rel = wall->rellight + getExtraLight();

	if (wall->flags & GLWall::GLWF_GLOW)
	{
		gl_RenderState.EnableGlow(true);
		gl_RenderState.SetGlowParams(wall->topglowcolor, wall->bottomglowcolor);
		gl_RenderState.SetGlowPlanes(wall->frontsector->ceilingplane, wall->frontsector->floorplane);
	}
	gl_RenderState.SetMaterial(wall->gltexture, wall->flags & 3, 0, -1, false);

	if (wall->flags & wall->GLWF_CLAMPY && (wall->type == RENDERWALL_M2S || wall->type == RENDERWALL_M2SNF))
	{
		if (tmode == TM_MODULATE) gl_RenderState.SetTextureMode(TM_CLAMPY);
	}

	if (wall->type == RENDERWALL_M2SNF)
	{
		mDrawer->SetFog(255, 0, NULL, false);
	}
	if (wall->type != RENDERWALL_COLOR && wall->seg->sidedef != nullptr)
	{
		auto side = wall->seg->sidedef;
		auto tierndx = renderwalltotier[wall->type];
		auto &tier = side->textures[tierndx];
		PalEntry color1 = side->GetSpecialColor(tierndx, side_t::walltop, wall->frontsector);
		PalEntry color2 = side->GetSpecialColor(tierndx, side_t::wallbottom, wall->frontsector);
		gl_RenderState.SetObjectColor(color1);
		gl_RenderState.SetObjectColor2(color2);
		gl_RenderState.SetAddColor(side->GetAdditiveColor(tierndx, wall->frontsector));
		if (color1 != color2)
		{
			// Do gradient setup only if there actually is a gradient.

			gl_RenderState.EnableGradient(true);
			if ((tier.flags & side_t::part::ClampGradient) && wall->backsector)
			{
				if (tierndx == side_t::top)
				{
					gl_RenderState.SetGradientPlanes(wall->frontsector->ceilingplane, wall->backsector->ceilingplane);
				}
				else if (tierndx == side_t::mid)
				{
					gl_RenderState.SetGradientPlanes(wall->backsector->ceilingplane, wall->backsector->floorplane);
				}
				else // side_t::bottom:
				{
					gl_RenderState.SetGradientPlanes(wall->backsector->floorplane, wall->frontsector->floorplane);
				}
			}
			else
			{
				gl_RenderState.SetGradientPlanes(wall->frontsector->ceilingplane, wall->frontsector->floorplane);
			}
		}
	}

	float absalpha = fabsf(wall->alpha);
	if (wall->lightlist == nullptr)
	{
		if (wall->type != RENDERWALL_M2SNF) mDrawer->SetFog(wall->lightlevel, rel, &wall->Colormap, wall->RenderStyle == STYLE_Add);
		mDrawer->SetColor(wall->lightlevel, rel, wall->Colormap, absalpha);
		RenderWall(wall, rflags);
	}
	else
	{
		gl_RenderState.EnableSplit(true);

		for (unsigned i = 0; i < wall->lightlist->Size(); i++)
		{
			secplane_t &lowplane = i == (*wall->lightlist).Size() - 1 ? wall->frontsector->floorplane : (*wall->lightlist)[i + 1].plane;
			// this must use the exact same calculation method as GLWall::Process etc.
			float low1 = lowplane.ZatPoint(wall->vertexes[0]);
			float low2 = lowplane.ZatPoint(wall->vertexes[1]);

			if (low1 < wall->ztop[0] || low2 < wall->ztop[1])
			{
				int thisll = (*wall->lightlist)[i].caster != nullptr ? hw_ClampLight(*(*wall->lightlist)[i].p_lightlevel) : wall->lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = wall->Colormap.FadeColor;
				thiscm.FogDensity = wall->Colormap.FogDensity;
				thiscm.CopyFrom3DLight(&(*wall->lightlist)[i]);
				mDrawer->SetColor(thisll, rel, thiscm, absalpha);
				if (wall->type != RENDERWALL_M2SNF) mDrawer->SetFog(thisll, rel, &thiscm, wall->RenderStyle == STYLE_Add);
				gl_RenderState.SetSplitPlanes((*wall->lightlist)[i].plane, lowplane);
				RenderWall(wall, rflags);
			}
			if (low1 <= wall->zbottom[0] && low2 <= wall->zbottom[1]) break;
		}

		gl_RenderState.EnableSplit(false);
	}
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetObjectColor2(0);
	gl_RenderState.SetAddColor(0);
	gl_RenderState.SetTextureMode(tmode);
	gl_RenderState.EnableGlow(false);
	gl_RenderState.EnableGradient(false);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::RenderTranslucentWall(GLWall *wall)
{
	if (wall->gltexture)
	{
		if (mDrawer->FixedColormap == CM_DEFAULT && gl_lights && gl.lightmethod == LM_DIRECT)
		{
			if (wall->SetupLights(lightdata))
				wall->dynlightindex = GLRenderer->mLights->UploadLights(lightdata);
		}
		if (!wall->gltexture->tex->GetTranslucency()) gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
		else gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
		if (wall->RenderStyle == STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA,GL_ONE);
		RenderTexturedWall(wall, GLWall::RWF_TEXTURED | GLWall::RWF_NOSPLIT);
		if (wall->RenderStyle == STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
		mDrawer->SetColor(wall->lightlevel, 0, wall->Colormap, fabsf(wall->alpha));
		mDrawer->SetFog(wall->lightlevel, 0, &wall->Colormap, wall->RenderStyle == STYLE_Add);
		gl_RenderState.EnableTexture(false);
		RenderWall(wall, GLWall::RWF_NOSPLIT);
		gl_RenderState.EnableTexture(true);
	}
}

//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::DrawWall(GLWall *wall, int pass)
{
	gl_RenderState.SetNormal(wall->glseg.Normal());
	switch (pass)
	{
	case GLPASS_LIGHTSONLY:
		if (wall->SetupLights(lightdata))
			wall->dynlightindex = GLRenderer->mLights->UploadLights(lightdata);
		break;

	case GLPASS_ALL:
		if (wall->SetupLights(lightdata))
			wall->dynlightindex = GLRenderer->mLights->UploadLights(lightdata);
		// fall through
	case GLPASS_PLAIN:
		RenderTexturedWall(wall, GLWall::RWF_TEXTURED);
		break;

	case GLPASS_TRANSLUCENT:

		switch (wall->type)
		{
		case RENDERWALL_MIRRORSURFACE:
			RenderMirrorSurface(wall);
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundary(wall);
			break;

		default:
			RenderTranslucentWall(wall);
			break;
		}
		break;

	case GLPASS_LIGHTTEX:
	case GLPASS_LIGHTTEX_ADDITIVE:
	case GLPASS_LIGHTTEX_FOGGY:
		gl_drawinfo->RenderLightsCompat(wall, pass);
		break;

	case GLPASS_TEXONLY:
		gl_RenderState.SetMaterial(wall->gltexture, wall->flags & 3, 0, -1, false);
		RenderWall(wall, GLWall::RWF_TEXTURED);
		break;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddWall(GLWall *wall)
{
	bool translucent = !!(wall->flags & GLWall::GLWF_TRANSLUCENT);
	int list;

	if (translucent) // translucent walls
	{
		if (!gl.legacyMode && mDrawer->FixedColormap == CM_DEFAULT && wall->gltexture != nullptr)
		{
			if (wall->SetupLights(lightdata))
				wall->dynlightindex = GLRenderer->mLights->UploadLights(lightdata);
		}
		wall->ViewDistance = (r_viewpoint.Pos - (wall->seg->linedef->v1->fPos() + wall->seg->linedef->Delta() / 2)).XY().LengthSquared();
		wall->MakeVertices(this, true);
		auto newwall = drawlists[GLDL_TRANSLUCENT].NewWall();
		*newwall = *wall;
	}
	else
	{
		if (gl.legacyMode)
		{
			if (PutWallCompat(wall, GLWall::passflag[wall->type])) return;
		}
		else if (mDrawer->FixedColormap == CM_DEFAULT)
		{
			if (wall->SetupLights(lightdata))
				wall->dynlightindex = GLRenderer->mLights->UploadLights(lightdata);
		}


		bool masked;

		masked = GLWall::passflag[wall->type] == 1 ? false : (wall->gltexture && wall->gltexture->isMasked());

		if ((wall->flags & GLWall::GLWF_SKYHACK && wall->type == RENDERWALL_M2S))
		{
			list = GLDL_MASKEDWALLSOFS;
		}
		else
		{
			list = masked ? GLDL_MASKEDWALLS : GLDL_PLAINWALLS;
		}
		wall->MakeVertices(this, false);
		auto newwall = drawlists[list].NewWall();
		*newwall = *wall;
		if (!masked) newwall->ProcessDecals(this);
	}
	wall->dynlightindex = -1;
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddMirrorSurface(GLWall *w)
{
	w->type = RENDERWALL_MIRRORSURFACE;
	auto newwall = drawlists[GLDL_TRANSLUCENTBORDER].NewWall();
	*newwall = *w;

	FVector3 v = newwall->glseg.Normal();
	auto tcs = newwall->tcs;
	tcs[GLWall::LOLFT].u = tcs[GLWall::LORGT].u = tcs[GLWall::UPLFT].u = tcs[GLWall::UPRGT].u = v.X;
	tcs[GLWall::LOLFT].v = tcs[GLWall::LORGT].v = tcs[GLWall::UPLFT].v = tcs[GLWall::UPRGT].v = v.Z;
	newwall->MakeVertices(this, false);
	newwall->ProcessDecals(this);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddPortal(GLWall *wall, int ptype)
{
	GLPortal * portal;

	wall->MakeVertices(this, false);
	switch (ptype)
	{
		// portals don't go into the draw list.
		// Instead they are added to the portal manager
	case PORTALTYPE_HORIZON:
		wall->horizon = UniqueHorizons.Get(wall->horizon);
		portal = GLPortal::FindPortal(wall->horizon);
		if (!portal) portal = new GLHorizonPortal(wall->horizon);
		portal->AddLine(wall);
		break;

	case PORTALTYPE_SKYBOX:
		portal = GLPortal::FindPortal(wall->secportal);
		if (!portal)
		{
			// either a regular skybox or an Eternity-style horizon
			if (wall->secportal->mType != PORTS_SKYVIEWPOINT) portal = new GLEEHorizonPortal(wall->secportal);
			else portal = new GLSkyboxPortal(wall->secportal);
		}
		portal->AddLine(wall);
		break;

	case PORTALTYPE_SECTORSTACK:
		portal = wall->portal->GetRenderState();
		portal->AddLine(wall);
		break;

	case PORTALTYPE_PLANEMIRROR:
		if (GLPortal::PlaneMirrorMode * wall->planemirror->fC() <= 0)
		{
			//@sync-portal
			wall->planemirror = UniquePlaneMirrors.Get(wall->planemirror);
			portal = GLPortal::FindPortal(wall->planemirror);
			if (!portal) portal = new GLPlaneMirrorPortal(wall->planemirror);
			portal->AddLine(wall);
		}
		break;

	case PORTALTYPE_MIRROR:
		portal = GLPortal::FindPortal(wall->seg->linedef);
		if (!portal) portal = new GLMirrorPortal(wall->seg->linedef);
		portal->AddLine(wall);
		if (gl_mirror_envmap)
		{
			// draw a reflective layer over the mirror
			AddMirrorSurface(wall);
		}
		break;

	case PORTALTYPE_LINETOLINE:
		portal = GLPortal::FindPortal(wall->lineportal);
		if (!portal)
		{
			line_t *otherside = wall->lineportal->lines[0]->mDestination;
			if (otherside != NULL && otherside->portalindex < level.linePortals.Size())
			{
				ProcessActorsInPortal(otherside->getPortal()->mGroup, mDrawer->in_area);
			}
			portal = new GLLineToLinePortal(wall->lineportal);
		}
		portal->AddLine(wall);
		break;

	case PORTALTYPE_SKY:
		wall->sky = UniqueSkies.Get(wall->sky);
		portal = GLPortal::FindPortal(wall->sky);
		if (!portal) portal = new GLSkyPortal(wall->sky);
		portal->AddLine(wall);
		break;
	}
	wall->vertcount = 0;
}

//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::DrawDecal(GLDecal *gldecal)
{
	auto wall = gldecal->wall;
	auto decal = gldecal->decal;
	auto tex = gldecal->gltexture;
	auto &seg = wall->seg;
	
	// calculate dynamic light effect.
	if (gl_lights && GLRenderer->mLightCount && !mDrawer->FixedColormap && gl_light_sprites)
	{
		// Note: This should be replaced with proper shader based lighting.
		double x, y;
		float out[3];
		decal->GetXY(seg->sidedef, x, y);
		hw_GetDynSpriteLight(nullptr, x, y, gldecal->zcenter, wall->sub, out);
		gl_RenderState.SetDynLight(out[0], out[1], out[2]);
	}

	// alpha color only has an effect when using an alpha texture.
	if (decal->RenderStyle.Flags & STYLEF_RedIsAlpha)
	{
		gl_RenderState.SetObjectColor(decal->AlphaColor | 0xff000000);
	}

	gl_SetRenderStyle(decal->RenderStyle, false, false);
	gl_RenderState.SetMaterial(tex, CLAMP_XY, decal->Translation, 0, !!(decal->RenderStyle.Flags & STYLEF_RedIsAlpha));


	// If srcalpha is one it looks better with a higher alpha threshold
	if (decal->RenderStyle.SrcAlpha == STYLEALPHA_One) gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	else gl_RenderState.AlphaFunc(GL_GREATER, 0.f);


	mDrawer->SetColor(gldecal->light, gldecal->rel, gldecal->colormap, gldecal->a);
	// for additively drawn decals we must temporarily set the fog color to black.
	PalEntry fc = gl_RenderState.GetFogColor();
	if (decal->RenderStyle.BlendOp == STYLEOP_Add && decal->RenderStyle.DestAlpha == STYLEALPHA_One)
	{
		gl_RenderState.SetFog(0, -1);
	}

	gl_RenderState.SetNormal(wall->glseg.Normal());

	if (wall->lightlist == nullptr)
	{
		gl_RenderState.Apply();
		GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, gldecal->vertindex, 4);
	}
	else
	{
		auto &lightlist = *wall->lightlist;

		for (unsigned k = 0; k < lightlist.Size(); k++)
		{
			secplane_t &lowplane = k == lightlist.Size() - 1 ? wall->frontsector->floorplane : lightlist[k + 1].plane;

			DecalVertex *dv = gldecal->dv;
			float low1 = lowplane.ZatPoint(dv[1].x, dv[1].y);
			float low2 = lowplane.ZatPoint(dv[2].x, dv[2].y);

			if (low1 < dv[1].z || low2 < dv[2].z)
			{
				int thisll = lightlist[k].caster != NULL ? hw_ClampLight(*lightlist[k].p_lightlevel) : wall->lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = wall->Colormap.FadeColor;
				thiscm.CopyFrom3DLight(&lightlist[k]);
				mDrawer->SetColor(thisll, gldecal->rel, thiscm, gldecal->a);
				if (level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING) thiscm.Decolorize();
				mDrawer->SetFog(thisll, gldecal->rel, &thiscm, wall->RenderStyle == STYLE_Add);
				gl_RenderState.SetSplitPlanes(lightlist[k].plane, lowplane);

				gl_RenderState.Apply();
				GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, gldecal->vertindex, 4);
			}
			if (low1 <= dv[0].z && low2 <= dv[3].z) break;
		}
	}

	rendered_decals++;
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetFog(fc, -1);
	gl_RenderState.SetDynLight(0, 0, 0);
}

//==========================================================================
//
//
//
//==========================================================================
void FDrawInfo::DrawDecals()
{
	GLWall *wall = nullptr;
	for (auto gldecal : decals[0])
	{
		if (gldecal->wall != wall)
		{
			wall = gldecal->wall;
			if (wall->lightlist != nullptr)
			{
				gl_RenderState.EnableSplit(true);
			}
			else
			{
				gl_RenderState.EnableSplit(false);
				mDrawer->SetFog(wall->lightlevel, wall->rellight + getExtraLight(), &wall->Colormap, false);
			}
		}
		DrawDecal(gldecal);
	}
	if (wall && wall->lightlist != nullptr) gl_RenderState.EnableSplit(false);
}

//==========================================================================
//
// This list will never get long, so this code should be ok.
//
//==========================================================================
void FDrawInfo::DrawDecalsForMirror(GLWall *wall)
{
	mDrawer->SetFog(wall->lightlevel, wall->rellight + getExtraLight(), &wall->Colormap, false);
	for (auto gldecal : decals[1])
	{
		if (gldecal->wall == wall)
		{
			DrawDecal(gldecal);
		}
	}
}

