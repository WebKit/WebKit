/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GRAPHICS_CONTEXT_GL)

#include "GraphicsContextGLAttributes.h"
#include "GraphicsTypesGL.h"
#include "Image.h"
#include "IntRect.h"
#include "IntSize.h"
#include "PlatformLayer.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
// Defined in winerror.h
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#endif

typedef void* PlatformGraphicsContextGL;
typedef void* PlatformGraphicsContextGLDisplay;
typedef void* PlatformGraphicsContextGLSurface;
typedef void* PlatformGraphicsContextGLConfig;

namespace WebCore {
class ExtensionsGL;
class HostWindow;
class ImageBuffer;
class ImageData;

class GraphicsContextGL : public RefCounted<GraphicsContextGL> {
public:
    enum {
        // WebGL 1 constants.
        DEPTH_BUFFER_BIT = 0x00000100,
        STENCIL_BUFFER_BIT = 0x00000400,
        COLOR_BUFFER_BIT = 0x00004000,
        POINTS = 0x0000,
        LINES = 0x0001,
        LINE_LOOP = 0x0002,
        LINE_STRIP = 0x0003,
        TRIANGLES = 0x0004,
        TRIANGLE_STRIP = 0x0005,
        TRIANGLE_FAN = 0x0006,
        ZERO = 0,
        ONE = 1,
        SRC_COLOR = 0x0300,
        ONE_MINUS_SRC_COLOR = 0x0301,
        SRC_ALPHA = 0x0302,
        ONE_MINUS_SRC_ALPHA = 0x0303,
        DST_ALPHA = 0x0304,
        ONE_MINUS_DST_ALPHA = 0x0305,
        DST_COLOR = 0x0306,
        ONE_MINUS_DST_COLOR = 0x0307,
        SRC_ALPHA_SATURATE = 0x0308,
        FUNC_ADD = 0x8006,
        BLEND_EQUATION = 0x8009,
        BLEND_EQUATION_RGB = 0x8009,
        BLEND_EQUATION_ALPHA = 0x883D,
        FUNC_SUBTRACT = 0x800A,
        FUNC_REVERSE_SUBTRACT = 0x800B,
        BLEND_DST_RGB = 0x80C8,
        BLEND_SRC_RGB = 0x80C9,
        BLEND_DST_ALPHA = 0x80CA,
        BLEND_SRC_ALPHA = 0x80CB,
        CONSTANT_COLOR = 0x8001,
        ONE_MINUS_CONSTANT_COLOR = 0x8002,
        CONSTANT_ALPHA = 0x8003,
        ONE_MINUS_CONSTANT_ALPHA = 0x8004,
        BLEND_COLOR = 0x8005,
        ARRAY_BUFFER = 0x8892,
        ELEMENT_ARRAY_BUFFER = 0x8893,
        ARRAY_BUFFER_BINDING = 0x8894,
        ELEMENT_ARRAY_BUFFER_BINDING = 0x8895,
        STREAM_DRAW = 0x88E0,
        STATIC_DRAW = 0x88E4,
        DYNAMIC_DRAW = 0x88E8,
        BUFFER_SIZE = 0x8764,
        BUFFER_USAGE = 0x8765,
        CURRENT_VERTEX_ATTRIB = 0x8626,
        FRONT = 0x0404,
        BACK = 0x0405,
        FRONT_AND_BACK = 0x0408,
        TEXTURE_2D = 0x0DE1,
        CULL_FACE = 0x0B44,
        BLEND = 0x0BE2,
        DITHER = 0x0BD0,
        STENCIL_TEST = 0x0B90,
        DEPTH_TEST = 0x0B71,
        SCISSOR_TEST = 0x0C11,
        POLYGON_OFFSET_FILL = 0x8037,
        SAMPLE_ALPHA_TO_COVERAGE = 0x809E,
        SAMPLE_COVERAGE = 0x80A0,
        NO_ERROR = 0,
        INVALID_ENUM = 0x0500,
        INVALID_VALUE = 0x0501,
        INVALID_OPERATION = 0x0502,
        OUT_OF_MEMORY = 0x0505,
        CW = 0x0900,
        CCW = 0x0901,
        LINE_WIDTH = 0x0B21,
        ALIASED_POINT_SIZE_RANGE = 0x846D,
        ALIASED_LINE_WIDTH_RANGE = 0x846E,
        CULL_FACE_MODE = 0x0B45,
        FRONT_FACE = 0x0B46,
        DEPTH_RANGE = 0x0B70,
        DEPTH_WRITEMASK = 0x0B72,
        DEPTH_CLEAR_VALUE = 0x0B73,
        DEPTH_FUNC = 0x0B74,
        STENCIL_CLEAR_VALUE = 0x0B91,
        STENCIL_FUNC = 0x0B92,
        STENCIL_FAIL = 0x0B94,
        STENCIL_PASS_DEPTH_FAIL = 0x0B95,
        STENCIL_PASS_DEPTH_PASS = 0x0B96,
        STENCIL_REF = 0x0B97,
        STENCIL_VALUE_MASK = 0x0B93,
        STENCIL_WRITEMASK = 0x0B98,
        STENCIL_BACK_FUNC = 0x8800,
        STENCIL_BACK_FAIL = 0x8801,
        STENCIL_BACK_PASS_DEPTH_FAIL = 0x8802,
        STENCIL_BACK_PASS_DEPTH_PASS = 0x8803,
        STENCIL_BACK_REF = 0x8CA3,
        STENCIL_BACK_VALUE_MASK = 0x8CA4,
        STENCIL_BACK_WRITEMASK = 0x8CA5,
        VIEWPORT = 0x0BA2,
        SCISSOR_BOX = 0x0C10,
        COLOR_CLEAR_VALUE = 0x0C22,
        COLOR_WRITEMASK = 0x0C23,
        UNPACK_ALIGNMENT = 0x0CF5,
        PACK_ALIGNMENT = 0x0D05,
        MAX_TEXTURE_SIZE = 0x0D33,
        MAX_VIEWPORT_DIMS = 0x0D3A,
        SUBPIXEL_BITS = 0x0D50,
        RED_BITS = 0x0D52,
        GREEN_BITS = 0x0D53,
        BLUE_BITS = 0x0D54,
        ALPHA_BITS = 0x0D55,
        DEPTH_BITS = 0x0D56,
        STENCIL_BITS = 0x0D57,
        POLYGON_OFFSET_UNITS = 0x2A00,
        POLYGON_OFFSET_FACTOR = 0x8038,
        TEXTURE_BINDING_2D = 0x8069,
        SAMPLE_BUFFERS = 0x80A8,
        SAMPLES = 0x80A9,
        SAMPLE_COVERAGE_VALUE = 0x80AA,
        SAMPLE_COVERAGE_INVERT = 0x80AB,
        NUM_COMPRESSED_TEXTURE_FORMATS = 0x86A2,
        COMPRESSED_TEXTURE_FORMATS = 0x86A3,
        DONT_CARE = 0x1100,
        FASTEST = 0x1101,
        NICEST = 0x1102,
        GENERATE_MIPMAP_HINT = 0x8192,
        BYTE = 0x1400,
        UNSIGNED_BYTE = 0x1401,
        SHORT = 0x1402,
        UNSIGNED_SHORT = 0x1403,
        INT = 0x1404,
        UNSIGNED_INT = 0x1405,
        FLOAT = 0x1406,
        HALF_FLOAT_OES = 0x8D61,
        FIXED = 0x140C,
        DEPTH_COMPONENT = 0x1902,
        ALPHA = 0x1906,
        RGB = 0x1907,
        RGBA = 0x1908,
        BGRA = 0x80E1,
        LUMINANCE = 0x1909,
        LUMINANCE_ALPHA = 0x190A,
        UNSIGNED_SHORT_4_4_4_4 = 0x8033,
        UNSIGNED_SHORT_5_5_5_1 = 0x8034,
        UNSIGNED_SHORT_5_6_5 = 0x8363,
        FRAGMENT_SHADER = 0x8B30,
        VERTEX_SHADER = 0x8B31,
        MAX_VERTEX_ATTRIBS = 0x8869,
        MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB,
        MAX_VARYING_VECTORS = 0x8DFC,
        MAX_COMBINED_TEXTURE_IMAGE_UNITS = 0x8B4D,
        MAX_VERTEX_TEXTURE_IMAGE_UNITS = 0x8B4C,
        MAX_TEXTURE_IMAGE_UNITS = 0x8872,
        MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD,
        SHADER_TYPE = 0x8B4F,
        DELETE_STATUS = 0x8B80,
        LINK_STATUS = 0x8B82,
        VALIDATE_STATUS = 0x8B83,
        ATTACHED_SHADERS = 0x8B85,
        ACTIVE_UNIFORMS = 0x8B86,
        ACTIVE_UNIFORM_MAX_LENGTH = 0x8B87,
        ACTIVE_ATTRIBUTES = 0x8B89,
        ACTIVE_ATTRIBUTE_MAX_LENGTH = 0x8B8A,
        SHADING_LANGUAGE_VERSION = 0x8B8C,
        CURRENT_PROGRAM = 0x8B8D,
        NEVER = 0x0200,
        LESS = 0x0201,
        EQUAL = 0x0202,
        LEQUAL = 0x0203,
        GREATER = 0x0204,
        NOTEQUAL = 0x0205,
        GEQUAL = 0x0206,
        ALWAYS = 0x0207,
        KEEP = 0x1E00,
        REPLACE = 0x1E01,
        INCR = 0x1E02,
        DECR = 0x1E03,
        INVERT = 0x150A,
        INCR_WRAP = 0x8507,
        DECR_WRAP = 0x8508,
        VENDOR = 0x1F00,
        RENDERER = 0x1F01,
        VERSION = 0x1F02,
        EXTENSIONS = 0x1F03,
        NEAREST = 0x2600,
        LINEAR = 0x2601,
        NEAREST_MIPMAP_NEAREST = 0x2700,
        LINEAR_MIPMAP_NEAREST = 0x2701,
        NEAREST_MIPMAP_LINEAR = 0x2702,
        LINEAR_MIPMAP_LINEAR = 0x2703,
        TEXTURE_MAG_FILTER = 0x2800,
        TEXTURE_MIN_FILTER = 0x2801,
        TEXTURE_WRAP_S = 0x2802,
        TEXTURE_WRAP_T = 0x2803,
        TEXTURE = 0x1702,
        TEXTURE_CUBE_MAP = 0x8513,
        TEXTURE_BINDING_CUBE_MAP = 0x8514,
        TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515,
        TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516,
        TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517,
        TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518,
        TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519,
        TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A,
        MAX_CUBE_MAP_TEXTURE_SIZE = 0x851C,
        TEXTURE0 = 0x84C0,
        TEXTURE1 = 0x84C1,
        TEXTURE2 = 0x84C2,
        TEXTURE3 = 0x84C3,
        TEXTURE4 = 0x84C4,
        TEXTURE5 = 0x84C5,
        TEXTURE6 = 0x84C6,
        TEXTURE7 = 0x84C7,
        TEXTURE8 = 0x84C8,
        TEXTURE9 = 0x84C9,
        TEXTURE10 = 0x84CA,
        TEXTURE11 = 0x84CB,
        TEXTURE12 = 0x84CC,
        TEXTURE13 = 0x84CD,
        TEXTURE14 = 0x84CE,
        TEXTURE15 = 0x84CF,
        TEXTURE16 = 0x84D0,
        TEXTURE17 = 0x84D1,
        TEXTURE18 = 0x84D2,
        TEXTURE19 = 0x84D3,
        TEXTURE20 = 0x84D4,
        TEXTURE21 = 0x84D5,
        TEXTURE22 = 0x84D6,
        TEXTURE23 = 0x84D7,
        TEXTURE24 = 0x84D8,
        TEXTURE25 = 0x84D9,
        TEXTURE26 = 0x84DA,
        TEXTURE27 = 0x84DB,
        TEXTURE28 = 0x84DC,
        TEXTURE29 = 0x84DD,
        TEXTURE30 = 0x84DE,
        TEXTURE31 = 0x84DF,
        ACTIVE_TEXTURE = 0x84E0,
        REPEAT = 0x2901,
        CLAMP_TO_EDGE = 0x812F,
        MIRRORED_REPEAT = 0x8370,
        FLOAT_VEC2 = 0x8B50,
        FLOAT_VEC3 = 0x8B51,
        FLOAT_VEC4 = 0x8B52,
        INT_VEC2 = 0x8B53,
        INT_VEC3 = 0x8B54,
        INT_VEC4 = 0x8B55,
        BOOL = 0x8B56,
        BOOL_VEC2 = 0x8B57,
        BOOL_VEC3 = 0x8B58,
        BOOL_VEC4 = 0x8B59,
        FLOAT_MAT2 = 0x8B5A,
        FLOAT_MAT3 = 0x8B5B,
        FLOAT_MAT4 = 0x8B5C,
        SAMPLER_2D = 0x8B5E,
        SAMPLER_CUBE = 0x8B60,
        VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622,
        VERTEX_ATTRIB_ARRAY_SIZE = 0x8623,
        VERTEX_ATTRIB_ARRAY_STRIDE = 0x8624,
        VERTEX_ATTRIB_ARRAY_TYPE = 0x8625,
        VERTEX_ATTRIB_ARRAY_NORMALIZED = 0x886A,
        VERTEX_ATTRIB_ARRAY_POINTER = 0x8645,
        VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F,
        IMPLEMENTATION_COLOR_READ_TYPE = 0x8B9A,
        IMPLEMENTATION_COLOR_READ_FORMAT = 0x8B9B,
        COMPILE_STATUS = 0x8B81,
        INFO_LOG_LENGTH = 0x8B84,
        SHADER_SOURCE_LENGTH = 0x8B88,
        SHADER_COMPILER = 0x8DFA,
        SHADER_BINARY_FORMATS = 0x8DF8,
        NUM_SHADER_BINARY_FORMATS = 0x8DF9,
        LOW_FLOAT = 0x8DF0,
        MEDIUM_FLOAT = 0x8DF1,
        HIGH_FLOAT = 0x8DF2,
        LOW_INT = 0x8DF3,
        MEDIUM_INT = 0x8DF4,
        HIGH_INT = 0x8DF5,
        FRAMEBUFFER = 0x8D40,
        RENDERBUFFER = 0x8D41,
        RGBA4 = 0x8056,
        RGB5_A1 = 0x8057,
        RGB565 = 0x8D62,
        DEPTH_COMPONENT16 = 0x81A5,
        STENCIL_INDEX = 0x1901,
        STENCIL_INDEX8 = 0x8D48,
        RENDERBUFFER_WIDTH = 0x8D42,
        RENDERBUFFER_HEIGHT = 0x8D43,
        RENDERBUFFER_INTERNAL_FORMAT = 0x8D44,
        RENDERBUFFER_RED_SIZE = 0x8D50,
        RENDERBUFFER_GREEN_SIZE = 0x8D51,
        RENDERBUFFER_BLUE_SIZE = 0x8D52,
        RENDERBUFFER_ALPHA_SIZE = 0x8D53,
        RENDERBUFFER_DEPTH_SIZE = 0x8D54,
        RENDERBUFFER_STENCIL_SIZE = 0x8D55,
        FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE = 0x8CD0,
        FRAMEBUFFER_ATTACHMENT_OBJECT_NAME = 0x8CD1,
        FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL = 0x8CD2,
        FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE = 0x8CD3,
        COLOR_ATTACHMENT0 = 0x8CE0,
        DEPTH_ATTACHMENT = 0x8D00,
        STENCIL_ATTACHMENT = 0x8D20,
        NONE = 0,
        FRAMEBUFFER_COMPLETE = 0x8CD5,
        FRAMEBUFFER_INCOMPLETE_ATTACHMENT = 0x8CD6,
        FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT = 0x8CD7,
        FRAMEBUFFER_INCOMPLETE_DIMENSIONS = 0x8CD9,
        FRAMEBUFFER_UNSUPPORTED = 0x8CDD,
        FRAMEBUFFER_BINDING = 0x8CA6,
        RENDERBUFFER_BINDING = 0x8CA7,
        MAX_RENDERBUFFER_SIZE = 0x84E8,
        INVALID_FRAMEBUFFER_OPERATION = 0x0506,

