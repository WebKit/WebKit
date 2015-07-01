//
// Copyright (c) 2012-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes.h : Defines a variety of structures and enum types that are used throughout libGLESv2

#ifndef LIBANGLE_ANGLETYPES_H_
#define LIBANGLE_ANGLETYPES_H_

#include "libANGLE/Constants.h"
#include "libANGLE/RefCountObject.h"

#include <stdint.h>

namespace gl
{
class Buffer;
class State;
class Program;
struct VertexAttribute;
struct VertexAttribCurrentValueData;

enum SamplerType
{
    SAMPLER_PIXEL,
    SAMPLER_VERTEX
};

template <typename T>
struct Color
{
    T red;
    T green;
    T blue;
    T alpha;

    Color() : red(0), green(0), blue(0), alpha(0) { }
    Color(T r, T g, T b, T a) : red(r), green(g), blue(b), alpha(a) { }
};

template <typename T>
bool operator==(const Color<T> &a, const Color<T> &b)
{
    return a.red == b.red &&
           a.green == b.green &&
           a.blue == b.blue &&
           a.alpha == b.alpha;
}

template <typename T>
bool operator!=(const Color<T> &a, const Color<T> &b)
{
    return !(a == b);
}

typedef Color<float> ColorF;
typedef Color<int> ColorI;
typedef Color<unsigned int> ColorUI;

struct Rectangle
{
    int x;
    int y;
    int width;
    int height;

    Rectangle() : x(0), y(0), width(0), height(0) { }
    Rectangle(int x_in, int y_in, int width_in, int height_in) : x(x_in), y(y_in), width(width_in), height(height_in) { }
};

bool operator==(const Rectangle &a, const Rectangle &b);
bool operator!=(const Rectangle &a, const Rectangle &b);

bool ClipRectangle(const Rectangle &source, const Rectangle &clip, Rectangle *intersection);

struct Offset
{
    int x;
    int y;
    int z;

    Offset() : x(0), y(0), z(0) { }
    Offset(int x_in, int y_in, int z_in) : x(x_in), y(y_in), z(z_in) { }
};

struct Extents
{
    int width;
    int height;
    int depth;

    Extents() : width(0), height(0), depth(0) { }
    Extents(int width_, int height_, int depth_) : width(width_), height(height_), depth(depth_) { }

    bool empty() const { return (width * height * depth) == 0; }
};

struct Box
{
    int x;
    int y;
    int z;
    int width;
    int height;
    int depth;

    Box() : x(0), y(0), z(0), width(0), height(0), depth(0) { }
    Box(int x_in, int y_in, int z_in, int width_in, int height_in, int depth_in) : x(x_in), y(y_in), z(z_in), width(width_in), height(height_in), depth(depth_in) { }
    Box(const Offset &offset, const Extents &size) : x(offset.x), y(offset.y), z(offset.z), width(size.width), height(size.height), depth(size.depth) { }
    bool operator==(const Box &other) const;
    bool operator!=(const Box &other) const;
};


struct RasterizerState
{
    bool cullFace;
    GLenum cullMode;
    GLenum frontFace;

    bool polygonOffsetFill;
    GLfloat polygonOffsetFactor;
    GLfloat polygonOffsetUnits;

    bool pointDrawMode;
    bool multiSample;

    bool rasterizerDiscard;
};

struct BlendState
{
    bool blend;
    GLenum sourceBlendRGB;
    GLenum destBlendRGB;
    GLenum sourceBlendAlpha;
    GLenum destBlendAlpha;
    GLenum blendEquationRGB;
    GLenum blendEquationAlpha;

    bool colorMaskRed;
    bool colorMaskGreen;
    bool colorMaskBlue;
    bool colorMaskAlpha;

    bool sampleAlphaToCoverage;

    bool dither;
};

struct DepthStencilState
{
    bool depthTest;
    GLenum depthFunc;
    bool depthMask;

    bool stencilTest;
    GLenum stencilFunc;
    GLuint stencilMask;
    GLenum stencilFail;
    GLenum stencilPassDepthFail;
    GLenum stencilPassDepthPass;
    GLuint stencilWritemask;
    GLenum stencilBackFunc;
    GLuint stencilBackMask;
    GLenum stencilBackFail;
    GLenum stencilBackPassDepthFail;
    GLenum stencilBackPassDepthPass;
    GLuint stencilBackWritemask;
};

struct SamplerState
{
    SamplerState();

    GLenum minFilter;
    GLenum magFilter;
    GLenum wrapS;
    GLenum wrapT;
    GLenum wrapR;
    float maxAnisotropy;

