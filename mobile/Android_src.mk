LOCAL_PATH := $(call my-dir)/../src


include $(CLEAR_VARS)

LOCAL_MODULE    := qzdoom

LOCAL_CFLAGS   :=  -D__MOBILE__ -DNO_PIX_BUFF -DOPNMIDI_DISABLE_GX_EMULATOR -DGZDOOM  -DLZDOOM -DNO_VBO -D__STDINT_LIMITS -DENGINE_NAME=\"lzdoom\"

LOCAL_CPPFLAGS := -DHAVE_FLUIDSYNTH -DHAVE_MPG123 -DHAVE_SNDFILE -std=c++14 -DHAVE_JWZGLES -Wno-switch -Wno-inconsistent-missing-override -Werror=format-security \
    -fexceptions -fpermissive -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -D__forceinline=inline -DNO_GTK -DNO_SSE -fsigned-char

LOCAL_CFLAGS  += -DNO_SEND_STATS

LOCAL_CFLAGS  += -DOPNMIDI_USE_LEGACY_EMULATOR
LOCAL_CFLAGS  += -DADLMIDI_DISABLE_MUS_SUPPORT -DADLMIDI_DISABLE_XMI_SUPPORT -DADLMIDI_DISABLE_MIDI_SEQUENCER

ifeq ($(BUILD_SERIAL),1)
LOCAL_CPPFLAGS += -DANTI_HACK 
endif

	
LOCAL_C_INCLUDES := \
 $(TOP_DIR)/ \
 ${TOP_DIR}/OpenXR-SDK/include \
 ${TOP_DIR}/OpenXR-SDK/src/common \
	 $(GZDOOM_TOP_PATH)/src/g_statusbar \
	$(GZDOOM_TOP_PATH)/src/g_shared \
	$(GZDOOM_TOP_PATH)/src/gamedata \
	$(GZDOOM_TOP_PATH)/src/gamedata/textures \
	$(GZDOOM_TOP_PATH)/src/gamedata/fonts \
	$(GZDOOM_TOP_PATH)/src/rendering \
	$(GZDOOM_TOP_PATH)/src/rendering/2d \
	$(GZDOOM_TOP_PATH)/src/sound \
	$(GZDOOM_TOP_PATH)/src/sound/music \
	$(GZDOOM_TOP_PATH)/src/sound/backend \
	$(GZDOOM_TOP_PATH)/src/xlat \
	$(GZDOOM_TOP_PATH)/src/utility \
	$(GZDOOM_TOP_PATH)/src/utility/nodebuilder \
	$(GZDOOM_TOP_PATH)/src/scripting \
	$(GZDOOM_TOP_PATH)/src/scripting/vm \
	$(GZDOOM_TOP_PATH)/src/rendering/vulkan/thirdparty \
	$(GZDOOM_TOP_PATH)/src/../libraries/gdtoa \
    $(GZDOOM_TOP_PATH)/src/../libraries/bzip2 \
	$(GZDOOM_TOP_PATH)/src/../libraries/game-music-emu/ \
	$(GZDOOM_TOP_PATH)/src/../libraries/dumb/include \
	$(GZDOOM_TOP_PATH)/src/../libraries/glslang/glslang/Public \
	$(GZDOOM_TOP_PATH)/src/../libraries/glslang/spirv \
	$(GZDOOM_TOP_PATH)/src/../libraries/lzma/C \
	$(GZDOOM_TOP_PATH)/src/../libraries/zmusic \
	$(GZDOOM_TOP_PATH)/src/posix \
	$(GZDOOM_TOP_PATH)/src/posix/nosdl \
 $(SUPPORT_LIBS)/fluidsynth-lite/include \
 $(SUPPORT_LIBS)/openal/include/AL \
 $(SUPPORT_LIBS)/libsndfile-android/jni/ \
 $(SUPPORT_LIBS)/libmpg123 \
 $(SUPPORT_LIBS)/jpeg8d \
 $(GZDOOM_TOP_PATH)/mobile/src/extrafiles  \
 $(GZDOOM_TOP_PATH)/mobile/src


