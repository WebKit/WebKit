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
#include "libANGLE/Context.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Image.h"
#include "libANGLE/Surface.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/TextureImpl.h"

namespace gl
{

namespace
{
bool IsPointSampled(const SamplerState &samplerState)
{
    return (samplerState.magFilter == GL_NEAREST &&
            (samplerState.minFilter == GL_NEAREST ||
             samplerState.minFilter == GL_NEAREST_MIPMAP_NEAREST));
}

size_t GetImageDescIndex(GLenum target, size_t level)
{
    return IsCubeMapTextureTarget(target) ? ((level * 6) + CubeMapTextureTargetToLayerIndex(target))
                                          : level;
}
}  // namespace

bool IsMipmapFiltered(const SamplerState &samplerState)
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
        default:
            UNREACHABLE();
            return false;
    }
}

SwizzleState::SwizzleState()
    : swizzleRed(GL_INVALID_INDEX),
      swizzleGreen(GL_INVALID_INDEX),
      swizzleBlue(GL_INVALID_INDEX),
      swizzleAlpha(GL_INVALID_INDEX)
{
}

SwizzleState::SwizzleState(GLenum red, GLenum green, GLenum blue, GLenum alpha)
    : swizzleRed(red), swizzleGreen(green), swizzleBlue(blue), swizzleAlpha(alpha)
{
}

bool SwizzleState::swizzleRequired() const
{
    return swizzleRed != GL_RED || swizzleGreen != GL_GREEN || swizzleBlue != GL_BLUE ||
           swizzleAlpha != GL_ALPHA;
}

bool SwizzleState::operator==(const SwizzleState &other) const
{
    return swizzleRed == other.swizzleRed && swizzleGreen == other.swizzleGreen &&
           swizzleBlue == other.swizzleBlue && swizzleAlpha == other.swizzleAlpha;
}

bool SwizzleState::operator!=(const SwizzleState &other) const
{
    return !(*this == other);
}

TextureState::TextureState(GLenum target)
    : mTarget(target),
      mSwizzleState(GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA),
      mSamplerState(SamplerState::CreateDefaultForTarget(target)),
      mBaseLevel(0),
      mMaxLevel(1000),
      mDepthStencilTextureMode(GL_DEPTH_COMPONENT),
      mImmutableFormat(false),
      mImmutableLevels(0),
      mUsage(GL_NONE),
      mImageDescs((IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1) *
                  (target == GL_TEXTURE_CUBE_MAP ? 6 : 1)),
      mCompletenessCache()
{
}

bool TextureState::swizzleRequired() const
{
    return mSwizzleState.swizzleRequired();
}

GLuint TextureState::getEffectiveBaseLevel() const
{
    if (mImmutableFormat)
    {
        // GLES 3.0.4 section 3.8.10
        return std::min(mBaseLevel, mImmutableLevels - 1);
    }
    // Some classes use the effective base level to index arrays with level data. By clamping the
    // effective base level to max levels these arrays need just one extra item to store properties
    // that should be returned for all out-of-range base level values, instead of needing special
    // handling for out-of-range base levels.
    return std::min(mBaseLevel, static_cast<GLuint>(IMPLEMENTATION_MAX_TEXTURE_LEVELS));
}

GLuint TextureState::getEffectiveMaxLevel() const
{
    if (mImmutableFormat)
    {
        // GLES 3.0.4 section 3.8.10
        GLuint clampedMaxLevel = std::max(mMaxLevel, getEffectiveBaseLevel());
        clampedMaxLevel        = std::min(clampedMaxLevel, mImmutableLevels - 1);
        return clampedMaxLevel;
    }
    return mMaxLevel;
}

GLuint TextureState::getMipmapMaxLevel() const
{
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
    GLuint expectedMipLevels       = 0;
    if (mTarget == GL_TEXTURE_3D)
    {
        const int maxDim = std::max(std::max(baseImageDesc.size.width, baseImageDesc.size.height),
                                    baseImageDesc.size.depth);
        expectedMipLevels = static_cast<GLuint>(log2(maxDim));
    }
    else
    {
        expectedMipLevels = static_cast<GLuint>(
            log2(std::max(baseImageDesc.size.width, baseImageDesc.size.height)));
    }

    return std::min<GLuint>(getEffectiveBaseLevel() + expectedMipLevels, getEffectiveMaxLevel());
}