        // WebGL-specific enums
        UNPACK_FLIP_Y_WEBGL = 0x9240,
        UNPACK_PREMULTIPLY_ALPHA_WEBGL = 0x9241,
        CONTEXT_LOST_WEBGL = 0x9242,
        UNPACK_COLORSPACE_CONVERSION_WEBGL = 0x9243,
        BROWSER_DEFAULT_WEBGL = 0x9244,
        VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE = 0x88FE,
        
        // WebGL2 constants
        READ_BUFFER = 0x0C02,
        UNPACK_ROW_LENGTH = 0x0CF2,
        UNPACK_SKIP_ROWS = 0x0CF3,
        UNPACK_SKIP_PIXELS = 0x0CF4,
        PACK_ROW_LENGTH = 0x0D02,
        PACK_SKIP_ROWS = 0x0D03,
        PACK_SKIP_PIXELS = 0x0D04,
        COLOR = 0x1800,
        DEPTH = 0x1801,
        STENCIL = 0x1802,
        RED = 0x1903,
        RGB8 = 0x8051,
        RGBA8 = 0x8058,
        RGB10_A2 = 0x8059,
        TEXTURE_BINDING_3D = 0x806A,
        UNPACK_SKIP_IMAGES = 0x806D,
        UNPACK_IMAGE_HEIGHT = 0x806E,
        TEXTURE_3D = 0x806F,
        TEXTURE_WRAP_R = 0x8072,
        MAX_3D_TEXTURE_SIZE = 0x8073,
        UNSIGNED_INT_2_10_10_10_REV = 0x8368,
        MAX_ELEMENTS_VERTICES = 0x80E8,
        MAX_ELEMENTS_INDICES = 0x80E9,
        TEXTURE_MIN_LOD = 0x813A,
        TEXTURE_MAX_LOD = 0x813B,
        TEXTURE_BASE_LEVEL = 0x813C,
        TEXTURE_MAX_LEVEL = 0x813D,
        MIN = 0x8007,
        MAX = 0x8008,
        DEPTH_COMPONENT24 = 0x81A6,
        MAX_TEXTURE_LOD_BIAS = 0x84FD,
        TEXTURE_COMPARE_MODE = 0x884C,
        TEXTURE_COMPARE_FUNC = 0x884D,
        CURRENT_QUERY = 0x8865,
        QUERY_RESULT = 0x8866,
        QUERY_RESULT_AVAILABLE = 0x8867,
        STREAM_READ = 0x88E1,
        STREAM_COPY = 0x88E2,
        STATIC_READ = 0x88E5,
        STATIC_COPY = 0x88E6,
        DYNAMIC_READ = 0x88E9,
        DYNAMIC_COPY = 0x88EA,
        MAX_DRAW_BUFFERS = 0x8824,
        DRAW_BUFFER0 = 0x8825,
        DRAW_BUFFER1 = 0x8826,
        DRAW_BUFFER2 = 0x8827,
        DRAW_BUFFER3 = 0x8828,
        DRAW_BUFFER4 = 0x8829,
        DRAW_BUFFER5 = 0x882A,
        DRAW_BUFFER6 = 0x882B,
        DRAW_BUFFER7 = 0x882C,
        DRAW_BUFFER8 = 0x882D,
        DRAW_BUFFER9 = 0x882E,
        DRAW_BUFFER10 = 0x882F,
        DRAW_BUFFER11 = 0x8830,
        DRAW_BUFFER12 = 0x8831,
        DRAW_BUFFER13 = 0x8832,
        DRAW_BUFFER14 = 0x8833,
        DRAW_BUFFER15 = 0x8834,
        MAX_FRAGMENT_UNIFORM_COMPONENTS = 0x8B49,
        MAX_VERTEX_UNIFORM_COMPONENTS = 0x8B4A,
        SAMPLER_3D = 0x8B5F,
        SAMPLER_2D_SHADOW = 0x8B62,
        FRAGMENT_SHADER_DERIVATIVE_HINT = 0x8B8B,
        PIXEL_PACK_BUFFER = 0x88EB,
        PIXEL_UNPACK_BUFFER = 0x88EC,
        PIXEL_PACK_BUFFER_BINDING = 0x88ED,
        PIXEL_UNPACK_BUFFER_BINDING = 0x88EF,
        FLOAT_MAT2x3 = 0x8B65,
        FLOAT_MAT2x4 = 0x8B66,
        FLOAT_MAT3x2 = 0x8B67,
        FLOAT_MAT3x4 = 0x8B68,
        FLOAT_MAT4x2 = 0x8B69,
        FLOAT_MAT4x3 = 0x8B6A,
        SRGB = 0x8C40,
        SRGB8 = 0x8C41,
        SRGB_ALPHA = 0x8C42,
        SRGB8_ALPHA8 = 0x8C43,
        COMPARE_REF_TO_TEXTURE = 0x884E,
        RGBA32F = 0x8814,
        RGB32F = 0x8815,
        RGBA16F = 0x881A,
        RGB16F = 0x881B,
        VERTEX_ATTRIB_ARRAY_INTEGER = 0x88FD,
        MAX_ARRAY_TEXTURE_LAYERS = 0x88FF,
        MIN_PROGRAM_TEXEL_OFFSET = 0x8904,
        MAX_PROGRAM_TEXEL_OFFSET = 0x8905,
        MAX_VARYING_COMPONENTS = 0x8B4B,
        TEXTURE_2D_ARRAY = 0x8C1A,
        TEXTURE_BINDING_2D_ARRAY = 0x8C1D,
        R11F_G11F_B10F = 0x8C3A,
        UNSIGNED_INT_10F_11F_11F_REV = 0x8C3B,
        RGB9_E5 = 0x8C3D,
        UNSIGNED_INT_5_9_9_9_REV = 0x8C3E,
        TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH = 0x8C76,
        TRANSFORM_FEEDBACK_BUFFER_MODE = 0x8C7F,
        MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS = 0x8C80,
        TRANSFORM_FEEDBACK_VARYINGS = 0x8C83,
        TRANSFORM_FEEDBACK_BUFFER_START = 0x8C84,
        TRANSFORM_FEEDBACK_BUFFER_SIZE = 0x8C85,
        TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN = 0x8C88,
        RASTERIZER_DISCARD = 0x8C89,
        MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS = 0x8C8A,
        MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS = 0x8C8B,
        INTERLEAVED_ATTRIBS = 0x8C8C,
        SEPARATE_ATTRIBS = 0x8C8D,
        TRANSFORM_FEEDBACK_BUFFER = 0x8C8E,
        TRANSFORM_FEEDBACK_BUFFER_BINDING = 0x8C8F,
        RGBA32UI = 0x8D70,
        RGB32UI = 0x8D71,
        RGBA16UI = 0x8D76,
        RGB16UI = 0x8D77,
        RGBA8UI = 0x8D7C,
        RGB8UI = 0x8D7D,
        RGBA32I = 0x8D82,
        RGB32I = 0x8D83,
        RGBA16I = 0x8D88,
        RGB16I = 0x8D89,
        RGBA8I = 0x8D8E,
        RGB8I = 0x8D8F,
        RED_INTEGER = 0x8D94,
        RGB_INTEGER = 0x8D98,
        RGBA_INTEGER = 0x8D99,
        SAMPLER_2D_ARRAY = 0x8DC1,
        SAMPLER_2D_ARRAY_SHADOW = 0x8DC4,
        SAMPLER_CUBE_SHADOW = 0x8DC5,
        UNSIGNED_INT_VEC2 = 0x8DC6,
        UNSIGNED_INT_VEC3 = 0x8DC7,
        UNSIGNED_INT_VEC4 = 0x8DC8,
        INT_SAMPLER_2D = 0x8DCA,
        INT_SAMPLER_3D = 0x8DCB,
        INT_SAMPLER_CUBE = 0x8DCC,
        INT_SAMPLER_2D_ARRAY = 0x8DCF,
        UNSIGNED_INT_SAMPLER_2D = 0x8DD2,
        UNSIGNED_INT_SAMPLER_3D = 0x8DD3,
        UNSIGNED_INT_SAMPLER_CUBE = 0x8DD4,
        UNSIGNED_INT_SAMPLER_2D_ARRAY = 0x8DD7,
        DEPTH_COMPONENT32F = 0x8CAC,
        DEPTH32F_STENCIL8 = 0x8CAD,
        FLOAT_32_UNSIGNED_INT_24_8_REV = 0x8DAD,
        FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING = 0x8210,
        FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE = 0x8211,
        FRAMEBUFFER_ATTACHMENT_RED_SIZE = 0x8212,
        FRAMEBUFFER_ATTACHMENT_GREEN_SIZE = 0x8213,
        FRAMEBUFFER_ATTACHMENT_BLUE_SIZE = 0x8214,
        FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE = 0x8215,
        FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE = 0x8216,
        FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE = 0x8217,
        FRAMEBUFFER_DEFAULT = 0x8218,
        DEPTH_STENCIL_ATTACHMENT = 0x821A,
        DEPTH_STENCIL = 0x84F9,
        UNSIGNED_INT_24_8 = 0x84FA,
        DEPTH24_STENCIL8 = 0x88F0,
        UNSIGNED_NORMALIZED = 0x8C17,
        DRAW_FRAMEBUFFER_BINDING = 0x8CA6, /* Same as FRAMEBUFFER_BINDING */
        READ_FRAMEBUFFER = 0x8CA8,
        DRAW_FRAMEBUFFER = 0x8CA9,
        READ_FRAMEBUFFER_BINDING = 0x8CAA,
        RENDERBUFFER_SAMPLES = 0x8CAB,
        FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER = 0x8CD4,
        MAX_COLOR_ATTACHMENTS = 0x8CDF,
        COLOR_ATTACHMENT1 = 0x8CE1,
        COLOR_ATTACHMENT2 = 0x8CE2,
        COLOR_ATTACHMENT3 = 0x8CE3,
        COLOR_ATTACHMENT4 = 0x8CE4,
        COLOR_ATTACHMENT5 = 0x8CE5,
        COLOR_ATTACHMENT6 = 0x8CE6,
        COLOR_ATTACHMENT7 = 0x8CE7,
        COLOR_ATTACHMENT8 = 0x8CE8,
        COLOR_ATTACHMENT9 = 0x8CE9,
        COLOR_ATTACHMENT10 = 0x8CEA,
        COLOR_ATTACHMENT11 = 0x8CEB,
        COLOR_ATTACHMENT12 = 0x8CEC,
        COLOR_ATTACHMENT13 = 0x8CED,
        COLOR_ATTACHMENT14 = 0x8CEE,
        COLOR_ATTACHMENT15 = 0x8CEF,
        FRAMEBUFFER_INCOMPLETE_MULTISAMPLE = 0x8D56,
        MAX_SAMPLES = 0x8D57,
        HALF_FLOAT = 0x140B,
        RG = 0x8227,
        RG_INTEGER = 0x8228,
        R8 = 0x8229,
        RG8 = 0x822B,
        R16F = 0x822D,
        R32F = 0x822E,
        RG16F = 0x822F,
        RG32F = 0x8230,
        R8I = 0x8231,
        R8UI = 0x8232,
        R16I = 0x8233,
        R16UI = 0x8234,
        R32I = 0x8235,
        R32UI = 0x8236,
        RG8I = 0x8237,
        RG8UI = 0x8238,
        RG16I = 0x8239,
        RG16UI = 0x823A,
        RG32I = 0x823B,
        RG32UI = 0x823C,
        VERTEX_ARRAY_BINDING = 0x85B5,
        R8_SNORM = 0x8F94,
        RG8_SNORM = 0x8F95,
        RGB8_SNORM = 0x8F96,
        RGBA8_SNORM = 0x8F97,
        SIGNED_NORMALIZED = 0x8F9C,
        COPY_READ_BUFFER = 0x8F36,
        COPY_WRITE_BUFFER = 0x8F37,
        COPY_READ_BUFFER_BINDING = 0x8F36, /* Same as COPY_READ_BUFFER */
        COPY_WRITE_BUFFER_BINDING = 0x8F37, /* Same as COPY_WRITE_BUFFER */
        UNIFORM_BUFFER = 0x8A11,
        UNIFORM_BUFFER_BINDING = 0x8A28,
        UNIFORM_BUFFER_START = 0x8A29,
        UNIFORM_BUFFER_SIZE = 0x8A2A,
        MAX_VERTEX_UNIFORM_BLOCKS = 0x8A2B,
        MAX_FRAGMENT_UNIFORM_BLOCKS = 0x8A2D,
        MAX_COMBINED_UNIFORM_BLOCKS = 0x8A2E,
        MAX_UNIFORM_BUFFER_BINDINGS = 0x8A2F,
        MAX_UNIFORM_BLOCK_SIZE = 0x8A30,
        MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS = 0x8A31,
        MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS = 0x8A33,
        UNIFORM_BUFFER_OFFSET_ALIGNMENT = 0x8A34,
        ACTIVE_UNIFORM_BLOCKS = 0x8A36,
        UNIFORM_TYPE = 0x8A37,
        UNIFORM_SIZE = 0x8A38,
        UNIFORM_BLOCK_INDEX = 0x8A3A,
        UNIFORM_OFFSET = 0x8A3B,
        UNIFORM_ARRAY_STRIDE = 0x8A3C,
        UNIFORM_MATRIX_STRIDE = 0x8A3D,
        UNIFORM_IS_ROW_MAJOR = 0x8A3E,
        UNIFORM_BLOCK_BINDING = 0x8A3F,
        UNIFORM_BLOCK_DATA_SIZE = 0x8A40,
        UNIFORM_BLOCK_ACTIVE_UNIFORMS = 0x8A42,
        UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES = 0x8A43,
        UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER = 0x8A44,
        UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER = 0x8A46,
        INVALID_INDEX = 0xFFFFFFFF,
        MAX_VERTEX_OUTPUT_COMPONENTS = 0x9122,
        MAX_FRAGMENT_INPUT_COMPONENTS = 0x9125,
        MAX_SERVER_WAIT_TIMEOUT = 0x9111,
        OBJECT_TYPE = 0x9112,
        SYNC_CONDITION = 0x9113,
        SYNC_STATUS = 0x9114,
        SYNC_FLAGS = 0x9115,
        SYNC_FENCE = 0x9116,
        SYNC_GPU_COMMANDS_COMPLETE = 0x9117,
        UNSIGNALED = 0x9118,
        SIGNALED = 0x9119,
        ALREADY_SIGNALED = 0x911A,
        TIMEOUT_EXPIRED = 0x911B,
        CONDITION_SATISFIED = 0x911C,
#if PLATFORM(WIN)
        WAIT_FAILED_WIN = 0x911D,
#else
        WAIT_FAILED = 0x911D,
#endif
        SYNC_FLUSH_COMMANDS_BIT = 0x00000001,
        VERTEX_ATTRIB_ARRAY_DIVISOR = 0x88FE,
        ANY_SAMPLES_PASSED = 0x8C2F,
        ANY_SAMPLES_PASSED_CONSERVATIVE = 0x8D6A,
        SAMPLER_BINDING = 0x8919,
        RGB10_A2UI = 0x906F,
        TEXTURE_SWIZZLE_R = 0x8E42,
        TEXTURE_SWIZZLE_G = 0x8E43,
        TEXTURE_SWIZZLE_B = 0x8E44,
        TEXTURE_SWIZZLE_A = 0x8E45,
        GREEN = 0x1904,
        BLUE = 0x1905,
        INT_2_10_10_10_REV = 0x8D9F,
        TRANSFORM_FEEDBACK = 0x8E22,
        TRANSFORM_FEEDBACK_PAUSED = 0x8E23,
        TRANSFORM_FEEDBACK_ACTIVE = 0x8E24,
        TRANSFORM_FEEDBACK_BINDING = 0x8E25,
        COMPRESSED_R11_EAC = 0x9270,
        COMPRESSED_SIGNED_R11_EAC = 0x9271,
        COMPRESSED_RG11_EAC = 0x9272,
        COMPRESSED_SIGNED_RG11_EAC = 0x9273,
        COMPRESSED_RGB8_ETC2 = 0x9274,
        COMPRESSED_SRGB8_ETC2 = 0x9275,
        COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9276,
        COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 = 0x9277,
        COMPRESSED_RGBA8_ETC2_EAC = 0x9278,
        COMPRESSED_SRGB8_ALPHA8_ETC2_EAC = 0x9279,
        TEXTURE_IMMUTABLE_FORMAT = 0x912F,
        MAX_ELEMENT_INDEX = 0x8D6B,
        NUM_SAMPLE_COUNTS = 0x9380,
        TEXTURE_IMMUTABLE_LEVELS = 0x82DF,
        PRIMITIVE_RESTART_FIXED_INDEX = 0x8D69,
        PRIMITIVE_RESTART = 0x8F9D,