#############################################################################
# CLIENT/SERVER
#############################################################################


ANDROID_SRC_FILES = \
    ../mobile/src/i_specialpaths_android.cpp

PLAT_POSIX_SOURCES = \
	posix/i_steam.cpp \
	posix/i_system_posix.cpp

PLAT_NOSDL_SOURCES = \
	posix/nosdl/crashcatcher.c \
	posix/nosdl/hardware.cpp \
	posix/nosdl/i_gui.cpp \
	posix/nosdl/i_joystick.cpp \
	posix/nosdl/i_system.cpp \
	posix/nosdl/i_input.cpp \
	posix/nosdl/glvideo.cpp \
	posix/nosdl/st_start.cpp


FASTMATH_SOURCES = \
	rendering/swrenderer/r_all.cpp \
	rendering/swrenderer/r_swscene.cpp \
	rendering/polyrenderer/poly_all.cpp \
	sound/music/music_midi_base.cpp \
	sound/backend/oalsound.cpp \
	gamedata/textures/hires/hqnx/init.cpp \
	gamedata/textures/hires/hqnx/hq2x.cpp \
	gamedata/textures/hires/hqnx/hq3x.cpp \
	gamedata/textures/hires/hqnx/hq4x.cpp \
	gamedata/textures/hires/xbr/xbrz.cpp \
	gamedata/textures/hires/xbr/xbrz_old.cpp \
	rendering/gl_load/gl_load.c \
	rendering/hwrenderer/dynlights/hw_dynlightdata.cpp \
	rendering/hwrenderer/scene/hw_bsp.cpp \
	rendering/hwrenderer/scene/hw_fakeflat.cpp \
	rendering/hwrenderer/scene/hw_decal.cpp \
	rendering/hwrenderer/scene/hw_drawinfo.cpp \
	rendering/hwrenderer/scene/hw_drawlist.cpp \
	rendering/hwrenderer/scene/hw_clipper.cpp \
	rendering/hwrenderer/scene/hw_flats.cpp \
	rendering/hwrenderer/scene/hw_portal.cpp \
	rendering/hwrenderer/scene/hw_renderhacks.cpp \
	rendering/hwrenderer/scene/hw_sky.cpp \
	rendering/hwrenderer/scene/hw_skyportal.cpp \
	rendering/hwrenderer/scene/hw_sprites.cpp \
	rendering/hwrenderer/scene/hw_spritelight.cpp \
	rendering/hwrenderer/scene/hw_walls.cpp \
	rendering/hwrenderer/scene/hw_walls_vertex.cpp \
	rendering/hwrenderer/scene/hw_weapon.cpp \
	r_data/models/models.cpp \
	r_data/matrix.cpp \



