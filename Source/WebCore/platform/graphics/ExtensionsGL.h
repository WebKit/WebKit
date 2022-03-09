/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GraphicsTypesGL.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

// This is a base class containing only pure virtual functions.
// Implementations must provide a subclass.
//
// The supported extensions are defined below and in subclasses,
// possibly platform-specific ones.
//
// Calling any extension function not supported by the current context
// must be a no-op; in particular, it may not have side effects. In
// this situation, if the function has a return value, 0 is returned.
class ExtensionsGL {
public:
    virtual ~ExtensionsGL() = default;

    // Supported extensions:
    //   GL_EXT_texture_format_BGRA8888
    //   GL_EXT_read_format_bgra
    //   GL_ARB_robustness
    //   GL_ARB_texture_non_power_of_two / GL_OES_texture_npot
    //   GL_EXT_packed_depth_stencil / GL_OES_packed_depth_stencil
    //   GL_ANGLE_framebuffer_blit / GL_ANGLE_framebuffer_multisample
    //   GL_IMG_multisampled_render_to_texture
    //   GL_OES_texture_float
    //   GL_OES_texture_float_linear
    //   GL_OES_texture_half_float
    //   GL_OES_texture_half_float_linear
    //   GL_OES_standard_derivatives
    //   GL_OES_rgb8_rgba8
    //   GL_OES_vertex_array_object
    //   GL_OES_element_index_uint
    //   GL_ANGLE_translated_shader_source
    //   GL_ARB_texture_rectangle (only the subset required to
    //     implement IOSurface binding; it's recommended to support
    //     this only on Mac OS X to limit the amount of code dependent
    //     on this extension)
    //   GL_EXT_texture_compression_dxt1
    //   GL_EXT_texture_compression_s3tc
    //   GL_EXT_texture_compression_s3tc_srgb
    //   GL_OES_compressed_ETC1_RGB8_texture
    //   GL_IMG_texture_compression_pvrtc
    //   GL_KHR_texture_compression_astc_hdr
    //   GL_KHR_texture_compression_astc_ldr
    //   EXT_texture_filter_anisotropic
    //   GL_EXT_debug_marker
    //   GL_ARB_draw_buffers / GL_EXT_draw_buffers
    //   GL_ANGLE_instanced_arrays
    //   GL_ANGLE_robust_client_memory

    // Takes full name of extension; for example,
    // "GL_EXT_texture_format_BGRA8888".
    virtual bool supports(const String&) = 0;

    // Certain OpenGL and WebGL implementations may support enabling
    // extensions lazily. This method may only be called with
    // extension names for which supports returns true.
    virtual void ensureEnabled(const String&) = 0;

    // Takes full name of extension: for example, "GL_EXT_texture_format_BGRA8888".
    // Checks to see whether the given extension is actually enabled (see ensureEnabled).
    // Has no other side-effects.
    virtual bool isEnabled(const String&) = 0;

    // EXT_sRGB formats
    static constexpr GCGLenum SRGB_EXT = 0x8C40;
    static constexpr GCGLenum SRGB_ALPHA_EXT = 0x8C42;
    static constexpr GCGLenum SRGB8_ALPHA8_EXT = 0x8C43;
    static constexpr GCGLenum FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT = 0x8210;

    // EXT_blend_minmax enums
    static constexpr GCGLenum MIN_EXT = 0x8007;
    static constexpr GCGLenum MAX_EXT = 0x8008;

    // GL_EXT_texture_format_BGRA8888 enums
    static constexpr GCGLenum BGRA_EXT = 0x80E1;

    // GL_ARB_robustness enums
    static constexpr GCGLenum GUILTY_CONTEXT_RESET_ARB = 0x8253;
    static constexpr GCGLenum INNOCENT_CONTEXT_RESET_ARB = 0x8254;
    static constexpr GCGLenum UNKNOWN_CONTEXT_RESET_ARB = 0x8255;
    static constexpr GCGLenum CONTEXT_ROBUST_ACCESS = 0x90F3;

    // GL_EXT/OES_packed_depth_stencil enums
    static constexpr GCGLenum DEPTH24_STENCIL8 = 0x88F0;

    // GL_ANGLE_framebuffer_blit names
    static constexpr GCGLenum READ_FRAMEBUFFER = 0x8CA8;
    static constexpr GCGLenum DRAW_FRAMEBUFFER = 0x8CA9;
    static constexpr GCGLenum DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
    static constexpr GCGLenum READ_FRAMEBUFFER_BINDING = 0x8CAA;

    // GL_ANGLE_framebuffer_multisample names
    static constexpr GCGLenum RENDERBUFFER_SAMPLES = 0x8CAB;
    static constexpr GCGLenum FRAMEBUFFER_INCOMPLETE_MULTISAMPLE = 0x8D56;
    static constexpr GCGLenum MAX_SAMPLES = 0x8D57;

    // GL_IMG_multisampled_render_to_texture
    static constexpr GCGLenum RENDERBUFFER_SAMPLES_IMG = 0x9133;
    static constexpr GCGLenum FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG = 0x9134;
    static constexpr GCGLenum MAX_SAMPLES_IMG = 0x9135;
    static constexpr GCGLenum TEXTURE_SAMPLES_IMG = 0x9136;

