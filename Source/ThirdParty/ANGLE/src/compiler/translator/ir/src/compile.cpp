//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ir/src/compile.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/PoolAlloc.h"

namespace sh
{
namespace ir
{
namespace
{
void SetEnabledExtensions(const TExtensionBehavior &behavior, ffi::ExtensionsEnabled *extensions)
{
    extensions->ANDROID_extension_pack_es31a =
        IsExtensionEnabled(behavior, TExtension::ANDROID_extension_pack_es31a);
    extensions->ANGLE_base_vertex_base_instance_shader_builtin =
        IsExtensionEnabled(behavior, TExtension::ANGLE_base_vertex_base_instance_shader_builtin);
    extensions->ANGLE_clip_cull_distance =
        IsExtensionEnabled(behavior, TExtension::ANGLE_clip_cull_distance);
    extensions->ANGLE_multi_draw = IsExtensionEnabled(behavior, TExtension::ANGLE_multi_draw);
    extensions->ANGLE_shader_pixel_local_storage =
        IsExtensionEnabled(behavior, TExtension::ANGLE_shader_pixel_local_storage);
    extensions->ANGLE_texture_multisample =
        IsExtensionEnabled(behavior, TExtension::ANGLE_texture_multisample);
    extensions->APPLE_clip_distance = IsExtensionEnabled(behavior, TExtension::APPLE_clip_distance);
    extensions->ARB_fragment_shader_interlock =
        IsExtensionEnabled(behavior, TExtension::ARB_fragment_shader_interlock);
    extensions->ARB_texture_rectangle =
        IsExtensionEnabled(behavior, TExtension::ARB_texture_rectangle);
    extensions->ARM_shader_framebuffer_fetch =
        IsExtensionEnabled(behavior, TExtension::ARM_shader_framebuffer_fetch);
    extensions->ARM_shader_framebuffer_fetch_depth_stencil =
        IsExtensionEnabled(behavior, TExtension::ARM_shader_framebuffer_fetch_depth_stencil);
    extensions->EXT_YUV_target = IsExtensionEnabled(behavior, TExtension::EXT_YUV_target);
    extensions->EXT_blend_func_extended =
        IsExtensionEnabled(behavior, TExtension::EXT_blend_func_extended);
    extensions->EXT_clip_cull_distance =
        IsExtensionEnabled(behavior, TExtension::EXT_clip_cull_distance);
    extensions->EXT_conservative_depth =
        IsExtensionEnabled(behavior, TExtension::EXT_conservative_depth);
    extensions->EXT_draw_buffers = IsExtensionEnabled(behavior, TExtension::EXT_draw_buffers);
    extensions->EXT_frag_depth   = IsExtensionEnabled(behavior, TExtension::EXT_frag_depth);
    extensions->EXT_fragment_shading_rate =
        IsExtensionEnabled(behavior, TExtension::EXT_fragment_shading_rate);
    extensions->EXT_fragment_shading_rate_primitive =
        IsExtensionEnabled(behavior, TExtension::EXT_fragment_shading_rate_primitive);
    extensions->EXT_geometry_shader = IsExtensionEnabled(behavior, TExtension::EXT_geometry_shader);
    extensions->EXT_gpu_shader5     = IsExtensionEnabled(behavior, TExtension::EXT_gpu_shader5);
    extensions->EXT_primitive_bounding_box =
        IsExtensionEnabled(behavior, TExtension::EXT_primitive_bounding_box);
    extensions->EXT_separate_shader_objects =
        IsExtensionEnabled(behavior, TExtension::EXT_separate_shader_objects);
    extensions->EXT_shader_framebuffer_fetch =
        IsExtensionEnabled(behavior, TExtension::EXT_shader_framebuffer_fetch);
    extensions->EXT_shader_framebuffer_fetch_non_coherent =
        IsExtensionEnabled(behavior, TExtension::EXT_shader_framebuffer_fetch_non_coherent);
    extensions->EXT_shader_io_blocks =
        IsExtensionEnabled(behavior, TExtension::EXT_shader_io_blocks);
    extensions->EXT_shader_non_constant_global_initializers =
        IsExtensionEnabled(behavior, TExtension::EXT_shader_non_constant_global_initializers);
    extensions->EXT_shader_texture_lod =
        IsExtensionEnabled(behavior, TExtension::EXT_shader_texture_lod);
    extensions->EXT_shadow_samplers = IsExtensionEnabled(behavior, TExtension::EXT_shadow_samplers);
    extensions->EXT_tessellation_shader =
        IsExtensionEnabled(behavior, TExtension::EXT_tessellation_shader);
    extensions->EXT_texture_buffer = IsExtensionEnabled(behavior, TExtension::EXT_texture_buffer);
    extensions->EXT_texture_cube_map_array =
        IsExtensionEnabled(behavior, TExtension::EXT_texture_cube_map_array);
    extensions->EXT_texture_query_lod =
        IsExtensionEnabled(behavior, TExtension::EXT_texture_query_lod);
    extensions->EXT_texture_shadow_lod =
        IsExtensionEnabled(behavior, TExtension::EXT_texture_shadow_lod);
    extensions->INTEL_fragment_shader_ordering =
        IsExtensionEnabled(behavior, TExtension::INTEL_fragment_shader_ordering);
    extensions->KHR_blend_equation_advanced =
        IsExtensionEnabled(behavior, TExtension::KHR_blend_equation_advanced);
    extensions->NV_EGL_stream_consumer_external =
        IsExtensionEnabled(behavior, TExtension::NV_EGL_stream_consumer_external);
    extensions->NV_fragment_shader_interlock =
        IsExtensionEnabled(behavior, TExtension::NV_fragment_shader_interlock);
    extensions->NV_shader_noperspective_interpolation =
        IsExtensionEnabled(behavior, TExtension::NV_shader_noperspective_interpolation);
    extensions->OES_EGL_image_external =
        IsExtensionEnabled(behavior, TExtension::OES_EGL_image_external);
    extensions->OES_EGL_image_external_essl3 =
        IsExtensionEnabled(behavior, TExtension::OES_EGL_image_external_essl3);
    extensions->OES_geometry_shader = IsExtensionEnabled(behavior, TExtension::OES_geometry_shader);
    extensions->OES_gpu_shader5     = IsExtensionEnabled(behavior, TExtension::OES_gpu_shader5);
    extensions->OES_primitive_bounding_box =
        IsExtensionEnabled(behavior, TExtension::OES_primitive_bounding_box);
    extensions->OES_sample_variables =
        IsExtensionEnabled(behavior, TExtension::OES_sample_variables);
    extensions->OES_shader_image_atomic =
        IsExtensionEnabled(behavior, TExtension::OES_shader_image_atomic);
    extensions->OES_shader_io_blocks =
        IsExtensionEnabled(behavior, TExtension::OES_shader_io_blocks);
    extensions->OES_shader_multisample_interpolation =
        IsExtensionEnabled(behavior, TExtension::OES_shader_multisample_interpolation);
    extensions->OES_standard_derivatives =
        IsExtensionEnabled(behavior, TExtension::OES_standard_derivatives);
    extensions->OES_tessellation_shader =
        IsExtensionEnabled(behavior, TExtension::OES_tessellation_shader);
    extensions->OES_texture_3D     = IsExtensionEnabled(behavior, TExtension::OES_texture_3D);
    extensions->OES_texture_buffer = IsExtensionEnabled(behavior, TExtension::OES_texture_buffer);
    extensions->OES_texture_cube_map_array =
        IsExtensionEnabled(behavior, TExtension::OES_texture_cube_map_array);
    extensions->OES_texture_storage_multisample_2d_array =
        IsExtensionEnabled(behavior, TExtension::OES_texture_storage_multisample_2d_array);
    extensions->OVR_multiview       = IsExtensionEnabled(behavior, TExtension::OVR_multiview);
    extensions->OVR_multiview2      = IsExtensionEnabled(behavior, TExtension::OVR_multiview2);
    extensions->WEBGL_video_texture = IsExtensionEnabled(behavior, TExtension::WEBGL_video_texture);
}
}  // namespace

Output GenerateAST(IR ir, TCompiler *compiler, const ShCompileOptions &options)
{
    ffi::CompileOptions opt;
    opt.is_es1 = compiler->getShaderVersion() == 100;

    SetEnabledExtensions(compiler->getExtensionBehavior(), &opt.extensions);

    ffi::Output output = ffi::generate_ast(std::move(ir), compiler, GetGlobalPoolAllocator(), opt);

    Output out;
    out.root          = output.ast;
    out.TODOvariables = {};

    return out;
}
}  // namespace ir
}  // namespace sh
