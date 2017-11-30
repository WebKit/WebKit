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

ImageIndex GetImageIndexFromDescIndex(GLenum target, size_t descIndex)
{
    if (target == GL_TEXTURE_CUBE_MAP)
    {
        size_t faceIndex = descIndex % 6;
        size_t mipIndex  = descIndex / 6;
        return ImageIndex::MakeCube(LayerIndexToCubeMapTextureTarget(faceIndex),
                                    static_cast<GLint>(mipIndex));
    }

    return ImageIndex::MakeGeneric(target, static_cast<GLint>(descIndex));
}

InitState DetermineInitState(const Context *context, const uint8_t *pixels)
{
    // Can happen in tests.
    if (!context || !context->isRobustResourceInitEnabled())
        return InitState::Initialized;

    const auto &glState = context->getGLState();
    return (pixels == nullptr && glState.getTargetBuffer(gl::BufferBinding::PixelUnpack) == nullptr)
               ? InitState::MayNeedInit
               : InitState::Initialized;
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
      mInitState(InitState::MayNeedInit)
{
}

TextureState::~TextureState()
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
        return true;
    }
    return false;
}

bool TextureState::setMaxLevel(GLuint maxLevel)
{
    if (mMaxLevel != maxLevel)
    {
        mMaxLevel = maxLevel;
        return true;
    }

    return false;
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
// According to [OpenGL ES 3.0.5] section 3.8.13 Texture Completeness page 160 any
// per-level checks begin at the base-level.
// For OpenGL ES2 the base level is always zero.
bool TextureState::isCubeComplete() const
{
    ASSERT(mTarget == GL_TEXTURE_CUBE_MAP);

    const ImageDesc &baseImageDesc =
        getImageDesc(FirstCubeMapTextureTarget, getEffectiveBaseLevel());
    if (baseImageDesc.size.width == 0 || baseImageDesc.size.width != baseImageDesc.size.height)
    {
        return false;
    }

    for (GLenum face = FirstCubeMapTextureTarget + 1; face <= LastCubeMapTextureTarget; face++)
    {
        const ImageDesc &faceImageDesc = getImageDesc(face, getEffectiveBaseLevel());
        if (faceImageDesc.size.width != baseImageDesc.size.width ||
            faceImageDesc.size.height != baseImageDesc.size.height ||
            !Format::SameSized(faceImageDesc.format, baseImageDesc.format))
        {
            return false;
        }
    }

    return true;
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

    // According to es 3.1 spec, texture is justified as incomplete if sized internalformat is
    // unfilterable(table 20.11) and filter is not GL_NEAREST(8.16). The default value of minFilter
    // is NEAREST_MIPMAP_LINEAR and magFilter is LINEAR(table 20.11,). For multismaple texture,
    // filter state of multisample texture is ignored(11.1.3.3). So it shouldn't be judged as
    // incomplete texture. So, we ignore filtering for multisample texture completeness here.
    if (mTarget != GL_TEXTURE_2D_MULTISAMPLE &&
        !baseImageDesc.format.info->filterSupport(data.getClientVersion(), data.getExtensions()) &&
        !IsPointSampled(samplerState))
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

    if (mTarget != GL_TEXTURE_2D_MULTISAMPLE && IsMipmapFiltered(samplerState))
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
    if (mTarget != GL_TEXTURE_2D_MULTISAMPLE && baseImageDesc.format.info->depthBits > 0 &&
        data.getClientMajorVersion() >= 3)
    {
        // Note: we restrict this validation to sized types. For the OES_depth_textures
        // extension, due to some underspecification problems, we must allow linear filtering
        // for legacy compatibility with WebGL 1.
        // See http://crbug.com/649200
        if (samplerState.compareMode == GL_NONE && baseImageDesc.format.info->sized)
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

ImageDesc::ImageDesc()
    : ImageDesc(Extents(0, 0, 0), Format::Invalid(), 0, GL_TRUE, InitState::Initialized)
{
}

ImageDesc::ImageDesc(const Extents &size, const Format &format, const InitState initState)
    : size(size), format(format), samples(0), fixedSampleLocations(GL_TRUE), initState(initState)
{
}

ImageDesc::ImageDesc(const Extents &size,
                     const Format &format,
                     const GLsizei samples,
                     const bool fixedSampleLocations,
                     const InitState initState)
    : size(size),
      format(format),
      samples(samples),
      fixedSampleLocations(fixedSampleLocations),
      initState(initState)
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
    if (desc.initState == InitState::MayNeedInit)
    {
        mInitState = InitState::MayNeedInit;
    }
}

const ImageDesc &TextureState::getImageDesc(const ImageIndex &imageIndex) const
{
    return getImageDesc(imageIndex.type, imageIndex.mipIndex);
}

void TextureState::setImageDescChain(GLuint baseLevel,
                                     GLuint maxLevel,
                                     Extents baseSize,
                                     const Format &format,
                                     InitState initState)
{
    for (GLuint level = baseLevel; level <= maxLevel; level++)
    {
        int relativeLevel = (level - baseLevel);
        Extents levelSize(std::max<int>(baseSize.width >> relativeLevel, 1),
                          std::max<int>(baseSize.height >> relativeLevel, 1),
                          (mTarget == GL_TEXTURE_2D_ARRAY)
                              ? baseSize.depth
                              : std::max<int>(baseSize.depth >> relativeLevel, 1));
        ImageDesc levelInfo(levelSize, format, initState);

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
                                                bool fixedSampleLocations,
                                                InitState initState)
{
    ASSERT(mTarget == GL_TEXTURE_2D_MULTISAMPLE);
    ImageDesc levelInfo(baseSize, format, samples, fixedSampleLocations, initState);
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

Error Texture::onDestroy(const Context *context)
{
    if (mBoundSurface)
    {
        ANGLE_TRY(mBoundSurface->releaseTexImage(context, EGL_BACK_BUFFER));
        mBoundSurface = nullptr;
    }
    if (mBoundStream)
    {
        mBoundStream->releaseTextures();
        mBoundStream = nullptr;
    }

    ANGLE_TRY(orphanImages(context));

    if (mTexture)
    {
        ANGLE_TRY(mTexture->onDestroy(context));
    }
    return NoError();
}

Texture::~Texture()
{
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

Error Texture::setBaseLevel(const Context *context, GLuint baseLevel)
{
    if (mState.setBaseLevel(baseLevel))
    {
        ANGLE_TRY(mTexture->setBaseLevel(context, mState.getEffectiveBaseLevel()));
        mDirtyBits.set(DIRTY_BIT_BASE_LEVEL);
        invalidateCompletenessCache();
    }

    return NoError();
}

GLuint Texture::getBaseLevel() const
{
    return mState.mBaseLevel;
}

void Texture::setMaxLevel(GLuint maxLevel)
{
    if (mState.setMaxLevel(maxLevel))
    {
        mDirtyBits.set(DIRTY_BIT_MAX_LEVEL);
        invalidateCompletenessCache();
    }
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

bool Texture::getFixedSampleLocations(GLenum target, size_t level) const
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));
    return mState.getImageDesc(target, level).fixedSampleLocations;
}

GLuint Texture::getMipmapMaxLevel() const
{
    return mState.getMipmapMaxLevel();
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

void Texture::signalDirty(InitState initState) const
{
    mDirtyChannel.signal(initState);
    invalidateCompletenessCache();
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
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setImage(context, target, level, internalFormat, size, format, type,
                                 unpackState, pixels));

    InitState initState = DetermineInitState(context, pixels);
    mState.setImageDesc(target, level, ImageDesc(size, Format(internalFormat, type), initState));
    signalDirty(initState);

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

    ANGLE_TRY(ensureSubImageInitialized(context, target, level, area));

    return mTexture->setSubImage(context, target, level, area, format, type, unpackState, pixels);
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
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setCompressedImage(context, target, level, internalFormat, size,
                                           unpackState, imageSize, pixels));

    InitState initState = DetermineInitState(context, pixels);
    mState.setImageDesc(target, level, ImageDesc(size, Format(internalFormat), initState));
    signalDirty(initState);

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

    ANGLE_TRY(ensureSubImageInitialized(context, target, level, area));

    return mTexture->setCompressedSubImage(context, target, level, area, format, unpackState,
                                           imageSize, pixels);
}

Error Texture::copyImage(const Context *context,
                         GLenum target,
                         size_t level,
                         const Rectangle &sourceArea,
                         GLenum internalFormat,
                         Framebuffer *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    // Ensure source FBO is initialized.
    ANGLE_TRY(source->ensureReadAttachmentInitialized(context, GL_COLOR_BUFFER_BIT));

    // Use the source FBO size as the init image area.
    Box destBox(0, 0, 0, sourceArea.width, sourceArea.height, 1);
    ANGLE_TRY(ensureSubImageInitialized(context, target, level, destBox));

    ANGLE_TRY(mTexture->copyImage(context, target, level, sourceArea, internalFormat, source));

    const InternalFormat &internalFormatInfo =
        GetInternalFormatInfo(internalFormat, GL_UNSIGNED_BYTE);

    mState.setImageDesc(target, level,
                        ImageDesc(Extents(sourceArea.width, sourceArea.height, 1),
                                  Format(internalFormatInfo), InitState::Initialized));

    // We need to initialize this texture only if the source attachment is not initialized.
    signalDirty(InitState::Initialized);

    return NoError();
}

Error Texture::copySubImage(const Context *context,
                            GLenum target,
                            size_t level,
                            const Offset &destOffset,
                            const Rectangle &sourceArea,
                            Framebuffer *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Ensure source FBO is initialized.
    ANGLE_TRY(source->ensureReadAttachmentInitialized(context, GL_COLOR_BUFFER_BIT));

    Box destBox(destOffset.x, destOffset.y, destOffset.y, sourceArea.width, sourceArea.height, 1);
    ANGLE_TRY(ensureSubImageInitialized(context, target, level, destBox));

    return mTexture->copySubImage(context, target, level, destOffset, sourceArea, source);
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
                           Texture *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    // Initialize source texture.
    // Note: we don't have a way to notify which portions of the image changed currently.
    ANGLE_TRY(source->ensureInitialized(context));

    ANGLE_TRY(mTexture->copyTexture(context, target, level, internalFormat, type, sourceLevel,
                                    unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha,
                                    source));

    const auto &sourceDesc   = source->mState.getImageDesc(source->getTarget(), 0);
    const InternalFormat &internalFormatInfo = GetInternalFormatInfo(internalFormat, type);
    mState.setImageDesc(
        target, level,
        ImageDesc(sourceDesc.size, Format(internalFormatInfo), InitState::Initialized));

    signalDirty(InitState::Initialized);

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
                              Texture *source)
{
    ASSERT(target == mState.mTarget ||
           (mState.mTarget == GL_TEXTURE_CUBE_MAP && IsCubeMapTextureTarget(target)));

    // Ensure source is initialized.
    ANGLE_TRY(source->ensureInitialized(context));

    Box destBox(destOffset.x, destOffset.y, destOffset.y, sourceArea.width, sourceArea.height, 1);
    ANGLE_TRY(ensureSubImageInitialized(context, target, level, destBox));

    return mTexture->copySubTexture(context, target, level, destOffset, sourceLevel, sourceArea,
                                    unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha,
                                    source);
}

Error Texture::copyCompressedTexture(const Context *context, const Texture *source)
{
    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->copyCompressedTexture(context, source));

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
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setStorage(context, target, levels, internalFormat, size));

    mState.mImmutableFormat = true;
    mState.mImmutableLevels = static_cast<GLuint>(levels);
    mState.clearImageDescs();
    mState.setImageDescChain(0, static_cast<GLuint>(levels - 1), size, Format(internalFormat),
                             InitState::MayNeedInit);

    // Changing the texture to immutable can trigger a change in the base and max levels:
    // GLES 3.0.4 section 3.8.10 pg 158:
    // "For immutable-format textures, levelbase is clamped to the range[0;levels],levelmax is then
    // clamped to the range[levelbase;levels].
    mDirtyBits.set(DIRTY_BIT_BASE_LEVEL);
    mDirtyBits.set(DIRTY_BIT_MAX_LEVEL);

    signalDirty(InitState::MayNeedInit);

    return NoError();
}

