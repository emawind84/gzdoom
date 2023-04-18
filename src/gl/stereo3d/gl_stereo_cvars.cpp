// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
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
** gl_stereo_cvars.cpp
** Console variables related to stereoscopic 3D in GZDoom
**
*/

#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/stereo3d/gl_stereo_leftright.h"
#include "gl/stereo3d/gl_anaglyph.h"
#include "gl/stereo3d/gl_quadstereo.h"
#include "gl/stereo3d/gl_sidebyside3d.h"
#include "gl/stereo3d/gl_interleaved3d.h"
#include "gl/stereo3d/gl_openxrdevice.h"
#include "gl/system/gl_cvars.h"
#include "menu/menu.h"
#include "version.h"

// Set up 3D-specific console variables:
CVAR(Int, vr_mode, 15, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)

// switch left and right eye views
CVAR(Bool, vr_swap_eyes, false, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)

// For broadest GL compatibility, require user to explicitly enable quad-buffered stereo mode.
// Setting vr_enable_quadbuffered_stereo does not automatically invoke quad-buffered stereo,
// but makes it possible for subsequent "vr_mode 7" to invoke quad-buffered stereo
CUSTOM_CVAR(Bool, vr_enable_quadbuffered, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
    //Does nothing
	//Printf("You must restart " GAMENAME " to switch quad stereo mode\n");
}

// intraocular distance in meters
CVAR(Float, vr_ipd, 0.064f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS

// distance between viewer and the display screen
CVAR(Float, vr_screendist, 0.80f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS

// default conversion between (vertical) DOOM units and meters
CVAR(Float, vr_vunits_per_meter, 34.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
CVAR(Float, vr_height_adjust, 0.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // METERS
CUSTOM_CVAR(Int, vr_control_scheme, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	M_ResetButtonStates();
}
CVAR(Bool, vr_move_use_offhand, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_teleport, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponRotate, -30, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponScale, 1.02f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponOffsetX, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponOffsetY, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_weaponOffsetZ, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_snapTurn, 45.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, vr_move_speed, 19, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_run_multiplier, 1.5, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_switch_sticks, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_secondary_button_mappings, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_two_handed_weapons, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_momentum, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // Only used in player.zs
CVAR(Bool, vr_crouch_use_button, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, use_action_spawn_yzoffset, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR(Float, vr_pickup_haptic_level, 0.2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_quake_haptic_level, 0.8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

//HUD control
CVAR(Float, vr_hud_scale, 0.25f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_stereo, 1.4f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_hud_rotate, 10.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_hud_fixed_pitch, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_hud_fixed_roll, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

//AutoMap control
CVAR(Bool, vr_automap_use_hud, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_scale, 0.4f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_stereo, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Float, vr_automap_rotate, 13.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_automap_fixed_pitch, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, vr_automap_fixed_roll, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


// Manage changing of 3D modes:
namespace s3d {

// Initialize static member
Stereo3DMode const * Stereo3DMode::currentStereo3DMode = 0; // "nullptr" not resolved on linux (presumably not C++11)

/* static */
void Stereo3DMode::setCurrentMode(const Stereo3DMode& mode) {
	Stereo3DMode::currentStereo3DMode = &mode;
}

/* static */
const Stereo3DMode& Stereo3DMode::getCurrentMode() 
{
	setCurrentMode(OpenXRDeviceMode::getInstance());
	return *currentStereo3DMode;
}

const Stereo3DMode& Stereo3DMode::getMonoMode()
{
	setCurrentMode(MonoView::getInstance());
	return *currentStereo3DMode;
}


} /* namespace s3d */

