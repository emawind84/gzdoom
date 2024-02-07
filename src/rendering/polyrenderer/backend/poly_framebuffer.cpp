/*
**  Softpoly backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "v_video.h"
#include "m_png.h"
#include "templates.h"
#include "r_videoscale.h"
#include "i_time.h"
#include "g_game.h"
#include "v_text.h"
#include "i_video.h"
#include "v_draw.h"

#include "hwrenderer/utility/hw_clock.h"
#include "hw_vrmodes.h"
#include "hw_cvars.h"
#include "hwrenderer/models/hw_models.h"
#include "hw_skydome.h"
#include "hwrenderer/scene/hw_fakeflat.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"
#include "flatvertices.h"
#include "hwrenderer/data/shaderuniforms.h"
#include "hw_lightbuffer.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

#include "swrenderer/r_swscene.h"

#include "poly_framebuffer.h"
#include "poly_buffers.h"
#include "poly_renderstate.h"
#include "poly_hwtexture.h"
#include "engineerrors.h"

void Draw2D(F2DDrawer *drawer, FRenderState &state, bool outside2D = false);

EXTERN_CVAR(Bool, r_drawvoxels)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Bool, cl_capfps)

extern int rendered_commandbuffers;
extern int current_rendered_commandbuffers;

extern bool gpuStatActive;
extern bool keepGpuStatActive;
extern FString gpuStatOutput;

PolyFrameBuffer::PolyFrameBuffer(void *hMonitor, bool fullscreen) : Super(hMonitor, fullscreen) 
{
	I_PolyPresentInit();
}

PolyFrameBuffer::~PolyFrameBuffer()
{
	// screen is already null at this point, but PolyHardwareTexture::ResetAll needs it during clean up. Is there a better way we can do this?
	auto tmp = screen;
	screen = this;

	PolyHardwareTexture::ResetAll();
	PolyBuffer::ResetAll();
	PPResource::ResetAll();

	delete mScreenQuad.VertexBuffer;
	delete mScreenQuad.IndexBuffer;

	delete mVertexData;
	delete mSkyData;
	delete mViewpoints;
	delete mLights;
	mShadowMap.Reset();

	screen = tmp;

	I_PolyPresentDeinit();
}

void PolyFrameBuffer::InitializeState()
{
	vendorstring = "Poly";
	hwcaps = RFL_SHADER_STORAGE_BUFFER | RFL_BUFFER_STORAGE;
	glslversion = 4.50f;
	uniformblockalignment = 1;
	maxuniformblock = 0x7fffffff;

	mRenderState.reset(new PolyRenderState());

	mVertexData = new FFlatVertexBuffer(GetWidth(), GetHeight());
	mSkyData = new FSkyVertexBuffer;
	mViewpoints = new HWViewpointBuffer;
	mLights = new FLightBuffer();

	static const FVertexBufferAttribute format[] =
	{
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(ScreenQuadVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(ScreenQuadVertex, u) },
		{ 0, VATTR_COLOR, VFmt_Byte4, (int)myoffsetof(ScreenQuadVertex, color0) }
	};

	uint32_t indices[6] = { 0, 1, 2, 1, 3, 2 };

	mScreenQuad.VertexBuffer = screen->CreateVertexBuffer();
	mScreenQuad.VertexBuffer->SetFormat(1, 3, sizeof(ScreenQuadVertex), format);

	mScreenQuad.IndexBuffer = screen->CreateIndexBuffer();
	mScreenQuad.IndexBuffer->SetData(6 * sizeof(uint32_t), indices, false);

	CheckCanvas();
}

void PolyFrameBuffer::CheckCanvas()
{
	if (!mCanvas || mCanvas->GetWidth() != GetWidth() || mCanvas->GetHeight() != GetHeight())
	{
		FlushDrawCommands();
		DrawerThreads::WaitForWorkers();

		mCanvas.reset(new DCanvas(0, 0, true));
		mCanvas->Resize(GetWidth(), GetHeight(), false);
		mDepthStencil.reset();
		mDepthStencil.reset(new PolyDepthStencil(GetWidth(), GetHeight()));

		mRenderState->SetRenderTarget(GetCanvas(), GetDepthStencil(), true);
	}
}

PolyCommandBuffer *PolyFrameBuffer::GetDrawCommands()
{
	if (!mDrawCommands)
	{
		mDrawCommands.reset(new PolyCommandBuffer(&mFrameMemory));
		mDrawCommands->SetLightBuffer(mLightBuffer->Memory());
	}
	return mDrawCommands.get();
}

void PolyFrameBuffer::FlushDrawCommands()
{
	mRenderState->EndRenderPass();
	if (mDrawCommands)
	{
		mDrawCommands->Submit();
		mDrawCommands.reset();
	}
}

void PolyFrameBuffer::Update()
{
	twoD.Reset();
	Flush3D.Reset();

	Flush3D.Clock();

	Draw2D();
	twod->Clear();

	Flush3D.Unclock();

	FlushDrawCommands();

	if (mCanvas)
	{
		int w = mCanvas->GetWidth();
		int h = mCanvas->GetHeight();
		int pixelsize = 4;
		const uint8_t *src = (const uint8_t*)mCanvas->GetPixels();
		int pitch = 0;
		uint8_t *dst = I_PolyPresentLock(w, h, cur_vsync, pitch);
		if (dst)
		{
#if 1
			auto copyqueue = std::make_shared<DrawerCommandQueue>(&mFrameMemory);
			copyqueue->Push<MemcpyCommand>(dst, pitch / pixelsize, src, w, h, w, pixelsize);
			DrawerThreads::Execute(copyqueue);
#else
			for (int y = 0; y < h; y++)
			{
				memcpy(dst + y * pitch, src + y * w * pixelsize, w * pixelsize);
			}
#endif

			DrawerThreads::WaitForWorkers();
			I_PolyPresentUnlock(mOutputLetterbox.left, mOutputLetterbox.top, mOutputLetterbox.width, mOutputLetterbox.height);
		}
		FPSLimit();
	}

	DrawerThreads::WaitForWorkers();
	mFrameMemory.Clear();
	FrameDeleteList.Buffers.clear();
	FrameDeleteList.Images.clear();

	CheckCanvas();

	Super::Update();
}


void PolyFrameBuffer::RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc)
{
	auto BaseLayer = static_cast<PolyHardwareTexture*>(tex->GetHardwareTexture(0, 0));

	DCanvas *image = BaseLayer->GetImage(tex, 0, 0);
	PolyDepthStencil *depthStencil = BaseLayer->GetDepthStencil(tex);
	mRenderState->SetRenderTarget(image, depthStencil, false);

	IntRect bounds;
	bounds.left = bounds.top = 0;
	bounds.width = std::min(tex->GetWidth(), image->GetWidth());
	bounds.height = std::min(tex->GetHeight(), image->GetHeight());

	renderFunc(bounds);

	FlushDrawCommands();
	DrawerThreads::WaitForWorkers();
	mRenderState->SetRenderTarget(GetCanvas(), GetDepthStencil(), true);

	tex->SetUpdated(true);
}

static uint8_t ToIntColorComponent(float v)
{
	return clamp((int)(v * 255.0f + 0.5f), 0, 255);
}

void PolyFrameBuffer::PostProcessScene(bool swscene, int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D)
{
	afterBloomDrawEndScene2D();

	if (fixedcm >= CM_FIRSTSPECIALCOLORMAP && fixedcm < CM_MAXCOLORMAP)
	{
		FSpecialColormap* scm = &SpecialColormaps[fixedcm - CM_FIRSTSPECIALCOLORMAP];

		mRenderState->SetViewport(mScreenViewport.left, mScreenViewport.top, mScreenViewport.width, mScreenViewport.height);
		screen->mViewpoints->Set2D(*mRenderState, screen->GetWidth(), screen->GetHeight());

		ScreenQuadVertex vertices[4] =
		{
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			{ (float)mScreenViewport.width, 0.0f, 1.0f, 0.0f },
			{ 0.0f, (float)mScreenViewport.height, 0.0f, 1.0f },
			{ (float)mScreenViewport.width, (float)mScreenViewport.height, 1.0f, 1.0f }
		};
		mScreenQuad.VertexBuffer->SetData(4 * sizeof(ScreenQuadVertex), vertices, false);

		mRenderState->SetVertexBuffer(mScreenQuad.VertexBuffer, 0, 0);
		mRenderState->SetIndexBuffer(mScreenQuad.IndexBuffer);

		mRenderState->SetObjectColor(PalEntry(255, int(scm->ColorizeStart[0] * 127.5f), int(scm->ColorizeStart[1] * 127.5f), int(scm->ColorizeStart[2] * 127.5f)));
		mRenderState->SetAddColor(PalEntry(255, int(scm->ColorizeEnd[0] * 127.5f), int(scm->ColorizeEnd[1] * 127.5f), int(scm->ColorizeEnd[2] * 127.5f)));

		mRenderState->EnableDepthTest(false);
		mRenderState->EnableMultisampling(false);
		mRenderState->SetCulling(Cull_None);

		mRenderState->SetScissor(-1, -1, -1, -1);
		mRenderState->SetColor(1, 1, 1, 1);
		mRenderState->AlphaFunc(Alpha_GEqual, 0.f);
		mRenderState->EnableTexture(false);
		mRenderState->SetColormapShader(true);
		mRenderState->DrawIndexed(DT_Triangles, 0, 6);
		mRenderState->SetColormapShader(false);
		mRenderState->SetObjectColor(0xffffffff);
		mRenderState->SetAddColor(0);
		mRenderState->SetVertexBuffer(screen->mVertexData);
		mRenderState->EnableTexture(true);
		mRenderState->ResetColor();
	}
}


void PolyFrameBuffer::SetVSync(bool vsync)
{
	cur_vsync = vsync;
}

FRenderState* PolyFrameBuffer::RenderState()
{ 
	return mRenderState.get(); 
}


void PolyFrameBuffer::PrecacheMaterial(FMaterial *mat, int translation)
{
	if (mat->Source()->GetUseType() == ETextureType::SWCanvas) return;

	MaterialLayerInfo* layer;
	auto systex = static_cast<PolyHardwareTexture*>(mat->GetLayer(0, translation, &layer));
	systex->GetImage(layer->layerTexture, translation, layer->scaleFlags);

	int numLayers = mat->NumLayers();
	for (int i = 1; i < numLayers; i++)
	{
		auto systex = static_cast<PolyHardwareTexture*>(mat->GetLayer(i, 0, &layer));
		systex->GetImage(layer->layerTexture, 0, layer->scaleFlags);	// fixme: Upscale flags must be disabled for certain layers.
	}
}

IHardwareTexture *PolyFrameBuffer::CreateHardwareTexture()
{
	return new PolyHardwareTexture();
}

IVertexBuffer *PolyFrameBuffer::CreateVertexBuffer()
{
	return new PolyVertexBuffer();
}

IIndexBuffer *PolyFrameBuffer::CreateIndexBuffer()
{
	return new PolyIndexBuffer();
}

IDataBuffer *PolyFrameBuffer::CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize)
{
	IDataBuffer *buffer = new PolyDataBuffer(bindingpoint, ssbo, needsresize);
	if (bindingpoint == LIGHTBUF_BINDINGPOINT)
		mLightBuffer = buffer;
	return buffer;
}

void PolyFrameBuffer::SetTextureFilterMode()
{
	TextureFilterChanged();
}

void PolyFrameBuffer::TextureFilterChanged()
{
}

void PolyFrameBuffer::BlurScene(float amount)
{
}

void PolyFrameBuffer::UpdatePalette()
{
}

FTexture *PolyFrameBuffer::WipeStartScreen()
{
	SetViewportRects(nullptr);

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<PolyHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeStartScreen");

	return tex;
}

FTexture *PolyFrameBuffer::WipeEndScreen()
{
	Draw2D();
	twod->Clear();

	auto tex = new FWrapperTexture(mScreenViewport.width, mScreenViewport.height, 1);
	auto systex = static_cast<PolyHardwareTexture*>(tex->GetSystemTexture());

	systex->CreateWipeTexture(mScreenViewport.width, mScreenViewport.height, "WipeEndScreen");

	return tex;
}

TArray<uint8_t> PolyFrameBuffer::GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma)
{
	int w = SCREENWIDTH;
	int h = SCREENHEIGHT;

	TArray<uint8_t> ScreenshotBuffer(w * h * 3, true);
	const uint8_t* pixels = GetCanvas()->GetPixels();
	int dindex = 0;

	// Convert to RGB
	for (int y = 0; y < h; y++)
	{
		int sindex = y * w * 4;

		for (int x = 0; x < w; x++)
		{
			ScreenshotBuffer[dindex    ] = pixels[sindex + 2];
			ScreenshotBuffer[dindex + 1] = pixels[sindex + 1];
			ScreenshotBuffer[dindex + 2] = pixels[sindex    ];
			dindex += 3;
			sindex += 4;
		}
	}

	pitch = w * 3;
	color_type = SS_RGB;
	gamma = 1.0f;
	return ScreenshotBuffer;
}

void PolyFrameBuffer::BeginFrame()
{
	SetViewportRects(nullptr);
	CheckCanvas();

	swrenderer::R_InitFuzzTable(GetCanvas()->GetPitch());
	static int next_random = 0;
	swrenderer::fuzzpos = (swrenderer::fuzzpos + swrenderer::fuzz_random_x_offset[next_random] * FUZZTABLE / 100) % FUZZTABLE;
	next_random++;
	if (next_random == FUZZ_RANDOM_X_SIZE)
		next_random = 0;
}

void PolyFrameBuffer::Draw2D(bool outside2D)
{
	::Draw2D(twod, *mRenderState, outside2D);
}

unsigned int PolyFrameBuffer::GetLightBufferBlockSize() const
{
	return mLights->GetBlockSize();
}

void PolyFrameBuffer::UpdateShadowMap()
{
}

void PolyFrameBuffer::AmbientOccludeScene(float m5)
{
	//mPostprocess->AmbientOccludeScene(m5);
}