        // OpenGL ES 3 constants.
        MAP_READ_BIT = 0x0001,

        // WebGL-specific.
        MAX_CLIENT_WAIT_TIMEOUT_WEBGL = 0x9247,

        // Necessary desktop OpenGL constants.
        TEXTURE_RECTANGLE_ARB = 0x84F5,
        TEXTURE_BINDING_RECTANGLE_ARB = 0x84F6,
    };

    // Attempt to enumerate all possible native image formats to
    // reduce the amount of temporary allocations during texture
    // uploading. This enum must be public because it is accessed
    // by non-member functions.
    // "_S" postfix indicates signed type.
    enum class DataFormat : uint8_t {
        RGBA8 = 0,
        RGBA8_S,
        RGBA16,
        RGBA16_S,
        RGBA16Little, // CoreGraphics-specific source format.
        RGBA16Big, // CoreGraphics-specific source format.
        RGBA32,
        RGBA32_S,
        RGBA16F,
        RGBA32F,
        RGBA2_10_10_10,
        RGB8,
        RGB8_S,
        RGB16,
        RGB16_S,
        RGB16Little, // CoreGraphics-specific source format.
        RGB16Big, // CoreGraphics-specific source format.
        RGB32,
        RGB32_S,
        RGB16F,
        RGB32F,
        BGR8,
        BGRA8,
        BGRA16Little, // CoreGraphics-specific source format.
        BGRA16Big, // CoreGraphics-specific source format.
        ARGB8,
        ARGB16Little, // CoreGraphics-specific source format.
        ARGB16Big, // CoreGraphics-specific source format.
        ABGR8,
        RGBA5551,
        RGBA4444,
        RGB565,
        RGB10F11F11F,
        RGB5999,
        RG8,
        RG8_S,
        RG16,
        RG16_S,
        RG32,
        RG32_S,
        RG16F,
        RG32F,
        R8,
        R8_S,
        R16,
        R16_S,
        R16Little, // CoreGraphics-specific source format.
        R16Big, // CoreGraphics-specific source format.
        R32,
        R32_S,
        R16F,
        R32F,
        RA8,
        RA16Little, // CoreGraphics-specific source format.
        RA16Big, // CoreGraphics-specific source format.
        RA16F,
        RA32F,
        AR8,
        AR16Little, // CoreGraphics-specific source format.
        AR16Big, // CoreGraphics-specific source format.
        A8,
        A16Little, // CoreGraphics-specific source format.
        A16Big, // CoreGraphics-specific source format.
        A16F,
        A32F,
        D16,
        D32,
        D32F,
        DS24_8,
        NumFormats
    };

