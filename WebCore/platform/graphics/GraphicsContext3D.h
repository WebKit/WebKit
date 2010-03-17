/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef GraphicsContext3D_h
#define GraphicsContext3D_h

#include "PlatformString.h"

#include <wtf/ListHashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

// FIXME: Find a better way to avoid the name confliction for NO_ERROR.
#if ((PLATFORM(CHROMIUM) && OS(WINDOWS)) || PLATFORM(WIN))
#undef NO_ERROR
#endif

#if PLATFORM(MAC)
#include <OpenGL/OpenGL.h>

typedef void* PlatformGraphicsContext3D;
const  PlatformGraphicsContext3D NullPlatformGraphicsContext3D = 0;
typedef GLuint Platform3DObject;
const Platform3DObject NullPlatform3DObject = 0;
#elif PLATFORM(QT)
#include <QtOpenGL/QtOpenGL>

typedef void* PlatformGraphicsContext3D;
const  PlatformGraphicsContext3D NullPlatformGraphicsContext3D = 0;
typedef int Platform3DObject;
const Platform3DObject NullPlatform3DObject = 0;
#else
typedef void* PlatformGraphicsContext3D;
const  PlatformGraphicsContext3D NullPlatformGraphicsContext3D = 0;
typedef int Platform3DObject;
const Platform3DObject NullPlatform3DObject = 0;
#endif

namespace WebCore {
    class WebGLActiveInfo;
    class WebGLArray;
    class WebGLBuffer;
    class WebGLUnsignedByteArray;
    class WebGLFloatArray;
    class WebGLFramebuffer;
    class WebGLIntArray;
    class WebGLProgram;
    class WebGLRenderbuffer;
    class WebGLRenderingContext;
    class WebGLShader;
    class WebGLTexture;
    class Image;
    class ImageData;

    struct ActiveInfo {
        String name;
        unsigned type;
        int size;
    };

    // FIXME: ideally this would be used on all platforms.
#if PLATFORM(CHROMIUM) || PLATFORM(QT)
    class GraphicsContext3DInternal;
#endif

    class GraphicsContext3D : public Noncopyable {
    public:
        enum WebGLEnumType {
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
            FIXED = 0x140C,
            DEPTH_COMPONENT = 0x1902,
            ALPHA = 0x1906,
            RGB = 0x1907,
            RGBA = 0x1908,
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
            INVALID_FRAMEBUFFER_OPERATION = 0x0506
        };
        
        // Context creation attributes.
        struct Attributes {
            Attributes()
                : alpha(true)
                , depth(true)
                , stencil(true)
                , antialias(true)
                , premultipliedAlpha(true)
            {
            }

            bool alpha;
            bool depth;
            bool stencil;
            bool antialias;
            bool premultipliedAlpha;
        };

        static PassOwnPtr<GraphicsContext3D> create(Attributes attrs);
        virtual ~GraphicsContext3D();

#if PLATFORM(MAC)
        PlatformGraphicsContext3D platformGraphicsContext3D() const { return m_contextObj; }
        Platform3DObject platformTexture() const { return m_texture; }
#elif PLATFORM(CHROMIUM)
        PlatformGraphicsContext3D platformGraphicsContext3D() const;
        Platform3DObject platformTexture() const;
#elif PLATFORM(QT)
        PlatformGraphicsContext3D platformGraphicsContext3D();
        Platform3DObject platformTexture() const;
#else
        PlatformGraphicsContext3D platformGraphicsContext3D() const { return NullPlatformGraphicsContext3D; }
        Platform3DObject platformTexture() const { return NullPlatform3DObject; }
#endif
        void makeContextCurrent();
        
        // Helper to return the size in bytes of OpenGL data types
        // like GL_FLOAT, GL_INT, etc.
        int sizeInBytes(int type);

        //----------------------------------------------------------------------
        // Helpers for texture uploading.
        //

        // Extracts the contents of the given Image into the passed
        // Vector, obeying the flipY and premultiplyAlpha flags.
        // Returns the applicable OpenGL format and internalFormat for
        // the subsequent glTexImage2D or glTexSubImage2D call.
        // Returns true upon success.
        bool extractImageData(Image* image,
                              bool flipY,
                              bool premultiplyAlpha,
                              Vector<uint8_t>& imageData,
                              unsigned int* format,
                              unsigned int* internalFormat);

        // Extracts the contents of the given ImageData into the passed
        // Vector, obeying the flipY and premultiplyAlpha flags.
        // Returns true upon success.
        bool extractImageData(ImageData*,
                              bool flipY,
                              bool premultiplyAlpha,
                              Vector<uint8_t>& data);

        // Processes the given image data in preparation for uploading
        // via texImage2D or texSubImage2D. The input data must be in
        // 4-component format with the alpha channel last (i.e., RGBA
        // or BGRA), tightly packed, with no space between rows.

        enum AlphaOp {
            kAlphaDoNothing = 0,
            kAlphaDoPremultiply = 1,
            kAlphaDoUnmultiply = 2
        };

        void processImageData(uint8_t* imageData,
                              unsigned width,
                              unsigned height,
                              bool flipVertically,
                              AlphaOp alphaOp);