    GLint baseLevel;
    GLint maxLevel;
    GLfloat minLod;
    GLfloat maxLod;

    GLenum compareMode;
    GLenum compareFunc;

    GLenum swizzleRed;
    GLenum swizzleGreen;
    GLenum swizzleBlue;
    GLenum swizzleAlpha;

    bool swizzleRequired() const;

    bool operator==(const SamplerState &other) const;
    bool operator!=(const SamplerState &other) const;
};

struct PixelUnpackState
{
    BindingPointer<Buffer> pixelBuffer;
    GLint alignment;
    GLint rowLength;
    GLint skipRows;
    GLint skipPixels;
    GLint imageHeight;
    GLint skipImages;

    PixelUnpackState()
        : alignment(4),
          rowLength(0),
          skipRows(0),
          skipPixels(0),
          imageHeight(0),
          skipImages(0)
    {}

    PixelUnpackState(GLint alignmentIn, GLint rowLengthIn)
        : alignment(alignmentIn),
          rowLength(rowLengthIn),
          skipRows(0),
          skipPixels(0),
          imageHeight(0),
          skipImages(0)
    {}
};

struct PixelPackState
{
    BindingPointer<Buffer> pixelBuffer;
    GLint alignment;
    bool reverseRowOrder;
    GLint rowLength;
    GLint skipRows;
    GLint skipPixels;

    PixelPackState()
        : alignment(4),
          reverseRowOrder(false),
          rowLength(0),
          skipRows(0),
          skipPixels(0)
    {}

    explicit PixelPackState(GLint alignmentIn, bool reverseRowOrderIn)
        : alignment(alignmentIn),
          reverseRowOrder(reverseRowOrderIn),
          rowLength(0),
          skipRows(0),
          skipPixels(0)
    {}
};

struct VertexFormat
{
    GLenum      mType;
    GLboolean   mNormalized;
    GLuint      mComponents;
    bool        mPureInteger;

    VertexFormat();
    VertexFormat(GLenum type, GLboolean normalized, GLuint components, bool pureInteger);
    explicit VertexFormat(const VertexAttribute &attribute);
    VertexFormat(const VertexAttribute &attribute, GLenum currentValueType);

    static void GetInputLayout(VertexFormat *inputLayout,
                               Program *program,
                               const State& currentValues);

    bool operator==(const VertexFormat &other) const;
    bool operator!=(const VertexFormat &other) const;
    bool operator<(const VertexFormat& other) const;
};

}

namespace rx
{

enum VendorID : uint32_t
{
    VENDOR_ID_AMD = 0x1002,
    VENDOR_ID_INTEL = 0x8086,
    VENDOR_ID_NVIDIA = 0x10DE,
};

// A macro that determines whether an object has a given runtime type.
#if defined(__clang__)
#if __has_feature(cxx_rtti)
#define ANGLE_HAS_DYNAMIC_CAST 1
#endif
#elif !defined(NDEBUG) && (!defined(_MSC_VER) || defined(_CPPRTTI)) && (!defined(__GNUC__) || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3) || defined(__GXX_RTTI))
#define ANGLE_HAS_DYNAMIC_CAST 1
#endif

#ifdef ANGLE_HAS_DYNAMIC_CAST
#define ANGLE_HAS_DYNAMIC_TYPE(type, obj) (dynamic_cast<type >(obj) != nullptr)
#undef ANGLE_HAS_DYNAMIC_CAST
#else
#define ANGLE_HAS_DYNAMIC_TYPE(type, obj) (obj != nullptr)
#endif

// Downcast a base implementation object (EG TextureImpl to TextureD3D)
template <typename DestT, typename SrcT>
inline DestT *GetAs(SrcT *src)
{
    ASSERT(ANGLE_HAS_DYNAMIC_TYPE(DestT*, src));
    return static_cast<DestT*>(src);
}

template <typename DestT, typename SrcT>
inline const DestT *GetAs(const SrcT *src)
{
    ASSERT(ANGLE_HAS_DYNAMIC_TYPE(const DestT*, src));
    return static_cast<const DestT*>(src);
}

#undef ANGLE_HAS_DYNAMIC_TYPE

// Downcast a GL object to an Impl (EG gl::Texture to rx::TextureD3D)
template <typename DestT, typename SrcT>
inline DestT *GetImplAs(SrcT *src)
{
    return GetAs<DestT>(src->getImplementation());
}

template <typename DestT, typename SrcT>
inline const DestT *GetImplAs(const SrcT *src)
{
    return GetAs<const DestT>(src->getImplementation());
}

}

#endif // LIBANGLE_ANGLETYPES_H_