    enum class Destination : uint8_t {
        Offscreen,
        DirectlyToHostWindow,
    };

    enum class ChannelBits : uint8_t {
        Red = 1,
        Green = 2,
        Blue = 4,
        Alpha = 8,
        Depth = 16,
        Stencil = 32,
        RGB = Red | Green | Blue,
        RGBA = RGB | Alpha,
        DepthStencil = Depth | Stencil,
    };

    // Possible alpha operations that may need to occur during
    // pixel packing. FIXME: kAlphaDoUnmultiply is lossy and must
    // be removed.
    enum AlphaOp : uint8_t {
        DoNothing,
        DoPremultiply,
        DoUnmultiply,
    };

    enum class DOMSource : uint8_t {
        Image,
        Canvas,
        Video,
        DOMSourceNone,
    };

#if USE(ANGLE)
    enum {
        // These constants are redefined here from ANGLE's OpenGL ES headers to
        // provide a single place where multiple portions of platform code can
        // reference them.
#if PLATFORM(MAC)
        IOSurfaceTextureTarget = TEXTURE_RECTANGLE_ARB, // also GL_TEXTURE_RECTANGLE_ANGLE
        IOSurfaceTextureTargetQuery = TEXTURE_BINDING_RECTANGLE_ARB,
        EGLIOSurfaceTextureTarget = 0x345B, // EGL_TEXTURE_RECTANGLE_ANGLE
#else
        IOSurfaceTextureTarget = TEXTURE_2D,
        IOSurfaceTextureTargetQuery = TEXTURE_BINDING_2D,
        EGLIOSurfaceTextureTarget = 0x305F, // EGL_TEXTURE_2D
#endif // PLATFORM(MAC)
    };
#endif // USE(ANGLE)