        //----------------------------------------------------------------------
        // Entry points for WebGL.
        //

        void activeTexture(unsigned long texture);
        void attachShader(WebGLProgram* program, WebGLShader* shader);
        void bindAttribLocation(WebGLProgram*, unsigned long index, const String& name);
        void bindBuffer(unsigned long target, WebGLBuffer*);
        void bindFramebuffer(unsigned long target, WebGLFramebuffer*);
        void bindRenderbuffer(unsigned long target, WebGLRenderbuffer*);
        void bindTexture(unsigned long target, WebGLTexture* texture);
        void blendColor(double red, double green, double blue, double alpha);
        void blendEquation(unsigned long mode);
        void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
        void blendFunc(unsigned long sfactor, unsigned long dfactor);
        void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);

        void bufferData(unsigned long target, int size, unsigned long usage);
        void bufferData(unsigned long target, WebGLArray* data, unsigned long usage);
        void bufferSubData(unsigned long target, long offset, WebGLArray* data);

        unsigned long checkFramebufferStatus(unsigned long target);
        void clear(unsigned long mask);
        void clearColor(double red, double green, double blue, double alpha);
        void clearDepth(double depth);
        void clearStencil(long s);
        void colorMask(bool red, bool green, bool blue, bool alpha);
        void compileShader(WebGLShader*);
        
        //void compressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
        //void compressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);
        
