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
** gl_openxrdevice.cpp
** Stereoscopic virtual reality mode for OpenXR Support
**
*/

#ifdef USE_OPENXR

#include "gl_openxrdevice.h"

#include <string>
#include <map>
#include <cmath>
#include "p_trace.h"
#include "p_linetracedata.h"
#include "gl_load/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "gl_renderer.h"
#include "r_utility.h"
#include "v_video.h"
#include "g_levellocals.h" // pixelstretch
#include "math/cmath.h"
#include "c_cvars.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "LSMatrix.h"
#include "d_gui.h"
#include "d_event.h"
#include "doomstat.h"
#include "hw_models.h"
#include "hw_renderstate.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"

using namespace OpenGLRenderer;

EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Bool, cl_noprediction)
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Float, vr_vunits_per_meter)
EXTERN_CVAR(Float, vr_height_adjust)

EXTERN_CVAR(Int, vr_control_scheme)
EXTERN_CVAR(Bool, vr_move_use_offhand)
EXTERN_CVAR(Float, vr_weaponRotate);
EXTERN_CVAR(Float, vr_snapTurn);
EXTERN_CVAR(Float, vr_ipd);
EXTERN_CVAR(Float, vr_weaponScale);
EXTERN_CVAR(Bool, vr_teleport);
EXTERN_CVAR(Bool, vr_switch_sticks);
EXTERN_CVAR(Bool, vr_secondary_button_mappings);
EXTERN_CVAR(Bool, vr_two_handed_weapons);
EXTERN_CVAR(Bool, vr_crouch_use_button);
EXTERN_CVAR(Float, vr_2dweaponScale)
EXTERN_CVAR(Float, vr_2dweaponOffsetX);
EXTERN_CVAR(Float, vr_2dweaponOffsetY);
EXTERN_CVAR(Float, vr_2dweaponOffsetZ);

//HUD control
EXTERN_CVAR(Float, vr_hud_scale);
EXTERN_CVAR(Float, vr_hud_stereo);
EXTERN_CVAR(Float, vr_hud_distance);
EXTERN_CVAR(Float, vr_hud_rotate);
EXTERN_CVAR(Bool, vr_hud_fixed_pitch);
EXTERN_CVAR(Bool, vr_hud_fixed_roll);

//Automap  control
EXTERN_CVAR(Bool, vr_automap_use_hud);
EXTERN_CVAR(Float, vr_automap_scale);
EXTERN_CVAR(Float, vr_automap_stereo);
EXTERN_CVAR(Float, vr_automap_distance);
EXTERN_CVAR(Float, vr_automap_rotate);
EXTERN_CVAR(Bool,  vr_automap_fixed_pitch);
EXTERN_CVAR(Bool,  vr_automap_fixed_roll);


#include "QzDoom/mathlib.h"

extern vec3_t hmdPosition;
extern vec3_t hmdorientation;
extern vec3_t weaponoffset;
extern vec3_t weaponangles;
extern vec3_t offhandoffset;
extern vec3_t offhandangles;

extern float playerYaw;
extern float doomYaw;
extern float previousPitch;
extern float snapTurn;

extern bool ready_teleport;
extern bool trigger_teleport;
extern bool shutdown;
extern bool resetDoomYaw;
extern bool resetPreviousPitch;
extern bool cinemamode;
extern float cinemamodeYaw;
extern float cinemamodePitch;

bool TBXR_FrameSetup();
void TBXR_prepareEyeBuffer(int eye );
void TBXR_finishEyeBuffer(int eye );
void TBXR_submitFrame();

void QzDoom_setUseScreenLayer(bool use);
void QzDoom_Vibrate(float duration, int channel, float intensity);
void VR_GetMove( float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up, float *yaw, float *pitch, float *roll );
bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection);
void VR_HapticEnable();


double P_XYMovement(AActor *mo, DVector2 scroll);
void ST_Endoom();

extern bool		automapactive;	// in AM_map.c

