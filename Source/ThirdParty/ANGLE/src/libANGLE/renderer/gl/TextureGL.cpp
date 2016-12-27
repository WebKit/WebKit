//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureGL.cpp: Implements the class methods for TextureGL.

#include "libANGLE/renderer/gl/TextureGL.h"

#include "common/BitSetIterator.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/gl/BlitGL.h"
#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/renderer/gl/formatutilsgl.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

using angle::CheckedNumeric;

namespace rx
{

namespace
{

bool UseTexImage2D(GLenum textureType)
{
    return textureType == GL_TEXTURE_2D || textureType == GL_TEXTURE_CUBE_MAP;
}

bool UseTexImage3D(GLenum textureType)
{
    return textureType == GL_TEXTURE_2D_ARRAY || textureType == GL_TEXTURE_3D;
}

bool CompatibleTextureTarget(GLenum textureType, GLenum textureTarget)
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

bool IsLUMAFormat(GLenum format)
{
    return format == GL_LUMINANCE || format == GL_ALPHA || format == GL_LUMINANCE_ALPHA;
}

LUMAWorkaroundGL GetLUMAWorkaroundInfo(const gl::InternalFormat &originalFormatInfo,
                                       GLenum destinationFormat)
{
    if (IsLUMAFormat(originalFormatInfo.format))
    {
        const gl::InternalFormat &destinationFormatInfo =
            gl::GetInternalFormatInfo(destinationFormat);
        return LUMAWorkaroundGL(!IsLUMAFormat(destinationFormatInfo.format),
                                destinationFormatInfo.format);
    }
    else
    {
        return LUMAWorkaroundGL(false, GL_NONE);
    }
}

bool IsDepthStencilFormat(GLenum format)
{
    return format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL;
}

bool GetDepthStencilWorkaround(const gl::InternalFormat &originalFormatInfo)
{
    return IsDepthStencilFormat(originalFormatInfo.format);
}

LevelInfoGL GetLevelInfo(GLenum originalFormat, GLenum destinationFormat)
{
    const gl::InternalFormat &originalFormatInfo = gl::GetInternalFormatInfo(originalFormat);
    return LevelInfoGL(originalFormat, GetDepthStencilWorkaround(originalFormatInfo),
                       GetLUMAWorkaroundInfo(originalFormatInfo, destinationFormat));
}

gl::Texture::DirtyBits GetLevelWorkaroundDirtyBits()
{
    gl::Texture::DirtyBits bits;
    bits.set(gl::Texture::DIRTY_BIT_SWIZZLE_RED);
    bits.set(gl::Texture::DIRTY_BIT_SWIZZLE_GREEN);
    bits.set(gl::Texture::DIRTY_BIT_SWIZZLE_BLUE);
    bits.set(gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA);
    return bits;
}

}  // anonymous namespace

LUMAWorkaroundGL::LUMAWorkaroundGL() : LUMAWorkaroundGL(false, GL_NONE)
{
}

LUMAWorkaroundGL::LUMAWorkaroundGL(bool enabled_, GLenum workaroundFormat_)
    : enabled(enabled_), workaroundFormat(workaroundFormat_)
{
}

LevelInfoGL::LevelInfoGL() : LevelInfoGL(GL_NONE, false, LUMAWorkaroundGL())
{
}

LevelInfoGL::LevelInfoGL(GLenum sourceFormat_,
                         bool depthStencilWorkaround_,
                         const LUMAWorkaroundGL &lumaWorkaround_)
    : sourceFormat(sourceFormat_),
      depthStencilWorkaround(depthStencilWorkaround_),
      lumaWorkaround(lumaWorkaround_)
{
}

TextureGL::TextureGL(const gl::TextureState &state,
                     const FunctionsGL *functions,
                     const WorkaroundsGL &workarounds,
                     StateManagerGL *stateManager,
                     BlitGL *blitter)
    : TextureImpl(state),
      mFunctions(functions),
      mWorkarounds(workarounds),
      mStateManager(stateManager),
      mBlitter(blitter),
      mLevelInfo(gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1),
      mAppliedTextureState(state.mTarget),
      mTextureID(0)
{
    ASSERT(mFunctions);
    ASSERT(mStateManager);
    ASSERT(mBlitter);

    mFunctions->genTextures(1, &mTextureID);
    mStateManager->bindTexture(mState.mTarget, mTextureID);
}

TextureGL::~TextureGL()
{
    mStateManager->deleteTexture(mTextureID);
    mTextureID = 0;
}

gl::Error TextureGL::setImage(GLenum target,
                              size_t level,
                              GLenum internalFormat,
                              const gl::Extents &size,
                              GLenum format,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              const uint8_t *pixels)
{
    if (mWorkarounds.unpackOverlappingRowsSeparatelyUnpackBuffer && unpack.pixelBuffer.get() &&
        unpack.rowLength != 0 && unpack.rowLength < size.width)
    {
        // The rows overlap in unpack memory. Upload the texture row by row to work around
        // driver bug.
        reserveTexImageToBeFilled(target, level, internalFormat, size, format, type);

        if (size.width == 0 || size.height == 0 || size.depth == 0)
        {
            return gl::NoError();
        }

        gl::Box area(0, 0, 0, size.width, size.height, size.depth);
        return setSubImageRowByRowWorkaround(target, level, area, format, type, unpack, pixels);
    }

    if (mWorkarounds.unpackLastRowSeparatelyForPaddingInclusion)
    {
        bool apply;
        ANGLE_TRY_RESULT(ShouldApplyLastRowPaddingWorkaround(size, unpack, format, type,
                                                             UseTexImage3D(mState.mTarget), pixels),
                         apply);

        // The driver will think the pixel buffer doesn't have enough data, work around this bug
        // by uploading the last row (and last level if 3D) separately.
        if (apply)
        {
            reserveTexImageToBeFilled(target, level, internalFormat, size, format, type);

            if (size.width == 0 || size.height == 0 || size.depth == 0)
            {
                return gl::NoError();
            }

            gl::Box area(0, 0, 0, size.width, size.height, size.depth);
            return setSubImagePaddingWorkaround(target, level, area, format, type, unpack, pixels);
        }
    }

    setImageHelper(target, level, internalFormat, size, format, type, pixels);

    return gl::NoError();
}

void TextureGL::setImageHelper(GLenum target,
                               size_t level,
                               GLenum internalFormat,
                               const gl::Extents &size,
                               GLenum format,
                               GLenum type,
                               const uint8_t *pixels)
{
    UNUSED_ASSERTION_VARIABLE(&CompatibleTextureTarget); // Reference this function to avoid warnings.
    ASSERT(CompatibleTextureTarget(mState.mTarget, target));

    nativegl::TexImageFormat texImageFormat =
        nativegl::GetTexImageFormat(mFunctions, mWorkarounds, internalFormat, format, type);

    mStateManager->bindTexture(mState.mTarget, mTextureID);

    if (UseTexImage2D(mState.mTarget))
    {
        ASSERT(size.depth == 1);
        mFunctions->texImage2D(target, static_cast<GLint>(level), texImageFormat.internalFormat,
                               size.width, size.height, 0, texImageFormat.format,
                               texImageFormat.type, pixels);
    }
    else if (UseTexImage3D(mState.mTarget))
    {
        mFunctions->texImage3D(target, static_cast<GLint>(level), texImageFormat.internalFormat,
                               size.width, size.height, size.depth, 0, texImageFormat.format,
                               texImageFormat.type, pixels);
    }
    else
    {
        UNREACHABLE();
    }

    setLevelInfo(level, 1, GetLevelInfo(internalFormat, texImageFormat.internalFormat));
}

void TextureGL::reserveTexImageToBeFilled(GLenum target,
                                          size_t level,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          GLenum format,
                                          GLenum type)
{
    GLuint unpackBuffer = mStateManager->getBoundBuffer(GL_PIXEL_UNPACK_BUFFER);
    mStateManager->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    gl::PixelUnpackState unpack;
    setImageHelper(target, level, internalFormat, size, format, type, nullptr);
    mStateManager->bindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
}

gl::Error TextureGL::setSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format, GLenum type,
                                 const gl::PixelUnpackState &unpack, const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(mState.mTarget, target));

    nativegl::TexSubImageFormat texSubImageFormat =
        nativegl::GetTexSubImageFormat(mFunctions, mWorkarounds, format, type);

    ASSERT(mLevelInfo[level].lumaWorkaround.enabled ==
           GetLevelInfo(format, texSubImageFormat.format).lumaWorkaround.enabled);

    mStateManager->bindTexture(mState.mTarget, mTextureID);
    if (mWorkarounds.unpackOverlappingRowsSeparatelyUnpackBuffer && unpack.pixelBuffer.get() &&
        unpack.rowLength != 0 && unpack.rowLength < area.width)
    {
        return setSubImageRowByRowWorkaround(target, level, area, format, type, unpack, pixels);
    }

    if (mWorkarounds.unpackLastRowSeparatelyForPaddingInclusion)
    {
        gl::Extents size(area.width, area.height, area.depth);

        bool apply;
        ANGLE_TRY_RESULT(ShouldApplyLastRowPaddingWorkaround(size, unpack, format, type,
                                                             UseTexImage3D(mState.mTarget), pixels),
                         apply);

        // The driver will think the pixel buffer doesn't have enough data, work around this bug
        // by uploading the last row (and last level if 3D) separately.
        if (apply)
        {
            return setSubImagePaddingWorkaround(target, level, area, format, type, unpack, pixels);
        }
    }

    if (UseTexImage2D(mState.mTarget))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        mFunctions->texSubImage2D(target, static_cast<GLint>(level), area.x, area.y, area.width,
                                  area.height, texSubImageFormat.format, texSubImageFormat.type,
                                  pixels);
    }
    else
    {
        ASSERT(UseTexImage3D(mState.mTarget));
        mFunctions->texSubImage3D(target, static_cast<GLint>(level), area.x, area.y, area.z,
                                  area.width, area.height, area.depth, texSubImageFormat.format,
                                  texSubImageFormat.type, pixels);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setSubImageRowByRowWorkaround(GLenum target,
                                                   size_t level,
                                                   const gl::Box &area,
                                                   GLenum format,
                                                   GLenum type,
                                                   const gl::PixelUnpackState &unpack,
                                                   const uint8_t *pixels)
{
    gl::PixelUnpackState directUnpack;
    directUnpack.pixelBuffer = unpack.pixelBuffer;
    directUnpack.alignment   = 1;
    mStateManager->setPixelUnpackState(directUnpack);
    directUnpack.pixelBuffer.set(nullptr);

    const gl::InternalFormat &glFormat =
        gl::GetInternalFormatInfo(gl::GetSizedInternalFormat(format, type));
    GLuint rowBytes                      = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, unpack.alignment, unpack.rowLength),
                     rowBytes);
    GLuint imageBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeDepthPitch(area.height, unpack.imageHeight, rowBytes),
                     imageBytes);
    bool useTexImage3D = UseTexImage3D(mState.mTarget);
    GLuint skipBytes   = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, imageBytes, unpack, useTexImage3D),
                     skipBytes);

    const uint8_t *pixelsWithSkip = pixels + skipBytes;
    if (useTexImage3D)
    {
        for (GLint image = 0; image < area.depth; ++image)
        {
            GLint imageByteOffset = image * imageBytes;
            for (GLint row = 0; row < area.height; ++row)
            {
                GLint byteOffset         = imageByteOffset + row * rowBytes;
                const GLubyte *rowPixels = pixelsWithSkip + byteOffset;
                mFunctions->texSubImage3D(target, static_cast<GLint>(level), area.x, row + area.y,
                                          image + area.z, area.width, 1, 1, format, type,
                                          rowPixels);
            }
        }
    }
    else
    {
        ASSERT(UseTexImage2D(mState.mTarget));
        for (GLint row = 0; row < area.height; ++row)
        {
            GLint byteOffset         = row * rowBytes;
            const GLubyte *rowPixels = pixelsWithSkip + byteOffset;
            mFunctions->texSubImage2D(target, static_cast<GLint>(level), area.x, row + area.y,
                                      area.width, 1, format, type, rowPixels);
        }
    }
    return gl::NoError();
}

