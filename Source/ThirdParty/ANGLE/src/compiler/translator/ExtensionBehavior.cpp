//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ExtensionBehavior.cpp: Extension name enumeration and data structures for storing extension
// behavior.

#include "compiler/translator/ExtensionBehavior.h"

#include "common/debug.h"

#include <string.h>

#define LIST_EXTENSIONS(OP)                            \
    OP(ANDROID_extension_pack_es31a)                   \
    OP(ANGLE_base_vertex_base_instance_shader_builtin) \
    OP(ANGLE_multi_draw)                               \
    OP(ANGLE_shader_pixel_local_storage)               \
    OP(ANGLE_texture_multisample)                      \
    OP(APPLE_clip_distance)                            \
    OP(ARB_texture_rectangle)                          \
    OP(ARM_shader_framebuffer_fetch)                   \
    OP(EXT_blend_func_extended)                        \
    OP(EXT_clip_cull_distance)                         \
    OP(EXT_draw_buffers)                               \
    OP(EXT_frag_depth)                                 \
    OP(EXT_geometry_shader)                            \
    OP(OES_geometry_shader)                            \
    OP(OES_shader_io_blocks)                           \
    OP(EXT_shader_io_blocks)                           \
    OP(EXT_gpu_shader5)                                \
    OP(EXT_primitive_bounding_box)                     \
    OP(OES_primitive_bounding_box)                     \
    OP(EXT_shader_framebuffer_fetch)                   \
    OP(EXT_shader_framebuffer_fetch_non_coherent)      \
    OP(EXT_shader_non_constant_global_initializers)    \
    OP(EXT_shader_texture_lod)                         \
    OP(EXT_shadow_samplers)                            \
    OP(EXT_tessellation_shader)                        \
    OP(EXT_texture_buffer)                             \
    OP(EXT_texture_cube_map_array)                     \
    OP(EXT_YUV_target)                                 \
    OP(KHR_blend_equation_advanced)                    \
    OP(NV_EGL_stream_consumer_external)                \
    OP(NV_shader_framebuffer_fetch)                    \
    OP(NV_shader_noperspective_interpolation)          \
    OP(OES_EGL_image_external)                         \
    OP(OES_EGL_image_external_essl3)                   \
    OP(OES_sample_variables)                           \
    OP(OES_shader_multisample_interpolation)           \
    OP(OES_shader_image_atomic)                        \
    OP(OES_standard_derivatives)                       \
    OP(OES_texture_3D)                                 \
    OP(OES_texture_buffer)                             \
    OP(OES_texture_cube_map_array)                     \
    OP(OES_texture_storage_multisample_2d_array)       \
    OP(OVR_multiview)                                  \
    OP(OVR_multiview2)                                 \
    OP(WEBGL_video_texture)

namespace sh
{

#define RETURN_EXTENSION_NAME_CASE(ext) \
    case TExtension::ext:               \
        return "GL_" #ext;

const char *GetExtensionNameString(TExtension extension)
{
    switch (extension)
    {
        LIST_EXTENSIONS(RETURN_EXTENSION_NAME_CASE)
        default:
            UNREACHABLE();
            return "";
    }
}

#define RETURN_EXTENSION_IF_NAME_MATCHES(ext)  \
    if (strcmp(extWithoutGLPrefix, #ext) == 0) \
    {                                          \
        return TExtension::ext;                \
    }

TExtension GetExtensionByName(const char *extension)
{
    // If first characters of the extension don't equal "GL_", early out.
    if (strncmp(extension, "GL_", 3) != 0)
    {
        return TExtension::UNDEFINED;
    }
    const char *extWithoutGLPrefix = extension + 3;

    LIST_EXTENSIONS(RETURN_EXTENSION_IF_NAME_MATCHES)

    return TExtension::UNDEFINED;
}

const char *GetBehaviorString(TBehavior b)
{
    switch (b)
    {
        case EBhRequire:
            return "require";
        case EBhEnable:
            return "enable";
        case EBhWarn:
            return "warn";
        case EBhDisable:
            return "disable";
        default:
            return nullptr;
    }
}

bool IsExtensionEnabled(const TExtensionBehavior &extBehavior, TExtension extension)
{
    ASSERT(extension != TExtension::UNDEFINED);
    auto iter = extBehavior.find(extension);
    return iter != extBehavior.end() &&
           (iter->second == EBhEnable || iter->second == EBhRequire || iter->second == EBhWarn);
}

}  // namespace sh