bool TextureState::setBaseLevel(GLuint baseLevel)
{
    if (mBaseLevel != baseLevel)
    {
        mBaseLevel = baseLevel;
        invalidateCompletenessCache();
        return true;
    }
    return false;
}

void TextureState::setMaxLevel(GLuint maxLevel)
{
    if (mMaxLevel != maxLevel)
    {
        mMaxLevel = maxLevel;
        invalidateCompletenessCache();
    }
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool TextureState::isCubeComplete() const
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
            !Format::SameSized(faceImageDesc.format, baseImageDesc.format))
        {
            return false;
        }
    }

    return true;
}

bool TextureState::isSamplerComplete(const SamplerState &samplerState,
                                     const ContextState &data) const
{
    bool newEntry  = false;
    auto cacheIter = mCompletenessCache.find(data.getContextID());
    if (cacheIter == mCompletenessCache.end())
    {
        // Add a new cache entry
        cacheIter = mCompletenessCache
                        .insert(std::make_pair(data.getContextID(), SamplerCompletenessCache()))
                        .first;
        newEntry = true;
    }

    SamplerCompletenessCache *cacheEntry = &cacheIter->second;
    if (newEntry || cacheEntry->samplerState != samplerState)
    {
        cacheEntry->samplerState    = samplerState;
        cacheEntry->samplerComplete = computeSamplerCompleteness(samplerState, data);
    }

    return cacheEntry->samplerComplete;
}

void TextureState::invalidateCompletenessCache()
{
    mCompletenessCache.clear();
}

bool TextureState::computeSamplerCompleteness(const SamplerState &samplerState,
                                              const ContextState &data) const
{
    if (mBaseLevel > mMaxLevel)
    {
        return false;
    }
    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.height == 0 ||
        baseImageDesc.size.depth == 0)
    {
        return false;
    }
    // The cases where the texture is incomplete because base level is out of range should be
    // handled by the above condition.
    ASSERT(mBaseLevel < IMPLEMENTATION_MAX_TEXTURE_LEVELS || mImmutableFormat);

    if (mTarget == GL_TEXTURE_CUBE_MAP && baseImageDesc.size.width != baseImageDesc.size.height)
    {
        return false;
    }

    const TextureCaps &textureCaps = data.getTextureCap(baseImageDesc.format.asSized());
    if (!textureCaps.filterable && !IsPointSampled(samplerState))
    {
        return false;
    }
    bool npotSupport = data.getExtensions().textureNPOT || data.getClientMajorVersion() >= 3;
    if (!npotSupport)
    {
        if ((samplerState.wrapS != GL_CLAMP_TO_EDGE && !isPow2(baseImageDesc.size.width)) ||
            (samplerState.wrapT != GL_CLAMP_TO_EDGE && !isPow2(baseImageDesc.size.height)))
        {
            return false;
        }
    }

    if (IsMipmapFiltered(samplerState))
    {
        if (!npotSupport)
        {
            if (!isPow2(baseImageDesc.size.width) || !isPow2(baseImageDesc.size.height))
            {
                return false;
            }
        }

        if (!computeMipmapCompleteness())
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

    // From GL_OES_EGL_image_external_essl3: If state is present in a sampler object bound to a
    // texture unit that would have been rejected by a call to TexParameter* for the texture bound
    // to that unit, the behavior of the implementation is as if the texture were incomplete. For
    // example, if TEXTURE_WRAP_S or TEXTURE_WRAP_T is set to anything but CLAMP_TO_EDGE on the
    // sampler object bound to a texture unit and the texture bound to that unit is an external
    // texture, the texture will be considered incomplete.
    // Sampler object state which does not affect sampling for the type of texture bound to a
    // texture unit, such as TEXTURE_WRAP_R for an external texture, does not affect completeness.
    if (mTarget == GL_TEXTURE_EXTERNAL_OES)
    {
        if (samplerState.wrapS != GL_CLAMP_TO_EDGE || samplerState.wrapT != GL_CLAMP_TO_EDGE)
        {
            return false;
        }

        if (samplerState.minFilter != GL_LINEAR && samplerState.minFilter != GL_NEAREST)
        {
            return false;
        }
    }

    // OpenGLES 3.0.2 spec section 3.8.13 states that a texture is not mipmap complete if:
    // The internalformat specified for the texture arrays is a sized internal depth or
    // depth and stencil format (see table 3.13), the value of TEXTURE_COMPARE_-
    // MODE is NONE, and either the magnification filter is not NEAREST or the mini-
    // fication filter is neither NEAREST nor NEAREST_MIPMAP_NEAREST.
    if (baseImageDesc.format.info->depthBits > 0 && data.getClientMajorVersion() >= 3)
    {
        // Note: we restrict this validation to sized types. For the OES_depth_textures
        // extension, due to some underspecification problems, we must allow linear filtering
        // for legacy compatibility with WebGL 1.
        // See http://crbug.com/649200
        if (samplerState.compareMode == GL_NONE && baseImageDesc.format.sized)
        {
            if ((samplerState.minFilter != GL_NEAREST &&
                 samplerState.minFilter != GL_NEAREST_MIPMAP_NEAREST) ||
                samplerState.magFilter != GL_NEAREST)
            {
                return false;
            }
        }
    }

    return true;
}