PCH_SOURCES = \
	am_map.cpp \
	b_bot.cpp \
	b_func.cpp \
	b_game.cpp \
	b_move.cpp \
	b_think.cpp \
	bbannouncer.cpp \
	c_bind.cpp \
	c_cmds.cpp \
	c_console.cpp \
	c_consolebuffer.cpp \
	c_cvars.cpp \
	c_dispatch.cpp \
	c_expr.cpp \
	c_functions.cpp \
	ct_chat.cpp \
	d_iwad.cpp \
	d_main.cpp \
	d_anonstats.cpp \
	d_net.cpp \
	d_netinfo.cpp \
	d_protocol.cpp \
	dobject.cpp \
	dobjgc.cpp \
	dobjtype.cpp \
	doomstat.cpp \
	g_cvars.cpp \
	g_dumpinfo.cpp \
	g_game.cpp \
	g_hub.cpp \
	g_level.cpp \
	gameconfigfile.cpp \
	gitinfo.cpp \
	hu_scores.cpp \
	i_net.cpp \
	m_cheat.cpp \
	m_joy.cpp \
	m_misc.cpp \
	p_acs.cpp \
	p_actionfunctions.cpp \
	p_conversation.cpp \
	p_destructible.cpp \
	p_effect.cpp \
	p_enemy.cpp \
	p_interaction.cpp \
	p_lnspec.cpp \
	p_map.cpp \
	p_maputl.cpp \
	p_mobj.cpp \
	p_openmap.cpp \
	p_pspr.cpp \
	p_saveg.cpp \
	p_setup.cpp \
	p_spec.cpp \
	p_states.cpp \
	p_things.cpp \
	p_tick.cpp \
	p_user.cpp \
	r_utility.cpp \
	r_sky.cpp \
	r_videoscale.cpp \
	sound/s_advsound.cpp \
	sound/s_environment.cpp \
	sound/s_reverbedit.cpp \
	sound/s_sndseq.cpp \
	sound/s_doomsound.cpp \
	sound/s_sound.cpp \
	sound/s_music.cpp \
	serializer.cpp \
	scriptutil.cpp \
	st_stuff.cpp \
	v_framebuffer.cpp \
	v_palette.cpp \
	v_video.cpp \
	wi_stuff.cpp \
	gamedata/a_keys.cpp \
	gamedata/a_weapons.cpp \
	gamedata/decallib.cpp \
	gamedata/g_mapinfo.cpp \
	gamedata/g_skill.cpp \
	gamedata/gi.cpp \
	gamedata/stringtable.cpp \
	gamedata/umapinfo.cpp \
	gamedata/w_wad.cpp \
	gamedata/d_dehacked.cpp \
	gamedata/g_doomedmap.cpp \
	gamedata/info.cpp \
	gamedata/keysections.cpp \
	gamedata/p_terrain.cpp \
	gamedata/statistics.cpp \
	gamedata/teaminfo.cpp \
	g_shared/a_pickups.cpp \
	g_shared/a_action.cpp \
	g_shared/a_decals.cpp \
	g_shared/a_decalfx.cpp \
	g_shared/a_doors.cpp \
	g_shared/a_dynlight.cpp \
	g_shared/a_flashfader.cpp \
	g_shared/a_lightning.cpp \
	g_shared/a_morph.cpp \
	g_shared/a_quake.cpp \
	g_shared/a_specialspot.cpp \
	g_shared/a_ceiling.cpp \
	g_shared/a_floor.cpp \
	g_shared/a_lights.cpp \
	g_shared/a_lighttransfer.cpp \
	g_shared/a_pillar.cpp \
	g_shared/a_plats.cpp \
	g_shared/a_pusher.cpp \
	g_shared/a_scroll.cpp \
	g_shared/dsectoreffect.cpp \
	g_shared/p_secnodes.cpp \
	g_shared/p_sectors.cpp \
	g_shared/p_sight.cpp \
	g_shared/p_switch.cpp \
	g_shared/p_tags.cpp \
	g_shared/p_teleport.cpp \
	g_shared/actorptrselect.cpp \
	g_shared/dthinker.cpp \
	g_shared/p_3dfloors.cpp \
	g_shared/p_3dmidtex.cpp \
	g_shared/p_linkedsectors.cpp \
	g_shared/p_trace.cpp \
	g_shared/po_man.cpp \
	g_shared/portal.cpp \
	g_statusbar/hudmessages.cpp \
	g_statusbar/shared_hud.cpp \
	g_statusbar/sbarinfo.cpp \
	g_statusbar/sbar_mugshot.cpp \
	g_statusbar/shared_sbar.cpp \
	rendering/2d/f_wipe.cpp \
	rendering/2d/v_2ddrawer.cpp \
	rendering/2d/v_drawtext.cpp \
	rendering/2d/v_blend.cpp \
	rendering/2d/v_draw.cpp \
	rendering/gl/renderer/gl_renderer.cpp \
	rendering/gl/renderer/gl_renderstate.cpp \
	rendering/gl/renderer/gl_renderbuffers.cpp \
	rendering/gl/renderer/gl_postprocess.cpp \
	rendering/gl/renderer/gl_postprocessstate.cpp \
	rendering/gl/renderer/gl_stereo3d.cpp \
	rendering/gl/renderer/gl_scene.cpp \
	rendering/gl/shaders/gl_shader.cpp \
	rendering/gl/shaders/gl_shaderprogram.cpp \
	gl/stereo3d/gl_openxrdevice.cpp \
	rendering/gl_load/gl_interface.cpp \
	rendering/gl/system/gl_framebuffer.cpp \
	rendering/gl/system/gl_debug.cpp \
	rendering/gl/system/gl_buffers.cpp \
	rendering/gl/textures/gl_hwtexture.cpp \
	rendering/gl/textures/gl_samplers.cpp \
	rendering/hwrenderer/data/hw_vertexbuilder.cpp \
	rendering/hwrenderer/data/flatvertices.cpp \
	rendering/hwrenderer/data/hw_viewpointbuffer.cpp \
	rendering/hwrenderer/dynlights/hw_aabbtree.cpp \
	rendering/hwrenderer/dynlights/hw_shadowmap.cpp \
	rendering/hwrenderer/dynlights/hw_lightbuffer.cpp \
	rendering/hwrenderer/models/hw_models.cpp \
	rendering/hwrenderer/scene/hw_skydome.cpp \
	rendering/hwrenderer/scene/hw_drawlistadd.cpp \
	rendering/hwrenderer/scene/hw_renderstate.cpp \
	rendering/hwrenderer/postprocessing/hw_postprocess.cpp \
	rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp \
	rendering/hwrenderer/postprocessing/hw_postprocessshader.cpp \
	rendering/hwrenderer/textures/hw_material.cpp \
	rendering/hwrenderer/textures/hw_precache.cpp \
	rendering/hwrenderer/utility/hw_clock.cpp \
	rendering/hwrenderer/utility/hw_cvars.cpp \
	rendering/hwrenderer/utility/hw_draw2d.cpp \
	rendering/hwrenderer/utility/hw_lighting.cpp \
	rendering/hwrenderer/utility/hw_shaderpatcher.cpp \
	rendering/hwrenderer/utility/hw_vrmodes.cpp \
	maploader/edata.cpp \
	maploader/specials.cpp \
	maploader/maploader.cpp \
	maploader/slopes.cpp \
	maploader/glnodes.cpp \
	maploader/udmf.cpp \
	maploader/usdf.cpp \
	maploader/strifedialogue.cpp \
	maploader/polyobjects.cpp \
	maploader/renderinfo.cpp \
	maploader/compatibility.cpp \
	menu/joystickmenu.cpp \
	menu/loadsavemenu.cpp \
	menu/menu.cpp \
	menu/menudef.cpp \
	menu/messagebox.cpp \
	menu/optionmenu.cpp \
	menu/playermenu.cpp \
	menu/resolutionmenu.cpp \
	menu/profiledef.cpp \
	gamedata/resourcefiles/ancientzip.cpp \
	gamedata/resourcefiles/file_7z.cpp \
	gamedata/resourcefiles/file_grp.cpp \
	gamedata/resourcefiles/file_lump.cpp \
	gamedata/resourcefiles/file_rff.cpp \
	gamedata/resourcefiles/file_wad.cpp \
	gamedata/resourcefiles/file_zip.cpp \
	gamedata/resourcefiles/file_pak.cpp \
	gamedata/resourcefiles/file_directory.cpp \
	gamedata/resourcefiles/resourcefile.cpp \
	gamedata/textures/animations.cpp \
	gamedata/textures/anim_switches.cpp \
	gamedata/textures/bitmap.cpp \
	gamedata/textures/texture.cpp \
	gamedata/textures/image.cpp \
	gamedata/textures/imagetexture.cpp \
	gamedata/textures/texturemanager.cpp \
	gamedata/textures/multipatchtexturebuilder.cpp \
	gamedata/textures/skyboxtexture.cpp \
	gamedata/textures/formats/automaptexture.cpp \
	gamedata/textures/formats/brightmaptexture.cpp \
	gamedata/textures/formats/buildtexture.cpp \
	gamedata/textures/formats/canvastexture.cpp \
	gamedata/textures/formats/ddstexture.cpp \
	gamedata/textures/formats/flattexture.cpp \
	gamedata/textures/formats/fontchars.cpp \
	gamedata/textures/formats/imgztexture.cpp \
	gamedata/textures/formats/jpegtexture.cpp \
	gamedata/textures/formats/md5check.cpp \
	gamedata/textures/formats/multipatchtexture.cpp \
	gamedata/textures/formats/patchtexture.cpp \
	gamedata/textures/formats/pcxtexture.cpp \
	gamedata/textures/formats/pngtexture.cpp \
	gamedata/textures/formats/rawpagetexture.cpp \
	gamedata/textures/formats/emptytexture.cpp \
	gamedata/textures/formats/shadertexture.cpp \
	gamedata/textures/formats/tgatexture.cpp \
	gamedata/textures/hires/hqresize.cpp \
	gamedata/textures/hires/hirestex.cpp \
	gamedata/fonts/singlelumpfont.cpp \
	gamedata/fonts/singlepicfont.cpp \
	gamedata/fonts/specialfont.cpp \
	gamedata/fonts/font.cpp \
	gamedata/fonts/hexfont.cpp \
	gamedata/fonts/v_font.cpp \
	gamedata/fonts/v_text.cpp \
	gamedata/p_xlat.cpp \
	gamedata/xlat/parse_xlat.cpp \
	gamedata/xlat/parsecontext.cpp \
	fragglescript/t_func.cpp \
	fragglescript/t_load.cpp \
	fragglescript/t_oper.cpp \
	fragglescript/t_parse.cpp \
	fragglescript/t_prepro.cpp \
	fragglescript/t_script.cpp \
	fragglescript/t_spec.cpp \
	fragglescript/t_variable.cpp \
	fragglescript/t_cmd.cpp \
	intermission/intermission.cpp \
	intermission/intermission_parse.cpp \
	r_data/colormaps.cpp \
	r_data/cycler.cpp \
	r_data/gldefs.cpp \
	r_data/a_dynlightdata.cpp \
	r_data/r_translate.cpp \
	r_data/sprites.cpp \
	r_data/portalgroups.cpp \
	r_data/voxels.cpp \
	r_data/renderstyle.cpp \
	r_data/r_canvastexture.cpp \
	r_data/r_interpolate.cpp \
	r_data/r_vanillatrans.cpp \
	r_data/r_sections.cpp \
	r_data/models/models_md3.cpp \
	r_data/models/models_md2.cpp \
	r_data/models/models_voxel.cpp \
	r_data/models/models_ue1.cpp \
	r_data/models/models_obj.cpp \
	scripting/symbols.cpp \
	scripting/vmiterators.cpp \
	scripting/vmthunks.cpp \
	scripting/vmthunks_actors.cpp \
	scripting/types.cpp \
	scripting/thingdef.cpp \
	scripting/thingdef_data.cpp \
	scripting/thingdef_properties.cpp \
	scripting/backend/codegen.cpp \
	scripting/backend/scopebarrier.cpp \
	scripting/backend/dynarrays.cpp \
	scripting/backend/vmbuilder.cpp \
	scripting/backend/vmdisasm.cpp \
	scripting/decorate/olddecorations.cpp \
	scripting/decorate/thingdef_exp.cpp \
	scripting/decorate/thingdef_parse.cpp \
	scripting/decorate/thingdef_states.cpp \
	scripting/vm/vmexec.cpp \
	scripting/vm/vmframe.cpp \
	scripting/zscript/ast.cpp \
	scripting/zscript/zcc_compile.cpp \
	scripting/zscript/zcc_parser.cpp \
	utility/sfmt/SFMT.cpp \
	sound/music/i_music.cpp \
	sound/music/i_soundfont.cpp \
	sound/backend/i_sound.cpp \
	sound/music/music_config.cpp \
	rendering/swrenderer/textures/r_swtexture.cpp \
	rendering/swrenderer/textures/warptexture.cpp \
	rendering/swrenderer/textures/swcanvastexture.cpp \
	events.cpp \
	utility/files.cpp \
	utility/files_decompress.cpp \
	utility/m_png.cpp \
	utility/m_random.cpp \
	utility/memarena.cpp \
	utility/md5.cpp \
	utility/nodebuilder/nodebuild.cpp \
	utility/nodebuilder/nodebuild_classify_nosse2.cpp \
	utility/nodebuilder/nodebuild_events.cpp \
	utility/nodebuilder/nodebuild_extract.cpp \
	utility/nodebuilder/nodebuild_gl.cpp \
	utility/nodebuilder/nodebuild_utility.cpp \
	utility/sc_man.cpp \
	utility/stats.cpp \
	utility/cmdlib.cpp \
	utility/colormatcher.cpp \
	utility/configfile.cpp \
	utility/i_module.cpp \
	utility/i_time.cpp \
	utility/m_alloc.cpp \
	utility/m_argv.cpp \
	utility/m_bbox.cpp \
	utility/name.cpp \
	utility/s_playlist.cpp \
	utility/v_collection.cpp \
	utility/utf8.cpp \
	utility/zstrformat.cpp \
	