Error Texture::setStorageMultisample(const Context *context,
                                     GLenum target,
                                     GLsizei samples,
                                     GLint internalFormat,
                                     const Extents &size,
                                     bool fixedSampleLocations)
{
    ASSERT(target == mState.mTarget);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setStorageMultisample(context, target, samples, internalFormat, size,
                                              fixedSampleLocations));

    mState.mImmutableFormat = true;
    mState.mImmutableLevels = static_cast<GLuint>(1);
    mState.clearImageDescs();
    mState.setImageDescChainMultisample(size, Format(internalFormat), samples, fixedSampleLocations,
                                        InitState::MayNeedInit);

    signalDirty(InitState::MayNeedInit);

    return NoError();
}

Error Texture::generateMipmap(const Context *context)
{
    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));

    // EGL_KHR_gl_image states that images are only orphaned when generating mipmaps if the texture
    // is not mip complete.
    if (!isMipmapComplete())
    {
        ANGLE_TRY(orphanImages(context));
    }

    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();

    if (maxLevel > baseLevel)
    {
        syncState();
        const ImageDesc &baseImageInfo =
            mState.getImageDesc(mState.getBaseImageTarget(), baseLevel);

        // Clear the base image immediately if necessary.
        if (context->isRobustResourceInitEnabled() &&
            baseImageInfo.initState == InitState::MayNeedInit)
        {
            ANGLE_TRY(initializeContents(
                context, GetImageIndexFromDescIndex(mState.getBaseImageTarget(), baseLevel)));
        }

        ANGLE_TRY(mTexture->generateMipmap(context));

        mState.setImageDescChain(baseLevel, maxLevel, baseImageInfo.size, baseImageInfo.format,
                                 InitState::Initialized);
    }

    signalDirty(InitState::Initialized);

    return NoError();
}