bool TextureState::computeMipmapCompleteness() const
{
    const GLuint maxLevel = getMipmapMaxLevel();

    for (GLuint level = getEffectiveBaseLevel(); level <= maxLevel; level++)
    {
        if (mTarget == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = FirstCubeMapTextureTarget; face <= LastCubeMapTextureTarget; face++)
            {
                if (!computeLevelCompleteness(face, level))
                {
                    return false;
                }
            }
        }
        else
        {
            if (!computeLevelCompleteness(mTarget, level))
            {
                return false;
            }
        }
    }

    return true;
}

bool TextureState::computeLevelCompleteness(GLenum target, size_t level) const
{
    ASSERT(level < IMPLEMENTATION_MAX_TEXTURE_LEVELS);

    if (mImmutableFormat)
    {
        return true;
    }

    const ImageDesc &baseImageDesc = getImageDesc(getBaseImageTarget(), getEffectiveBaseLevel());
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.height == 0 ||
        baseImageDesc.size.depth == 0)
    {
        return false;
    }

    const ImageDesc &levelImageDesc = getImageDesc(target, level);
    if (levelImageDesc.size.width == 0 || levelImageDesc.size.height == 0 ||
        levelImageDesc.size.depth == 0)
    {
        return false;
    }

    if (!Format::SameSized(levelImageDesc.format, baseImageDesc.format))
    {
        return false;
    }

    ASSERT(level >= getEffectiveBaseLevel());
    const size_t relativeLevel = level - getEffectiveBaseLevel();
    if (levelImageDesc.size.width != std::max(1, baseImageDesc.size.width >> relativeLevel))
    {
        return false;
    }

    if (levelImageDesc.size.height != std::max(1, baseImageDesc.size.height >> relativeLevel))
    {
        return false;
    }

    if (mTarget == GL_TEXTURE_3D)
    {
        if (levelImageDesc.size.depth != std::max(1, baseImageDesc.size.depth >> relativeLevel))
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

GLenum TextureState::getBaseImageTarget() const
{
    return mTarget == GL_TEXTURE_CUBE_MAP ? FirstCubeMapTextureTarget : mTarget;
}

ImageDesc::ImageDesc() : ImageDesc(Extents(0, 0, 0), Format::Invalid(), 0, GL_TRUE)
{
}

ImageDesc::ImageDesc(const Extents &size, const Format &format)
    : size(size), format(format), samples(0), fixedSampleLocations(GL_TRUE)
{
}

ImageDesc::ImageDesc(const Extents &size,
                     const Format &format,
                     const GLsizei samples,
                     const GLboolean fixedSampleLocations)
    : size(size), format(format), samples(samples), fixedSampleLocations(fixedSampleLocations)
{
}

const ImageDesc &TextureState::getImageDesc(GLenum target, size_t level) const
{
    size_t descIndex = GetImageDescIndex(target, level);
    ASSERT(descIndex < mImageDescs.size());
    return mImageDescs[descIndex];
}

void TextureState::setImageDesc(GLenum target, size_t level, const ImageDesc &desc)
{
    size_t descIndex = GetImageDescIndex(target, level);
    ASSERT(descIndex < mImageDescs.size());
    mImageDescs[descIndex] = desc;
    invalidateCompletenessCache();
}

void TextureState::setImageDescChain(GLuint baseLevel,
                                     GLuint maxLevel,
                                     Extents baseSize,
                                     const Format &format)
{
    for (GLuint level = baseLevel; level <= maxLevel; level++)
    {
        int relativeLevel = (level - baseLevel);
        Extents levelSize(std::max<int>(baseSize.width >> relativeLevel, 1),
                          std::max<int>(baseSize.height >> relativeLevel, 1),
                          (mTarget == GL_TEXTURE_2D_ARRAY)
                              ? baseSize.depth
                              : std::max<int>(baseSize.depth >> relativeLevel, 1));
        ImageDesc levelInfo(levelSize, format);

        if (mTarget == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = FirstCubeMapTextureTarget; face <= LastCubeMapTextureTarget; face++)
            {
                setImageDesc(face, level, levelInfo);
            }
        }
        else
        {
            setImageDesc(mTarget, level, levelInfo);
        }
    }
}

