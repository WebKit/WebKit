//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureGL.cpp: Implements the class methods for TextureGL.

#include "libANGLE/renderer/gl/TextureGL.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

namespace rx
{

static void SetUnpackStateForTexImage(StateManagerGL *stateManager, const gl::PixelUnpackState &unpack)
{
    const gl::Buffer *unpackBuffer = unpack.pixelBuffer.get();
    if (unpackBuffer != nullptr)
    {
        UNIMPLEMENTED();
    }
    if (unpack.skipRows != 0 || unpack.skipPixels != 0 || unpack.imageHeight != 0 || unpack.skipImages != 0)
    {
        UNIMPLEMENTED();
    }
    stateManager->setPixelUnpackState(unpack.alignment, unpack.rowLength);
}

static bool UseTexImage2D(GLenum textureType)
{
    return textureType == GL_TEXTURE_2D || textureType == GL_TEXTURE_CUBE_MAP;
}

static bool UseTexImage3D(GLenum textureType)
{
    return textureType == GL_TEXTURE_2D_ARRAY || textureType == GL_TEXTURE_3D;
}

static bool CompatibleTextureTarget(GLenum textureType, GLenum textureTarget)
{
    if (textureType != GL_TEXTURE_CUBE_MAP)
    {
        return textureType == textureTarget;
    }
    else
    {
        return gl::IsCubeMapTextureTarget(textureTarget);
    }
}

TextureGL::TextureGL(GLenum type, const FunctionsGL *functions, StateManagerGL *stateManager)
    : TextureImpl(),
      mTextureType(type),
      mFunctions(functions),
      mStateManager(stateManager),
      mAppliedSamplerState(),
      mTextureID(0)
{
    ASSERT(mFunctions);
    ASSERT(mStateManager);

    mFunctions->genTextures(1, &mTextureID);
}

TextureGL::~TextureGL()
{
    if (mTextureID)
    {
        mFunctions->deleteTextures(1, &mTextureID);
        mTextureID = 0;
    }
}

void TextureGL::setUsage(GLenum usage)
{
    // GL_ANGLE_texture_usage not implemented for desktop GL
    UNREACHABLE();
}

gl::Error TextureGL::setImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size, GLenum format, GLenum type,
                              const gl::PixelUnpackState &unpack, const uint8_t *pixels)
{
    UNUSED_ASSERTION_VARIABLE(&CompatibleTextureTarget); // Reference this function to avoid warnings.
    ASSERT(CompatibleTextureTarget(mTextureType, target));

    SetUnpackStateForTexImage(mStateManager, unpack);

    mStateManager->bindTexture(mTextureType, mTextureID);
    if (UseTexImage2D(mTextureType))
    {
        ASSERT(size.depth == 1);
        mFunctions->texImage2D(target, level, internalFormat, size.width, size.height, 0, format, type, pixels);
    }
    else if (UseTexImage3D(mTextureType))
    {
        mFunctions->texImage3D(target, level, internalFormat, size.width, size.height, size.depth, 0, format, type, pixels);
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                                 const gl::PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(mTextureType, target));

    SetUnpackStateForTexImage(mStateManager, unpack);

    mStateManager->bindTexture(mTextureType, mTextureID);
    if (UseTexImage2D(mTextureType))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        mFunctions->texSubImage2D(target, level, area.x, area.y, area.width, area.height, format, type, pixels);
    }
    else if (UseTexImage3D(mTextureType))
    {
        mFunctions->texSubImage3D(target, level, area.x, area.y, area.z, area.width, area.height, area.depth,
                                  format, type, pixels);
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                        const gl::PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(mTextureType, target));

    SetUnpackStateForTexImage(mStateManager, unpack);

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);
    size_t depthPitch = internalFormatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, size.width, size.height,
                                                             unpack.alignment, unpack.rowLength);
    size_t dataSize = internalFormatInfo.computeBlockSize(GL_UNSIGNED_BYTE, size.width, size.height) * depthPitch;

    mStateManager->bindTexture(mTextureType, mTextureID);
    if (UseTexImage2D(mTextureType))
    {
        ASSERT(size.depth == 1);
        mFunctions->compressedTexImage2D(target, level, internalFormat, size.width, size.height, 0, dataSize, pixels);
    }
    else if (UseTexImage3D(mTextureType))
    {
        mFunctions->compressedTexImage3D(target, level, internalFormat, size.width, size.height, size.depth, 0,
                                         dataSize, pixels);
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                           const gl::PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(mTextureType, target));

    SetUnpackStateForTexImage(mStateManager, unpack);

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(format);
    size_t depthPitch = internalFormatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, area.width, area.height,
                                                             unpack.alignment, unpack.rowLength);
    size_t dataSize = internalFormatInfo.computeBlockSize(GL_UNSIGNED_BYTE, area.width, area.height) * depthPitch;

    mStateManager->bindTexture(mTextureType, mTextureID);
    if (UseTexImage2D(mTextureType))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        mFunctions->compressedTexSubImage2D(target, level, area.x, area.y, area.width, area.height, format, dataSize,
                                            pixels);
    }
    else if (UseTexImage3D(mTextureType))
    {
        mFunctions->compressedTexSubImage3D(target, level, area.x, area.y, area.z, area.width, area.height, area.depth,
                                            format, dataSize, pixels);
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                               const gl::Framebuffer *source)
{
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);

    mStateManager->bindTexture(mTextureType, mTextureID);
    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    if (UseTexImage2D(mTextureType))
    {
        mFunctions->copyTexImage2D(target, level, internalFormat, sourceArea.x, sourceArea.y,
                                   sourceArea.width, sourceArea.height, 0);
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                                  const gl::Framebuffer *source)
{
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);

    mStateManager->bindTexture(mTextureType, mTextureID);
    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    if (UseTexImage2D(mTextureType))
    {
        ASSERT(destOffset.z == 0);
        mFunctions->copyTexSubImage2D(target, level, destOffset.x, destOffset.y,
                                      sourceArea.x, sourceArea.y, sourceArea.width, sourceArea.height);
    }
    else if (UseTexImage3D(mTextureType))
    {
        mFunctions->copyTexSubImage3D(target, level, destOffset.x, destOffset.y, destOffset.z,
                                      sourceArea.x, sourceArea.y, sourceArea.width, sourceArea.height);
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size)
{
    // TODO: emulate texture storage with TexImage calls if on GL version <4.2 or the
    // ARB_texture_storage extension is not available.

    mStateManager->bindTexture(mTextureType, mTextureID);
    if (UseTexImage2D(mTextureType))
    {
        ASSERT(size.depth == 1);
        if (mFunctions->texStorage2D)
        {
            mFunctions->texStorage2D(target, levels, internalFormat, size.width, size.height);
        }
        else
        {
            // Make sure no pixel unpack buffer is bound
            mStateManager->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.pixelBytes != 0);

            for (size_t level = 0; level < levels; level++)
            {
                gl::Extents levelSize(std::max(size.width >> level, 1),
                                      std::max(size.height >> level, 1),
                                      1);

                if (mTextureType == GL_TEXTURE_2D)
                {
                    mFunctions->texImage2D(target, level, internalFormat, levelSize.width, levelSize.height,
                                           0, internalFormatInfo.format, internalFormatInfo.type, nullptr);
                }
                else if (mTextureType == GL_TEXTURE_CUBE_MAP)
                {
                    for (GLenum face = gl::FirstCubeMapTextureTarget; face <= gl::LastCubeMapTextureTarget; face++)
                    {
                        mFunctions->texImage2D(face, level, internalFormat, levelSize.width, levelSize.height,
                                               0, internalFormatInfo.format, internalFormatInfo.type, nullptr);
                    }
                }
                else
                {
                    UNREACHABLE();
                }
            }
        }
    }
    else if (UseTexImage3D(mTextureType))
    {
        if (mFunctions->texStorage3D)
        {
            mFunctions->texStorage3D(target, levels, internalFormat, size.width, size.height, size.depth);
        }
        else
        {
            // Make sure no pixel unpack buffer is bound
            mStateManager->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.pixelBytes != 0);

            for (size_t i = 0; i < levels; i++)
            {
                gl::Extents levelSize(std::max(size.width >> i, 1),
                                      std::max(size.height >> i, 1),
                                      mTextureType == GL_TEXTURE_3D ? std::max(size.depth >> i, 1) : size.depth);

                mFunctions->texImage3D(target, i, internalFormat, levelSize.width, levelSize.height, levelSize.depth,
                                       0, internalFormatInfo.format, internalFormatInfo.type, nullptr);
            }
        }
    }
    else
    {
        UNREACHABLE();
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::generateMipmaps(const gl::SamplerState &samplerState)
{
    mStateManager->bindTexture(mTextureType, mTextureID);
    mFunctions->generateMipmap(mTextureType);
    return gl::Error(GL_NO_ERROR);
}

void TextureGL::bindTexImage(egl::Surface *surface)
{
    ASSERT(mTextureType == GL_TEXTURE_2D);

    // Make sure this texture is bound
    mStateManager->bindTexture(mTextureType, mTextureID);
}

void TextureGL::releaseTexImage()
{
    // Not all Surface implementations reset the size of mip 0 when releasing, do it manually
    ASSERT(mTextureType == GL_TEXTURE_2D);

    mStateManager->bindTexture(mTextureType, mTextureID);
    if (UseTexImage2D(mTextureType))
    {
        mFunctions->texImage2D(mTextureType, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    else
    {
        UNREACHABLE();
    }
}

template <typename T>
static inline void SyncSamplerStateMember(const FunctionsGL *functions, const gl::SamplerState &newState,
                                          gl::SamplerState &curState, GLenum textureType, GLenum name,
                                          T(gl::SamplerState::*samplerMember))
{
    if (curState.*samplerMember != newState.*samplerMember)
    {
        curState.*samplerMember = newState.*samplerMember;
        functions->texParameterf(textureType, name, static_cast<GLfloat>(curState.*samplerMember));
    }
}

void TextureGL::syncSamplerState(const gl::SamplerState &samplerState) const
{
    if (mAppliedSamplerState != samplerState)
    {
        mStateManager->bindTexture(mTextureType, mTextureID);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_MIN_FILTER, &gl::SamplerState::minFilter);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_MAG_FILTER, &gl::SamplerState::magFilter);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_WRAP_S, &gl::SamplerState::wrapS);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_WRAP_T, &gl::SamplerState::wrapT);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_WRAP_R, &gl::SamplerState::wrapR);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_MAX_ANISOTROPY_EXT, &gl::SamplerState::maxAnisotropy);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_BASE_LEVEL, &gl::SamplerState::baseLevel);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_MAX_LEVEL, &gl::SamplerState::maxLevel);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_MIN_LOD, &gl::SamplerState::minLod);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_MAX_LOD, &gl::SamplerState::maxLod);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_COMPARE_MODE, &gl::SamplerState::compareMode);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_COMPARE_FUNC, &gl::SamplerState::compareFunc);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_SWIZZLE_R, &gl::SamplerState::swizzleRed);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_SWIZZLE_G, &gl::SamplerState::swizzleGreen);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_SWIZZLE_B, &gl::SamplerState::swizzleBlue);
        SyncSamplerStateMember(mFunctions, samplerState, mAppliedSamplerState, mTextureType, GL_TEXTURE_SWIZZLE_A, &gl::SamplerState::swizzleAlpha);
    }
}

GLuint TextureGL::getTextureID() const
{
    return mTextureID;
}

}