    // GL_OES_standard_derivatives names
    static constexpr GCGLenum FRAGMENT_SHADER_DERIVATIVE_HINT_OES = 0x8B8B;

    // GL_OES_rgb8_rgba8 names
    static constexpr GCGLenum RGB8_OES = 0x8051;
    static constexpr GCGLenum RGBA8_OES = 0x8058;

    // GL_OES_vertex_array_object names
    static constexpr GCGLenum VERTEX_ARRAY_BINDING_OES = 0x85B5;

    // GL_ANGLE_translated_shader_source
    static constexpr GCGLenum TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE = 0x93A0;

    // GL_ARB_texture_rectangle
    static constexpr GCGLenum TEXTURE_RECTANGLE_ARB =  0x84F5;
    static constexpr GCGLenum TEXTURE_BINDING_RECTANGLE_ARB = 0x84F6;

    // GL_EXT_texture_compression_dxt1
    // GL_EXT_texture_compression_s3tc
    static constexpr GCGLenum COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0;
    static constexpr GCGLenum COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1;
    static constexpr GCGLenum COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2;
    static constexpr GCGLenum COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3;

    // GL_EXT_texture_compression_s3tc_srgb
    static constexpr GCGLenum COMPRESSED_SRGB_S3TC_DXT1_EXT = 0x8C4C;
    static constexpr GCGLenum COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT = 0x8C4D;
    static constexpr GCGLenum COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT = 0x8C4E;
    static constexpr GCGLenum COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT = 0x8C4F;

    // GL_OES_compressed_ETC1_RGB8_texture
    static constexpr GCGLenum ETC1_RGB8_OES = 0x8D64;

    // WEBGL_compressed_texture_etc
    static constexpr GCGLenum COMPRESSED_R11_EAC = 0x9270;
    static constexpr GCGLenum COMPRESSED_SIGNED_R11_EAC = 0x9271;
    static constexpr GCGLenum COMPRESSED_RG11_EAC = 0x9272;
    static constexpr GCGLenum COMPRESSED_SIGNED_RG11_EAC = 0x9273;
    static constexpr GCGLenum COMPRESSED_RGB8_ETC2 = 0x9274;
    static constexpr GCGLenum COMPRESSED_SRGB8_ETC2 = 0x9275;
    static constexpr GCGLenum COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9276;
    static constexpr GCGLenum COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9277;
    static constexpr GCGLenum COMPRESSED_RGBA8_ETC2_EAC = 0x9278;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ETC2_EAC = 0x9279;

    // GL_IMG_texture_compression_pvrtc
    static constexpr GCGLenum COMPRESSED_RGB_PVRTC_4BPPV1_IMG = 0x8C00;
    static constexpr GCGLenum COMPRESSED_RGB_PVRTC_2BPPV1_IMG = 0x8C01;
    static constexpr GCGLenum COMPRESSED_RGBA_PVRTC_4BPPV1_IMG = 0x8C02;
    static constexpr GCGLenum COMPRESSED_RGBA_PVRTC_2BPPV1_IMG = 0x8C03;

    // GL_AMD_compressed_ATC_texture
    static constexpr GCGLenum COMPRESSED_ATC_RGB_AMD = 0x8C92;
    static constexpr GCGLenum COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA_AMD = 0x8C93;
    static constexpr GCGLenum COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA_AMD = 0x87EE;

    // GL_KHR_texture_compression_astc_hdr
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_4x4_KHR = 0x93B0;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_5x4_KHR = 0x93B1;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_5x5_KHR = 0x93B2;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_6x5_KHR = 0x93B3;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_6x6_KHR = 0x93B4;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_8x5_KHR = 0x93B5;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_8x6_KHR = 0x93B6;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_8x8_KHR = 0x93B7;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_10x5_KHR = 0x93B8;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_10x6_KHR = 0x93B9;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_10x8_KHR = 0x93BA;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_10x10_KHR = 0x93BB;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_12x10_KHR = 0x93BC;
    static constexpr GCGLenum COMPRESSED_RGBA_ASTC_12x12_KHR = 0x93BD;

    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR = 0x93D0;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR = 0x93D1;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR = 0x93D2;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR = 0x93D3;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR = 0x93D4;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR = 0x93D5;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR = 0x93D6;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR = 0x93D7;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR = 0x93D8;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR = 0x93D9;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR = 0x93DA;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR = 0x93DB;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR = 0x93DC;
    static constexpr GCGLenum COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR = 0x93DD;

    // GL_EXT_texture_compression_rgtc
    static constexpr GCGLenum COMPRESSED_RED_RGTC1_EXT = 0x8DBB;
    static constexpr GCGLenum COMPRESSED_SIGNED_RED_RGTC1_EXT = 0x8DBC;
    static constexpr GCGLenum COMPRESSED_RED_GREEN_RGTC2_EXT = 0x8DBD;
    static constexpr GCGLenum COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT = 0x8DBE;