void TextureState::setImageDescChainMultisample(Extents baseSize,
                                                const Format &format,
                                                GLsizei samples,
                                                GLboolean fixedSampleLocations)
{
    ASSERT(mTarget == GL_TEXTURE_2D_MULTISAMPLE);
    ImageDesc levelInfo(baseSize, format, samples, fixedSampleLocations);
    setImageDesc(mTarget, 0, levelInfo);
}

void TextureState::clearImageDesc(GLenum target, size_t level)
{
    setImageDesc(target, level, ImageDesc());
}

void TextureState::clearImageDescs()
{
    for (size_t descIndex = 0; descIndex < mImageDescs.size(); descIndex++)
    {
        mImageDescs[descIndex] = ImageDesc();
    }
    invalidateCompletenessCache();
}

TextureState::SamplerCompletenessCache::SamplerCompletenessCache()
    : samplerState(), samplerComplete(false)
{
}

Texture::Texture(rx::GLImplFactory *factory, GLuint id, GLenum target)
    : egl::ImageSibling(id),
      mState(target),
      mTexture(factory->createTexture(mState)),
      mLabel(),
      mBoundSurface(nullptr),
      mBoundStream(nullptr)
{
}

Texture::~Texture()
{
    if (mBoundSurface)
    {
        mBoundSurface->releaseTexImage(EGL_BACK_BUFFER);
        mBoundSurface = nullptr;
    }
    if (mBoundStream)
    {
        mBoundStream->releaseTextures();
        mBoundStream = nullptr;
    }
    SafeDelete(mTexture);
}

void Texture::setLabel(const std::string &label)
{
    mLabel = label;
    mDirtyBits.set(DIRTY_BIT_LABEL);
}

const std::string &Texture::getLabel() const
{
    return mLabel;
}

GLenum Texture::getTarget() const
{
    return mState.mTarget;
}

void Texture::setSwizzleRed(GLenum swizzleRed)
{
    mState.mSwizzleState.swizzleRed = swizzleRed;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_RED);
}

GLenum Texture::getSwizzleRed() const
{
    return mState.mSwizzleState.swizzleRed;
}

void Texture::setSwizzleGreen(GLenum swizzleGreen)
{
    mState.mSwizzleState.swizzleGreen = swizzleGreen;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_GREEN);
}

GLenum Texture::getSwizzleGreen() const
{
    return mState.mSwizzleState.swizzleGreen;
}

void Texture::setSwizzleBlue(GLenum swizzleBlue)
{
    mState.mSwizzleState.swizzleBlue = swizzleBlue;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_BLUE);
}

GLenum Texture::getSwizzleBlue() const
{
    return mState.mSwizzleState.swizzleBlue;
}

void Texture::setSwizzleAlpha(GLenum swizzleAlpha)
{
    mState.mSwizzleState.swizzleAlpha = swizzleAlpha;
    mDirtyBits.set(DIRTY_BIT_SWIZZLE_ALPHA);
}

GLenum Texture::getSwizzleAlpha() const
{
    return mState.mSwizzleState.swizzleAlpha;
}

void Texture::setMinFilter(GLenum minFilter)
{
    mState.mSamplerState.minFilter = minFilter;
    mDirtyBits.set(DIRTY_BIT_MIN_FILTER);
}

