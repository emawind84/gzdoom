//
//---------------------------------------------------------------------------
// Copyright(C) 2016-2017 Christopher Bruns
// Oculus Quest changes Copyright(C) 2020 Simon Brown
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
** gl_oculusquest.cpp
** Stereoscopic virtual reality mode for the Oculus Quest HMD
**
*/

#ifndef _GL_OCULUSQUEST_H
#define _GL_OCULUSQUEST_H

#include "hw_vrmodes.h"


/* stereoscopic 3D API */
namespace s3d {

class OpenXRDeviceEyePose : public VREyeInfo
{
public:
	friend class OpenXRDeviceMode;

	OpenXRDeviceEyePose(int eye);
	virtual ~OpenXRDeviceEyePose() override;
	virtual VSMatrix GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const override;
	DVector3 GetViewShift(FRenderViewpoint& vp) const override;
	virtual void AdjustHud() const override;
	virtual void AdjustBlend(HWDrawInfo* di) const override;

	bool submitFrame() const;

protected:
	mutable uint32_t framebuffer;
	int eye;

	VSMatrix getHUDProjection() const;

	mutable VSMatrix projection;
};

class OpenXRDeviceMode : public VRMode
{
public:
	friend class OpenXRDeviceEyePose;
	static const VRMode& getInstance(); // Might return Mono mode, if no HMD available

	OpenXRDeviceMode(OpenXRDeviceEyePose eyes[2]);
	virtual ~OpenXRDeviceMode() override;
	virtual void SetUp() const override; // called immediately before rendering a scene frame
	virtual void TearDown() const override; // called immediately after rendering a scene frame
	virtual bool IsVR() const override { return true; }
	virtual void Present() const override;
	virtual void AdjustViewport(DFrameBuffer* screen) const override;
	virtual void AdjustPlayerSprites(int hand = 0) const override;
	virtual void UnAdjustPlayerSprites() const override;

	virtual bool GetHandTransform(int hand, VSMatrix* out) const override;
	virtual bool RenderPlayerSpritesInScene() const { return true; }
	virtual bool GetTeleportLocation(DVector3 &out) const override;
	virtual void Vibrate(float duration, int channel, float intensity) const override;

protected:
	OpenXRDeviceMode();

	void updateHmdPose(FRenderViewpoint& vp) const;

	OpenXRDeviceEyePose* leftEyeView;
	OpenXRDeviceEyePose* rightEyeView;

	mutable int cachedScreenBlocks;

	mutable bool isSetup;

private:
	typedef VRMode super;
	uint32_t sceneWidth, sceneHeight;

    mutable DVector3        m_TeleportLocation;
    mutable int             m_TeleportTarget;
};

} /* namespace st3d */


#endif /* _GL_OCULUSQUEST_H */
