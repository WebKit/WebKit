#include "ImageIndex.h"
//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ImageIndex.cpp: Implementation for ImageIndex methods.

#include "libANGLE/ImageIndex.h"
#include "libANGLE/Constants.h"
#include "common/utilities.h"

namespace gl
{

ImageIndex::ImageIndex(const ImageIndex &other)
    : type(other.type),
      mipIndex(other.mipIndex),
      layerIndex(other.layerIndex),
      numLayers(other.numLayers)
{}

ImageIndex &ImageIndex::operator=(const ImageIndex &other)
{
    type = other.type;
    mipIndex = other.mipIndex;
    layerIndex = other.layerIndex;
    numLayers  = other.numLayers;
    return *this;
}

bool ImageIndex::is3D() const
{
    return type == GL_TEXTURE_3D || type == GL_TEXTURE_2D_ARRAY;
}

ImageIndex ImageIndex::Make2D(GLint mipIndex)
{
    return ImageIndex(GL_TEXTURE_2D, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeRectangle(GLint mipIndex)
{
    return ImageIndex(GL_TEXTURE_RECTANGLE_ANGLE, mipIndex, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeCube(GLenum target, GLint mipIndex)
{
    ASSERT(gl::IsCubeMapTextureTarget(target));
    return ImageIndex(target, mipIndex,
                      static_cast<GLint>(CubeMapTextureTargetToLayerIndex(target)), 1);
}

ImageIndex ImageIndex::Make2DArray(GLint mipIndex, GLint layerIndex)
{
    return ImageIndex(GL_TEXTURE_2D_ARRAY, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::Make2DArrayRange(GLint mipIndex, GLint layerIndex, GLint numLayers)
{
    return ImageIndex(GL_TEXTURE_2D_ARRAY, mipIndex, layerIndex, numLayers);
}

ImageIndex ImageIndex::Make3D(GLint mipIndex, GLint layerIndex)
{
    return ImageIndex(GL_TEXTURE_3D, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::MakeGeneric(GLenum target, GLint mipIndex)
{
    GLint layerIndex = IsCubeMapTextureTarget(target)
                           ? static_cast<GLint>(CubeMapTextureTargetToLayerIndex(target))
                           : ENTIRE_LEVEL;
    return ImageIndex(target, mipIndex, layerIndex, 1);
}

ImageIndex ImageIndex::Make2DMultisample()
{
    return ImageIndex(GL_TEXTURE_2D_MULTISAMPLE, 0, ENTIRE_LEVEL, 1);
}

ImageIndex ImageIndex::MakeInvalid()
{
    return ImageIndex(GL_NONE, -1, -1, -1);
}

bool ImageIndex::operator<(const ImageIndex &other) const
{
    if (type != other.type)
    {
        return type < other.type;
    }
    else if (mipIndex != other.mipIndex)
    {
        return mipIndex < other.mipIndex;
    }
    else if (layerIndex != other.layerIndex)
    {
        return layerIndex < other.layerIndex;
    }
    else
    {
        return numLayers < other.numLayers;
    }
}

bool ImageIndex::operator==(const ImageIndex &other) const
{
    return (type == other.type) && (mipIndex == other.mipIndex) &&
           (layerIndex == other.layerIndex) && (numLayers == other.numLayers);
}

bool ImageIndex::operator!=(const ImageIndex &other) const
{
    return !(*this == other);
}

ImageIndex::ImageIndex(GLenum typeIn, GLint mipIndexIn, GLint layerIndexIn, GLint numLayersIn)
    : type(typeIn), mipIndex(mipIndexIn), layerIndex(layerIndexIn), numLayers(numLayersIn)
{}

ImageIndexIterator::ImageIndexIterator(const ImageIndexIterator &other) = default;

ImageIndexIterator ImageIndexIterator::Make2D(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(GL_TEXTURE_2D, Range<GLint>(minMip, maxMip),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeRectangle(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(GL_TEXTURE_RECTANGLE_ANGLE, Range<GLint>(minMip, maxMip),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::MakeCube(GLint minMip, GLint maxMip)
{
    return ImageIndexIterator(GL_TEXTURE_CUBE_MAP, Range<GLint>(minMip, maxMip), Range<GLint>(0, 6),
                              nullptr);
}

ImageIndexIterator ImageIndexIterator::Make3D(GLint minMip, GLint maxMip,
                                              GLint minLayer, GLint maxLayer)
{
    return ImageIndexIterator(GL_TEXTURE_3D, Range<GLint>(minMip, maxMip),
                              Range<GLint>(minLayer, maxLayer), nullptr);
}

ImageIndexIterator ImageIndexIterator::Make2DArray(GLint minMip, GLint maxMip,
                                                   const GLsizei *layerCounts)
{
    return ImageIndexIterator(GL_TEXTURE_2D_ARRAY, Range<GLint>(minMip, maxMip),
                              Range<GLint>(0, IMPLEMENTATION_MAX_2D_ARRAY_TEXTURE_LAYERS), layerCounts);
}

ImageIndexIterator ImageIndexIterator::Make2DMultisample()
{
    return ImageIndexIterator(GL_TEXTURE_2D_MULTISAMPLE, Range<GLint>(0, 0),
                              Range<GLint>(ImageIndex::ENTIRE_LEVEL, ImageIndex::ENTIRE_LEVEL),
                              nullptr);
}

ImageIndexIterator::ImageIndexIterator(GLenum type,
                                       const Range<GLint> &mipRange,
                                       const Range<GLint> &layerRange,
                                       const GLsizei *layerCounts)
    : mType(type),
      mMipRange(mipRange),
      mLayerRange(layerRange),
      mLayerCounts(layerCounts),
      mCurrentMip(mipRange.low()),
      mCurrentLayer(layerRange.low())
{}

GLint ImageIndexIterator::maxLayer() const
{
    if (mLayerCounts)
    {
        ASSERT(mCurrentMip >= 0);
        return (mCurrentMip < mMipRange.high()) ? mLayerCounts[mCurrentMip] : 0;
    }
    return mLayerRange.high();
}

ImageIndex ImageIndexIterator::next()
{
    ASSERT(hasNext());

    ImageIndex value = current();

    // Iterate layers in the inner loop for now. We can add switchable
    // layer or mip iteration if we need it.

    if (mCurrentLayer != ImageIndex::ENTIRE_LEVEL)
    {
        if (mCurrentLayer < maxLayer() - 1)
        {
            mCurrentLayer++;
        }
        else if (mCurrentMip < mMipRange.high() - 1)
        {
            mCurrentMip++;
            mCurrentLayer = mLayerRange.low();
        }
        else
        {
            done();
        }
    }
    else if (mCurrentMip < mMipRange.high() - 1)
    {
        mCurrentMip++;
        mCurrentLayer = mLayerRange.low();
    }
    else
    {
        done();
    }

    return value;
}

ImageIndex ImageIndexIterator::current() const
{
    ImageIndex value(mType, mCurrentMip, mCurrentLayer, 1);

    if (mType == GL_TEXTURE_CUBE_MAP)
    {
        value.type = LayerIndexToCubeMapTextureTarget(mCurrentLayer);
    }

    return value;
}

bool ImageIndexIterator::hasNext() const
{
    return (mCurrentMip < mMipRange.high() || mCurrentLayer < maxLayer());
}

void ImageIndexIterator::done()
{
    mCurrentMip   = mMipRange.high();
    mCurrentLayer = maxLayer();
}

}  // namespace gl