GLenum Texture::getMinFilter() const
{
    return mState.mSamplerState.minFilter;
}

void Texture::setMagFilter(GLenum magFilter)
{
    mState.mSamplerState.magFilter = magFilter;
    mDirtyBits.set(DIRTY_BIT_MAG_FILTER);
}

GLenum Texture::getMagFilter() const
{
    return mState.mSamplerState.magFilter;
}

void Texture::setWrapS(GLenum wrapS)
{
    mState.mSamplerState.wrapS = wrapS;
    mDirtyBits.set(DIRTY_BIT_WRAP_S);
}

GLenum Texture::getWrapS() const
{
    return mState.mSamplerState.wrapS;
}

void Texture::setWrapT(GLenum wrapT)
{
    mState.mSamplerState.wrapT = wrapT;
    mDirtyBits.set(DIRTY_BIT_WRAP_T);
}

GLenum Texture::getWrapT() const
{
    return mState.mSamplerState.wrapT;
}

void Texture::setWrapR(GLenum wrapR)
{
    mState.mSamplerState.wrapR = wrapR;
    mDirtyBits.set(DIRTY_BIT_WRAP_R);
}

GLenum Texture::getWrapR() const
{
    return mState.mSamplerState.wrapR;
}

void Texture::setMaxAnisotropy(float maxAnisotropy)
{
    mState.mSamplerState.maxAnisotropy = maxAnisotropy;
    mDirtyBits.set(DIRTY_BIT_MAX_ANISOTROPY);
}

float Texture::getMaxAnisotropy() const
{
    return mState.mSamplerState.maxAnisotropy;
}

void Texture::setMinLod(GLfloat minLod)
{
    mState.mSamplerState.minLod = minLod;
    mDirtyBits.set(DIRTY_BIT_MIN_LOD);
}

GLfloat Texture::getMinLod() const
{
    return mState.mSamplerState.minLod;
}

void Texture::setMaxLod(GLfloat maxLod)
{
    mState.mSamplerState.maxLod = maxLod;
    mDirtyBits.set(DIRTY_BIT_MAX_LOD);
}

GLfloat Texture::getMaxLod() const
{
    return mState.mSamplerState.maxLod;
}

void Texture::setCompareMode(GLenum compareMode)
{
    mState.mSamplerState.compareMode = compareMode;
    mDirtyBits.set(DIRTY_BIT_COMPARE_MODE);
}

GLenum Texture::getCompareMode() const
{
    return mState.mSamplerState.compareMode;
}

void Texture::setCompareFunc(GLenum compareFunc)
{
    mState.mSamplerState.compareFunc = compareFunc;
    mDirtyBits.set(DIRTY_BIT_COMPARE_FUNC);
}

GLenum Texture::getCompareFunc() const
{
    return mState.mSamplerState.compareFunc;
}

void Texture::setSRGBDecode(GLenum sRGBDecode)
{
    mState.mSamplerState.sRGBDecode = sRGBDecode;
    mDirtyBits.set(DIRTY_BIT_SRGB_DECODE);
}

GLenum Texture::getSRGBDecode() const
{
    return mState.mSamplerState.sRGBDecode;
}

const SamplerState &Texture::getSamplerState() const
{
    return mState.mSamplerState;
}

void Texture::setBaseLevel(GLuint baseLevel)
{
    if (mState.setBaseLevel(baseLevel))
    {
        mTexture->setBaseLevel(mState.getEffectiveBaseLevel());
        mDirtyBits.set(DIRTY_BIT_BASE_LEVEL);
    }
}

GLuint Texture::getBaseLevel() const
{
    return mState.mBaseLevel;
}

void Texture::setMaxLevel(GLuint maxLevel)
{
    mState.setMaxLevel(maxLevel);
    mDirtyBits.set(DIRTY_BIT_MAX_LEVEL);
}

GLuint Texture::getMaxLevel() const
{
    return mState.mMaxLevel;
}

void Texture::setDepthStencilTextureMode(GLenum mode)
{
    if (mode != mState.mDepthStencilTextureMode)
    {
        // Changing the mode from the default state (GL_DEPTH_COMPONENT) is not implemented yet
        UNIMPLEMENTED();
    }

    // TODO(geofflang): add dirty bits
    mState.mDepthStencilTextureMode = mode;
}

