#pragma once


#include "c_cvars.h"

EXTERN_CVAR(Bool,gl_enhanced_nightvision)
EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Int, gl_texture_filter)
EXTERN_CVAR(Float, gl_texture_filter_anisotropic)
EXTERN_CVAR(Int, gl_texture_format)
EXTERN_CVAR(Bool, gl_usefb)

EXTERN_CVAR(Int, gl_weaponlight)

EXTERN_CVAR (Bool, gl_light_sprites);
EXTERN_CVAR (Bool, gl_light_particles);
EXTERN_CVAR (Bool, gl_light_weapons);
EXTERN_CVAR (Bool, gl_light_shadowmap);
EXTERN_CVAR (Int, gl_shadowmap_quality);
EXTERN_CVAR (Int, gl_storage_buffer_type);
EXTERN_CVAR (Float, gl_light_distance_cull);
EXTERN_CVAR (Int, gl_light_flat_max_lights);
EXTERN_CVAR (Int, gl_light_wall_max_lights);
EXTERN_CVAR (Int, gl_light_range_limit);

EXTERN_CVAR(Bool, gl_global_fade);
EXTERN_CVAR(Bool, gl_global_fade_debug);
EXTERN_CVAR(Color, gl_global_fade_color);
EXTERN_CVAR(Float, gl_global_fade_gradient);
EXTERN_CVAR(Float, gl_global_fade_density);

EXTERN_CVAR(Int, gl_fogmode)
EXTERN_CVAR(Bool,gl_mirror_envmap)

EXTERN_CVAR(Bool,gl_mirrors)
EXTERN_CVAR(Bool,gl_mirror_envmap)
EXTERN_CVAR(Bool,gl_mirror_player)
EXTERN_CVAR(Bool, gl_seamless)

EXTERN_CVAR(Float, gl_mask_threshold)
EXTERN_CVAR(Float, gl_mask_sprite_threshold)

EXTERN_CVAR(Int, gl_multisample)

EXTERN_CVAR(Bool, gl_bloom)
EXTERN_CVAR(Float, gl_bloom_amount)
EXTERN_CVAR(Int, gl_bloom_kernel_size)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Float, gl_exposure)
EXTERN_CVAR(Bool, gl_lens)
EXTERN_CVAR(Float, gl_lens_k)
EXTERN_CVAR(Float, gl_lens_kcube)
EXTERN_CVAR(Float, gl_lens_chromatic)
EXTERN_CVAR(Int, gl_ssao)
EXTERN_CVAR(Int, gl_ssao_portals)
EXTERN_CVAR(Float, gl_ssao_strength)
EXTERN_CVAR(Int, gl_ssao_debug)
EXTERN_CVAR(Float, gl_ssao_bias)
EXTERN_CVAR(Float, gl_ssao_radius)
EXTERN_CVAR(Float, gl_ssao_blur_amount)

EXTERN_CVAR(Int, gl_debug_level)
EXTERN_CVAR(Bool, gl_debug_breakpoint)

EXTERN_CVAR(Int, gl_shadowmap_filter)

EXTERN_CVAR(Bool, gl_brightfog)
EXTERN_CVAR(Bool, gl_lightadditivesurfaces)
EXTERN_CVAR(Bool, gl_notexturefill)

EXTERN_CVAR(Bool, gl_no_persistent_buffer)
EXTERN_CVAR(Bool, gl_no_clip_planes)
EXTERN_CVAR(Bool, gl_no_ssbo)

EXTERN_CVAR(Bool, r_radarclipper)
EXTERN_CVAR(Bool, r_dithertransparency)
