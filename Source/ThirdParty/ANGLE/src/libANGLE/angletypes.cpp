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

bool operator==(const Rectangle &a, const Rectangle &b)
{
    return a.x == b.x &&
           a.y == b.y &&
           a.width == b.width &&
           a.height == b.height;
}

bool operator!=(const Rectangle &a, const Rectangle &b)
{
    return !(a == b);
}

SamplerState::SamplerState()
    : minFilter(GL_NEAREST_MIPMAP_LINEAR),
      magFilter(GL_LINEAR),
      wrapS(GL_REPEAT),
      wrapT(GL_REPEAT),
      wrapR(GL_REPEAT),
      maxAnisotropy(1.0f),
      baseLevel(0),
      maxLevel(1000),
      minLod(-1000.0f),
      maxLod(1000.0f),
      compareMode(GL_NONE),
      compareFunc(GL_LEQUAL),
      swizzleRed(GL_RED),
      swizzleGreen(GL_GREEN),
      swizzleBlue(GL_BLUE),
      swizzleAlpha(GL_ALPHA)
{}

bool SamplerState::swizzleRequired() const
{
    return swizzleRed != GL_RED || swizzleGreen != GL_GREEN ||
           swizzleBlue != GL_BLUE || swizzleAlpha != GL_ALPHA;
}

bool SamplerState::operator==(const SamplerState &other) const
{
    return minFilter == other.minFilter &&
           magFilter == other.magFilter &&
           wrapS == other.wrapS &&
           wrapT == other.wrapT &&
           wrapR == other.wrapR &&
           maxAnisotropy == other.maxAnisotropy &&
           baseLevel == other.baseLevel &&
           maxLevel == other.maxLevel &&
           minLod == other.minLod &&
           maxLod == other.maxLod &&
           compareMode == other.compareMode &&
           compareFunc == other.compareFunc &&
           swizzleRed == other.swizzleRed &&
           swizzleGreen == other.swizzleGreen &&
           swizzleBlue == other.swizzleBlue &&
           swizzleAlpha == other.swizzleAlpha;
}

bool SamplerState::operator!=(const SamplerState &other) const
{
    return !(*this == other);
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

VertexFormat::VertexFormat()
    : mType(GL_NONE),
      mNormalized(GL_FALSE),
      mComponents(0),
      mPureInteger(false)
{}

VertexFormat::VertexFormat(GLenum type, GLboolean normalized, GLuint components, bool pureInteger)
    : mType(type),
      mNormalized(normalized),
      mComponents(components),
      mPureInteger(pureInteger)
{
    // Float data can not be normalized, so ignore the user setting
    if (mType == GL_FLOAT || mType == GL_HALF_FLOAT || mType == GL_FIXED)
    {
        mNormalized = GL_FALSE;
    }
}

VertexFormat::VertexFormat(const VertexAttribute &attrib)
    : mType(attrib.type),
      mNormalized(attrib.normalized ? GL_TRUE : GL_FALSE),
      mComponents(attrib.size),
      mPureInteger(attrib.pureInteger)
{
    // Ensure we aren't initializing a vertex format which should be using
    // the current-value type
    ASSERT(attrib.enabled);

    // Float data can not be normalized, so ignore the user setting
    if (mType == GL_FLOAT || mType == GL_HALF_FLOAT || mType == GL_FIXED)
    {
        mNormalized = GL_FALSE;
    }
}

VertexFormat::VertexFormat(const VertexAttribute &attrib, GLenum currentValueType)
    : mType(attrib.type),
      mNormalized(attrib.normalized ? GL_TRUE : GL_FALSE),
      mComponents(attrib.size),
      mPureInteger(attrib.pureInteger)
{
    if (!attrib.enabled)
    {
        mType = currentValueType;
        mNormalized = GL_FALSE;
        mComponents = 4;
        mPureInteger = (currentValueType != GL_FLOAT);
    }

    // Float data can not be normalized, so ignore the user setting
    if (mType == GL_FLOAT || mType == GL_HALF_FLOAT || mType == GL_FIXED)
    {
        mNormalized = GL_FALSE;
    }
}

void VertexFormat::GetInputLayout(VertexFormat *inputLayout,
                                  Program *program,
                                  const State &state)
{
    const std::vector<VertexAttribute> &vertexAttributes = state.getVertexArray()->getVertexAttributes();
    for (unsigned int attributeIndex = 0; attributeIndex < vertexAttributes.size(); attributeIndex++)
    {
        int semanticIndex = program->getSemanticIndex(attributeIndex);

        if (semanticIndex != -1)
        {
            inputLayout[semanticIndex] = VertexFormat(vertexAttributes[attributeIndex], state.getVertexAttribCurrentValue(attributeIndex).Type);
        }
    }
}

bool VertexFormat::operator==(const VertexFormat &other) const
{
    return (mType == other.mType                &&
            mComponents == other.mComponents    &&
            mNormalized == other.mNormalized    &&
            mPureInteger == other.mPureInteger  );
}

bool VertexFormat::operator!=(const VertexFormat &other) const
{
    return !(*this == other);
}

bool VertexFormat::operator<(const VertexFormat& other) const
{
    if (mType != other.mType)
    {
        return mType < other.mType;
    }
    if (mNormalized != other.mNormalized)
    {
        return mNormalized < other.mNormalized;
    }
    if (mComponents != other.mComponents)
    {
        return mComponents < other.mComponents;
    }
    return mPureInteger < other.mPureInteger;
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

}