GLenum Texture::getDepthStencilTextureMode() const
{
    return mState.mDepthStencilTextureMode;
}

bool Texture::getImmutableFormat() const
{
    return mState.mImmutableFormat;
}

GLuint Texture::getImmutableLevels() const
{
    return mState.mImmutableLevels;
}

void Texture::setUsage(GLenum usage)
{
    mState.mUsage = usage;
    mDirtyBits.set(DIRTY_BIT_USAGE);
}

GLenum Texture::getUsage() const
{
    return mState.mUsage;
}

const TextureState &Texture::getTextureState() const
{
    return mState;
}

size_t Texture::getWidth(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).size.width;
}

size_t Texture::getHeight(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).size.height;
}

size_t Texture::getDepth(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).size.depth;
}

const Format &Texture::getFormat(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).format;
}

GLsizei Texture::getSamples(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).samples;
}

GLboolean Texture::getFixedSampleLocations(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).fixedSampleLocations;
}

bool Texture::isMipmapComplete() const
{
    return mState.computeMipmapCompleteness();
}

egl::Surface *Texture::getBoundSurface() const
{
    return mBoundSurface;
}

egl::Stream *Texture::getBoundStream() const
{
    return mBoundStream;
}

void Texture::invalidateCompletenessCache()
{
    mState.invalidateCompletenessCache();
    mDirtyChannel.signal();
}

Error Texture::setImage(const Context *context,
                        const PixelUnpackState &unpackState,
                        GLenum target,
                        size_t level,
                        GLenum internalFormat,
                        const Extents &size,
                        GLenum format,
                        GLenum type,
                        const uint8_t *pixels)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->setImage(rx::SafeGetImpl(context), target, level, internalFormat, size,
                                 format, type, unpackState, pixels));

    mState.setImageDesc(target, level, ImageDesc(size, Format(internalFormat, format, type)));
    mDirtyChannel.signal();

    return NoError();
}

Error Texture::setSubImage(const Context *context,
                           const PixelUnpackState &unpackState,
                           GLenum target,
                           size_t level,
                           const Box &area,
                           GLenum format,
                           GLenum type,
                           const uint8_t *pixels)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mTexture->setSubImage(rx::SafeGetImpl(context), target, level, area, format, type,
                                 unpackState, pixels);
}

Error Texture::setCompressedImage(const Context *context,
                                  const PixelUnpackState &unpackState,
                                  GLenum target,
                                  size_t level,
                                  GLenum internalFormat,
                                  const Extents &size,
                                  size_t imageSize,
                                  const uint8_t *pixels)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->setCompressedImage(rx::SafeGetImpl(context), target, level, internalFormat,
                                           size, unpackState, imageSize, pixels));

    mState.setImageDesc(target, level, ImageDesc(size, Format(internalFormat)));
    mDirtyChannel.signal();

    return NoError();
}

Error Texture::setCompressedSubImage(const Context *context,
                                     const PixelUnpackState &unpackState,
                                     GLenum target,
                                     size_t level,
                                     const Box &area,
                                     GLenum format,
                                     size_t imageSize,
                                     const uint8_t *pixels)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    return mTexture->setCompressedSubImage(rx::SafeGetImpl(context), target, level, area, format,
                                           unpackState, imageSize, pixels);
}

Error Texture::copyImage(const Context *context,
                         GLenum target,
                         size_t level,
                         const Rectangle &sourceArea,
                         GLenum internalFormat,
                         const Framebuffer *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->copyImage(rx::SafeGetImpl(context), target, level, sourceArea,
                                  internalFormat, source));

    const GLenum sizedFormat = GetSizedInternalFormat(internalFormat, GL_UNSIGNED_BYTE);
    mState.setImageDesc(target, level, ImageDesc(Extents(sourceArea.width, sourceArea.height, 1),
                                                 Format(sizedFormat)));
    mDirtyChannel.signal();

    return NoError();
}

Error Texture::copySubImage(const Context *context,
                            GLenum target,
                            size_t level,
                            const Offset &destOffset,
                            const Rectangle &sourceArea,
                            const Framebuffer *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    return mTexture->copySubImage(rx::SafeGetImpl(context), target, level, destOffset, sourceArea,
                                  source);
}

