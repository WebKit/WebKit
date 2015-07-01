//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Texture.cpp: Implements the gl::Texture class. [OpenGL ES 2.0.24] section 3.7 page 63.

#include "libANGLE/Texture.h"

#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Data.h"
#include "libANGLE/Surface.h"
#include "libANGLE/formatutils.h"

namespace gl
{

bool IsMipmapFiltered(const gl::SamplerState &samplerState)
{
    switch (samplerState.minFilter)
    {
      case GL_NEAREST:
      case GL_LINEAR:
        return false;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
        return true;
      default: UNREACHABLE();
        return false;
    }
}

bool IsPointSampled(const gl::SamplerState &samplerState)
{
    return (samplerState.magFilter == GL_NEAREST && (samplerState.minFilter == GL_NEAREST || samplerState.minFilter == GL_NEAREST_MIPMAP_NEAREST));
}

static size_t GetImageDescIndex(GLenum target, size_t level)
{
    return IsCubeMapTextureTarget(target) ? ((level * 6) + CubeMapTextureTargetToLayerIndex(target)) : level;
}

unsigned int Texture::mCurrentTextureSerial = 1;

Texture::Texture(rx::TextureImpl *impl, GLuint id, GLenum target)
    : FramebufferAttachmentObject(id),
      mTexture(impl),
      mTextureSerial(issueTextureSerial()),
      mUsage(GL_NONE),
      mImmutableLevelCount(0),
      mTarget(target),
      mImageDescs(IMPLEMENTATION_MAX_TEXTURE_LEVELS * (target == GL_TEXTURE_CUBE_MAP ? 6 : 1)),
      mCompletenessCache(),
      mBoundSurface(NULL)
{
}

Texture::~Texture()
{
    if (mBoundSurface)
    {
        mBoundSurface->releaseTexImage(EGL_BACK_BUFFER);
        mBoundSurface = NULL;
    }
    SafeDelete(mTexture);
}

GLenum Texture::getTarget() const
{
    return mTarget;
}

void Texture::setUsage(GLenum usage)
{
    mUsage = usage;
    getImplementation()->setUsage(usage);
}

GLenum Texture::getUsage() const
{
    return mUsage;
}

size_t Texture::getWidth(GLenum target, size_t level) const
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return getImageDesc(target, level).size.width;
}

size_t Texture::getHeight(GLenum target, size_t level) const
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return getImageDesc(target, level).size.height;
}

size_t Texture::getDepth(GLenum target, size_t level) const
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return getImageDesc(target, level).size.depth;
}

GLenum Texture::getInternalFormat(GLenum target, size_t level) const
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return getImageDesc(target, level).internalFormat;
}

bool Texture::isSamplerComplete(const SamplerState &samplerState, const Data &data) const
{
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), samplerState.baseLevel);
    const TextureCaps &textureCaps = data.textureCaps->get(baseImageDesc.internalFormat);
    if (!mCompletenessCache.cacheValid ||
        mCompletenessCache.samplerState != samplerState ||
        mCompletenessCache.filterable != textureCaps.filterable ||
        mCompletenessCache.clientVersion != data.clientVersion ||
        mCompletenessCache.supportsNPOT != data.extensions->textureNPOT)
    {
        mCompletenessCache.cacheValid = true;
        mCompletenessCache.samplerState = samplerState;
        mCompletenessCache.filterable = textureCaps.filterable;
        mCompletenessCache.clientVersion = data.clientVersion;
        mCompletenessCache.supportsNPOT = data.extensions->textureNPOT;
        mCompletenessCache.samplerComplete = computeSamplerCompleteness(samplerState, data);
    }
    return mCompletenessCache.samplerComplete;
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool Texture::isCubeComplete() const
{
    ASSERT(mTarget == GL_TEXTURE_CUBE_MAP);

    const ImageDesc &baseImageDesc = getImageDesc(FirstCubeMapTextureTarget, 0);
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.width != baseImageDesc.size.height)
    {
        return false;
    }

    for (GLenum face = FirstCubeMapTextureTarget + 1; face <= LastCubeMapTextureTarget; face++)
    {
        const ImageDesc &faceImageDesc = getImageDesc(face, 0);
        if (faceImageDesc.size.width != baseImageDesc.size.width ||
            faceImageDesc.size.height != baseImageDesc.size.height ||
            faceImageDesc.internalFormat != baseImageDesc.internalFormat)
        {
            return false;
        }
    }

    return true;
}