gl::Error TextureGL::setSubImagePaddingWorkaround(GLenum target,
                                                  size_t level,
                                                  const gl::Box &area,
                                                  GLenum format,
                                                  GLenum type,
                                                  const gl::PixelUnpackState &unpack,
                                                  const uint8_t *pixels)
{
    const gl::InternalFormat &glFormat =
        gl::GetInternalFormatInfo(gl::GetSizedInternalFormat(format, type));
    GLuint rowBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, unpack.alignment, unpack.rowLength),
                     rowBytes);
    GLuint imageBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeDepthPitch(area.height, unpack.imageHeight, rowBytes),
                     imageBytes);
    bool useTexImage3D = UseTexImage3D(mState.mTarget);
    GLuint skipBytes   = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, imageBytes, unpack, useTexImage3D),
                     skipBytes);

    gl::PixelUnpackState directUnpack;
    directUnpack.pixelBuffer = unpack.pixelBuffer;
    directUnpack.alignment   = 1;

    if (useTexImage3D)
    {
        // Upload all but the last slice
        if (area.depth > 1)
        {
            mFunctions->texSubImage3D(target, static_cast<GLint>(level), area.x, area.y, area.z,
                                      area.width, area.height, area.depth - 1, format, type,
                                      pixels);
        }

        // Upload the last slice but its last row
        if (area.height > 1)
        {
            // Do not include skipBytes in the last image pixel start offset as it will be done by
            // the driver
            GLint lastImageOffset          = (area.depth - 1) * imageBytes;
            const GLubyte *lastImagePixels = pixels + lastImageOffset;
            mFunctions->texSubImage3D(target, static_cast<GLint>(level), area.x, area.y,
                                      area.z + area.depth - 1, area.width, area.height - 1, 1,
                                      format, type, lastImagePixels);
        }

        // Upload the last row of the last slice "manually"
        mStateManager->setPixelUnpackState(directUnpack);

        GLint lastRowOffset =
            skipBytes + (area.depth - 1) * imageBytes + (area.height - 1) * rowBytes;
        const GLubyte *lastRowPixels = pixels + lastRowOffset;
        mFunctions->texSubImage3D(target, static_cast<GLint>(level), area.x,
                                  area.y + area.height - 1, area.z + area.depth - 1, area.width, 1,
                                  1, format, type, lastRowPixels);
    }
    else
    {
        ASSERT(UseTexImage2D(mState.mTarget));

        // Upload all but the last row
        if (area.height > 1)
        {
            mFunctions->texSubImage2D(target, static_cast<GLint>(level), area.x, area.y, area.width,
                                      area.height - 1, format, type, pixels);
        }

        // Upload the last row "manually"
        mStateManager->setPixelUnpackState(directUnpack);

        GLint lastRowOffset          = skipBytes + (area.height - 1) * rowBytes;
        const GLubyte *lastRowPixels = pixels + lastRowOffset;
        mFunctions->texSubImage2D(target, static_cast<GLint>(level), area.x,
                                  area.y + area.height - 1, area.width, 1, format, type,
                                  lastRowPixels);
    }

    directUnpack.pixelBuffer.set(nullptr);

    return gl::NoError();
}

