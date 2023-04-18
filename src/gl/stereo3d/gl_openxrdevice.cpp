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

#include "gl_openxrdevice.h"

#include <string>
#include <map>
#include <cmath>
#include "p_trace.h"
#include "p_linetracedata.h"
#include "gl/system/gl_system.h"
#include "doomtype.h" // Printf
#include "d_player.h"
#include "g_game.h" // G_Add...
#include "p_local.h" // P_TryMove
#include "r_utility.h" // viewpitch
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "g_levellocals.h" // pixelstretch
#include "math/cmath.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "d_gui.h"
#include "d_event.h"
#include "doomstat.h"
#include "c_console.h"

#include "LSMatrix.h"


EXTERN_CVAR(Bool, puristmode);
EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Float, movebob);
EXTERN_CVAR(Float, r_weapscale);
EXTERN_CVAR(Bool, cl_noprediction)
EXTERN_CVAR(Bool, gl_billboard_faces_camera);
EXTERN_CVAR(Int, gl_multisample);
EXTERN_CVAR(Float, vr_vunits_per_meter)
EXTERN_CVAR(Float, vr_height_adjust)

EXTERN_CVAR(Int, vr_control_scheme)
EXTERN_CVAR(Bool, vr_move_use_offhand)
EXTERN_CVAR(Float, vr_weaponRotate);
EXTERN_CVAR(Float, vr_weaponOffsetX);
EXTERN_CVAR(Float, vr_weaponOffsetY);
EXTERN_CVAR(Float, vr_weaponOffsetZ);
EXTERN_CVAR(Float, vr_snapTurn);
EXTERN_CVAR(Float, vr_ipd);
EXTERN_CVAR(Float, vr_weaponScale);
EXTERN_CVAR(Bool, vr_teleport);
EXTERN_CVAR(Bool, vr_switch_sticks);
EXTERN_CVAR(Bool, vr_secondary_button_mappings);
EXTERN_CVAR(Bool, vr_two_handed_weapons);
EXTERN_CVAR(Bool, vr_crouch_use_button);

//HUD control
EXTERN_CVAR(Float, vr_hud_scale);
EXTERN_CVAR(Float, vr_hud_stereo);
EXTERN_CVAR(Float, vr_hud_rotate);
EXTERN_CVAR(Bool, vr_hud_fixed_pitch);
EXTERN_CVAR(Bool, vr_hud_fixed_roll);

//Automap  control
EXTERN_CVAR(Bool, vr_automap_use_hud);
EXTERN_CVAR(Float, vr_automap_scale);
EXTERN_CVAR(Float, vr_automap_stereo);
EXTERN_CVAR(Float, vr_automap_rotate);
EXTERN_CVAR(Bool,  vr_automap_fixed_pitch);
EXTERN_CVAR(Bool,  vr_automap_fixed_roll);


#include "QzDoom/mathlib.h"

extern vec3_t hmdPosition;
extern vec3_t hmdorientation;
extern vec3_t weaponoffset;
extern vec3_t weaponangles;
extern vec3_t offhandoffset;
extern vec3_t offhandoffset;
extern vec3_t offhandangles;

extern float playerYaw;
extern float doomYaw;
extern bool cinemamode;

extern bool ready_teleport;
extern bool trigger_teleport;
extern bool shutdown;
extern bool resetDoomYaw;
extern bool resetPreviousPitch;
extern float previousPitch;

bool TBXR_FrameSetup();
void TBXR_prepareEyeBuffer(int eye );
void TBXR_finishEyeBuffer(int eye );
void TBXR_submitFrame();

void QzDoom_setUseScreenLayer(bool use);
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
float getDoomPlayerHeightWithoutCrouch(const player_t *player)
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
        return r_viewpoint.Angles.Yaw.Degrees;
    }

    return doomYaw;
}

namespace s3d
{
    static DVector3 oculusquest_origin(0, 0, 0);
    static float deltaYawDegrees;