    virtual PlatformGraphicsContextGL platformGraphicsContextGL() const = 0;
    virtual PlatformGLObject platformTexture() const = 0;
    virtual PlatformLayer* platformLayer() const = 0;

    ALWAYS_INLINE static bool hasAlpha(DataFormat format)
    {
        switch (format) {
        case DataFormat::A8:
        case DataFormat::A16F:
        case DataFormat::A32F:
        case DataFormat::RA8:
        case DataFormat::AR8:
        case DataFormat::RA16F:
        case DataFormat::RA32F:
        case DataFormat::RGBA8:
        case DataFormat::BGRA8:
        case DataFormat::ARGB8:
        case DataFormat::ABGR8:
        case DataFormat::RGBA16F:
        case DataFormat::RGBA32F:
        case DataFormat::RGBA4444:
        case DataFormat::RGBA5551:
        case DataFormat::RGBA8_S:
        case DataFormat::RGBA16:
        case DataFormat::RGBA16_S:
        case DataFormat::RGBA32:
        case DataFormat::RGBA32_S:
        case DataFormat::RGBA2_10_10_10:
            return true;
        default:
            return false;
        }
    }

    ALWAYS_INLINE static bool hasColor(DataFormat format)
    {
        switch (format) {
        case DataFormat::RGBA8:
        case DataFormat::RGBA16F:
        case DataFormat::RGBA32F:
        case DataFormat::RGB8:
        case DataFormat::RGB16F:
        case DataFormat::RGB32F:
        case DataFormat::BGR8:
        case DataFormat::BGRA8:
        case DataFormat::ARGB8:
        case DataFormat::ABGR8:
        case DataFormat::RGBA5551:
        case DataFormat::RGBA4444:
        case DataFormat::RGB565:
        case DataFormat::R8:
        case DataFormat::R16F:
        case DataFormat::R32F:
        case DataFormat::RA8:
        case DataFormat::RA16F:
        case DataFormat::RA32F:
        case DataFormat::AR8:
        case DataFormat::RGBA8_S:
        case DataFormat::RGBA16:
        case DataFormat::RGBA16_S:
        case DataFormat::RGBA32:
        case DataFormat::RGBA32_S:
        case DataFormat::RGBA2_10_10_10:
        case DataFormat::RGB8_S:
        case DataFormat::RGB16:
        case DataFormat::RGB16_S:
        case DataFormat::RGB32:
        case DataFormat::RGB32_S:
        case DataFormat::RGB10F11F11F:
        case DataFormat::RGB5999:
        case DataFormat::RG8:
        case DataFormat::RG8_S:
        case DataFormat::RG16:
        case DataFormat::RG16_S:
        case DataFormat::RG32:
        case DataFormat::RG32_S:
        case DataFormat::RG16F:
        case DataFormat::RG32F:
        case DataFormat::R8_S:
        case DataFormat::R16:
        case DataFormat::R16_S:
        case DataFormat::R32:
        case DataFormat::R32_S:
            return true;
        default:
            return false;
        }
    }