Error Texture::bindTexImageFromSurface(const Context *context, egl::Surface *surface)
{
    ASSERT(surface);

    if (mBoundSurface)
    {
        ANGLE_TRY(releaseTexImageFromSurface(context));
    }

    ANGLE_TRY(mTexture->bindTexImage(context, surface));
    mBoundSurface = surface;

    // Set the image info to the size and format of the surface
    ASSERT(mState.mTarget == GL_TEXTURE_2D || mState.mTarget == GL_TEXTURE_RECTANGLE_ANGLE);
    Extents size(surface->getWidth(), surface->getHeight(), 1);
    ImageDesc desc(size, Format(surface->getConfig()->renderTargetFormat), InitState::Initialized);
    mState.setImageDesc(mState.mTarget, 0, desc);
    signalDirty(InitState::Initialized);
    return NoError();
}

Error Texture::releaseTexImageFromSurface(const Context *context)
{
    ASSERT(mBoundSurface);
    mBoundSurface = nullptr;
    ANGLE_TRY(mTexture->releaseTexImage(context));

    // Erase the image info for level 0
    ASSERT(mState.mTarget == GL_TEXTURE_2D || mState.mTarget == GL_TEXTURE_RECTANGLE_ANGLE);
    mState.clearImageDesc(mState.mTarget, 0);
    signalDirty(InitState::Initialized);
    return NoError();
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

Error Texture::acquireImageFromStream(const Context *context,
                                      const egl::Stream::GLTextureDescription &desc)
{
    ASSERT(mBoundStream != nullptr);
    ANGLE_TRY(mTexture->setImageExternal(context, mState.mTarget, mBoundStream, desc));

    Extents size(desc.width, desc.height, 1);
    mState.setImageDesc(mState.mTarget, 0,
                        ImageDesc(size, Format(desc.internalFormat), InitState::Initialized));
    signalDirty(InitState::Initialized);
    return NoError();
}

Error Texture::releaseImageFromStream(const Context *context)
{
    ASSERT(mBoundStream != nullptr);
    ANGLE_TRY(mTexture->setImageExternal(context, mState.mTarget, nullptr,
                                         egl::Stream::GLTextureDescription()));

    // Set to incomplete
    mState.clearImageDesc(mState.mTarget, 0);
    signalDirty(InitState::Initialized);
    return NoError();
}

Error Texture::releaseTexImageInternal(const Context *context)
{
    if (mBoundSurface)
    {
        // Notify the surface
        mBoundSurface->releaseTexImageFromTexture(context);

        // Then, call the same method as from the surface
        ANGLE_TRY(releaseTexImageFromSurface(context));
    }
    return NoError();
}

Error Texture::setEGLImageTarget(const Context *context, GLenum target, egl::Image *imageTarget)
{
    ASSERT(target == mState.mTarget);
    ASSERT(target == GL_TEXTURE_2D || target == GL_TEXTURE_EXTERNAL_OES);

    // Release from previous calls to eglBindTexImage, to avoid calling the Impl after
    ANGLE_TRY(releaseTexImageInternal(context));
    ANGLE_TRY(orphanImages(context));

    ANGLE_TRY(mTexture->setEGLImageTarget(context, target, imageTarget));

    setTargetImage(context, imageTarget);

    Extents size(static_cast<int>(imageTarget->getWidth()),
                 static_cast<int>(imageTarget->getHeight()), 1);

    auto initState = imageTarget->sourceInitState();

    mState.clearImageDescs();
    mState.setImageDesc(target, 0, ImageDesc(size, imageTarget->getFormat(), initState));
    signalDirty(initState);

    return NoError();
}

Extents Texture::getAttachmentSize(const ImageIndex &imageIndex) const
{
    return mState.getImageDesc(imageIndex).size;
}

const Format &Texture::getAttachmentFormat(GLenum /*binding*/, const ImageIndex &imageIndex) const
{
    return mState.getImageDesc(imageIndex).format;
}

GLsizei Texture::getAttachmentSamples(const ImageIndex &imageIndex) const
{
    return getSamples(imageIndex.type, 0);
}

void Texture::onAttach(const Context *context)
{
    addRef();
}

void Texture::onDetach(const Context *context)
{
    release(context);
}

GLuint Texture::getId() const
{
    return id();
}

void Texture::syncState()
{
    mTexture->syncState(mDirtyBits);
    mDirtyBits.reset();
}

rx::FramebufferAttachmentObjectImpl *Texture::getAttachmentImpl() const
{
    return mTexture;
}

bool Texture::isSamplerComplete(const Context *context, const Sampler *optionalSampler)
{
    const auto &samplerState =
        optionalSampler ? optionalSampler->getSamplerState() : mState.mSamplerState;
    const auto &contextState = context->getContextState();

    if (contextState.getContextID() != mCompletenessCache.context ||
        mCompletenessCache.samplerState != samplerState)
    {
        mCompletenessCache.context      = context->getContextState().getContextID();
        mCompletenessCache.samplerState = samplerState;
        mCompletenessCache.samplerComplete =
            mState.computeSamplerCompleteness(samplerState, contextState);
    }

    return mCompletenessCache.samplerComplete;
}

Texture::SamplerCompletenessCache::SamplerCompletenessCache()
    : context(0), samplerState(), samplerComplete(false)
{
}

void Texture::invalidateCompletenessCache() const
{
    mCompletenessCache.context = 0;
}

Error Texture::ensureInitialized(const Context *context)
{
    if (!context->isRobustResourceInitEnabled() || mState.mInitState == InitState::Initialized)
    {
        return NoError();
    }

    bool anyDirty = false;

    for (size_t descIndex = 0; descIndex < mState.mImageDescs.size(); ++descIndex)
    {
        auto &imageDesc = mState.mImageDescs[descIndex];
        if (imageDesc.initState == InitState::MayNeedInit)
        {
            ASSERT(mState.mInitState == InitState::MayNeedInit);
            const auto &imageIndex = GetImageIndexFromDescIndex(mState.mTarget, descIndex);
            ANGLE_TRY(initializeContents(context, imageIndex));
            imageDesc.initState = InitState::Initialized;
            anyDirty            = true;
        }
    }
    if (anyDirty)
    {
        signalDirty(InitState::Initialized);
    }
    mState.mInitState = InitState::Initialized;

    return NoError();
}

InitState Texture::initState(const ImageIndex &imageIndex) const
{
    return mState.getImageDesc(imageIndex).initState;
}

InitState Texture::initState() const
{
    return mState.mInitState;
}

void Texture::setInitState(const ImageIndex &imageIndex, InitState initState)
{
    ImageDesc newDesc = mState.getImageDesc(imageIndex);
    newDesc.initState = initState;
    mState.setImageDesc(imageIndex.type, imageIndex.mipIndex, newDesc);
}

Error Texture::ensureSubImageInitialized(const Context *context,
                                         GLenum target,
                                         size_t level,
                                         const gl::Box &area)
{
    if (!context->isRobustResourceInitEnabled() || mState.mInitState == InitState::Initialized)
    {
        return NoError();
    }

    // Pre-initialize the texture contents if necessary.
    // TODO(jmadill): Check if area overlaps the entire texture.
    const auto &imageIndex = GetImageIndexFromDescIndex(target, level);
    const auto &desc       = mState.getImageDesc(imageIndex);
    if (desc.initState == InitState::MayNeedInit)
    {
        ASSERT(mState.mInitState == InitState::MayNeedInit);
        bool coversWholeImage = area.x == 0 && area.y == 0 && area.z == 0 &&
                                area.width == desc.size.width && area.height == desc.size.height &&
                                area.depth == desc.size.depth;
        if (!coversWholeImage)
        {
            ANGLE_TRY(initializeContents(context, imageIndex));
        }
        setInitState(imageIndex, InitState::Initialized);
    }

    return NoError();
}

}  // namespace gl