    // GL_EXT_texture_filter_anisotropic
    static constexpr GCGLenum TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
    static constexpr GCGLenum MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

    // GL_ARB_draw_buffers / GL_EXT_draw_buffers
    static constexpr GCGLenum MAX_DRAW_BUFFERS_EXT = 0x8824;
    static constexpr GCGLenum DRAW_BUFFER0_EXT = 0x8825;
    static constexpr GCGLenum DRAW_BUFFER1_EXT = 0x8826;
    static constexpr GCGLenum DRAW_BUFFER2_EXT = 0x8827;
    static constexpr GCGLenum DRAW_BUFFER3_EXT = 0x8828;
    static constexpr GCGLenum DRAW_BUFFER4_EXT = 0x8829;
    static constexpr GCGLenum DRAW_BUFFER5_EXT = 0x882A;
    static constexpr GCGLenum DRAW_BUFFER6_EXT = 0x882B;
    static constexpr GCGLenum DRAW_BUFFER7_EXT = 0x882C;
    static constexpr GCGLenum DRAW_BUFFER8_EXT = 0x882D;
    static constexpr GCGLenum DRAW_BUFFER9_EXT = 0x882E;
    static constexpr GCGLenum DRAW_BUFFER10_EXT = 0x882F;
    static constexpr GCGLenum DRAW_BUFFER11_EXT = 0x8830;
    static constexpr GCGLenum DRAW_BUFFER12_EXT = 0x8831;
    static constexpr GCGLenum DRAW_BUFFER13_EXT = 0x8832;
    static constexpr GCGLenum DRAW_BUFFER14_EXT = 0x8833;
    static constexpr GCGLenum DRAW_BUFFER15_EXT = 0x8834;
    static constexpr GCGLenum MAX_COLOR_ATTACHMENTS_EXT = 0x8CDF;
    static constexpr GCGLenum COLOR_ATTACHMENT0_EXT = 0x8CE0;
    static constexpr GCGLenum COLOR_ATTACHMENT1_EXT = 0x8CE1;
    static constexpr GCGLenum COLOR_ATTACHMENT2_EXT = 0x8CE2;
    static constexpr GCGLenum COLOR_ATTACHMENT3_EXT = 0x8CE3;
    static constexpr GCGLenum COLOR_ATTACHMENT4_EXT = 0x8CE4;
    static constexpr GCGLenum COLOR_ATTACHMENT5_EXT = 0x8CE5;
    static constexpr GCGLenum COLOR_ATTACHMENT6_EXT = 0x8CE6;
    static constexpr GCGLenum COLOR_ATTACHMENT7_EXT = 0x8CE7;
    static constexpr GCGLenum COLOR_ATTACHMENT8_EXT = 0x8CE8;
    static constexpr GCGLenum COLOR_ATTACHMENT9_EXT = 0x8CE9;
    static constexpr GCGLenum COLOR_ATTACHMENT10_EXT = 0x8CEA;
    static constexpr GCGLenum COLOR_ATTACHMENT11_EXT = 0x8CEB;
    static constexpr GCGLenum COLOR_ATTACHMENT12_EXT = 0x8CEC;
    static constexpr GCGLenum COLOR_ATTACHMENT13_EXT = 0x8CED;
    static constexpr GCGLenum COLOR_ATTACHMENT14_EXT = 0x8CEE;
    static constexpr GCGLenum COLOR_ATTACHMENT15_EXT = 0x8CEF;

    // GL_KHR_parallel_shader_compile
    static constexpr GCGLenum COMPLETION_STATUS_KHR = 0x91B1;

    // GL_ANGLE_request_extension
    static constexpr GCGLenum REQUESTABLE_EXTENSIONS_ANGLE = 0x93A8;

    // WebGL functions.

    // GL_ARB_robustness
    // Note: This method's behavior differs from the GL_ARB_robustness
    // specification in the following way:
    // The implementation must not reset the error state during this call.
    // If getGraphicsResetStatusARB returns an error, it should continue
    // returning the same error. Restoring the GraphicsContextGLOpenGL is handled
    // externally.
    virtual GCGLint getGraphicsResetStatusARB() = 0;

    // GL_ANGLE_translated_shader_source
    virtual String getTranslatedShaderSourceANGLE(PlatformGLObject) = 0;

    // GL_ARB_draw_buffers / GL_EXT_draw_buffers
    virtual void drawBuffersEXT(GCGLSpan<const GCGLenum> bufs) = 0;

    // Other functions.
#if !USE(ANGLE)
    // EXT Robustness - uses getGraphicsResetStatusARB
    virtual void readnPixelsEXT(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLsizei bufSize, GCGLvoid* data) = 0;
    virtual void getnUniformfvEXT(GCGLuint program, GCGLint location, GCGLsizei bufSize, GCGLfloat* params) = 0;
    virtual void getnUniformivEXT(GCGLuint program, GCGLint location, GCGLsizei bufSize, GCGLint* params) = 0;

#endif

};

} // namespace WebCore
