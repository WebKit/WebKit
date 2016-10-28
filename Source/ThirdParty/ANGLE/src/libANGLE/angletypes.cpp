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

SamplerState::SamplerState()
    : minFilter(GL_NEAREST_MIPMAP_LINEAR),
      magFilter(GL_LINEAR),
      wrapS(GL_REPEAT),
      wrapT(GL_REPEAT),
      wrapR(GL_REPEAT),
      maxAnisotropy(1.0f),
      minLod(-1000.0f),
      maxLod(1000.0f),
      compareMode(GL_NONE),
      compareFunc(GL_LEQUAL)
{
}

// static
SamplerState SamplerState::CreateDefaultForTarget(GLenum target)
{
    SamplerState state;

    // According to OES_EGL_image_external: For external textures, the default min filter is
    // GL_LINEAR and the default s and t wrap modes are GL_CLAMP_TO_EDGE.
    if (target == GL_TEXTURE_EXTERNAL_OES)
    {
        state.minFilter = GL_LINEAR;
        state.wrapS     = GL_CLAMP_TO_EDGE;
        state.wrapT     = GL_CLAMP_TO_EDGE;
    }

    return state;
}

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

bool operator==(const Extents &lhs, const Extents &rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
}

bool operator!=(const Extents &lhs, const Extents &rhs)
{
    return !(lhs == rhs);
}
}
