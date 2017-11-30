//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes.h : Defines a variety of structures and enum types that are used throughout libGLESv2

#include "libANGLE/angletypes.h"
#include "libANGLE/Program.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"

namespace gl
{

PrimitiveType GetPrimitiveType(GLenum drawMode)
{
    switch (drawMode)
    {
        case GL_POINTS:
            return PRIMITIVE_POINTS;
        case GL_LINES:
            return PRIMITIVE_LINES;
        case GL_LINE_STRIP:
            return PRIMITIVE_LINE_STRIP;
        case GL_LINE_LOOP:
            return PRIMITIVE_LINE_LOOP;
        case GL_TRIANGLES:
            return PRIMITIVE_TRIANGLES;
        case GL_TRIANGLE_STRIP:
            return PRIMITIVE_TRIANGLE_STRIP;
        case GL_TRIANGLE_FAN:
            return PRIMITIVE_TRIANGLE_FAN;
        default:
            UNREACHABLE();
            return PRIMITIVE_TYPE_MAX;
    }
}

RasterizerState::RasterizerState()
{
    memset(this, 0, sizeof(RasterizerState));

    rasterizerDiscard   = false;
    cullFace            = false;
    cullMode            = CullFaceMode::Back;
    frontFace           = GL_CCW;
    polygonOffsetFill   = false;
    polygonOffsetFactor = 0.0f;
    polygonOffsetUnits  = 0.0f;
    pointDrawMode       = false;
    multiSample         = false;
}

bool operator==(const RasterizerState &a, const RasterizerState &b)
{
    return memcmp(&a, &b, sizeof(RasterizerState)) == 0;
}

bool operator!=(const RasterizerState &a, const RasterizerState &b)
{
    return !(a == b);
}

BlendState::BlendState()
{
    memset(this, 0, sizeof(BlendState));

    blend                 = false;
    sourceBlendRGB        = GL_ONE;
    sourceBlendAlpha      = GL_ONE;
    destBlendRGB          = GL_ZERO;
    destBlendAlpha        = GL_ZERO;
    blendEquationRGB      = GL_FUNC_ADD;
    blendEquationAlpha    = GL_FUNC_ADD;
    sampleAlphaToCoverage = false;
    dither                = true;
}

BlendState::BlendState(const BlendState &other)
{
    memcpy(this, &other, sizeof(BlendState));
}

bool operator==(const BlendState &a, const BlendState &b)
{
    return memcmp(&a, &b, sizeof(BlendState)) == 0;
}

bool operator!=(const BlendState &a, const BlendState &b)
{
    return !(a == b);
}

DepthStencilState::DepthStencilState()
{
    memset(this, 0, sizeof(DepthStencilState));

    depthTest                = false;
    depthFunc                = GL_LESS;
    depthMask                = true;
    stencilTest              = false;
    stencilFunc              = GL_ALWAYS;
    stencilMask              = static_cast<GLuint>(-1);
    stencilWritemask         = static_cast<GLuint>(-1);
    stencilBackFunc          = GL_ALWAYS;
    stencilBackMask          = static_cast<GLuint>(-1);
    stencilBackWritemask     = static_cast<GLuint>(-1);
    stencilFail              = GL_KEEP;
    stencilPassDepthFail     = GL_KEEP;
    stencilPassDepthPass     = GL_KEEP;
    stencilBackFail          = GL_KEEP;
    stencilBackPassDepthFail = GL_KEEP;
    stencilBackPassDepthPass = GL_KEEP;
}

DepthStencilState::DepthStencilState(const DepthStencilState &other)
{
    memcpy(this, &other, sizeof(DepthStencilState));
}

bool operator==(const DepthStencilState &a, const DepthStencilState &b)
{
    return memcmp(&a, &b, sizeof(DepthStencilState)) == 0;
}

bool operator!=(const DepthStencilState &a, const DepthStencilState &b)
{
    return !(a == b);
}

SamplerState::SamplerState()
{
    memset(this, 0, sizeof(SamplerState));

    minFilter     = GL_NEAREST_MIPMAP_LINEAR;
    magFilter     = GL_LINEAR;
    wrapS         = GL_REPEAT;
    wrapT         = GL_REPEAT;
    wrapR         = GL_REPEAT;
    maxAnisotropy = 1.0f;
    minLod        = -1000.0f;
    maxLod        = 1000.0f;
    compareMode   = GL_NONE;
    compareFunc   = GL_LEQUAL;
    sRGBDecode    = GL_DECODE_EXT;
}

SamplerState::SamplerState(const SamplerState &other) = default;

// static
SamplerState SamplerState::CreateDefaultForTarget(GLenum target)
{
    SamplerState state;

    // According to OES_EGL_image_external and ARB_texture_rectangle: For external textures, the
    // default min filter is GL_LINEAR and the default s and t wrap modes are GL_CLAMP_TO_EDGE.
    if (target == GL_TEXTURE_EXTERNAL_OES || target == GL_TEXTURE_RECTANGLE_ANGLE)
    {
        state.minFilter = GL_LINEAR;
        state.wrapS     = GL_CLAMP_TO_EDGE;
        state.wrapT     = GL_CLAMP_TO_EDGE;
    }

    return state;
}

ImageUnit::ImageUnit()
    : texture(), level(0), layered(false), layer(0), access(GL_READ_ONLY), format(GL_R32UI)
{
}

ImageUnit::ImageUnit(const ImageUnit &other) = default;

ImageUnit::~ImageUnit() = default;

static void MinMax(int a, int b, int *minimum, int *maximum)
{
    if (a < b)
    {
        *minimum = a;
        *maximum = b;
    }
    else
    {
        *minimum = b;
        *maximum = a;
    }
}

bool ClipRectangle(const Rectangle &source, const Rectangle &clip, Rectangle *intersection)
{
    int minSourceX, maxSourceX, minSourceY, maxSourceY;
    MinMax(source.x, source.x + source.width, &minSourceX, &maxSourceX);
    MinMax(source.y, source.y + source.height, &minSourceY, &maxSourceY);

    int minClipX, maxClipX, minClipY, maxClipY;
    MinMax(clip.x, clip.x + clip.width, &minClipX, &maxClipX);
    MinMax(clip.y, clip.y + clip.height, &minClipY, &maxClipY);

    if (minSourceX >= maxClipX || maxSourceX <= minClipX || minSourceY >= maxClipY || maxSourceY <= minClipY)
    {
        if (intersection)
        {
            intersection->x = minSourceX;
            intersection->y = maxSourceY;
            intersection->width = maxSourceX - minSourceX;
            intersection->height = maxSourceY - minSourceY;
        }

        return false;
    }
    else
    {
        if (intersection)
        {
            intersection->x = std::max(minSourceX, minClipX);
            intersection->y = std::max(minSourceY, minClipY);
            intersection->width  = std::min(maxSourceX, maxClipX) - std::max(minSourceX, minClipX);
            intersection->height = std::min(maxSourceY, maxClipY) - std::max(minSourceY, minClipY);
        }

        return true;
    }
}

bool Box::operator==(const Box &other) const
{
    return (x == other.x && y == other.y && z == other.z &&
            width == other.width && height == other.height && depth == other.depth);
}

bool Box::operator!=(const Box &other) const
{
    return !(*this == other);
}

bool operator==(const Offset &a, const Offset &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool operator!=(const Offset &a, const Offset &b)
{
    return !(a == b);
}

bool operator==(const Extents &lhs, const Extents &rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
}

bool operator!=(const Extents &lhs, const Extents &rhs)
{
    return !(lhs == rhs);
}
}  // namespace gl