    // Check if the format is one of the formats from the ImageData or DOM elements.
    // The formats from ImageData is always RGBA8.
    // The formats from DOM elements vary with Graphics ports. It can only be RGBA8 or BGRA8 for non-CG port while a little more for CG port.
    static ALWAYS_INLINE bool srcFormatComesFromDOMElementOrImageData(DataFormat SrcFormat)
    {
#if USE(CG)
#if CPU(BIG_ENDIAN)
    return SrcFormat == DataFormat::RGBA8 || SrcFormat == DataFormat::ARGB8 || SrcFormat == DataFormat::RGB8
        || SrcFormat == DataFormat::RA8 || SrcFormat == DataFormat::AR8 || SrcFormat == DataFormat::R8 || SrcFormat == DataFormat::A8;
#else
    // That LITTLE_ENDIAN case has more possible formats than BIG_ENDIAN case is because some decoded image data is actually big endian
    // even on little endian architectures.
    return SrcFormat == DataFormat::BGRA8 || SrcFormat == DataFormat::ABGR8 || SrcFormat == DataFormat::BGR8
        || SrcFormat == DataFormat::RGBA8 || SrcFormat == DataFormat::ARGB8 || SrcFormat == DataFormat::RGB8
        || SrcFormat == DataFormat::R8 || SrcFormat == DataFormat::A8
        || SrcFormat == DataFormat::RA8 || SrcFormat == DataFormat::AR8;
#endif
#else
    return SrcFormat == DataFormat::BGRA8 || SrcFormat == DataFormat::RGBA8;
#endif
    }

    struct ActiveInfo {
        String name;
        GCGLenum type;
        GCGLint size;
    };

    GraphicsContextGL(GraphicsContextGLAttributes, Destination = Destination::Offscreen, GraphicsContextGL* sharedContext = nullptr);
    virtual ~GraphicsContextGL() = default;

    // ========== WebGL 1 entry points.