gl::Error TextureGL::setCompressedImage(GLenum target, size_t level, GLenum internalFormat, const gl::Extents &size,
                                        const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(mState.mTarget, target));

    nativegl::CompressedTexImageFormat compressedTexImageFormat =
        nativegl::GetCompressedTexImageFormat(mFunctions, mWorkarounds, internalFormat);

    mStateManager->bindTexture(mState.mTarget, mTextureID);
    if (UseTexImage2D(mState.mTarget))
    {
        ASSERT(size.depth == 1);
        mFunctions->compressedTexImage2D(target, static_cast<GLint>(level),
                                         compressedTexImageFormat.internalFormat, size.width,
                                         size.height, 0, static_cast<GLsizei>(imageSize), pixels);
    }
    else if (UseTexImage3D(mState.mTarget))
    {
        mFunctions->compressedTexImage3D(
            target, static_cast<GLint>(level), compressedTexImageFormat.internalFormat, size.width,
            size.height, size.depth, 0, static_cast<GLsizei>(imageSize), pixels);
    }
    else
    {
        UNREACHABLE();
    }

    setLevelInfo(level, 1, GetLevelInfo(internalFormat, compressedTexImageFormat.internalFormat));
    ASSERT(!mLevelInfo[level].lumaWorkaround.enabled);

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setCompressedSubImage(GLenum target, size_t level, const gl::Box &area, GLenum format,
                                           const gl::PixelUnpackState &unpack, size_t imageSize, const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(mState.mTarget, target));

    nativegl::CompressedTexSubImageFormat compressedTexSubImageFormat =
        nativegl::GetCompressedSubTexImageFormat(mFunctions, mWorkarounds, format);

    mStateManager->bindTexture(mState.mTarget, mTextureID);
    if (UseTexImage2D(mState.mTarget))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        mFunctions->compressedTexSubImage2D(
            target, static_cast<GLint>(level), area.x, area.y, area.width, area.height,
            compressedTexSubImageFormat.format, static_cast<GLsizei>(imageSize), pixels);
    }
    else if (UseTexImage3D(mState.mTarget))
    {
        mFunctions->compressedTexSubImage3D(target, static_cast<GLint>(level), area.x, area.y,
                                            area.z, area.width, area.height, area.depth,
                                            compressedTexSubImageFormat.format,
                                            static_cast<GLsizei>(imageSize), pixels);
    }
    else
    {
        UNREACHABLE();
    }

    ASSERT(!mLevelInfo[level].lumaWorkaround.enabled &&
           !GetLevelInfo(format, compressedTexSubImageFormat.format).lumaWorkaround.enabled);

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::copyImage(GLenum target, size_t level, const gl::Rectangle &sourceArea, GLenum internalFormat,
                               const gl::Framebuffer *source)
{
    nativegl::CopyTexImageImageFormat copyTexImageFormat = nativegl::GetCopyTexImageImageFormat(
        mFunctions, mWorkarounds, internalFormat, source->getImplementationColorReadType());

    LevelInfoGL levelInfo = GetLevelInfo(internalFormat, copyTexImageFormat.internalFormat);
    if (levelInfo.lumaWorkaround.enabled)
    {
        gl::Error error = mBlitter->copyImageToLUMAWorkaroundTexture(
            mTextureID, mState.mTarget, target, levelInfo.sourceFormat, level, sourceArea,
            copyTexImageFormat.internalFormat, source);
        if (error.isError())
        {
            return error;
        }
    }
    else
    {
        const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);

        mStateManager->bindTexture(mState.mTarget, mTextureID);
        mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER,
                                       sourceFramebufferGL->getFramebufferID());

        if (UseTexImage2D(mState.mTarget))
        {
            mFunctions->copyTexImage2D(target, static_cast<GLint>(level),
                                       copyTexImageFormat.internalFormat, sourceArea.x,
                                       sourceArea.y, sourceArea.width, sourceArea.height, 0);
        }
        else
        {
            UNREACHABLE();
        }
    }

    setLevelInfo(level, 1, levelInfo);

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::copySubImage(GLenum target, size_t level, const gl::Offset &destOffset, const gl::Rectangle &sourceArea,
                                  const gl::Framebuffer *source)
{
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);

    mStateManager->bindTexture(mState.mTarget, mTextureID);
    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    const LevelInfoGL &levelInfo = mLevelInfo[level];
    if (levelInfo.lumaWorkaround.enabled)
    {
        gl::Error error = mBlitter->copySubImageToLUMAWorkaroundTexture(
            mTextureID, mState.mTarget, target, levelInfo.sourceFormat, level, destOffset,
            sourceArea, source);
        if (error.isError())
        {
            return error;
        }
    }
    else
    {
        if (UseTexImage2D(mState.mTarget))
        {
            ASSERT(destOffset.z == 0);
            mFunctions->copyTexSubImage2D(target, static_cast<GLint>(level), destOffset.x,
                                          destOffset.y, sourceArea.x, sourceArea.y,
                                          sourceArea.width, sourceArea.height);
        }
        else if (UseTexImage3D(mState.mTarget))
        {
            mFunctions->copyTexSubImage3D(target, static_cast<GLint>(level), destOffset.x,
                                          destOffset.y, destOffset.z, sourceArea.x, sourceArea.y,
                                          sourceArea.width, sourceArea.height);
        }
        else
        {
            UNREACHABLE();
        }
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setStorage(GLenum target, size_t levels, GLenum internalFormat, const gl::Extents &size)
{
    // TODO: emulate texture storage with TexImage calls if on GL version <4.2 or the
    // ARB_texture_storage extension is not available.

    nativegl::TexStorageFormat texStorageFormat =
        nativegl::GetTexStorageFormat(mFunctions, mWorkarounds, internalFormat);

    mStateManager->bindTexture(mState.mTarget, mTextureID);
    if (UseTexImage2D(mState.mTarget))
    {
        ASSERT(size.depth == 1);
        if (mFunctions->texStorage2D)
        {
            mFunctions->texStorage2D(target, static_cast<GLsizei>(levels),
                                     texStorageFormat.internalFormat, size.width, size.height);
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

                if (mState.mTarget == GL_TEXTURE_2D)
                {
                    if (internalFormatInfo.compressed)
                    {
                        GLuint dataSize = 0;
                        ANGLE_TRY_RESULT(internalFormatInfo.computeCompressedImageSize(
                                             GL_UNSIGNED_BYTE, levelSize),
                                         dataSize);
                        mFunctions->compressedTexImage2D(target, static_cast<GLint>(level),
                                                         texStorageFormat.internalFormat,
                                                         levelSize.width, levelSize.height, 0,
                                                         static_cast<GLsizei>(dataSize), nullptr);
                    }
                    else
                    {
                        mFunctions->texImage2D(target, static_cast<GLint>(level),
                                               texStorageFormat.internalFormat, levelSize.width,
                                               levelSize.height, 0, internalFormatInfo.format,
                                               internalFormatInfo.type, nullptr);
                    }
                }
                else if (mState.mTarget == GL_TEXTURE_CUBE_MAP)
                {
                    for (GLenum face = gl::FirstCubeMapTextureTarget; face <= gl::LastCubeMapTextureTarget; face++)
                    {
                        if (internalFormatInfo.compressed)
                        {
                            GLuint dataSize = 0;
                            ANGLE_TRY_RESULT(internalFormatInfo.computeCompressedImageSize(
                                                 GL_UNSIGNED_BYTE, levelSize),
                                             dataSize);
                            mFunctions->compressedTexImage2D(
                                face, static_cast<GLint>(level), texStorageFormat.internalFormat,
                                levelSize.width, levelSize.height, 0,
                                static_cast<GLsizei>(dataSize), nullptr);
                        }
                        else
                        {
                            mFunctions->texImage2D(face, static_cast<GLint>(level),
                                                   texStorageFormat.internalFormat, levelSize.width,
                                                   levelSize.height, 0, internalFormatInfo.format,
                                                   internalFormatInfo.type, nullptr);
                        }
                    }
                }
                else
                {
                    UNREACHABLE();
                }
            }
        }
    }
    else if (UseTexImage3D(mState.mTarget))
    {
        if (mFunctions->texStorage3D)
        {
            mFunctions->texStorage3D(target, static_cast<GLsizei>(levels),
                                     texStorageFormat.internalFormat, size.width, size.height,
                                     size.depth);
        }
        else
        {
            // Make sure no pixel unpack buffer is bound
            mStateManager->bindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.pixelBytes != 0);

            for (GLsizei i = 0; i < static_cast<GLsizei>(levels); i++)
            {
                gl::Extents levelSize(
                    std::max(size.width >> i, 1), std::max(size.height >> i, 1),
                    mState.mTarget == GL_TEXTURE_3D ? std::max(size.depth >> i, 1) : size.depth);

                if (internalFormatInfo.compressed)
                {
                    GLuint dataSize = 0;
                    ANGLE_TRY_RESULT(
                        internalFormatInfo.computeCompressedImageSize(GL_UNSIGNED_BYTE, levelSize),
                        dataSize);
                    mFunctions->compressedTexImage3D(target, i, texStorageFormat.internalFormat,
                                                     levelSize.width, levelSize.height,
                                                     levelSize.depth, 0,
                                                     static_cast<GLsizei>(dataSize), nullptr);
                }
                else
                {
                    mFunctions->texImage3D(target, i, texStorageFormat.internalFormat,
                                           levelSize.width, levelSize.height, levelSize.depth, 0,
                                           internalFormatInfo.format, internalFormatInfo.type,
                                           nullptr);
                }
            }
        }
    }
    else
    {
        UNREACHABLE();
    }

    setLevelInfo(0, levels, GetLevelInfo(internalFormat, texStorageFormat.internalFormat));

    return gl::Error(GL_NO_ERROR);
}