float getHmdAdjustedHeightInMapUnit()
{
    double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
    return ((hmdPosition[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch;
}

//bit of a hack, assume player is at "normal" height when not crouching
static float getDoomPlayerHeightWithoutCrouch(const player_t *player)
{
    if (!vr_crouch_use_button)
    {
        return getHmdAdjustedHeightInMapUnit();
    }
    static float height = 0;
    if (height == 0)
    {
        height = player->DefaultViewHeight();
    }

    return height;
}

float getViewpointYaw()
{
    if (cinemamode)
    {
        return r_viewpoint.Angles.Yaw.Degrees();
    }

    return doomYaw;
}

namespace s3d
{
    static DVector3 oculusquest_origin(0, 0, 0);
    static float deltaYawDegrees;

    OpenXRDeviceEyePose::OpenXRDeviceEyePose(int eye)
            : VREyeInfo(0.0f, 1.f)
            , eye(eye)
    {
    }


/* virtual */
    OpenXRDeviceEyePose::~OpenXRDeviceEyePose()
    {
    }

/* virtual */
    DVector3 OpenXRDeviceEyePose::GetViewShift(FRenderViewpoint& vp) const
    {
        float outViewShift[3];
        outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

        vec3_t angles;
        VectorSet(angles, vp.HWAngles.Pitch.Degrees(),  getViewpointYaw(), vp.HWAngles.Roll.Degrees());

        vec3_t v_forward, v_right, v_up;
        AngleVectors(angles, v_forward, v_right, v_up);

        float stereo_separation = (vr_ipd * 0.5) * vr_vunits_per_meter * (eye == 0 ? -1.0 : 1.0);
        vec3_t tmp;
        VectorScale(v_right, stereo_separation, tmp);

        LSVec3 eyeOffset(tmp[0], tmp[1], tmp[2]);

        const player_t & player = players[consoleplayer];
        eyeOffset[2] += getHmdAdjustedHeightInMapUnit() - getDoomPlayerHeightWithoutCrouch(&player);

        outViewShift[0] = eyeOffset[0];
        outViewShift[1] = eyeOffset[1];
        outViewShift[2] = eyeOffset[2];

        return { outViewShift[0], outViewShift[1], outViewShift[2] };
    }

/* virtual */
    VSMatrix OpenXRDeviceEyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio, bool iso_ortho) const
    {
        float m[16];
        VR_GetVRProjection(eye, screen->GetZNear(), screen->GetZFar(), m);
        projection.loadMatrix(m);

        //projection = EyePose::GetProjection(fov, aspectRatio, fovRatio);
        return projection;
    }

    bool OpenXRDeviceEyePose::submitFrame() const
    {
        TBXR_prepareEyeBuffer(eye);

        GLRenderer->mBuffers->BindEyeTexture(eye, 0);
        IntRect box = {0, 0, screen->mSceneViewport.width, screen->mSceneViewport.height};
        GLRenderer->DrawPresentTexture(box, true);

        TBXR_finishEyeBuffer(eye);

        return true;
    }

    template<class TYPE>
    TYPE& getHUDValue(TYPE &automap, TYPE &hud)
    {
        return (automapactive && !vr_automap_use_hud) ? automap : hud;
    }

    VSMatrix OpenXRDeviceEyePose::getHUDProjection() const
    {
        VSMatrix new_projection;
        new_projection.loadIdentity();

        float stereo_separation = (vr_ipd * 0.5) * vr_vunits_per_meter * getHUDValue<FFloatCVarRef>(vr_automap_stereo, vr_hud_stereo) * (eye == 1 ? -1.0 : 1.0);
        new_projection.translate(stereo_separation, 0, 0);

        // doom_units from meters
        new_projection.scale(
                -vr_vunits_per_meter,
                vr_vunits_per_meter,
                -vr_vunits_per_meter);
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        new_projection.scale(1.0, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

        if (getHUDValue<FBoolCVarRef>(vr_automap_fixed_roll,vr_hud_fixed_roll))
        {
            new_projection.rotate(-hmdorientation[ROLL], 0, 0, 1);
        }

        new_projection.rotate(getHUDValue<FFloatCVarRef>(vr_automap_rotate, vr_hud_rotate), 1, 0, 0);

        if (getHUDValue<FBoolCVarRef>(vr_automap_fixed_pitch, vr_hud_fixed_pitch))
        {
            new_projection.rotate(-hmdorientation[PITCH], 1, 0, 0);
        }

        // hmd coordinates (meters) from ndc coordinates
        // const float weapon_distance_meters = 0.55f;
        // const float weapon_width_meters = 0.3f;
        double distance = getHUDValue<FFloatCVarRef>(vr_automap_distance, vr_hud_distance);
        new_projection.translate(0.0, 0.0, distance);
        double vr_scale = getHUDValue<FFloatCVarRef>(vr_automap_scale, vr_hud_scale);
        new_projection.scale(
                -vr_scale,
                vr_scale,
                -vr_scale);

        // ndc coordinates from pixel coordinates
        new_projection.translate(-1.0, 1.0, 0);
        new_projection.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);

        VSMatrix proj = projection;
        proj.multMatrix(new_projection);
        new_projection = proj;

        return new_projection;
    }

    void ApplyVPUniforms(HWDrawInfo* di)
    {
        auto& renderState = *screen->RenderState();
        di->VPUniforms.CalcDependencies();
        di->vpIndex = screen->mViewpoints->SetViewpoint(renderState, &di->VPUniforms);
    }

    void OpenXRDeviceEyePose::AdjustHud() const
    {
        // Draw crosshair on a separate quad, before updating HUD matrix
        const auto vrmode = VRMode::GetVRMode(true);
        if (vrmode->mEyeCount == 1)
        {
            return;
        }
        auto *di = HWDrawInfo::StartDrawInfo(r_viewpoint.ViewLevel, nullptr, r_viewpoint, nullptr);

        di->VPUniforms.mViewMatrix.loadIdentity();
        // Update HUD matrix to render on a separate quad
        di->VPUniforms.mProjectionMatrix = getHUDProjection();
        ApplyVPUniforms(di);
        di->EndDrawInfo();
    }

    void OpenXRDeviceEyePose::AdjustBlend(HWDrawInfo *di) const
    {
        bool new_di = false;
        if (di == nullptr)
        {
            di = HWDrawInfo::StartDrawInfo(r_viewpoint.ViewLevel, nullptr, r_viewpoint, nullptr);
            new_di = true;
        }

        VSMatrix& proj = di->VPUniforms.mProjectionMatrix;
        proj.loadIdentity();
        proj.translate(-1, 1, 0);
        proj.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
        ApplyVPUniforms(di);

        if (new_di)
        {
            di->EndDrawInfo();
        }
    }


/* static */
    const VRMode& OpenXRDeviceMode::getInstance()
    {
        static OpenXRDeviceEyePose vrmi_openvr_eyes[2] = { OpenXRDeviceEyePose(0), OpenXRDeviceEyePose(1) };
        static OpenXRDeviceMode instance(vrmi_openvr_eyes);
        return instance;
    }

    OpenXRDeviceMode::OpenXRDeviceMode(OpenXRDeviceEyePose eyes[2])
            : VRMode(2, 1.f, 1.f, 1.f, eyes)
            , isSetup(false)
            , sceneWidth(0), sceneHeight(0), cachedScreenBlocks(0)
    {
        //eye_ptrs.Push(&leftEyeView);
        //eye_ptrs.Push(&rightEyeView);

        leftEyeView = &eyes[0];
        rightEyeView = &eyes[1];
        mEyes[0] = &eyes[0];
        mEyes[1] = &eyes[1];

        //Get this from my code
        QzDoom_GetScreenRes(&sceneWidth, &sceneHeight);
    }

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
    void OpenXRDeviceMode::AdjustViewport(DFrameBuffer* screen) const
    {
        if (screen == nullptr)
            return;
        // Draw the 3D scene into the entire framebuffer
        screen->mSceneViewport.width = sceneWidth;
        screen->mSceneViewport.height = sceneHeight;
        screen->mSceneViewport.left = 0;
        screen->mSceneViewport.top = 0;

        screen->mScreenViewport.width = sceneWidth;
        screen->mScreenViewport.height = sceneHeight;
    }

    /* hand is 1 for left (offhand) and 0 for right (mainhand) */
    void OpenXRDeviceMode::AdjustPlayerSprites(FRenderState &state, int hand) const
    {
        if (GetWeaponTransform(&state.mModelMatrix, hand))
        {
            float scale = 0.000625f * vr_weaponScale * vr_2dweaponScale;
            state.mModelMatrix.scale(scale, -scale, scale);
            state.mModelMatrix.translate(-viewwidth / 2, -viewheight * 3 / 4, 0.0f); // What dis?!

            float offsetFactor = 40.f;
            state.mModelMatrix.translate(vr_2dweaponOffsetX * offsetFactor, -vr_2dweaponOffsetY * offsetFactor, vr_2dweaponOffsetZ * offsetFactor);
        }
        state.EnableModelMatrix(true);
    }

    void OpenXRDeviceMode::UnAdjustPlayerSprites(FRenderState &state) const {

        state.EnableModelMatrix(false);
    }

    //---------------------------------------------------------------------------
    //
    // This method has not been changed from its original implementation (before dualwield)
    // the hand parameter respect the old logic of 0 for left (offhand) and 1 for right (mainhand)
    //
    //---------------------------------------------------------------------------
    bool OpenXRDeviceMode::GetHandTransform(int hand, VSMatrix* mat) const
    {
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        if (player)
        {
            mat->loadIdentity();

            //We want to offset the weapon exactly from where we are seeing from
            mat->translate(r_viewpoint.CenterEyePos.X, r_viewpoint.CenterEyePos.Z - getDoomPlayerHeightWithoutCrouch(player), r_viewpoint.CenterEyePos.Y);

            mat->scale(vr_vunits_per_meter, vr_vunits_per_meter, -vr_vunits_per_meter);

            if ((vr_control_scheme < 10 && hand == 1)
                || (vr_control_scheme >= 10 && hand == 0)) {
                mat->translate(-weaponoffset[0], (hmdPosition[1] + weaponoffset[1] + vr_height_adjust) / pixelstretch, weaponoffset[2]);

                mat->scale(1, 1 / pixelstretch, 1);

                if (cinemamode)
                {
                    mat->rotate(-90 + r_viewpoint.Angles.Yaw.Degrees()  + (weaponangles[YAW]- playerYaw), 0, 1, 0);
                    mat->rotate(-weaponangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees(), 1, 0, 0);
                } else {
                    mat->rotate(-90 + doomYaw + (weaponangles[YAW]- hmdorientation[YAW]), 0, 1, 0);
                    mat->rotate(-weaponangles[PITCH], 1, 0, 0);
                }
                mat->rotate(-weaponangles[ROLL], 0, 0, 1);
            }
            else
            {
                mat->translate(-offhandoffset[0], (hmdPosition[1] + offhandoffset[1] + vr_height_adjust) / pixelstretch, offhandoffset[2]);

                mat->scale(1, 1 / pixelstretch, 1);

                if (cinemamode)
                {
                    mat->rotate(-90 + r_viewpoint.Angles.Yaw.Degrees()  + (offhandangles[YAW]- playerYaw), 0, 1, 0);
                    mat->rotate(-offhandangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees(), 1, 0, 0);
                } else {
                    mat->rotate(-90 + doomYaw + (offhandangles[YAW]- hmdorientation[YAW]), 0, 1, 0);
                    mat->rotate(-offhandangles[PITCH], 1, 0, 0);
                }
                mat->rotate(-offhandangles[ROLL], 0, 0, 1);
            }

            return true;

        }

        return false;
    }

/* virtual */
    void OpenXRDeviceMode::Present() const {

        if (!isSetup)
        {
            return;
        }

        leftEyeView->submitFrame();
        rightEyeView->submitFrame();

        TBXR_submitFrame();

        isSetup = false;
    }

    static int mAngleFromRadians(double radians)
    {
        double m = std::round(65535.0 * radians / (2.0 * M_PI));
        return int(m);
    }

    bool OpenXRDeviceMode::GetTeleportLocation(DVector3 &out) const
    {
        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        if (vr_teleport &&
            ready_teleport &&
            (player && player->mo->health > 0) &&
            m_TeleportTarget == TRACE_HitFloor) {
            out = m_TeleportLocation;
            return true;
        }

        return false;
    }

    /* virtual */
    void OpenXRDeviceMode::SetUp() const
    {
        super::SetUp();

        TBXR_FrameSetup();

        static bool enabled = false;
        if (!enabled)
        {
            enabled = true;
            VR_HapticEnable();
        }

        if (shutdown)
        {
            ST_Endoom();

            return;
        }

        if (gamestate == GS_LEVEL && menuactive == MENU_Off) {
            cachedScreenBlocks = screenblocks;
            screenblocks = 12;
            QzDoom_setUseScreenLayer(false);
        }
        else {
            //Ensure we are drawing on virtual screen
            QzDoom_setUseScreenLayer(true);
        }

        player_t* player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;

        //Some crazy stuff to ascertain the actual yaw that doom is using at the right times!
        if (gamestate != GS_LEVEL || menuactive != MENU_Off 
        || ConsoleState == c_down || ConsoleState == c_falling 
        || (player && player->playerstate == PST_DEAD)
        || (player && player->resetDoomYaw)
        || paused 
        )
        {
            resetDoomYaw = true;
        }
        else if (gamestate == GS_LEVEL && resetDoomYaw && r_viewpoint.camera != nullptr)
        {
            doomYaw = (float)r_viewpoint.camera->Angles.Yaw.Degrees();
            resetDoomYaw = false;
        }

        if (gamestate == GS_LEVEL && menuactive == MENU_Off)
        {
            if (player && player->mo)
            {
                double pixelstretch = level.info ? level.info->pixelstretch : 1.2;

                if (!vr_crouch_use_button)
                {
                    static double defaultViewHeight = player->DefaultViewHeight();
                    player->crouching = 10;
                    player->crouchfactor = getHmdAdjustedHeightInMapUnit() / defaultViewHeight;
                }
                else if (player->crouching == 10)
                {
                    player->Uncrouch();
                }

                //Weapon firing tracking - Thanks Fishbiter for the inspiration of how/where to use this!
                {
                    player->mo->AttackPitch = DAngle::fromDeg(cinemamode ? -weaponangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees()
                            : -weaponangles[PITCH]);

                    player->mo->AttackAngle = DAngle::fromDeg(-90 + getViewpointYaw() + (weaponangles[YAW]- playerYaw));
                    player->mo->AttackRoll = DAngle::fromDeg(weaponangles[ROLL]);

                    player->mo->AttackPos.X = player->mo->X() - (weaponoffset[0] * vr_vunits_per_meter);
                    player->mo->AttackPos.Y = player->mo->Y() - (weaponoffset[2] * vr_vunits_per_meter);
                    player->mo->AttackPos.Z = r_viewpoint.CenterEyePos.Z + (((hmdPosition[1] + weaponoffset[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch) -
                            getDoomPlayerHeightWithoutCrouch(player); // Fixes wrong shot height when in water
                }

                {
                    player->mo->OffhandPitch = DAngle::fromDeg(cinemamode ? -offhandangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees()
                            : -offhandangles[PITCH]);

                    player->mo->OffhandAngle = DAngle::fromDeg(-90 + getViewpointYaw() + (offhandangles[YAW]- playerYaw));
                    player->mo->OffhandRoll = DAngle::fromDeg(offhandangles[ROLL]);

                    player->mo->OffhandPos.X = player->mo->X() - (offhandoffset[0] * vr_vunits_per_meter);
                    player->mo->OffhandPos.Y = player->mo->Y() - (offhandoffset[2] * vr_vunits_per_meter);
                    player->mo->OffhandPos.Z = r_viewpoint.CenterEyePos.Z + (((hmdPosition[1] + offhandoffset[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch) -
                            getDoomPlayerHeightWithoutCrouch(player); // Fixes wrong shot height when in water
                }

                if (vr_teleport && player->mo->health > 0) {

                    DAngle yaw = DAngle::fromDeg(getViewpointYaw() - hmdorientation[YAW] + offhandangles[YAW]);
                    DAngle pitch = DAngle::fromDeg(offhandangles[PITCH]);
                    double pixelstretch = level.info ? level.info->pixelstretch : 1.2;

                    // Teleport Logic
                    if (ready_teleport) {
                        FLineTraceData trace;
                        if (P_LineTrace(player->mo, yaw, 8192, pitch, TRF_ABSOFFSET|TRF_BLOCKUSE|TRF_BLOCKSELF|TRF_SOLIDACTORS,
                                        ((hmdPosition[1] + offhandoffset[1] + vr_height_adjust) *
                                         vr_vunits_per_meter) / pixelstretch,
                                        -(offhandoffset[2] * vr_vunits_per_meter),
                                        -(offhandoffset[0] * vr_vunits_per_meter), &trace))
                        {
                            m_TeleportTarget = trace.HitType;
                            m_TeleportLocation = trace.HitLocation;
                        } else {
                            m_TeleportTarget = TRACE_HitNone;
                            m_TeleportLocation = DVector3(0, 0, 0);
                        }
                    }
                    else if (trigger_teleport && m_TeleportTarget == TRACE_HitFloor) {
                        auto vel = player->mo->Vel;
                        player->mo->Vel = DVector3(m_TeleportLocation.X - player->mo->X(),
                                                   m_TeleportLocation.Y - player->mo->Y(), 0);
                        bool wasOnGround = player->mo->Z() <= player->mo->floorz + 2;
                        double oldZ = player->mo->Z();
                        P_XYMovement(player->mo, DVector2(0, 0));

                        //if we were on the ground before offsetting, make sure we still are (this fixes not being able to move on lifts)
                        if (player->mo->Z() >= oldZ && wasOnGround) {
                            player->mo->SetZ(player->mo->floorz);
                        } else {
                            player->mo->SetZ(oldZ);
                        }
                        player->mo->Vel = vel;
                    }

                    trigger_teleport = false;
                }
#if 0  // this replace the move with offhand implementation in VrInputDefault.cpp
                LSMatrix44 mat;
                bool rightHanded = vr_control_scheme < 10;
                if (GetHandTransform(rightHanded ? 0 : 1, &mat) && vr_move_use_offhand)
                {
                    player->mo->ThrustAngleOffset = DAngle::fromDeg(RAD2DEG(atan2f(-mat[2][2], -mat[2][0]))) - player->mo->Angles.Yaw;
                }
                else
                {
                    player->mo->ThrustAngleOffset = nullAngle;
                }
#endif
                //Positional Movement
                float hmd_forward=0;
                float hmd_side=0;
                float dummy=0;
                VR_GetMove(&dummy, &dummy, &hmd_forward, &hmd_side, &dummy, &dummy, &dummy, &dummy);

                //Positional movement - Thanks fishbiter!!
                auto vel = player->mo->Vel;
                player->mo->Vel = DVector3((DVector2(hmd_side, hmd_forward) * vr_vunits_per_meter), 0);
                bool wasOnGround = player->mo->Z() <= player->mo->floorz + 2;
                double oldZ = player->mo->Z();
                P_XYMovement(player->mo, DVector2(0, 0));

                //if we were on the ground before offsetting, make sure we still are (this fixes not being able to move on lifts)
                if (player->mo->Z() >= oldZ && wasOnGround)
                {
                    player->mo->SetZ(player->mo->floorz);
                }
                else
                {
                    player->mo->SetZ(oldZ);
                }
                player->mo->Vel = vel;
            }
            updateHmdPose(r_viewpoint);
        }

        isSetup = true;
    }

    void OpenXRDeviceMode::updateHmdPose(FRenderViewpoint& vp) const
    {
        float dummy=0;
        float yaw=0;
        float pitch=0;
        float roll=0;

        // the yaw returned contains snapTurn input value
        VR_GetMove(&dummy, &dummy, &dummy, &dummy, &dummy, &yaw, &pitch, &roll);

        //Yaw
        double hmdYawDeltaDegrees;
        {
            static double previousHmdYaw = 0;
            static bool havePreviousYaw = false;
            if (!havePreviousYaw) {
                previousHmdYaw = yaw;
                havePreviousYaw = true;
            }
            hmdYawDeltaDegrees = yaw - previousHmdYaw;
            G_AddViewAngle(mAngleFromRadians(DEG2RAD(-hmdYawDeltaDegrees)));
            previousHmdYaw = yaw;
        }

        // Pitch
        {
            if (resetPreviousPitch)
            {
                previousPitch = vp.HWAngles.Pitch.Degrees();
                resetPreviousPitch = false;
            }

            double hmdPitchDeltaDegrees = pitch - previousPitch;

            //ALOGV("dPitch = %f", hmdPitchDeltaDegrees );

            G_AddViewPitch(mAngleFromRadians(DEG2RAD(-hmdPitchDeltaDegrees)));
            previousPitch = pitch;
        }

        if (!cinemamode)
        {
            if (gamestate == GS_LEVEL && menuactive == MENU_Off)
            {
                doomYaw += hmdYawDeltaDegrees;
                vp.HWAngles.Roll = FAngle::fromDeg(roll);
                vp.HWAngles.Pitch = FAngle::fromDeg(pitch);
            }

            {
                double viewYaw = doomYaw;
                while (viewYaw <= -180.0)
                    viewYaw += 360.0;
                while (viewYaw > 180.0)
                    viewYaw -= 360.0;
                vp.Angles.Yaw = DAngle::fromDeg(viewYaw);
            }
        }
    }

    void OpenXRDeviceMode::Vibrate(float duration, int channel, float intensity) const
    {
        QzDoom_Vibrate(duration, channel, intensity);
    }

/* virtual */
    void OpenXRDeviceMode::TearDown() const
    {
        if (gamestate == GS_LEVEL && cachedScreenBlocks != 0 && !menuactive) {
            screenblocks = cachedScreenBlocks;
        }
        super::TearDown();
    }

/* virtual */
    OpenXRDeviceMode::~OpenXRDeviceMode()
    {
    }

} /* namespace s3d */

CCMD (cinemamode)
{
    cinemamode = !cinemamode;

    //Store these
    cinemamodeYaw = hmdorientation[YAW] + snapTurn;
    cinemamodePitch = hmdorientation[PITCH];

    //Reset angles back to normal view
    if (!cinemamode)
    {
        resetDoomYaw = true;
        resetPreviousPitch = true;
    }
}

#endif  // USE_OPENXR
