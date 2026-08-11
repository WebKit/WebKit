/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdint.h>

namespace WebCore {

// Note: sorted alphabetically, including prefixes first, uppercases first.
enum class GCGLExtension : uint8_t {
    ANGLE_base_vertex_base_instance,
    ANGLE_clip_cull_distance,
    ANGLE_compressed_texture_etc,
    ANGLE_depth_texture,
    ANGLE_instanced_arrays,
    ANGLE_multi_draw,
    ANGLE_pack_reverse_row_order,
    ANGLE_polygon_mode,
    ANGLE_provoking_vertex,
    ANGLE_stencil_texturing,
    ANGLE_texture_compression_dxt3,
    ANGLE_texture_compression_dxt5,
    ANGLE_translated_shader_source,
    CHROMIUM_color_buffer_float_rgb,
    CHROMIUM_color_buffer_float_rgba,
    EXT_blend_func_extended,
    EXT_blend_minmax,
    EXT_clip_control,
    EXT_color_buffer_float,
    EXT_color_buffer_half_float,
    EXT_conservative_depth,
    EXT_depth_clamp,
    EXT_disjoint_timer_query,
    EXT_draw_buffers,
    EXT_float_blend,
    EXT_frag_depth,
    EXT_polygon_offset_clamp,
    EXT_render_snorm,
    EXT_sRGB,
    EXT_shader_texture_lod,
    EXT_texture_compression_bptc,
    EXT_texture_compression_dxt1,
    EXT_texture_compression_rgtc,
    EXT_texture_compression_s3tc_srgb,
    EXT_texture_filter_anisotropic,
    EXT_texture_mirror_clamp_to_edge,
    EXT_texture_norm16,
    IMG_texture_compression_pvrtc,
    KHR_parallel_shader_compile,
    KHR_texture_compression_astc_hdr,
    KHR_texture_compression_astc_ldr,
    NV_shader_noperspective_interpolation,
    OES_compressed_ETC1_RGB8_texture,
    OES_depth_texture,
    OES_draw_buffers_indexed,
    OES_element_index_uint,
    OES_fbo_render_mipmap,
    OES_packed_depth_stencil,
    OES_sample_variables,
    OES_shader_multisample_interpolation,
    OES_standard_derivatives,
    OES_texture_float,
    OES_texture_float_linear,
    OES_texture_half_float,
    OES_texture_half_float_linear,
    OES_vertex_array_object,
    QCOM_render_shared_exponent,
    HighestEnumValue = QCOM_render_shared_exponent
};

}
