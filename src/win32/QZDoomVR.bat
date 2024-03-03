REM This .bat file is for launching GZDoom in OpenVR virtual reality mode
REM Drag and Drop of .pk3 and PWAD files is supported
REM Change working folder to .bat location, so we can find gz3doom, even if a file from another folder is dragged onto this .bat
cd /d "%~dp0"
REM Place extra arguments first, since that seems to work better for brutal doom pk3 file.
qzdoom.exe %* ^
 +vid_vsync false ^
 +vr_mode 10 ^
 +turbo 80 ^
 +screenblocks 11 ^
 +crosshair 1 ^
 +crosshairscale 1 ^
 +use_joystick true ^
 +joy_xinput true ^
 +m_use_mouse false ^
 +smooth_mouse false ^
@REM  +vr_height_adjust=0.15 ^
@REM  +vr_hud_distance=0.9 ^
 +vr_hud_fixed_pitch=true ^
 +vr_hud_fixed_roll=false ^
@REM  +vr_hud_rotate=13 ^
@REM  +vr_hud_scale=0.35 ^
@REM  +vr_hud_stereo=0 ^
 +vr_automap_use_hud=true ^
 +vid_defheight=1400 ^
 +vid_defwidth=1400 ^
 +vid_scalemode=5 ^
 +dimamount=0.4 ^
 +gl_menu_blur=0.5