    virtual void activeTexture(GCGLenum texture) = 0;
    virtual void attachShader(PlatformGLObject program, PlatformGLObject shader) = 0;
    virtual void bindAttribLocation(PlatformGLObject, GCGLuint index, const String& name) = 0;
    virtual void bindBuffer(GCGLenum target, PlatformGLObject) = 0;
    virtual void bindFramebuffer(GCGLenum target, PlatformGLObject) = 0;
    virtual void bindRenderbuffer(GCGLenum target, PlatformGLObject) = 0;
    virtual void bindTexture(GCGLenum target, PlatformGLObject) = 0;
    virtual void blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) = 0;
    virtual void blendEquation(GCGLenum mode) = 0;
    virtual void blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha) = 0;
    virtual void blendFunc(GCGLenum sfactor, GCGLenum dfactor) = 0;
    virtual void blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha) = 0;

    virtual GCGLenum checkFramebufferStatus(GCGLenum target) = 0;
    virtual void clear(GCGLbitfield mask) = 0;
    virtual void clearColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha) = 0;
    virtual void clearDepth(GCGLclampf depth) = 0;
    virtual void clearStencil(GCGLint s) = 0;
    virtual void colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha) = 0;
    virtual void compileShader(PlatformGLObject) = 0;

    virtual void copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border) = 0;
    virtual void copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) = 0;

    virtual PlatformGLObject createBuffer() = 0;
    virtual PlatformGLObject createFramebuffer() = 0;
    virtual PlatformGLObject createProgram() = 0;
    virtual PlatformGLObject createRenderbuffer() = 0;
    virtual PlatformGLObject createShader(GCGLenum) = 0;
    virtual PlatformGLObject createTexture() = 0;

    virtual void cullFace(GCGLenum mode) = 0;

    virtual void deleteBuffer(PlatformGLObject) = 0;
    virtual void deleteFramebuffer(PlatformGLObject) = 0;
    virtual void deleteProgram(PlatformGLObject) = 0;
    virtual void deleteRenderbuffer(PlatformGLObject) = 0;
    virtual void deleteShader(PlatformGLObject) = 0;
    virtual void deleteTexture(PlatformGLObject) = 0;

    virtual void depthFunc(GCGLenum func) = 0;
    virtual void depthMask(GCGLboolean flag) = 0;
    virtual void depthRange(GCGLclampf zNear, GCGLclampf zFar) = 0;
    virtual void detachShader(PlatformGLObject, PlatformGLObject) = 0;
    virtual void disable(GCGLenum cap) = 0;
    virtual void disableVertexAttribArray(GCGLuint index) = 0;
    virtual void drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count) = 0;
    virtual void drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset) = 0;

    virtual void enable(GCGLenum cap) = 0;
    virtual void enableVertexAttribArray(GCGLuint index) = 0;
    virtual void finish() = 0;
    virtual void flush() = 0;
    virtual void framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject) = 0;
    virtual void framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject, GCGLint level) = 0;
    virtual void frontFace(GCGLenum mode) = 0;

    virtual void generateMipmap(GCGLenum target) = 0;

    virtual bool getActiveAttrib(PlatformGLObject program, GCGLuint index, ActiveInfo&) = 0;
    virtual bool getActiveUniform(PlatformGLObject program, GCGLuint index, ActiveInfo&) = 0;
    virtual void getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders) = 0;

    virtual GCGLint getAttribLocation(PlatformGLObject, const String& name) = 0;

    virtual void getBufferParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value) = 0;

    // getParameter
    virtual String getString(GCGLenum name) = 0;
    virtual void getFloatv(GCGLenum pname, GCGLfloat* value) = 0;
    virtual void getIntegerv(GCGLenum pname, GCGLint* value) = 0;
    virtual void getIntegeri_v(GCGLenum pname, GCGLuint index, GCGLint* value) = 0;
    virtual void getInteger64v(GCGLenum pname, GCGLint64* value) = 0;
    virtual void getInteger64i_v(GCGLenum pname, GCGLuint index, GCGLint64* value) = 0;
    virtual void getProgramiv(PlatformGLObject program, GCGLenum pname, GCGLint* value) = 0;
    virtual void getBooleanv(GCGLenum pname, GCGLboolean* value) = 0;

    virtual GCGLenum getError() = 0;

    // getFramebufferAttachmentParameter
    virtual void getFramebufferAttachmentParameteriv(GCGLenum target, GCGLenum attachment, GCGLenum pname, GCGLint* value) = 0;

    // getProgramParameter
    virtual String getProgramInfoLog(PlatformGLObject) = 0;

    // getRenderbufferParameter
    virtual void getRenderbufferParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value) = 0;

    // getShaderParameter
    virtual void getShaderiv(PlatformGLObject, GCGLenum pname, GCGLint* value) = 0;

    virtual String getShaderInfoLog(PlatformGLObject) = 0;
    virtual void getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLint* range, GCGLint* precision) = 0;

    virtual String getShaderSource(PlatformGLObject) = 0;

    // getTexParameter
    virtual void getTexParameterfv(GCGLenum target, GCGLenum pname, GCGLfloat* value) = 0;
    virtual void getTexParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value) = 0;

    // getUniform
    virtual void getUniformfv(PlatformGLObject program, GCGLint location, GCGLfloat* value) = 0;
    virtual void getUniformiv(PlatformGLObject program, GCGLint location, GCGLint* value) = 0;
    virtual void getUniformuiv(PlatformGLObject program, GCGLint location, GCGLuint* value) = 0;

    virtual GCGLint getUniformLocation(PlatformGLObject, const String& name) = 0;

    // getVertexAttrib
    virtual void getVertexAttribfv(GCGLuint index, GCGLenum pname, GCGLfloat* value) = 0;
    virtual void getVertexAttribiv(GCGLuint index, GCGLenum pname, GCGLint* value) = 0;

    virtual GCGLsizeiptr getVertexAttribOffset(GCGLuint index, GCGLenum pname) = 0;

    virtual void hint(GCGLenum target, GCGLenum mode) = 0;
    virtual GCGLboolean isBuffer(PlatformGLObject) = 0;
    virtual GCGLboolean isEnabled(GCGLenum cap) = 0;
    virtual GCGLboolean isFramebuffer(PlatformGLObject) = 0;
    virtual GCGLboolean isProgram(PlatformGLObject) = 0;
    virtual GCGLboolean isRenderbuffer(PlatformGLObject) = 0;
    virtual GCGLboolean isShader(PlatformGLObject) = 0;
    virtual GCGLboolean isTexture(PlatformGLObject) = 0;
    virtual void lineWidth(GCGLfloat) = 0;
    virtual void linkProgram(PlatformGLObject) = 0;
    virtual void pixelStorei(GCGLenum pname, GCGLint param) = 0;
    virtual void polygonOffset(GCGLfloat factor, GCGLfloat units) = 0;

    virtual void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) = 0;
    virtual void sampleCoverage(GCGLclampf value, GCGLboolean invert) = 0;
    virtual void scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) = 0;

    virtual void shaderSource(PlatformGLObject, const String& string) = 0;

    virtual void stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask) = 0;
    virtual void stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask) = 0;
    virtual void stencilMask(GCGLuint mask) = 0;
    virtual void stencilMaskSeparate(GCGLenum face, GCGLuint mask) = 0;
    virtual void stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass) = 0;
    virtual void stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass) = 0;

    virtual void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param) = 0;
    virtual void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param) = 0;

    virtual void uniform1f(GCGLint location, GCGLfloat x) = 0;
    virtual void uniform1fv(GCGLint location, GCGLsizei, const GCGLfloat* v) = 0;
    virtual void uniform1i(GCGLint location, GCGLint x) = 0;
    virtual void uniform1iv(GCGLint location, GCGLsizei, const GCGLint* v) = 0;
    virtual void uniform2f(GCGLint location, GCGLfloat x, GCGLfloat y) = 0;
    virtual void uniform2fv(GCGLint location, GCGLsizei, const GCGLfloat* v) = 0;
    virtual void uniform2i(GCGLint location, GCGLint x, GCGLint y) = 0;
    virtual void uniform2iv(GCGLint location, GCGLsizei, const GCGLint* v) = 0;
    virtual void uniform3f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z) = 0;
    virtual void uniform3fv(GCGLint location, GCGLsizei, const GCGLfloat* v) = 0;
    virtual void uniform3i(GCGLint location, GCGLint x, GCGLint y, GCGLint z) = 0;
    virtual void uniform3iv(GCGLint location, GCGLsizei, const GCGLint* v) = 0;
    virtual void uniform4f(GCGLint location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) = 0;
    virtual void uniform4fv(GCGLint location, GCGLsizei, const GCGLfloat* v) = 0;
    virtual void uniform4i(GCGLint location, GCGLint x, GCGLint y, GCGLint z, GCGLint w) = 0;
    virtual void uniform4iv(GCGLint location, GCGLsizei, const GCGLint* v) = 0;
    virtual void uniformMatrix2fv(GCGLint location, GCGLsizei, GCGLboolean transpose, const GCGLfloat* value) = 0;
    virtual void uniformMatrix3fv(GCGLint location, GCGLsizei, GCGLboolean transpose, const GCGLfloat* value) = 0;
    virtual void uniformMatrix4fv(GCGLint location, GCGLsizei, GCGLboolean transpose, const GCGLfloat* value) = 0;

    virtual void useProgram(PlatformGLObject) = 0;
    virtual void validateProgram(PlatformGLObject) = 0;

    virtual void vertexAttrib1f(GCGLuint index, GCGLfloat x) = 0;
    virtual void vertexAttrib1fv(GCGLuint index, const GCGLfloat* values) = 0;
    virtual void vertexAttrib2f(GCGLuint index, GCGLfloat x, GCGLfloat y) = 0;
    virtual void vertexAttrib2fv(GCGLuint index, const GCGLfloat* values) = 0;
    virtual void vertexAttrib3f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z) = 0;
    virtual void vertexAttrib3fv(GCGLuint index, const GCGLfloat* values) = 0;
    virtual void vertexAttrib4f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w) = 0;
    virtual void vertexAttrib4fv(GCGLuint index, const GCGLfloat* values) = 0;

    virtual void vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset) = 0;

    virtual void viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) = 0;

    virtual void bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage) = 0;
    virtual void bufferData(GCGLenum target, GCGLsizeiptr size, const void* data, GCGLenum usage) = 0;
    virtual void bufferSubData(GCGLenum target, GCGLintptr offset, GCGLsizeiptr size, const void* data) = 0;

    virtual void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, const void* data) = 0;
    virtual void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, const void* data) = 0;

    virtual void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, void* data) = 0;

    virtual bool texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels) = 0;
    virtual void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, const void* pixels) = 0;

    virtual void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount) = 0;
    virtual void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount) = 0;
    virtual void vertexAttribDivisor(GCGLuint index, GCGLuint divisor) = 0;

    GraphicsContextGLAttributes contextAttributes() const { return m_attrs; }
    void setContextAttributes(const GraphicsContextGLAttributes& attrs) { m_attrs = attrs; }

    // VertexArrayOject calls
    virtual PlatformGLObject createVertexArray() = 0;
    virtual void deleteVertexArray(PlatformGLObject) = 0;
    virtual GCGLboolean isVertexArray(PlatformGLObject) = 0;
    virtual void bindVertexArray(PlatformGLObject) = 0;