gl::Error TextureGL::setImageExternal(GLenum target,
                                      egl::Stream *stream,
                                      const egl::Stream::GLTextureDescription &desc)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error TextureGL::generateMipmap()
{
    mStateManager->bindTexture(mState.mTarget, mTextureID);
    mFunctions->generateMipmap(mState.mTarget);

    const GLuint effectiveBaseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel           = mState.getMipmapMaxLevel();

    ASSERT(maxLevel < mLevelInfo.size());

    setLevelInfo(effectiveBaseLevel, maxLevel - effectiveBaseLevel, mLevelInfo[effectiveBaseLevel]);

    return gl::Error(GL_NO_ERROR);
}

void TextureGL::bindTexImage(egl::Surface *surface)
{
    ASSERT(mState.mTarget == GL_TEXTURE_2D);

    // Make sure this texture is bound
    mStateManager->bindTexture(mState.mTarget, mTextureID);

    setLevelInfo(0, 1, LevelInfoGL());
}

void TextureGL::releaseTexImage()
{
    // Not all Surface implementations reset the size of mip 0 when releasing, do it manually
    ASSERT(mState.mTarget == GL_TEXTURE_2D);

    mStateManager->bindTexture(mState.mTarget, mTextureID);
    if (UseTexImage2D(mState.mTarget))
    {
        mFunctions->texImage2D(mState.mTarget, 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               nullptr);
    }
    else
    {
        UNREACHABLE();
    }
}