    OpenXRDeviceEyePose::OpenXRDeviceEyePose(int eye)
            : ShiftedEyePose( 0.0f )
            , eye(eye)
    {
    }


/* virtual */
    OpenXRDeviceEyePose::~OpenXRDeviceEyePose()
    {
    }

/* virtual */
    void OpenXRDeviceEyePose::GetViewShift(FLOATTYPE yaw, FLOATTYPE outViewShift[3]) const
    {
        outViewShift[0] = outViewShift[1] = outViewShift[2] = 0;

        vec3_t angles;
        VectorSet(angles, GLRenderer->mAngles.Pitch.Degrees,  getViewpointYaw(), GLRenderer->mAngles.Roll.Degrees);

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
    }

/* virtual */
    VSMatrix OpenXRDeviceEyePose::GetProjection(FLOATTYPE fov, FLOATTYPE aspectRatio, FLOATTYPE fovRatio) const
    {
        float m[16];
        VR_GetVRProjection(eye, FGLRenderer::GetZNear(), FGLRenderer::GetZFar(), m);
        projection.loadMatrix(m);

        //projection = EyePose::GetProjection(fov, aspectRatio, fovRatio);
        return projection;
    }

    bool OpenXRDeviceEyePose::submitFrame() const
    {
        TBXR_prepareEyeBuffer(eye);

        GLRenderer->mBuffers->BindEyeTexture(eye, 0);
        GL_IRECT box = {0, 0, GLRenderer->mSceneViewport.width, GLRenderer->mSceneViewport.height};
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

        float stereo_separation = (vr_ipd * 0.5) * vr_vunits_per_meter * getHUDValue<FFloatCVar>(vr_automap_stereo, vr_hud_stereo) * (eye == 1 ? -1.0 : 1.0);
        new_projection.translate(stereo_separation, 0, 0);

        // doom_units from meters
        new_projection.scale(
                -vr_vunits_per_meter,
                vr_vunits_per_meter,
                -vr_vunits_per_meter);
        double pixelstretch = level.info ? level.info->pixelstretch : 1.2;
        new_projection.scale(1.0, pixelstretch, 1.0); // Doom universe is scaled by 1990s pixel aspect ratio

        if (getHUDValue<FBoolCVar>(vr_automap_fixed_roll,vr_hud_fixed_roll))
        {
            new_projection.rotate(-hmdorientation[ROLL], 0, 0, 1);
        }

        new_projection.rotate(getHUDValue<FFloatCVar>(vr_automap_rotate, vr_hud_rotate), 1, 0, 0);

        if (getHUDValue<FBoolCVar>(vr_automap_fixed_pitch, vr_hud_fixed_pitch))
        {
            new_projection.rotate(-hmdorientation[PITCH], 1, 0, 0);
        }

        // hmd coordinates (meters) from ndc coordinates
        // const float weapon_distance_meters = 0.55f;
        // const float weapon_width_meters = 0.3f;
        new_projection.translate(0.0, 0.0, 1.0);
        double vr_scale = getHUDValue<FFloatCVar>(vr_automap_scale, vr_hud_scale);
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

    void OpenXRDeviceEyePose::AdjustHud() const
    {
        const Stereo3DMode * mode3d = &Stereo3DMode::getCurrentMode();
        if (mode3d->IsMono())
            return;

        // Update HUD matrix to render on a separate quad
        gl_RenderState.mProjectionMatrix = getHUDProjection();
        gl_RenderState.ApplyMatrices();
    }

    void OpenXRDeviceEyePose::AdjustBlend() const
    {
        VSMatrix& proj = gl_RenderState.mProjectionMatrix;
        proj.loadIdentity();
        proj.translate(-1, 1, 0);
        proj.scale(2.0 / SCREENWIDTH, -2.0 / SCREENHEIGHT, -1.0);
        gl_RenderState.ApplyMatrices();
    }


/* static */
    const Stereo3DMode& OpenXRDeviceMode::getInstance()
    {
        static OpenXRDeviceMode instance;
        return instance;
    }

    OpenXRDeviceMode::OpenXRDeviceMode()
            : leftEyeView(0)
            , rightEyeView(1)
            , isSetup(false)
            , sceneWidth(0), sceneHeight(0), cachedScreenBlocks(0)
    {
        eye_ptrs.Push(&leftEyeView);
        eye_ptrs.Push(&rightEyeView);

        //Get this from my code
        QzDoom_GetScreenRes(&sceneWidth, &sceneHeight);
    }

/* virtual */
// AdjustViewports() is called from within FLGRenderer::SetOutputViewport(...)
    void OpenXRDeviceMode::AdjustViewports() const
    {
        // Draw the 3D scene into the entire framebuffer
        GLRenderer->mSceneViewport.width = sceneWidth;
        GLRenderer->mSceneViewport.height = sceneHeight;
        GLRenderer->mSceneViewport.left = 0;
        GLRenderer->mSceneViewport.top = 0;

        GLRenderer->mScreenViewport.width = sceneWidth;
        GLRenderer->mScreenViewport.height = sceneHeight;
    }

    void OpenXRDeviceMode::AdjustPlayerSprites(int hand) const
    {
        if (GetWeaponTransform(&gl_RenderState.mModelMatrix, hand))
        {
            float scale = 0.000625f * vr_weaponScale * r_weapscale;
            gl_RenderState.mModelMatrix.scale(scale, -scale, scale);
            gl_RenderState.mModelMatrix.translate(-viewwidth / 2, -viewheight * 3 / 4, 0.0f); // What dis?!
        }
        gl_RenderState.EnableModelMatrix(true);
    }

    void OpenXRDeviceMode::UnAdjustPlayerSprites() const {

        gl_RenderState.EnableModelMatrix(false);
    }

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
                    mat->rotate(-90 + r_viewpoint.Angles.Yaw.Degrees  + (weaponangles[YAW]- playerYaw), 0, 1, 0);
                    mat->rotate(-weaponangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees, 1, 0, 0);
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
                    mat->rotate(-90 + r_viewpoint.Angles.Yaw.Degrees  + (offhandangles[YAW]- playerYaw), 0, 1, 0);
                    mat->rotate(-offhandangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees, 1, 0, 0);
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

    bool OpenXRDeviceMode::GetWeaponTransform(VSMatrix* out, int hand_weapon) const
    {
        player_t * player = r_viewpoint.camera ? r_viewpoint.camera->player : nullptr;
        bool autoReverse = true;
        if (player)
        {
            AActor *weap = hand_weapon ? player->OffhandWeapon : player->ReadyWeapon;
            autoReverse = weap == nullptr || !(weap->IntVar(NAME_WeaponFlags) & WIF_NO_AUTO_REVERSE);
        }
        bool oculusquest_rightHanded = vr_control_scheme < 10;
        int hand = hand_weapon ? 1 - oculusquest_rightHanded : oculusquest_rightHanded;
        if (GetHandTransform(hand, out))
        {
            if (!hand && autoReverse)
                out->scale(-1.0f, 1.0f, 1.0f);
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

        leftEyeView.submitFrame();
        rightEyeView.submitFrame();

        TBXR_submitFrame();

        isSetup = false;
    }

    static int mAngleFromRadians(double radians)
    {
        double m = std::round(65535.0 * radians / (2.0 * M_PI));
        return int(m);
    }


    //Fishbiter's Function.. Thank-you!!
    static DVector3 MapWeaponDir(AActor* actor, DAngle yaw, DAngle pitch, int hand = 0)
    {
        LSMatrix44 mat;
        if (!s3d::Stereo3DMode::getCurrentMode().GetWeaponTransform(&mat, hand))
        {
            double pc = pitch.Cos();

            DVector3 direction = { pc * yaw.Cos(), pc * yaw.Sin(), -pitch.Sin() };
            return direction;
        }

        yaw -= actor->Angles.Yaw;

        //ignore specified pitch(would need to compensate for auto aimand no(vanilla) Doom weapon varies this)
		pitch -= actor->Angles.Pitch;
		//pitch.Degrees = 0;

        double pc = pitch.Cos();

        LSVec3 local = { (float)(pc * yaw.Cos()), (float)(pc * yaw.Sin()), (float)(-pitch.Sin()), 0.0f };

        DVector3 dir;
        dir.X = local.x * -mat[2][0] + local.y * -mat[0][0] + local.z * -mat[1][0];
        dir.Y = local.x * -mat[2][2] + local.y * -mat[0][2] + local.z * -mat[1][2];
        dir.Z = local.x * -mat[2][1] + local.y * -mat[0][1] + local.z * -mat[1][1];
        dir.MakeUnit();

        return dir;
    }

    static DVector3 MapAttackDir(AActor* actor, DAngle yaw, DAngle pitch)
    {
        return MapWeaponDir(actor, yaw, pitch, 0);
    }

    static DVector3 MapOffhandDir(AActor* actor, DAngle yaw, DAngle pitch)
    {
        return MapWeaponDir(actor, yaw, pitch, 1);
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

        if (gamestate == GS_LEVEL && getMenuState() == MENU_Off) {
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
        if (getGameState() != GS_LEVEL || getMenuState() != MENU_Off 
        || ConsoleState == c_down || ConsoleState == c_falling 
        || (player && player->playerstate == PST_DEAD)
        || (player && player->resetDoomYaw)
        || paused 
        )
        {
            resetDoomYaw = true;
        }
        else if (getGameState() == GS_LEVEL && resetDoomYaw) {
            doomYaw = (float)r_viewpoint.Angles.Yaw.Degrees;
            resetDoomYaw = false;
        }

        if (gamestate == GS_LEVEL && getMenuState() == MENU_Off)
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
                    player->mo->OverrideAttackPosDir = !puristmode;

                    player->mo->AttackPitch = cinemamode ? -weaponangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees
                            : -weaponangles[PITCH];

                    player->mo->AttackAngle = -90 + getViewpointYaw() + (weaponangles[YAW]- playerYaw);
                    player->mo->AttackRoll = weaponangles[ROLL];

                    player->mo->AttackPos.X = player->mo->X() - (weaponoffset[0] * vr_vunits_per_meter);
                    player->mo->AttackPos.Y = player->mo->Y() - (weaponoffset[2] * vr_vunits_per_meter);
                    player->mo->AttackPos.Z = r_viewpoint.CenterEyePos.Z + (((hmdPosition[1] + weaponoffset[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch) -
                            getDoomPlayerHeightWithoutCrouch(player); // Fixes wrong shot height when in water

                    player->mo->AttackDir = MapAttackDir;
                }

                {
                    player->mo->OffhandPitch = cinemamode ? -offhandangles[PITCH] - r_viewpoint.Angles.Pitch.Degrees
                            : -offhandangles[PITCH];

                    player->mo->OffhandAngle = -90 + getViewpointYaw() + (offhandangles[YAW]- playerYaw);
                    player->mo->OffhandRoll = offhandangles[ROLL];

                    player->mo->OffhandPos.X = player->mo->X() - (offhandoffset[0] * vr_vunits_per_meter);
                    player->mo->OffhandPos.Y = player->mo->Y() - (offhandoffset[2] * vr_vunits_per_meter);
                    player->mo->OffhandPos.Z = r_viewpoint.CenterEyePos.Z + (((hmdPosition[1] + offhandoffset[1] + vr_height_adjust) * vr_vunits_per_meter) / pixelstretch) -
                            getDoomPlayerHeightWithoutCrouch(player); // Fixes wrong shot height when in water

                    player->mo->OffhandDir = MapOffhandDir;
                }

                if (vr_teleport && player->mo->health > 0) {

                    DAngle yaw((doomYaw - hmdorientation[YAW]) + offhandangles[YAW]);
                    DAngle pitch(offhandangles[PITCH]);
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
            updateHmdPose();
        }

        isSetup = true;
    }

    void OpenXRDeviceMode::updateHmdPose() const
    {
        float dummy=0;
        float yaw=0;
        float pitch=0;
        float roll=0;
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
                previousPitch = GLRenderer->mAngles.Pitch.Degrees;
                resetPreviousPitch = false;
            }

            double hmdPitchDeltaDegrees = pitch - previousPitch;

            //ALOGV("dPitch = %f", hmdPitchDeltaDegrees );

            G_AddViewPitch(mAngleFromRadians(DEG2RAD(-hmdPitchDeltaDegrees)));
            previousPitch = pitch;
        }

        if (!cinemamode)
        {
            if (getGameState() == GS_LEVEL && getMenuState() == MENU_Off)
            {
                doomYaw += hmdYawDeltaDegrees;
                GLRenderer->mAngles.Roll = roll;
                GLRenderer->mAngles.Pitch = pitch;
            }

            {
                double viewYaw = doomYaw;
                while (viewYaw <= -180.0)
                    viewYaw += 360.0;
                while (viewYaw > 180.0)
                    viewYaw -= 360.0;
                r_viewpoint.Angles.Yaw.Degrees = viewYaw;
            }
        }
    }

/* virtual */
    void OpenXRDeviceMode::TearDown() const
    {
        if (getGameState() == GS_LEVEL && cachedScreenBlocks != 0 && !getMenuState()) {
            screenblocks = cachedScreenBlocks;
        }
        super::TearDown();
    }

/* virtual */
    OpenXRDeviceMode::~OpenXRDeviceMode()
    {
    }

} /* namespace s3d */