        void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
        void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);
        void cullFace(unsigned long mode);
        void depthFunc(unsigned long func);
        void depthMask(bool flag);
        void depthRange(double zNear, double zFar);
        void detachShader(WebGLProgram*, WebGLShader*);
        void disable(unsigned long cap);
        void disableVertexAttribArray(unsigned long index);
        void drawArrays(unsigned long mode, long first, long count);
        void drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset);

        void enable(unsigned long cap);
        void enableVertexAttribArray(unsigned long index);
        void finish();
        void flush();
        void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLRenderbuffer*);
        void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLTexture*, long level);
        void frontFace(unsigned long mode);
        void generateMipmap(unsigned long target);

        bool getActiveAttrib(WebGLProgram* program, unsigned long index, ActiveInfo&);
        bool getActiveUniform(WebGLProgram* program, unsigned long index, ActiveInfo&);

        int  getAttribLocation(WebGLProgram*, const String& name);

        void getBooleanv(unsigned long pname, unsigned char* value);

        void getBufferParameteriv(unsigned long target, unsigned long pname, int* value);

        Attributes getContextAttributes();

        unsigned long getError();

        void getFloatv(unsigned long pname, float* value);

        void getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment, unsigned long pname, int* value);

        void getIntegerv(unsigned long pname, int* value);

        void getProgramiv(WebGLProgram* program, unsigned long pname, int* value);

        String getProgramInfoLog(WebGLProgram*);

        void getRenderbufferParameteriv(unsigned long target, unsigned long pname, int* value);

        void getShaderiv(WebGLShader*, unsigned long pname, int* value);

        String getShaderInfoLog(WebGLShader*);

        // TBD
        // void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

        String getShaderSource(WebGLShader*);
        String getString(unsigned long name);

        void getTexParameterfv(unsigned long target, unsigned long pname, float* value);
        void getTexParameteriv(unsigned long target, unsigned long pname, int* value);

        void getUniformfv(WebGLProgram* program, long location, float* value);
        void getUniformiv(WebGLProgram* program, long location, int* value);

        long getUniformLocation(WebGLProgram*, const String& name);

        void getVertexAttribfv(unsigned long index, unsigned long pname, float* value);
        void getVertexAttribiv(unsigned long index, unsigned long pname, int* value);

        long getVertexAttribOffset(unsigned long index, unsigned long pname);

        void hint(unsigned long target, unsigned long mode);
        bool isBuffer(WebGLBuffer*);
        bool isEnabled(unsigned long cap);
        bool isFramebuffer(WebGLFramebuffer*);
        bool isProgram(WebGLProgram*);
        bool isRenderbuffer(WebGLRenderbuffer*);
        bool isShader(WebGLShader*);
        bool isTexture(WebGLTexture*);
        void lineWidth(double);
        void linkProgram(WebGLProgram*);
        void pixelStorei(unsigned long pname, long param);
        void polygonOffset(double factor, double units);
        
        PassRefPtr<WebGLArray> readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type);
        
        void releaseShaderCompiler();
        void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
        void sampleCoverage(double value, bool invert);
        void scissor(long x, long y, unsigned long width, unsigned long height);
        void shaderSource(WebGLShader*, const String& string);
        void stencilFunc(unsigned long func, long ref, unsigned long mask);
        void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
        void stencilMask(unsigned long mask);
        void stencilMaskSeparate(unsigned long face, unsigned long mask);
        void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
        void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);

        // These next several functions return an error code (0 if no errors) rather than using an ExceptionCode.
        // Currently they return -1 on any error.
        int texImage2D(unsigned target, unsigned level, unsigned internalformat, unsigned width, unsigned height, unsigned border, unsigned format, unsigned type, void* pixels);
        int texImage2D(unsigned target, unsigned level, Image* image, bool flipY, bool premultiplyAlpha);

        void texParameterf(unsigned target, unsigned pname, float param);
        void texParameteri(unsigned target, unsigned pname, int param);

        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, unsigned width, unsigned height, unsigned format, unsigned type, void* pixels);
        int texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset, Image* image, bool flipY, bool premultiplyAlpha);

        void uniform1f(long location, float x);
        void uniform1fv(long location, float* v, int size);
        void uniform1i(long location, int x);
        void uniform1iv(long location, int* v, int size);
        void uniform2f(long location, float x, float y);
        void uniform2fv(long location, float* v, int size);
        void uniform2i(long location, int x, int y);
        void uniform2iv(long location, int* v, int size);
        void uniform3f(long location, float x, float y, float z);
        void uniform3fv(long location, float* v, int size);
        void uniform3i(long location, int x, int y, int z);
        void uniform3iv(long location, int* v, int size);
        void uniform4f(long location, float x, float y, float z, float w);
        void uniform4fv(long location, float* v, int size);
        void uniform4i(long location, int x, int y, int z, int w);
        void uniform4iv(long location, int* v, int size);
        void uniformMatrix2fv(long location, bool transpose, float* value, int size);
        void uniformMatrix3fv(long location, bool transpose, float* value, int size);
        void uniformMatrix4fv(long location, bool transpose, float* value, int size);

        void useProgram(WebGLProgram*);
        void validateProgram(WebGLProgram*);

        void vertexAttrib1f(unsigned long indx, float x);
        void vertexAttrib1fv(unsigned long indx, float* values);
        void vertexAttrib2f(unsigned long indx, float x, float y);
        void vertexAttrib2fv(unsigned long indx, float* values);
        void vertexAttrib3f(unsigned long indx, float x, float y, float z);
        void vertexAttrib3fv(unsigned long indx, float* values);
        void vertexAttrib4f(unsigned long indx, float x, float y, float z, float w);
        void vertexAttrib4fv(unsigned long indx, float* values);
        void vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                 unsigned long stride, unsigned long offset);

        void viewport(long x, long y, unsigned long width, unsigned long height);

        void reshape(int width, int height);
        
        // Helpers for notification about paint events
        void beginPaint(WebGLRenderingContext* context);
        void endPaint();

        // Support for buffer creation and deletion
        unsigned createBuffer();
        unsigned createFramebuffer();
        unsigned createProgram();
        unsigned createRenderbuffer();
        unsigned createShader(unsigned long);
        unsigned createTexture();
        
        void deleteBuffer(unsigned);
        void deleteFramebuffer(unsigned);
        void deleteProgram(unsigned);
        void deleteRenderbuffer(unsigned);
        void deleteShader(unsigned);
        void deleteTexture(unsigned);        
        
        // Synthesizes an OpenGL error which will be returned from a
        // later call to getError. This is used to emulate OpenGL ES
        // 2.0 behavior on the desktop and to enforce additional error
        // checking mandated by WebGL.
        //
        // Per the behavior of glGetError, this stores at most one
        // instance of any given error, and returns them from calls to
        // getError in the order they were added.
        void synthesizeGLError(unsigned long error);

    private:        
        GraphicsContext3D(Attributes attrs);

        // Helpers for texture uploading.
        void premultiplyAlpha(unsigned char* rgbaData, int numPixels);
        void unmultiplyAlpha(uint8_t* imageData, int numPixels);

        // Each platform must provide an implementation of this method.
        //
        // Gets the data for the given Image into outputVector,
        // without doing any processing of the data (vertical flip or
        // otherwise).
        //
        // premultiplyAlpha indicates whether the user will eventually
        // want the alpha channel multiplied into the color channels.
        //
        // The out parameter hasAlphaChannel indicates whether the
        // image actually had an alpha channel.
        //
        // The out parameter neededAlphaOp should be passed to a
        // subsequent call of processImageData.
        //
        // The out parameter format should be passed to subsequent
        // invocations of texImage2D and texSubImage2D.
        bool getImageData(Image* image,
                          Vector<uint8_t>& outputVector,
                          bool premultiplyAlpha,
                          bool* hasAlphaChannel,
                          AlphaOp* neededAlphaOp,
                          unsigned int* format);

        int m_currentWidth, m_currentHeight;
        
#if PLATFORM(MAC)
        Attributes m_attrs;
        Vector<Vector<float> > m_vertexArray;
        
        CGLContextObj m_contextObj;
        GLuint m_texture;
        GLuint m_fbo;
        GLuint m_depthBuffer;
        // Errors raised by synthesizeGLError().
        ListHashSet<unsigned long> m_syntheticErrors;
#endif        

        // FIXME: ideally this would be used on all platforms.
#if PLATFORM(CHROMIUM) || PLATFORM(QT)
        friend class GraphicsContext3DInternal;
        OwnPtr<GraphicsContext3DInternal> m_internal;
#endif
    };

} // namespace WebCore

#endif // GraphicsContext3D_h