#if USE(OPENGL) && ENABLE(WEBGL2)
    virtual void primitiveRestartIndex(GCGLuint) = 0;
#endif

    // Support for extensions. Returns a non-null object, though not
    // all methods it contains may necessarily be supported on the
    // current hardware. Must call ExtensionsGL::supports() to
    // determine this.
    virtual ExtensionsGL& getExtensions() = 0;

    // ========== WebGL 2 entry points.

    virtual void bufferData(GCGLenum target, const void* data, GCGLenum usage, GCGLuint srcOffset, GCGLuint length) = 0;
    virtual void bufferSubData(GCGLenum target, GCGLintptr dstByteOffset, const void* srcData, GCGLuint srcOffset, GCGLuint length) = 0;

    virtual void copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size) = 0;
    virtual void getBufferSubData(GCGLenum target, GCGLintptr srcByteOffset, const void* dstData, GCGLuint dstOffset, GCGLuint length) = 0;
    virtual void* mapBufferRange(GCGLenum target, GCGLintptr offset, GCGLsizeiptr length, GCGLbitfield access) = 0;
    virtual GCGLboolean unmapBuffer(GCGLenum target) = 0;

    virtual void blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter) = 0;
    virtual void framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer) = 0;
    virtual void invalidateFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments) = 0;
    virtual void invalidateSubFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) = 0;
    virtual void readBuffer(GCGLenum src) = 0;

    // getInternalFormatParameter
    virtual void getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLsizei bufSize, GCGLint* params) = 0;
    virtual void renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) = 0;

    virtual void texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) = 0;
    virtual void texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth) = 0;

    // tex{Sub}Image3D are supported only via ANGLE_robust_client_memory.

    virtual void copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height) = 0;

    // compressedTex{Sub}Image3D are supported only via ANGLE_robust_client_memory.

    virtual GCGLint getFragDataLocation(PlatformGLObject program, const String& name) = 0;

    virtual void uniform1ui(GCGLint location, GCGLuint v0) = 0;
    virtual void uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1) = 0;
    virtual void uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2) = 0;
    virtual void uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3) = 0;
    virtual void uniform1uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform2uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform3uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform4uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w) = 0;
    virtual void vertexAttribI4iv(GCGLuint index, const GCGLint* values) = 0;
    virtual void vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w) = 0;
    virtual void vertexAttribI4uiv(GCGLuint index, const GCGLuint* values) = 0;
    virtual void vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset) = 0;

    virtual void drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset) = 0;

    virtual void drawBuffers(const Vector<GCGLenum>& buffers) = 0;
    virtual void clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, const GCGLint* values, GCGLuint srcOffset) = 0;
    virtual void clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, const GCGLuint* values, GCGLuint srcOffset) = 0;
    virtual void clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, const GCGLfloat* values, GCGLuint srcOffset) = 0;
    virtual void clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil) = 0;

    virtual PlatformGLObject createQuery() = 0;
    virtual void deleteQuery(PlatformGLObject query) = 0;
    virtual GCGLboolean isQuery(PlatformGLObject query) = 0;
    virtual void beginQuery(GCGLenum target, PlatformGLObject query) = 0;
    virtual void endQuery(GCGLenum target) = 0;
    virtual PlatformGLObject getQuery(GCGLenum target, GCGLenum pname) = 0;
    // getQueryParameter
    virtual void getQueryObjectuiv(PlatformGLObject query, GCGLenum pname, GCGLuint* value) = 0;

    virtual PlatformGLObject createSampler() = 0;
    virtual void deleteSampler(PlatformGLObject sampler) = 0;
    virtual GCGLboolean isSampler(PlatformGLObject sampler) = 0;
    virtual void bindSampler(GCGLuint unit, PlatformGLObject sampler) = 0;
    virtual void samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param) = 0;
    virtual void samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param) = 0;
    // getSamplerParameter
    virtual void getSamplerParameterfv(PlatformGLObject sampler, GCGLenum pname, GCGLfloat* value) = 0;
    virtual void getSamplerParameteriv(PlatformGLObject sampler, GCGLenum pname, GCGLint* value) = 0;

    virtual GCGLsync fenceSync(GCGLenum condition, GCGLbitfield flags) = 0;
    virtual GCGLboolean isSync(GCGLsync) = 0;
    virtual void deleteSync(GCGLsync) = 0;
    virtual GCGLenum clientWaitSync(GCGLsync, GCGLbitfield flags, GCGLuint64 timeout) = 0;
    virtual void waitSync(GCGLsync, GCGLbitfield flags, GCGLint64 timeout) = 0;
    // getSyncParameter
    virtual void getSynciv(GCGLsync, GCGLenum pname, GCGLsizei bufSize, GCGLint* value) = 0;

    virtual PlatformGLObject createTransformFeedback() = 0;
    virtual void deleteTransformFeedback(PlatformGLObject id) = 0;
    virtual GCGLboolean isTransformFeedback(PlatformGLObject id) = 0;
    virtual void bindTransformFeedback(GCGLenum target, PlatformGLObject id) = 0;
    virtual void beginTransformFeedback(GCGLenum primitiveMode) = 0;
    virtual void endTransformFeedback() = 0;
    virtual void transformFeedbackVaryings(PlatformGLObject program, const Vector<String>& varyings, GCGLenum bufferMode) = 0;
    virtual void getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index, ActiveInfo&) = 0;
    virtual void pauseTransformFeedback() = 0;
    virtual void resumeTransformFeedback() = 0;

    virtual void bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer) = 0;
    virtual void bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size) = 0;
    // getIndexedParameter -> use getParameter calls above.
    virtual Vector<GCGLuint> getUniformIndices(PlatformGLObject program, const Vector<String>& uniformNames) = 0;
    virtual void getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname, Vector<GCGLint>& params) = 0;

    virtual GCGLuint getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName) = 0;
    // getActiveUniformBlockParameter
    virtual void getActiveUniformBlockiv(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLint* params) = 0;
    virtual String getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex) = 0;
    virtual void uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding) = 0;

    // texSubImage2D with pixel pack buffer, and srcData+offset, are
    // supported only via ANGLE_robust_client_memory.

    virtual void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLintptr offset) = 0;
    virtual void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, const void* srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride) = 0;

    virtual void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset) = 0;
    virtual void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, const void* srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride) = 0;

    virtual void uniform1fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform2fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform3fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform4fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;

    virtual void uniform1iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform2iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform3iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniform4iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;

    virtual void uniformMatrix2fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix3fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;
    virtual void uniformMatrix4fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength) = 0;

    virtual void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset) = 0;
    virtual void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, const void* dstData, GCGLuint dstOffset) = 0;

    // ========== Non-WebGL based entry points.

    static unsigned getClearBitsByAttachmentType(GCGLenum);
    static unsigned getClearBitsByFormat(GCGLenum);

    static uint8_t getChannelBitsByFormat(GCGLenum);

    Destination destination() const { return m_destination; }

private:
    GraphicsContextGLAttributes m_attrs;
    Destination m_destination;
};

} // namespace WebCore

#endif