gl::Error TextureGL::setEGLImageTarget(GLenum target, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

void TextureGL::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
    if (dirtyBits.none() && mLocalDirtyBits.none())
    {
        return;
    }

    mStateManager->bindTexture(mState.mTarget, mTextureID);

    if (dirtyBits[gl::Texture::DIRTY_BIT_BASE_LEVEL] || dirtyBits[gl::Texture::DIRTY_BIT_MAX_LEVEL])
    {
        // Don't know if the previous base level was using any workarounds, always re-sync the
        // workaround dirty bits
        mLocalDirtyBits |= GetLevelWorkaroundDirtyBits();
    }

    for (auto dirtyBit : angle::IterateBitSet(dirtyBits | mLocalDirtyBits))
    {
        switch (dirtyBit)
        {
            case gl::Texture::DIRTY_BIT_MIN_FILTER:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_MIN_FILTER,
                                          mState.getSamplerState().minFilter);
                break;
            case gl::Texture::DIRTY_BIT_MAG_FILTER:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_MAG_FILTER,
                                          mState.getSamplerState().magFilter);
                break;
            case gl::Texture::DIRTY_BIT_WRAP_S:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_WRAP_S,
                                          mState.getSamplerState().wrapS);
                break;
            case gl::Texture::DIRTY_BIT_WRAP_T:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_WRAP_T,
                                          mState.getSamplerState().wrapT);
                break;
            case gl::Texture::DIRTY_BIT_WRAP_R:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_WRAP_R,
                                          mState.getSamplerState().wrapR);
                break;
            case gl::Texture::DIRTY_BIT_MAX_ANISOTROPY:
                mFunctions->texParameterf(mState.mTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                          mState.getSamplerState().maxAnisotropy);
                break;
            case gl::Texture::DIRTY_BIT_MIN_LOD:
                mFunctions->texParameterf(mState.mTarget, GL_TEXTURE_MIN_LOD,
                                          mState.getSamplerState().minLod);
                break;
            case gl::Texture::DIRTY_BIT_MAX_LOD:
                mFunctions->texParameterf(mState.mTarget, GL_TEXTURE_MAX_LOD,
                                          mState.getSamplerState().maxLod);
                break;
            case gl::Texture::DIRTY_BIT_COMPARE_MODE:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_COMPARE_MODE,
                                          mState.getSamplerState().compareMode);
                break;
            case gl::Texture::DIRTY_BIT_COMPARE_FUNC:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_COMPARE_FUNC,
                                          mState.getSamplerState().compareFunc);
                break;

            // Texture state
            case gl::Texture::DIRTY_BIT_SWIZZLE_RED:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_R,
                                        mState.getSwizzleState().swizzleRed);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_GREEN:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_G,
                                        mState.getSwizzleState().swizzleGreen);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_BLUE:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_B,
                                        mState.getSwizzleState().swizzleBlue);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_A,
                                        mState.getSwizzleState().swizzleAlpha);
                break;
            case gl::Texture::DIRTY_BIT_BASE_LEVEL:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_BASE_LEVEL,
                                          mState.getEffectiveBaseLevel());
                break;
            case gl::Texture::DIRTY_BIT_MAX_LEVEL:
                mFunctions->texParameteri(mState.mTarget, GL_TEXTURE_MAX_LEVEL,
                                          mState.getEffectiveMaxLevel());
                break;
            case gl::Texture::DIRTY_BIT_USAGE:
                break;

            default:
                UNREACHABLE();
        }
    }

    mLocalDirtyBits.reset();
}

