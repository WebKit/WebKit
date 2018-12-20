/*
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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

#include "ANGLEWebKitBridge.h"
#include "GraphicsContext3DAttributes.h"
#include "GraphicsTypes3D.h"
#include "Image.h"
#include "IntRect.h"
#include "PlatformLayer.h"
#include <memory>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/WTFString.h>

#if USE(CA)
#include "PlatformCALayer.h"
#endif

// FIXME: Find a better way to avoid the name confliction for NO_ERROR.
#if PLATFORM(WIN)
#undef NO_ERROR
#elif PLATFORM(GTK)
// This define is from the X11 headers, but it's used below, so we must undefine it.
#undef VERSION
#endif

#if PLATFORM(COCOA)

#if USE(OPENGL_ES)
#include <OpenGLES/ES2/gl.h>
#ifdef __OBJC__
#import <OpenGLES/EAGL.h>
typedef EAGLContext* PlatformGraphicsContext3D;
#else
typedef void* PlatformGraphicsContext3D;
#endif // __OBJC__
#endif // USE(OPENGL_ES)

#if !USE(OPENGL_ES)
typedef struct _CGLContextObject *CGLContextObj;
typedef CGLContextObj PlatformGraphicsContext3D;
#endif

OBJC_CLASS CALayer;
OBJC_CLASS WebGLLayer;
typedef struct __IOSurface* IOSurfaceRef;
#endif // PLATFORM(COCOA)

#if USE(NICOSIA)
namespace Nicosia {
class GC3DLayer;
}
#endif

#if !PLATFORM(COCOA)
typedef unsigned GLuint;
typedef void* PlatformGraphicsContext3D;
typedef void* PlatformGraphicsSurface3D;
#endif // !PLATFORM(COCOA)

// These are currently the same among all implementations.
const PlatformGraphicsContext3D NullPlatformGraphicsContext3D = 0;
const Platform3DObject NullPlatform3DObject = 0;

namespace WebCore {
class Extensions3D;
#if !PLATFORM(COCOA) && USE(OPENGL_ES)
class Extensions3DOpenGLES;
#else
class Extensions3DOpenGL;
#endif
class HostWindow;
class Image;
class ImageBuffer;
class ImageData;
class IntRect;
class IntSize;
class WebGLRenderingContextBase;
#if USE(TEXTURE_MAPPER)
class TextureMapperGC3DPlatformLayer;
#endif

typedef WTF::HashMap<CString, uint64_t> ShaderNameHash;

struct ActiveInfo {
    String name;
    GC3Denum type;
    GC3Dint size;
};

class GraphicsContext3DPrivate;

class GraphicsContext3D : public RefCounted<GraphicsContext3D> {
public:
    enum {
        // WebGL 1 constants
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

        // OpenGL ES 3 constants
        MAP_READ_BIT = 0x0001
    };

    enum RenderStyle {
        RenderOffscreen,
        RenderDirectlyToHostWindow,
    };

    class ContextLostCallback {
    public:
        virtual void onContextLost() = 0;
        virtual ~ContextLostCallback() = default;
    };

    class ErrorMessageCallback {
    public:
        virtual void onErrorMessage(const String& message, GC3Dint id) = 0;
        virtual ~ErrorMessageCallback() = default;
    };

    void setContextLostCallback(std::unique_ptr<ContextLostCallback>);
    void setErrorMessageCallback(std::unique_ptr<ErrorMessageCallback>);

    static RefPtr<GraphicsContext3D> create(GraphicsContext3DAttributes, HostWindow*, RenderStyle = RenderOffscreen);
    ~GraphicsContext3D();

#if PLATFORM(COCOA)
    static Ref<GraphicsContext3D> createShared(GraphicsContext3D& sharedContext);
#endif

#if PLATFORM(COCOA)
    PlatformGraphicsContext3D platformGraphicsContext3D() const { return m_contextObj; }
    Platform3DObject platformTexture() const { return m_texture; }
    CALayer* platformLayer() const { return reinterpret_cast<CALayer*>(m_webGLLayer.get()); }
#else
    PlatformGraphicsContext3D platformGraphicsContext3D();
    Platform3DObject platformTexture() const;
    PlatformLayer* platformLayer() const;
#endif

    bool makeContextCurrent();
    void setWebGLContext(WebGLRenderingContextBase* base) { m_webglContext = base; }

    // With multisampling on, blit from multisampleFBO to regular FBO.
    void prepareTexture();

    // Equivalent to ::glTexImage2D(). Allows pixels==0 with no allocation.
    void texImage2DDirect(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels);

    // Get an attribute location without checking the name -> mangledname mapping.
    int getAttribLocationDirect(Platform3DObject program, const String& name);

    // Compile a shader without going through ANGLE.
    void compileShaderDirect(Platform3DObject);

    // Helper to texImage2D with pixel==0 case: pixels are initialized to 0.
    // Return true if no GL error is synthesized.
    // By default, alignment is 4, the OpenGL default setting.
    bool texImage2DResourceSafe(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint alignment = 4);

    bool isGLES2Compliant() const;

    //----------------------------------------------------------------------
    // Helpers for texture uploading and pixel readback.
    //

    // Computes the components per pixel and bytes per component
    // for the given format and type combination. Returns false if
    // either was an invalid enum.
    static bool computeFormatAndTypeParameters(GC3Denum format,
                                               GC3Denum type,
                                               unsigned int* componentsPerPixel,
                                               unsigned int* bytesPerComponent);

    // Computes the image size in bytes. If paddingInBytes is not null, padding
    // is also calculated in return. Returns NO_ERROR if succeed, otherwise
    // return the suggested GL error indicating the cause of the failure:
    //   INVALID_VALUE if width/height is negative or overflow happens.
    //   INVALID_ENUM if format/type is illegal.
    static GC3Denum computeImageSizeInBytes(GC3Denum format,
                                     GC3Denum type,
                                     GC3Dsizei width,
                                     GC3Dsizei height,
                                     GC3Dint alignment,
                                     unsigned int* imageSizeInBytes,
                                     unsigned int* paddingInBytes);

    static bool possibleFormatAndTypeForInternalFormat(GC3Denum internalFormat, GC3Denum& format, GC3Denum& type);

    // Extracts the contents of the given ImageData into the passed Vector,
    // packing the pixel data according to the given format and type,
    // and obeying the flipY and premultiplyAlpha flags. Returns true
    // upon success.
    static bool extractImageData(ImageData*,
                          GC3Denum format,
                          GC3Denum type,
                          bool flipY,
                          bool premultiplyAlpha,
                          Vector<uint8_t>& data);

    // Helper function which extracts the user-supplied texture
    // data, applying the flipY and premultiplyAlpha parameters.
    // If the data is not tightly packed according to the passed
    // unpackAlignment, the output data will be tightly packed.
    // Returns true if successful, false if any error occurred.
    static bool extractTextureData(unsigned int width, unsigned int height,
                            GC3Denum format, GC3Denum type,
                            unsigned int unpackAlignment,
                            bool flipY, bool premultiplyAlpha,
                            const void* pixels,
                            Vector<uint8_t>& data);


    // Attempt to enumerate all possible native image formats to
    // reduce the amount of temporary allocations during texture
    // uploading. This enum must be public because it is accessed
    // by non-member functions.
    enum DataFormat {
        DataFormatRGBA8 = 0,
        DataFormatRGBA16Little,
        DataFormatRGBA16Big,
        DataFormatRGBA16F,
        DataFormatRGBA32F,
        DataFormatRGB8,
        DataFormatRGB16Little,
        DataFormatRGB16Big,
        DataFormatRGB16F,
        DataFormatRGB32F,
        DataFormatBGR8,
        DataFormatBGRA8,
        DataFormatBGRA16Little,
        DataFormatBGRA16Big,
        DataFormatARGB8,
        DataFormatARGB16Little,
        DataFormatARGB16Big,
        DataFormatABGR8,
        DataFormatRGBA5551,
        DataFormatRGBA4444,
        DataFormatRGB565,
        DataFormatR8,
        DataFormatR16Little,
        DataFormatR16Big,
        DataFormatR16F,
        DataFormatR32F,
        DataFormatRA8,
        DataFormatRA16Little,
        DataFormatRA16Big,
        DataFormatRA16F,
        DataFormatRA32F,
        DataFormatAR8,
        DataFormatAR16Little,
        DataFormatAR16Big,
        DataFormatA8,
        DataFormatA16Little,
        DataFormatA16Big,
        DataFormatA16F,
        DataFormatA32F,
        DataFormatNumFormats
    };

    ALWAYS_INLINE static bool hasAlpha(DataFormat format)
    {
        switch (format) {
        case GraphicsContext3D::DataFormatA8:
        case GraphicsContext3D::DataFormatA16F:
        case GraphicsContext3D::DataFormatA32F:
        case GraphicsContext3D::DataFormatRA8:
        case GraphicsContext3D::DataFormatAR8:
        case GraphicsContext3D::DataFormatRA16F:
        case GraphicsContext3D::DataFormatRA32F:
        case GraphicsContext3D::DataFormatRGBA8:
        case GraphicsContext3D::DataFormatBGRA8:
        case GraphicsContext3D::DataFormatARGB8:
        case GraphicsContext3D::DataFormatABGR8:
        case GraphicsContext3D::DataFormatRGBA16F:
        case GraphicsContext3D::DataFormatRGBA32F:
        case GraphicsContext3D::DataFormatRGBA4444:
        case GraphicsContext3D::DataFormatRGBA5551:
            return true;
        default:
            return false;
        }
    }

    ALWAYS_INLINE static bool hasColor(DataFormat format)
    {
        switch (format) {
        case GraphicsContext3D::DataFormatRGBA8:
        case GraphicsContext3D::DataFormatRGBA16F:
        case GraphicsContext3D::DataFormatRGBA32F:
        case GraphicsContext3D::DataFormatRGB8:
        case GraphicsContext3D::DataFormatRGB16F:
        case GraphicsContext3D::DataFormatRGB32F:
        case GraphicsContext3D::DataFormatBGR8:
        case GraphicsContext3D::DataFormatBGRA8:
        case GraphicsContext3D::DataFormatARGB8:
        case GraphicsContext3D::DataFormatABGR8:
        case GraphicsContext3D::DataFormatRGBA5551:
        case GraphicsContext3D::DataFormatRGBA4444:
        case GraphicsContext3D::DataFormatRGB565:
        case GraphicsContext3D::DataFormatR8:
        case GraphicsContext3D::DataFormatR16F:
        case GraphicsContext3D::DataFormatR32F:
        case GraphicsContext3D::DataFormatRA8:
        case GraphicsContext3D::DataFormatRA16F:
        case GraphicsContext3D::DataFormatRA32F:
        case GraphicsContext3D::DataFormatAR8:
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
    return SrcFormat == DataFormatRGBA8 || SrcFormat == DataFormatARGB8 || SrcFormat == DataFormatRGB8
        || SrcFormat == DataFormatRA8 || SrcFormat == DataFormatAR8 || SrcFormat == DataFormatR8 || SrcFormat == DataFormatA8;
#else
    // That LITTLE_ENDIAN case has more possible formats than BIG_ENDIAN case is because some decoded image data is actually big endian
    // even on little endian architectures.
    return SrcFormat == DataFormatBGRA8 || SrcFormat == DataFormatABGR8 || SrcFormat == DataFormatBGR8
        || SrcFormat == DataFormatRGBA8 || SrcFormat == DataFormatARGB8 || SrcFormat == DataFormatRGB8
        || SrcFormat == DataFormatR8 || SrcFormat == DataFormatA8
        || SrcFormat == DataFormatRA8 || SrcFormat == DataFormatAR8;
#endif
#else
    return SrcFormat == DataFormatBGRA8 || SrcFormat == DataFormatRGBA8;
#endif
    }

    //----------------------------------------------------------------------
    // Entry points for WebGL.
    //

    void activeTexture(GC3Denum texture);
    void attachShader(Platform3DObject program, Platform3DObject shader);
    void bindAttribLocation(Platform3DObject, GC3Duint index, const String& name);
    void bindBuffer(GC3Denum target, Platform3DObject);
    void bindFramebuffer(GC3Denum target, Platform3DObject);
    void bindRenderbuffer(GC3Denum target, Platform3DObject);
    void bindTexture(GC3Denum target, Platform3DObject);
    void blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha);
    void blendEquation(GC3Denum mode);
    void blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha);
    void blendFunc(GC3Denum sfactor, GC3Denum dfactor);
    void blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha);

    void bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage);
    void bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage);
    void bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data);

    void* mapBufferRange(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr length, GC3Dbitfield access);
    GC3Dboolean unmapBuffer(GC3Denum target);
    void copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dintptr readOffset, GC3Dintptr writeOffset, GC3Dsizeiptr);

    void getInternalformativ(GC3Denum target, GC3Denum internalformat, GC3Denum pname, GC3Dsizei bufSize, GC3Dint* params);
    void renderbufferStorageMultisample(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);

    void texStorage2D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);
    void texStorage3D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth);

    void getActiveUniforms(Platform3DObject program, const Vector<GC3Duint>& uniformIndices, GC3Denum pname, Vector<GC3Dint>& params);

    GC3Denum checkFramebufferStatus(GC3Denum target);
    void clear(GC3Dbitfield mask);
    void clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha);
    void clearDepth(GC3Dclampf depth);
    void clearStencil(GC3Dint s);
    void colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha);
    void compileShader(Platform3DObject);

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data);
    void compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data);
    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border);
    void copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void cullFace(GC3Denum mode);
    void depthFunc(GC3Denum func);
    void depthMask(GC3Dboolean flag);
    void depthRange(GC3Dclampf zNear, GC3Dclampf zFar);
    void detachShader(Platform3DObject, Platform3DObject);
    void disable(GC3Denum cap);
    void disableVertexAttribArray(GC3Duint index);
    void drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count);
    void drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset);

    void enable(GC3Denum cap);
    void enableVertexAttribArray(GC3Duint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject);
    void framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject, GC3Dint level);
    void frontFace(GC3Denum mode);
    void generateMipmap(GC3Denum target);

    bool getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo&);
    bool getActiveAttribImpl(Platform3DObject program, GC3Duint index, ActiveInfo&);
    bool getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo&);
    bool getActiveUniformImpl(Platform3DObject program, GC3Duint index, ActiveInfo&);
    void getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders);
    GC3Dint getAttribLocation(Platform3DObject, const String& name);
    void getBooleanv(GC3Denum pname, GC3Dboolean* value);
    void getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value);
    GraphicsContext3DAttributes getContextAttributes();
    GC3Denum getError();
    void getFloatv(GC3Denum pname, GC3Dfloat* value);
    void getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum pname, GC3Dint* value);
    void getIntegerv(GC3Denum pname, GC3Dint* value);
    void getInteger64v(GC3Denum pname, GC3Dint64* value);
    void getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value);
    void getNonBuiltInActiveSymbolCount(Platform3DObject program, GC3Denum pname, GC3Dint* value);
    String getProgramInfoLog(Platform3DObject);
    String getUnmangledInfoLog(Platform3DObject[2], GC3Dsizei, const String&);
    void getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value);
    void getShaderiv(Platform3DObject, GC3Denum pname, GC3Dint* value);
    String getShaderInfoLog(Platform3DObject);
    void getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision);
    String getShaderSource(Platform3DObject);
    String getString(GC3Denum name);
    void getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value);
    void getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value);
    void getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value);
    void getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value);
    GC3Dint getUniformLocation(Platform3DObject, const String& name);
    void getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value);
    void getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value);
    GC3Dsizeiptr getVertexAttribOffset(GC3Duint index, GC3Denum pname);

    void hint(GC3Denum target, GC3Denum mode);
    GC3Dboolean isBuffer(Platform3DObject);
    GC3Dboolean isEnabled(GC3Denum cap);
    GC3Dboolean isFramebuffer(Platform3DObject);
    GC3Dboolean isProgram(Platform3DObject);
    GC3Dboolean isRenderbuffer(Platform3DObject);
    GC3Dboolean isShader(Platform3DObject);
    GC3Dboolean isTexture(Platform3DObject);
    void lineWidth(GC3Dfloat);
    void linkProgram(Platform3DObject);
    void pixelStorei(GC3Denum pname, GC3Dint param);
    void polygonOffset(GC3Dfloat factor, GC3Dfloat units);

    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data);

    void releaseShaderCompiler();

    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);
    void sampleCoverage(GC3Dclampf value, GC3Dboolean invert);
    void scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void shaderSource(Platform3DObject, const String& string);
    void stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilMask(GC3Duint mask);
    void stencilMaskSeparate(GC3Denum face, GC3Duint mask);
    void stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass);
    void stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass);

    bool texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels);
    void texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param);
    void texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels);

    void uniform1f(GC3Dint location, GC3Dfloat x);
    void uniform1fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v);
    void uniform1i(GC3Dint location, GC3Dint x);
    void uniform1iv(GC3Dint location, GC3Dsizei, const GC3Dint* v);
    void uniform2f(GC3Dint location, GC3Dfloat x, GC3Dfloat y);
    void uniform2fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v);
    void uniform2i(GC3Dint location, GC3Dint x, GC3Dint y);
    void uniform2iv(GC3Dint location, GC3Dsizei, const GC3Dint* v);
    void uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void uniform3fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v);
    void uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z);
    void uniform3iv(GC3Dint location, GC3Dsizei, const GC3Dint* v);
    void uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void uniform4fv(GC3Dint location, GC3Dsizei, const GC3Dfloat* v);
    void uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w);
    void uniform4iv(GC3Dint location, GC3Dsizei, const GC3Dint* v);
    void uniformMatrix2fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, const GC3Dfloat* value);
    void uniformMatrix3fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, const GC3Dfloat* value);
    void uniformMatrix4fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, const GC3Dfloat* value);

    void useProgram(Platform3DObject);
    void validateProgram(Platform3DObject);
    bool checkVaryingsPacking(Platform3DObject vertexShader, Platform3DObject fragmentShader) const;
    bool precisionsMatch(Platform3DObject vertexShader, Platform3DObject fragmentShader) const;

    void vertexAttrib1f(GC3Duint index, GC3Dfloat x);
    void vertexAttrib1fv(GC3Duint index, const GC3Dfloat* values);
    void vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y);
    void vertexAttrib2fv(GC3Duint index, const GC3Dfloat* values);
    void vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void vertexAttrib3fv(GC3Duint index, const GC3Dfloat* values);
    void vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void vertexAttrib4fv(GC3Duint index, const GC3Dfloat* values);
    void vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized,
                             GC3Dsizei stride, GC3Dintptr offset);

    void viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    void reshape(int width, int height);

    void drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount);
    void drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset, GC3Dsizei primcount);
    void vertexAttribDivisor(GC3Duint index, GC3Duint divisor);

    // VertexArrayOject calls
    Platform3DObject createVertexArray();
    void deleteVertexArray(Platform3DObject);
    GC3Dboolean isVertexArray(Platform3DObject);
    void bindVertexArray(Platform3DObject);

    void paintToCanvas(const unsigned char* imagePixels, const IntSize& imageSize, const IntSize& canvasSize, GraphicsContext&);

    void markContextChanged();
    void markLayerComposited();
    bool layerComposited() const;
    void forceContextLost();
    void recycleContext();

    void dispatchContextChangedNotification();
    void simulateContextChanged();

    void paintRenderingResultsToCanvas(ImageBuffer*);
    RefPtr<ImageData> paintRenderingResultsToImageData();
    bool paintCompositedResultsToCanvas(ImageBuffer*);

#if USE(OPENGL) && ENABLE(WEBGL2)
    void primitiveRestartIndex(GC3Duint);
#endif

#if PLATFORM(COCOA)
    bool texImageIOSurface2D(GC3Denum target, GC3Denum internalFormat, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, IOSurfaceRef, GC3Duint plane);

#if USE(OPENGL_ES)
    void presentRenderbuffer();
#endif

#if USE(OPENGL)
    void allocateIOSurfaceBackingStore(IntSize);
    void updateFramebufferTextureBackingStoreFromLayer();
    void updateCGLContext();
#endif
#endif // PLATFORM(COCOA)

    void setContextVisibility(bool);

    GraphicsContext3DPowerPreference powerPreferenceUsedForCreation() const { return m_powerPreferenceUsedForCreation; }

    // Support for buffer creation and deletion
    Platform3DObject createBuffer();
    Platform3DObject createFramebuffer();
    Platform3DObject createProgram();
    Platform3DObject createRenderbuffer();
    Platform3DObject createShader(GC3Denum);
    Platform3DObject createTexture();

    void deleteBuffer(Platform3DObject);
    void deleteFramebuffer(Platform3DObject);
    void deleteProgram(Platform3DObject);
    void deleteRenderbuffer(Platform3DObject);
    void deleteShader(Platform3DObject);
    void deleteTexture(Platform3DObject);

    // Synthesizes an OpenGL error which will be returned from a
    // later call to getError. This is used to emulate OpenGL ES
    // 2.0 behavior on the desktop and to enforce additional error
    // checking mandated by WebGL.
    //
    // Per the behavior of glGetError, this stores at most one
    // instance of any given error, and returns them from calls to
    // getError in the order they were added.
    void synthesizeGLError(GC3Denum error);

    // Read real OpenGL errors, and move them to the synthetic
    // error list. Return true if at least one error is moved.
    bool moveErrorsToSyntheticErrorList();

    // Support for extensions. Returns a non-null object, though not
    // all methods it contains may necessarily be supported on the
    // current hardware. Must call Extensions3D::supports() to
    // determine this.
    Extensions3D& getExtensions();

    IntSize getInternalFramebufferSize() const;

    static unsigned getClearBitsByAttachmentType(GC3Denum);
    static unsigned getClearBitsByFormat(GC3Denum);

    enum ChannelBits {
        ChannelRed = 1,
        ChannelGreen = 2,
        ChannelBlue = 4,
        ChannelAlpha = 8,
        ChannelDepth = 16,
        ChannelStencil = 32,
        ChannelRGB = ChannelRed | ChannelGreen | ChannelBlue,
        ChannelRGBA = ChannelRGB | ChannelAlpha,
    };

    static unsigned getChannelBitsByFormat(GC3Denum);

    // Possible alpha operations that may need to occur during
    // pixel packing. FIXME: kAlphaDoUnmultiply is lossy and must
    // be removed.
    enum AlphaOp {
        AlphaDoNothing = 0,
        AlphaDoPremultiply = 1,
        AlphaDoUnmultiply = 2
    };

    enum ImageHtmlDomSource {
        HtmlDomImage = 0,
        HtmlDomCanvas = 1,
        HtmlDomVideo = 2,
        HtmlDomNone = 3
    };

    // Packs the contents of the given Image which is passed in |pixels| into the passed Vector
    // according to the given format and type, and obeying the flipY and AlphaOp flags.
    // Returns true upon success.
    static bool packImageData(Image*, const void* pixels, GC3Denum format, GC3Denum type, bool flipY, AlphaOp, DataFormat sourceFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, Vector<uint8_t>& data);

    class ImageExtractor {
    public:
        ImageExtractor(Image*, ImageHtmlDomSource, bool premultiplyAlpha, bool ignoreGammaAndColorProfile);

        // Each platform must provide an implementation of this method to deallocate or release resources
        // associated with the image if needed.
        ~ImageExtractor();

        bool extractSucceeded() { return m_extractSucceeded; }
        const void* imagePixelData() { return m_imagePixelData; }
        unsigned imageWidth() { return m_imageWidth; }
        unsigned imageHeight() { return m_imageHeight; }
        DataFormat imageSourceFormat() { return m_imageSourceFormat; }
        AlphaOp imageAlphaOp() { return m_alphaOp; }
        unsigned imageSourceUnpackAlignment() { return m_imageSourceUnpackAlignment; }
        ImageHtmlDomSource imageHtmlDomSource() { return m_imageHtmlDomSource; }
    private:
        // Each platform must provide an implementation of this method.
        // Extracts the image and keeps track of its status, such as width, height, Source Alignment, format and AlphaOp etc,
        // needs to lock the resources or relevant data if needed and returns true upon success
        bool extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile);

#if USE(CAIRO)
        RefPtr<cairo_surface_t> m_imageSurface;
#elif USE(CG)
        RetainPtr<CGImageRef> m_cgImage;
        RetainPtr<CGImageRef> m_decodedImage;
        RetainPtr<CFDataRef> m_pixelData;
        UniqueArray<uint8_t> m_formalizedRGBA8Data;
#endif
        Image* m_image;
        ImageHtmlDomSource m_imageHtmlDomSource;
        bool m_extractSucceeded;
        const void* m_imagePixelData;
        unsigned m_imageWidth;
        unsigned m_imageHeight;
        DataFormat m_imageSourceFormat;
        AlphaOp m_alphaOp;
        unsigned m_imageSourceUnpackAlignment;
    };

    void setFailNextGPUStatusCheck() { m_failNextStatusCheck = true; }

    GC3Denum activeTextureUnit() const { return m_state.activeTextureUnit; }
    GC3Denum currentBoundTexture() const { return m_state.currentBoundTexture(); }
    GC3Denum currentBoundTarget() const { return m_state.currentBoundTarget(); }
    unsigned textureSeed(GC3Duint texture) { return m_state.textureSeedCount.count(texture); }

#if PLATFORM(MAC)
    using PlatformDisplayID = uint32_t;
    void screenDidChange(PlatformDisplayID);
#endif

private:
    GraphicsContext3D(GraphicsContext3DAttributes, HostWindow*, RenderStyle = RenderOffscreen, GraphicsContext3D* sharedContext = nullptr);

    // Helper for packImageData/extractImageData/extractTextureData which implement packing of pixel
    // data into the specified OpenGL destination format and type.
    // A sourceUnpackAlignment of zero indicates that the source
    // data is tightly packed. Non-zero values may take a slow path.
    // Destination data will have no gaps between rows.
    static bool packPixels(const uint8_t* sourceData, DataFormat sourceDataFormat, unsigned width, unsigned height, unsigned sourceUnpackAlignment, unsigned destinationFormat, unsigned destinationType, AlphaOp, void* destinationData, bool flipY);

    // Take into account the user's requested context creation attributes,
    // in particular stencil and antialias, and determine which could or
    // could not be honored based on the capabilities of the OpenGL
    // implementation.
    void validateDepthStencil(const char* packedDepthStencilExtension);
    void validateAttributes();
    
    // Did the most recent drawing operation leave the GPU in an acceptable state?
    void checkGPUStatus();

    // Read rendering results into a pixel array with the same format as the
    // backbuffer.
    void readRenderingResults(unsigned char* pixels, int pixelsSize);
    void readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels);

#if PLATFORM(IOS_FAMILY)
    void setRenderbufferStorageFromDrawable(GC3Dsizei width, GC3Dsizei height);
#endif

    bool reshapeFBOs(const IntSize&);
    void resolveMultisamplingIfNecessary(const IntRect& = IntRect());
    void attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height);

#if PLATFORM(COCOA)
    bool allowOfflineRenderers() const;
#endif

    int m_currentWidth { 0 };
    int m_currentHeight { 0 };

#if PLATFORM(COCOA)
    RetainPtr<WebGLLayer> m_webGLLayer;
    PlatformGraphicsContext3D m_contextObj { nullptr };
#endif

#if PLATFORM(WIN) && USE(CA)
    RefPtr<PlatformCALayer> m_webGLLayer;
#endif

    typedef HashMap<String, sh::ShaderVariable> ShaderSymbolMap;

    struct ShaderSourceEntry {
        GC3Denum type;
        String source;
        String translatedSource;
        String log;
        bool isValid;
        ShaderSymbolMap attributeMap;
        ShaderSymbolMap uniformMap;
        ShaderSymbolMap varyingMap;
        ShaderSourceEntry()
            : type(VERTEX_SHADER)
            , isValid(false)
        {
        }
        
        ShaderSymbolMap& symbolMap(enum ANGLEShaderSymbolType symbolType)
        {
            ASSERT(symbolType == SHADER_SYMBOL_TYPE_ATTRIBUTE || symbolType == SHADER_SYMBOL_TYPE_UNIFORM || symbolType == SHADER_SYMBOL_TYPE_VARYING);
            if (symbolType == SHADER_SYMBOL_TYPE_ATTRIBUTE)
                return attributeMap;
            if (symbolType == SHADER_SYMBOL_TYPE_VARYING)
                return varyingMap;
            return uniformMap;
        }
    };

    // FIXME: Shaders are never removed from this map, even if they and their program are deleted.
    // This is bad, and it also relies on the fact we never reuse Platform3DObject numbers.
    typedef HashMap<Platform3DObject, ShaderSourceEntry> ShaderSourceMap;
    ShaderSourceMap m_shaderSourceMap;

    typedef HashMap<Platform3DObject, std::pair<Platform3DObject, Platform3DObject>> LinkedShaderMap;
    LinkedShaderMap m_linkedShaderMap;

    struct ActiveShaderSymbolCounts {
        Vector<GC3Dint> filteredToActualAttributeIndexMap;
        Vector<GC3Dint> filteredToActualUniformIndexMap;

        ActiveShaderSymbolCounts()
        {
        }

        GC3Dint countForType(GC3Denum activeType)
        {
            ASSERT(activeType == ACTIVE_ATTRIBUTES || activeType == ACTIVE_UNIFORMS);
            if (activeType == ACTIVE_ATTRIBUTES)
                return filteredToActualAttributeIndexMap.size();

            return filteredToActualUniformIndexMap.size();
        }
    };
    typedef HashMap<Platform3DObject, ActiveShaderSymbolCounts> ShaderProgramSymbolCountMap;
    ShaderProgramSymbolCountMap m_shaderProgramSymbolCountMap;

    typedef HashMap<String, String> HashedSymbolMap;
    HashedSymbolMap m_possiblyUnusedAttributeMap;

    String mappedSymbolName(Platform3DObject program, ANGLEShaderSymbolType, const String& name);
    String mappedSymbolName(Platform3DObject shaders[2], size_t count, const String& name);
    String originalSymbolName(Platform3DObject program, ANGLEShaderSymbolType, const String& name);
    Optional<String> mappedSymbolInShaderSourceMap(Platform3DObject shader, ANGLEShaderSymbolType, const String& name);
    Optional<String> originalSymbolInShaderSourceMap(Platform3DObject shader, ANGLEShaderSymbolType, const String& name);

    std::unique_ptr<ShaderNameHash> nameHashMapForShaders;

#if !PLATFORM(COCOA) && USE(OPENGL_ES)
    friend class Extensions3DOpenGLES;
    std::unique_ptr<Extensions3DOpenGLES> m_extensions;
#else
    friend class Extensions3DOpenGL;
    std::unique_ptr<Extensions3DOpenGL> m_extensions;
#endif
    friend class Extensions3DOpenGLCommon;

    GraphicsContext3DAttributes m_attrs;
    GraphicsContext3DPowerPreference m_powerPreferenceUsedForCreation { GraphicsContext3DPowerPreference::Default };
    RenderStyle m_renderStyle;
    Vector<Vector<float>> m_vertexArray;

    ANGLEWebKitBridge m_compiler;

    GC3Duint m_texture { 0 };
    GC3Duint m_fbo { 0 };
#if USE(COORDINATED_GRAPHICS_THREADED)
    GC3Duint m_compositorTexture { 0 };
    GC3Duint m_intermediateTexture { 0 };
#endif

    GC3Duint m_depthBuffer { 0 };
    GC3Duint m_stencilBuffer { 0 };
    GC3Duint m_depthStencilBuffer { 0 };

    bool m_layerComposited { false };
    GC3Duint m_internalColorFormat { 0 };

    struct GraphicsContext3DState {
        GC3Duint boundFBO { 0 };
        GC3Denum activeTextureUnit { GraphicsContext3D::TEXTURE0 };

        using BoundTextureMap = HashMap<GC3Denum,
            std::pair<GC3Duint, GC3Denum>,
            WTF::IntHash<GC3Denum>, 
            WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>,
            WTF::PairHashTraits<WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>, WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>>
        >;
        BoundTextureMap boundTextureMap;
        GC3Duint currentBoundTexture() const { return boundTexture(activeTextureUnit); }
        GC3Duint boundTexture(GC3Denum textureUnit) const
        {
            auto iterator = boundTextureMap.find(textureUnit);
            if (iterator != boundTextureMap.end())
                return iterator->value.first;
            return 0;
        }

        GC3Duint currentBoundTarget() const { return boundTarget(activeTextureUnit); }
        GC3Denum boundTarget(GC3Denum textureUnit) const
        {
            auto iterator = boundTextureMap.find(textureUnit);
            if (iterator != boundTextureMap.end())
                return iterator->value.second;
            return 0;
        }

        void setBoundTexture(GC3Denum textureUnit, GC3Duint texture, GC3Denum target)
        {
            boundTextureMap.set(textureUnit, std::make_pair(texture, target));
        }

        using TextureSeedCount = HashCountedSet<GC3Duint, WTF::IntHash<GC3Duint>, WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>>;
        TextureSeedCount textureSeedCount;
    };

    GraphicsContext3DState m_state;

    // For multisampling
    GC3Duint m_multisampleFBO { 0 };
    GC3Duint m_multisampleDepthStencilBuffer { 0 };
    GC3Duint m_multisampleColorBuffer { 0 };

    // Errors raised by synthesizeGLError().
    ListHashSet<GC3Denum> m_syntheticErrors;

#if USE(NICOSIA) && USE(TEXTURE_MAPPER)
    friend class Nicosia::GC3DLayer;
    std::unique_ptr<Nicosia::GC3DLayer> m_nicosiaLayer;
#elif USE(TEXTURE_MAPPER)
    friend class TextureMapperGC3DPlatformLayer;
    std::unique_ptr<TextureMapperGC3DPlatformLayer> m_texmapLayer;
#else
    friend class GraphicsContext3DPrivate;
    std::unique_ptr<GraphicsContext3DPrivate> m_private;
#endif

    // FIXME: Layering violation.
    WebGLRenderingContextBase* m_webglContext { nullptr };

    bool m_isForWebGL2 { false };
    bool m_usingCoreProfile { false };

    unsigned m_statusCheckCount { 0 };
    bool m_failNextStatusCheck { false };

#if USE(CAIRO)
    Platform3DObject m_vao { 0 };
#endif

#if PLATFORM(COCOA) && USE(OPENGL)
    bool m_hasSwitchedToHighPerformanceGPU { false };
#endif
};

} // namespace WebCore