unsigned int Texture::getTextureSerial() const
{
    return mTextureSerial;
}

unsigned int Texture::issueTextureSerial()
{
    return mCurrentTextureSerial++;
}

bool Texture::isImmutable() const
{
    return (mImmutableLevelCount > 0);
}

int Texture::immutableLevelCount()
{
    return mImmutableLevelCount;
}

Error Texture::setImage(GLenum target, size_t level, GLenum internalFormat, const Extents &size, GLenum format, GLenum type,
                        const PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    Error error = mTexture->setImage(target, level, internalFormat, size, format, type, unpack, pixels);
    if (error.isError())
    {
        return error;
    }

    releaseTexImage();

    setImageDesc(target, level, ImageDesc(size, GetSizedInternalFormat(internalFormat, type)));

    return Error(GL_NO_ERROR);
}

Error Texture::setSubImage(GLenum target, size_t level, const Box &area, GLenum format, GLenum type,
                           const PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    return mTexture->setSubImage(target, level, area, format, type, unpack, pixels);
}

Error Texture::setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const Extents &size,
                                  const PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    Error error = mTexture->setCompressedImage(target, level, internalFormat, size, unpack, pixels);
    if (error.isError())
    {
        return error;
    }

    releaseTexImage();

    setImageDesc(target, level, ImageDesc(size, GetSizedInternalFormat(internalFormat, GL_UNSIGNED_BYTE)));

    return Error(GL_NO_ERROR);
}

Error Texture::setCompressedSubImage(GLenum target, size_t level, const Box &area, GLenum format,
                                     const PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    return mTexture->setCompressedSubImage(target, level, area, format, unpack, pixels);
}

Error Texture::copyImage(GLenum target, size_t level, const Rectangle &sourceArea, GLenum internalFormat,
                         const Framebuffer *source)
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    Error error = mTexture->copyImage(target, level, sourceArea, internalFormat, source);
    if (error.isError())
    {
        return error;
    }

    releaseTexImage();

    setImageDesc(target, level, ImageDesc(Extents(sourceArea.width, sourceArea.height, 1),
                                          GetSizedInternalFormat(internalFormat, GL_UNSIGNED_BYTE)));

    return Error(GL_NO_ERROR);
}