bool TextureGL::hasAnyDirtyBit() const
{
    return mLocalDirtyBits.any();
}

void TextureGL::syncTextureStateSwizzle(const FunctionsGL *functions, GLenum name, GLenum value)
{
    const LevelInfoGL &levelInfo = mLevelInfo[mState.getEffectiveBaseLevel()];
    GLenum resultSwizzle         = value;
    if (levelInfo.lumaWorkaround.enabled || levelInfo.depthStencilWorkaround)
    {
        if (levelInfo.lumaWorkaround.enabled)
        {
            UNUSED_ASSERTION_VARIABLE(levelInfo.lumaWorkaround.workaroundFormat);

            switch (value)
            {
            case GL_RED:
            case GL_GREEN:
            case GL_BLUE:
                if (levelInfo.sourceFormat == GL_LUMINANCE ||
                    levelInfo.sourceFormat == GL_LUMINANCE_ALPHA)
                {
                    // Texture is backed by a RED or RG texture, point all color channels at the red
                    // channel.
                    ASSERT(levelInfo.lumaWorkaround.workaroundFormat == GL_RED ||
                           levelInfo.lumaWorkaround.workaroundFormat == GL_RG);
                    resultSwizzle = GL_RED;
                }
                else if (levelInfo.sourceFormat == GL_ALPHA)
                {
                    // Color channels are not supposed to exist, make them always sample 0.
                    resultSwizzle = GL_ZERO;
                }
                else
                {
                    UNREACHABLE();
                }
                break;

            case GL_ALPHA:
                if (levelInfo.sourceFormat == GL_LUMINANCE)
                {
                    // Alpha channel is not supposed to exist, make it always sample 1.
                    resultSwizzle = GL_ONE;
                }
                else if (levelInfo.sourceFormat == GL_ALPHA)
                {
                    // Texture is backed by a RED texture, point the alpha channel at the red
                    // channel.
                    ASSERT(levelInfo.lumaWorkaround.workaroundFormat == GL_RED);
                    resultSwizzle = GL_RED;
                }
                else if (levelInfo.sourceFormat == GL_LUMINANCE_ALPHA)
                {
                    // Texture is backed by an RG texture, point the alpha channel at the green
                    // channel.
                    ASSERT(levelInfo.lumaWorkaround.workaroundFormat == GL_RG);
                    resultSwizzle = GL_GREEN;
                }
                else
                {
                    UNREACHABLE();
                }
                break;

            case GL_ZERO:
            case GL_ONE:
                // Don't modify the swizzle state when requesting ZERO or ONE.
                resultSwizzle = value;
                break;

            default:
                UNREACHABLE();
                break;
            }
        }
        else if (levelInfo.depthStencilWorkaround)
        {
            switch (value)
            {
                case GL_RED:
                    // Don't modify the swizzle state when requesting the red channel.
                    resultSwizzle = value;
                    break;

                case GL_GREEN:
                case GL_BLUE:
                    // Depth textures should sample 0 from the green and blue channels.
                    resultSwizzle = GL_ZERO;
                    break;

                case GL_ALPHA:
                    // Depth textures should sample 1 from the alpha channel.
                    resultSwizzle = GL_ONE;
                    break;

                case GL_ZERO:
                case GL_ONE:
                    // Don't modify the swizzle state when requesting ZERO or ONE.
                    resultSwizzle = value;
                    break;

                default:
                    UNREACHABLE();
                    break;
            }
        }
        else
        {
            UNREACHABLE();
        }

    }

    functions->texParameteri(mState.mTarget, name, resultSwizzle);
}

void TextureGL::setLevelInfo(size_t level, size_t levelCount, const LevelInfoGL &levelInfo)
{
    ASSERT(levelCount > 0 && level + levelCount < mLevelInfo.size());

    GLuint baseLevel              = mState.getEffectiveBaseLevel();
    bool needsResync              = level <= baseLevel && level + levelCount >= baseLevel &&
                       (levelInfo.depthStencilWorkaround || levelInfo.lumaWorkaround.enabled);
    if (needsResync)
    {
        mLocalDirtyBits |= GetLevelWorkaroundDirtyBits();
    }

    for (size_t i = level; i < level + levelCount; i++)
    {
        mLevelInfo[i] = levelInfo;
    }
}

GLuint TextureGL::getTextureID() const
{
    return mTextureID;
}

}