Error Texture::copyTexture(const Context *context,
                           GLenum target,
                           size_t level,
                           GLenum internalFormat,
                           GLenum type,
                           size_t sourceLevel,
                           bool unpackFlipY,
                           bool unpackPremultiplyAlpha,
                           bool unpackUnmultiplyAlpha,
                           const Texture *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->copyTexture(rx::SafeGetImpl(context), target, level, internalFormat, type,
                                    sourceLevel, unpackFlipY, unpackPremultiplyAlpha,
                                    unpackUnmultiplyAlpha, source));

    const auto &sourceDesc   = source->mState.getImageDesc(source->getTarget(), 0);
    const GLenum sizedFormat = GetSizedInternalFormat(internalFormat, type);
    mState.setImageDesc(target, level, ImageDesc(sourceDesc.size, Format(sizedFormat)));
    mDirtyChannel.signal();

    return NoError();
}

Error Texture::copySubTexture(const Context *context,
                              GLenum target,
                              size_t level,
                              const Offset &destOffset,
                              size_t sourceLevel,
                              const Rectangle &sourceArea,
                              bool unpackFlipY,
                              bool unpackPremultiplyAlpha,
                              bool unpackUnmultiplyAlpha,
                              const Texture *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    return mTexture->copySubTexture(rx::SafeGetImpl(context), target, level, destOffset,
                                    sourceLevel, sourceArea, unpackFlipY, unpackPremultiplyAlpha,
                                    unpackUnmultiplyAlpha, source);
}

Error Texture::copyCompressedTexture(const Context *context, const Texture *source)
{
    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->copyCompressedTexture(rx::SafeGetImpl(context), source));

    ASSERT(source->getTarget() != GL_TEXTURE_CUBE_MAP && getTarget() != GL_TEXTURE_CUBE_MAP);
    const auto &sourceDesc = source->mState.getImageDesc(source->getTarget(), 0);
    mState.setImageDesc(getTarget(), 0, sourceDesc);

    return NoError();
}

Error Texture::setStorage(const Context *context,
                          GLenum target,
                          GLsizei levels,
                          GLenum internalFormat,
                          const Extents &size)
{
    ASSERT(target == mState.mTarget);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->setStorage(rx::SafeGetImpl(context), target, levels, internalFormat, size));

    mState.mImmutableFormat = true;
    mState.mImmutableLevels = static_cast<GLuint>(levels);
    mState.clearImageDescs();
    mState.setImageDescChain(0, static_cast<GLuint>(levels - 1), size, Format(internalFormat));

    // Changing the texture to immutable can trigger a change in the base and max levels:
    // GLES 3.0.4 section 3.8.10 pg 158:
    // "For immutable-format textures, levelbase is clamped to the range[0;levels],levelmax is then
    // clamped to the range[levelbase;levels].
    mDirtyBits.set(DIRTY_BIT_BASE_LEVEL);
    mDirtyBits.set(DIRTY_BIT_MAX_LEVEL);

    mDirtyChannel.signal();

    return NoError();
}

Error Texture::setStorageMultisample(const Context *context,
                                     GLenum target,
                                     GLsizei samples,
                                     GLint internalFormat,
                                     const Extents &size,
                                     GLboolean fixedSampleLocations)
{
    ASSERT(target == mState.mTarget);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->setStorageMultisample(rx::SafeGetImpl(context), target, samples,
                                              internalFormat, size, fixedSampleLocations));

    mState.mImmutableFormat = true;
    mState.mImmutableLevels = static_cast<GLuint>(1);
    mState.clearImageDescs();
    mState.setImageDescChainMultisample(size, Format(internalFormat), samples,
                                        fixedSampleLocations);

    mDirtyChannel.signal();

    return NoError();
}

Error Texture::generateMipmap(const Context *context)
{
    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();

    // EGL_KHR_gl_image states that images are only orphaned when generating mipmaps if the texture
    // is not mip complete.
    if (!isMipmapComplete())
    {
        orphanImages();
    }

    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();

    if (maxLevel > baseLevel)
    {
        syncImplState();
        ANGLE_TRY(mTexture->generateMipmap(rx::SafeGetImpl(context)));

        const ImageDesc &baseImageInfo =
            mState.getImageDesc(mState.getBaseImageTarget(), baseLevel);
        mState.setImageDescChain(baseLevel, maxLevel, baseImageInfo.size, baseImageInfo.format);
    }

    mDirtyChannel.signal();

    return NoError();
}

