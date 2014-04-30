#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes.h : Defines a variety of structures and enum types that are used throughout libGLESv2

#include "libGLESv2/angletypes.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/VertexAttribute.h"

namespace gl
{

bool SamplerState::swizzleRequired() const
{
    return swizzleRed != GL_RED || swizzleGreen != GL_GREEN ||
           swizzleBlue != GL_BLUE || swizzleAlpha != GL_ALPHA;
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

VertexFormat::VertexFormat(const VertexAttribute &attribute)
    : mType(attribute.mType),
      mNormalized(attribute.mNormalized ? GL_TRUE : GL_FALSE),
      mComponents(attribute.mSize),
      mPureInteger(attribute.mPureInteger)
{
    // Ensure we aren't initializing a vertex format which should be using
    // the current-value type
    ASSERT(attribute.mArrayEnabled);

    // Float data can not be normalized, so ignore the user setting
    if (mType == GL_FLOAT || mType == GL_HALF_FLOAT || mType == GL_FIXED)
    {
        mNormalized = GL_FALSE;
    }
}

VertexFormat::VertexFormat(const VertexAttribute &attribute, GLenum currentValueType)
    : mType(attribute.mType),
      mNormalized(attribute.mNormalized ? GL_TRUE : GL_FALSE),
      mComponents(attribute.mSize),
      mPureInteger(attribute.mPureInteger)
{
    if (!attribute.mArrayEnabled)
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
                                  ProgramBinary *programBinary,
                                  const VertexAttribute *attributes,
                                  const gl::VertexAttribCurrentValueData *currentValues)
{
    for (unsigned int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        int semanticIndex = programBinary->getSemanticIndex(attributeIndex);

        if (semanticIndex != -1)
        {
            inputLayout[semanticIndex] = VertexFormat(attributes[attributeIndex], currentValues[attributeIndex].Type);
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

}