Error Texture::copySubImage(GLenum target, size_t level, const Offset &destOffset, const Rectangle &sourceArea,
                            const Framebuffer *source)
{
    ASSERT(target == mTarget || (mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    return mTexture->copySubImage(target, level, destOffset, sourceArea, source);
}

Error Texture::setStorage(GLenum target, size_t levels, GLenum internalFormat, const Extents &size)
{
    ASSERT(target == mTarget);

    Error error = mTexture->setStorage(target, levels, internalFormat, size);
    if (error.isError())
    {
        return error;
    }

    releaseTexImage();

    mImmutableLevelCount = static_cast<GLsizei>(levels);
    clearImageDescs();
    setImageDescChain(levels, size, internalFormat);

    return Error(GL_NO_ERROR);
}


Error Texture::generateMipmaps()
{
    Error error = mTexture->generateMipmaps(getSamplerState());
    if (error.isError())
    {
        return error;
    }

    releaseTexImage();

    const ImageDesc &baseImageInfo = getImageDesc(getBaseImageTarget(), 0);
    size_t mipLevels = log2(std::max(std::max(baseImageInfo.size.width, baseImageInfo.size.height), baseImageInfo.size.depth)) + 1;
    setImageDescChain(mipLevels, baseImageInfo.size, baseImageInfo.internalFormat);

    return Error(GL_NO_ERROR);
}

void Texture::setImageDescChain(size_t levels, Extents baseSize, GLenum sizedInternalFormat)
{
    for (size_t level = 0; level < levels; level++)
    {
        Extents levelSize(std::max<int>(baseSize.width >> level, 1),
                          std::max<int>(baseSize.height >> level, 1),
                          (mTarget == GL_TEXTURE_2D_ARRAY) ? baseSize.depth : std::max<int>(baseSize.depth >> level, 1));
        ImageDesc levelInfo(levelSize, sizedInternalFormat);

        if (mTarget == GL_TEXTURE_CUBE_MAP)
        {
            for (size_t face = FirstCubeMapTextureTarget; face <= LastCubeMapTextureTarget; face++)
            {
                setImageDesc(static_cast<GLenum>(face), level, levelInfo);
            }
        }
        else
        {
            setImageDesc(mTarget, level, levelInfo);
        }
    }
}

Texture::ImageDesc::ImageDesc()
    : ImageDesc(Extents(0, 0, 0), GL_NONE)
{
}

Texture::ImageDesc::ImageDesc(const Extents &size, GLenum internalFormat)
    : size(size),
      internalFormat(internalFormat)
{
}

const Texture::ImageDesc &Texture::getImageDesc(GLenum target, size_t level) const
{
    size_t descIndex = GetImageDescIndex(target, level);
    ASSERT(descIndex < mImageDescs.size());
    return mImageDescs[descIndex];
}

void Texture::setImageDesc(GLenum target, size_t level, const ImageDesc &desc)
{
    size_t descIndex = GetImageDescIndex(target, level);
    ASSERT(descIndex < mImageDescs.size());
    mImageDescs[descIndex] = desc;
    mCompletenessCache.cacheValid = false;
}

void Texture::clearImageDesc(GLenum target, size_t level)
{
    setImageDesc(target, level, ImageDesc());
}

void Texture::clearImageDescs()
{
    for (size_t descIndex = 0; descIndex < mImageDescs.size(); descIndex++)
    {
        mImageDescs[descIndex] = ImageDesc();
    }
    mCompletenessCache.cacheValid = false;
}

void Texture::bindTexImage(egl::Surface *surface)
{
    ASSERT(surface);

    releaseTexImage();
    mTexture->bindTexImage(surface);
    mBoundSurface = surface;

    // Set the image info to the size and format of the surface
    ASSERT(mTarget == GL_TEXTURE_2D);
    Extents size(surface->getWidth(), surface->getHeight(), 1);
    ImageDesc desc(size, surface->getConfig()->renderTargetFormat);
    setImageDesc(mTarget, 0, desc);
}

void Texture::releaseTexImage()
{
    if (mBoundSurface)
    {
        mBoundSurface = NULL;
        mTexture->releaseTexImage();

        // Erase the image info for level 0
        ASSERT(mTarget == GL_TEXTURE_2D);
        clearImageDesc(mTarget, 0);
    }
}

GLenum Texture::getBaseImageTarget() const
{
    return mTarget == GL_TEXTURE_CUBE_MAP ? FirstCubeMapTextureTarget : mTarget;
}

size_t Texture::getExpectedMipLevels() const
{
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), 0);
    if (mTarget == GL_TEXTURE_3D)
    {
        return log2(std::max(std::max(baseImageDesc.size.width, baseImageDesc.size.height), baseImageDesc.size.depth)) + 1;
    }
    else
    {
        return log2(std::max(baseImageDesc.size.width, baseImageDesc.size.height)) + 1;
    }
}