void Texture::bindTexImageFromSurface(egl::Surface *surface)
{
    ASSERT(surface);

    if (mBoundSurface)
    {
        releaseTexImageFromSurface();
    }

    mTexture->bindTexImage(surface);
    mBoundSurface = surface;

    // Set the image info to the size and format of the surface
    ASSERT(mState.mTarget == GL_TEXTURE_2D);
    Extents size(surface->getWidth(), surface->getHeight(), 1);
    ImageDesc desc(size, Format(surface->getConfig()->renderTargetFormat));
    mState.setImageDesc(mState.mTarget, 0, desc);
    mDirtyChannel.signal();
}

void Texture::releaseTexImageFromSurface()
{
    ASSERT(mBoundSurface);
    mBoundSurface = nullptr;
    mTexture->releaseTexImage();

    // Erase the image info for level 0
    ASSERT(mState.mTarget == GL_TEXTURE_2D);
    mState.clearImageDesc(mState.mTarget, 0);
    mDirtyChannel.signal();
}

void Texture::bindStream(egl::Stream *stream)
{
    ASSERT(stream);

    // It should not be possible to bind a texture already bound to another stream
    ASSERT(mBoundStream == nullptr);

    mBoundStream = stream;

    ASSERT(mState.mTarget == GL_TEXTURE_EXTERNAL_OES);
}

void Texture::releaseStream()
{
    ASSERT(mBoundStream);
    mBoundStream = nullptr;
}

void Texture::acquireImageFromStream(const egl::Stream::GLTextureDescription &desc)
{
    ASSERT(mBoundStream != nullptr);
    mTexture->setImageExternal(mState.mTarget, mBoundStream, desc);

    Extents size(desc.width, desc.height, 1);
    mState.setImageDesc(mState.mTarget, 0, ImageDesc(size, Format(desc.internalFormat)));
    mDirtyChannel.signal();
}

void Texture::releaseImageFromStream()
{
    ASSERT(mBoundStream != nullptr);
    mTexture->setImageExternal(mState.mTarget, nullptr, egl::Stream::GLTextureDescription());

    // Set to incomplete
    mState.clearImageDesc(mState.mTarget, 0);
    mDirtyChannel.signal();
}

void Texture::releaseTexImageInternal()
{
    if (mBoundSurface)
    {
        // Notify the surface
        mBoundSurface->releaseTexImageFromTexture();

        // Then, call the same method as from the surface
        releaseTexImageFromSurface();
    }
}

Error Texture::setEGLImageTarget(GLenum target, egl::Image *imageTarget)
{
    ASSERT(target == mState.mTarget);
    ASSERT(target == GL_TEXTURE_2D || target == GL_TEXTURE_EXTERNAL_OES);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    releaseTexImageInternal();
    orphanImages();

    ANGLE_TRY(mTexture->setEGLImageTarget(target, imageTarget));

    setTargetImage(imageTarget);

    Extents size(static_cast<int>(imageTarget->getWidth()),
                 static_cast<int>(imageTarget->getHeight()), 1);

    mState.clearImageDescs();
    mState.setImageDesc(target, 0, ImageDesc(size, imageTarget->getFormat()));
    mDirtyChannel.signal();

    return NoError();
}

Extents Texture::getAttachmentSize(const FramebufferAttachment::Target &target) const
{
    return mState.getImageDesc(target.textureIndex().type, target.textureIndex().mipIndex).size;
}

const Format &Texture::getAttachmentFormat(const FramebufferAttachment::Target &target) const
{
    return getFormat(target.textureIndex().type, target.textureIndex().mipIndex);
}

GLsizei Texture::getAttachmentSamples(const FramebufferAttachment::Target &target) const
{
    return getSamples(target.textureIndex().type, 0);
}

void Texture::onAttach()
{
    addRef();
}

void Texture::onDetach()
{
    release();
}

GLuint Texture::getId() const
{
    return id();
}

void Texture::syncImplState()
{
    mTexture->syncState(mDirtyBits);
    mDirtyBits.reset();
}

rx::FramebufferAttachmentObjectImpl *Texture::getAttachmentImpl() const
{
    return mTexture;
}
}  // namespace gl
