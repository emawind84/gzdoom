#include "doomtype.h"
#include "VrCommon.h"

EXTERN_CVAR(Float, fov)
EXTERN_CVAR(Bool, vr_snap_turning);
EXTERN_CVAR(Int, vr_overlayscreen);
EXTERN_CVAR(Bool, vr_overlayscreen_always);

//Define all variables here that were externs in the VrCommon.h
vec3_t weaponangles;
vec3_t weaponoffset;
vec3_t offhandangles;
vec3_t offhandoffset;
vec3_t worldPosition;
vec3_t hmdPosition;
vec3_t hmdorientation;
vec3_t positionDeltaThisFrame;

bool weaponStabilised;
bool resetDoomYaw;
bool resetPreviousPitch;
// bool shutdown;
bool ready_teleport;
bool trigger_teleport;
bool cinemamode;

float playerYaw;
float doomYaw;
float previousPitch;
float snapTurn;
float cinemamodeYaw;
float cinemamodePitch;
float remote_movementSideways;
float remote_movementForward;
float positional_movementSideways;
float positional_movementForward;

//This is now controlled by the engine
static bool useVirtualScreen = false;

/*
================================================================================

QuestZDoom Stuff

================================================================================
*/

int QzDoom_SetRefreshRate(int refreshRate)
{
    return 0;
}

void QzDoom_GetScreenRes(uint32_t *width, uint32_t *height)
{
}

float QzDoom_GetFOV()
{
	return fov;
}

void VR_HapticEvent(const char* event, int position, int intensity, float angle, float yHeight )
{
}

void QzDoom_Restart()
{
}

void QzDoom_setUseScreenLayer(bool use)
{
	useVirtualScreen = use;
}

bool VR_UseScreenLayer()
{
	return vr_overlayscreen && (useVirtualScreen || cinemamode || vr_overlayscreen_always);
}

void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	VectorSet(hmdorientation, pitch, yaw, roll);

	if (!VR_UseScreenLayer())
	{
		playerYaw = yaw;
	}
}

void VR_SetHMDPosition(float x, float y, float z )
{
 	VectorSet(hmdPosition, x, y, z);

	positionDeltaThisFrame[0] = (worldPosition[0] - x);
	positionDeltaThisFrame[1] = (worldPosition[1] - y);
	positionDeltaThisFrame[2] = (worldPosition[2] - z);

	worldPosition[0] = x;
	worldPosition[1] = y;
	worldPosition[2] = z;
}

static float DEG2RAD(float deg)
{
	return deg * float(M_PI / 180.0);
}

void VR_GetMove(float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up,
				float *yaw, float *pitch, float *roll)
{
    *joy_forward = remote_movementForward;
    *joy_side = remote_movementSideways;
    *hmd_forward = positional_movementForward;
    *hmd_side = positional_movementSideways;
    *up = 0.0f;
    *yaw = VR_UseScreenLayer() ? cinemamodeYaw : hmdorientation[YAW] + (vr_snap_turning ? snapTurn : 0.);
	*pitch = VR_UseScreenLayer() ? cinemamodePitch : hmdorientation[PITCH];
	*roll = VR_UseScreenLayer() ? 0.0f : hmdorientation[ROLL];
}