QZDOOM_SRC = \
   ../../QzDoom/TBXR_Common.cpp \
   ../../QzDoom/QzDoom_OpenXR.cpp \
   ../../QzDoom/OpenXrInput.cpp \
   ../../QzDoom/VrInputCommon.cpp \
   ../../QzDoom/VrInputDefault.cpp \
   ../../QzDoom/mathlib.c \
   ../../QzDoom/matrixlib.c \
   ../../QzDoom/argtable3.c


LOCAL_SRC_FILES = \
	__autostart.cpp \
	$(QZDOOM_SRC) \
	$(ANDROID_SRC_FILES) \
	$(PLAT_POSIX_SOURCES) \
	$(PLAT_NOSDL_SOURCES) \
	$(FASTMATH_SOURCES) \
	$(PCH_SOURCES) \
	utility/x86.cpp \
	utility/strnatcmp.c \
	utility/zstring.cpp \
	dictionary.cpp \
	utility/math/asin.c \
	utility/math/atan.c \
	utility/math/const.c \
	utility/math/cosh.c \
	utility/math/exp.c \
	utility/math/isnan.c \
	utility/math/log.c \
	utility/math/log10.c \
	utility/math/mtherr.c \
	utility/math/polevl.c \
	utility/math/pow.c \
	utility/math/powi.c \
	utility/math/sin.c \
	utility/math/sinh.c \
	utility/math/sqrt.c \
	utility/math/tan.c \
	utility/math/tanh.c \
	utility/math/fastsin.cpp \
	zzautozend.cpp \


# Turn down optimisation of this file so clang doesnt produce ldrd instructions which are missaligned
p_acs.cpp_CFLAGS := -O1

LOCAL_LDLIBS := -ldl -llog -lOpenSLES -landroid
LOCAL_LDLIBS += -lGLESv3

LOCAL_LDLIBS +=  -lEGL

# This is stop a linker warning for mp123 lib failing build
#LOCAL_LDLIBS += -Wl,--no-warn-shared-textrel

LOCAL_STATIC_LIBRARIES :=  sndfile mpg123 fluidsynth-static libjpeg zlib_lz lzma_lz gdtoa_lz dumb_lz gme_lz bzip2_lz zmusic_lz
LOCAL_SHARED_LIBRARIES :=  openal openxr_loader

LOCAL_STATIC_LIBRARIES +=

include $(BUILD_SHARED_LIBRARY)

$(call import-module,AndroidPrebuilt/jni)