bool Texture::computeSamplerCompleteness(const SamplerState &samplerState, const Data &data) const
{
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), samplerState.baseLevel);
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.height == 0 || baseImageDesc.size.depth == 0)
    {
        return false;
    }

    if (mTarget == GL_TEXTURE_CUBE_MAP && baseImageDesc.size.width != baseImageDesc.size.height)
    {
        return false;
    }

    const TextureCaps &textureCaps = data.textureCaps->get(baseImageDesc.internalFormat);
    if (!textureCaps.filterable && !IsPointSampled(samplerState))
    {
        return false;
    }

    bool npotSupport = data.extensions->textureNPOT || data.clientVersion >= 3;
    if (!npotSupport)
    {
        if ((samplerState.wrapS != GL_CLAMP_TO_EDGE && !gl::isPow2(baseImageDesc.size.width)) ||
            (samplerState.wrapT != GL_CLAMP_TO_EDGE && !gl::isPow2(baseImageDesc.size.height)))
        {
            return false;
        }
    }

    if (IsMipmapFiltered(samplerState))
    {
        if (!npotSupport)
        {
            if (!gl::isPow2(baseImageDesc.size.width) || !gl::isPow2(baseImageDesc.size.height))
            {
                return false;
            }
        }

        if (!computeMipmapCompleteness(samplerState))
        {
            return false;
        }
    }
    else
    {
        if (mTarget == GL_TEXTURE_CUBE_MAP && !isCubeComplete())
        {
            return false;
        }
    }

    // OpenGLES 3.0.2 spec section 3.8.13 states that a texture is not mipmap complete if:
    // The internalformat specified for the texture arrays is a sized internal depth or
    // depth and stencil format (see table 3.13), the value of TEXTURE_COMPARE_-
    // MODE is NONE, and either the magnification filter is not NEAREST or the mini-
    // fication filter is neither NEAREST nor NEAREST_MIPMAP_NEAREST.
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(baseImageDesc.internalFormat);
    if (formatInfo.depthBits > 0 && data.clientVersion > 2)
    {
        if (samplerState.compareMode == GL_NONE)
        {
            if ((samplerState.minFilter != GL_NEAREST && samplerState.minFilter != GL_NEAREST_MIPMAP_NEAREST) ||
                samplerState.magFilter != GL_NEAREST)
            {
                return false;
            }
        }
    }

    return true;
}

bool Texture::computeMipmapCompleteness(const gl::SamplerState &samplerState) const
{
    size_t expectedMipLevels = getExpectedMipLevels();

    size_t maxLevel = std::min<size_t>(expectedMipLevels, samplerState.maxLevel + 1);

    for (size_t level = samplerState.baseLevel; level < maxLevel; level++)
    {
        if (mTarget == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = FirstCubeMapTextureTarget; face <= LastCubeMapTextureTarget; face++)
            {
                if (!computeLevelCompleteness(face, level, samplerState))
                {
                    return false;
                }
            }
        }
        else
        {
            if (!computeLevelCompleteness(mTarget, level, samplerState))
            {
                return false;
            }
        }
    }

    return true;
}


bool Texture::computeLevelCompleteness(GLenum target, size_t level, const gl::SamplerState &samplerState) const
{
    ASSERT(level < IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (isImmutable())
    {
        return true;
    }

    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), samplerState.baseLevel);
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.height == 0 || baseImageDesc.size.depth == 0)
    {
        return false;
    }

    // The base image level is complete if the width and height are positive
    if (level == 0)
    {
        return true;
    }

    const ImageDesc &levelImageDesc = getImageDesc(target, level);
    if (levelImageDesc.internalFormat != baseImageDesc.internalFormat)
    {
        return false;
    }

    if (levelImageDesc.size.width != std::max(1, baseImageDesc.size.width >> level))
    {
        return false;
    }

    if (levelImageDesc.size.height != std::max(1, baseImageDesc.size.height >> level))
    {
        return false;
    }

    if (mTarget == GL_TEXTURE_3D)
    {
        if (levelImageDesc.size.depth != std::max(1, baseImageDesc.size.depth >> level))
        {
            return false;
        }
    }
    else if (mTarget == GL_TEXTURE_2D_ARRAY)
    {
        if (levelImageDesc.size.depth != baseImageDesc.size.depth)
        {
            return false;
        }
    }

    return true;
}

Texture::SamplerCompletenessCache::SamplerCompletenessCache()
    : cacheValid(false),
      samplerState(),
      filterable(false),
      clientVersion(0),
      supportsNPOT(false),
      samplerComplete(false)
{
}

GLsizei Texture::getAttachmentWidth(const gl::FramebufferAttachment::Target &target) const
{
    return static_cast<GLsizei>(getWidth(target.textureIndex().type, target.textureIndex().mipIndex));
}

GLsizei Texture::getAttachmentHeight(const gl::FramebufferAttachment::Target &target) const
{
    return static_cast<GLsizei>(getHeight(target.textureIndex().type, target.textureIndex().mipIndex));
}

GLenum Texture::getAttachmentInternalFormat(const gl::FramebufferAttachment::Target &target) const
{
    return getInternalFormat(target.textureIndex().type, target.textureIndex().mipIndex);
}

GLsizei Texture::getAttachmentSamples(const gl::FramebufferAttachment::Target &/*target*/) const
{
    // Multisample textures not currently supported
    return 0;
}

}
