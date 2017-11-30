//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureGL.cpp: Implements the class methods for TextureGL.

#include "libANGLE/renderer/gl/TextureGL.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
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

size_t GetLevelInfoIndex(GLenum target, size_t level)
{
    return gl::IsCubeMapTextureTarget(target)
               ? ((level * 6) + gl::CubeMapTextureTargetToLayerIndex(target))
               : level;
}

bool UseTexImage2D(GLenum textureType)
{
    return textureType == GL_TEXTURE_2D || textureType == GL_TEXTURE_CUBE_MAP ||
           textureType == GL_TEXTURE_RECTANGLE_ANGLE;
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

LUMAWorkaroundGL GetLUMAWorkaroundInfo(GLenum originalFormat, GLenum destinationFormat)
{
    if (IsLUMAFormat(originalFormat))
    {
        return LUMAWorkaroundGL(!IsLUMAFormat(destinationFormat), destinationFormat);
    }
    else
    {
        return LUMAWorkaroundGL(false, GL_NONE);
    }
}

bool GetDepthStencilWorkaround(GLenum format)
{
    return format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL;
}

LevelInfoGL GetLevelInfo(GLenum originalInternalFormat, GLenum destinationInternalFormat)
{
    GLenum originalFormat    = gl::GetUnsizedFormat(originalInternalFormat);
    GLenum destinationFormat = gl::GetUnsizedFormat(destinationInternalFormat);
    return LevelInfoGL(originalFormat, destinationInternalFormat,
                       GetDepthStencilWorkaround(originalFormat),
                       GetLUMAWorkaroundInfo(originalFormat, destinationFormat));
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

LevelInfoGL::LevelInfoGL() : LevelInfoGL(GL_NONE, GL_NONE, false, LUMAWorkaroundGL())
{
}

LevelInfoGL::LevelInfoGL(GLenum sourceFormat_,
                         GLenum nativeInternalFormat_,
                         bool depthStencilWorkaround_,
                         const LUMAWorkaroundGL &lumaWorkaround_)
    : sourceFormat(sourceFormat_),
      nativeInternalFormat(nativeInternalFormat_),
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
      mLevelInfo(),
      mAppliedSwizzle(state.getSwizzleState()),
      mAppliedSampler(state.getSamplerState()),
      mAppliedBaseLevel(state.getEffectiveBaseLevel()),
      mAppliedMaxLevel(state.getEffectiveMaxLevel()),
      mTextureID(0)
{
    ASSERT(mFunctions);
    ASSERT(mStateManager);
    ASSERT(mBlitter);

    mFunctions->genTextures(1, &mTextureID);
    mStateManager->bindTexture(getTarget(), mTextureID);
    mLevelInfo.resize((gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1) *
                      (getTarget() == GL_TEXTURE_CUBE_MAP ? 6 : 1));
}

TextureGL::~TextureGL()
{
    mStateManager->deleteTexture(mTextureID);
    mTextureID = 0;
}

gl::Error TextureGL::setImage(const gl::Context *context,
                              GLenum target,
                              size_t level,
                              GLenum internalFormat,
                              const gl::Extents &size,
                              GLenum format,
                              GLenum type,
                              const gl::PixelUnpackState &unpack,
                              const uint8_t *pixels)
{
    const gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);

    if (mWorkarounds.unpackOverlappingRowsSeparatelyUnpackBuffer && unpackBuffer &&
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
        return setSubImageRowByRowWorkaround(context, target, level, area, format, type, unpack,
                                             unpackBuffer, pixels);
    }

    if (mWorkarounds.unpackLastRowSeparatelyForPaddingInclusion)
    {
        bool apply;
        ANGLE_TRY_RESULT(
            ShouldApplyLastRowPaddingWorkaround(size, unpack, unpackBuffer, format, type,
                                                UseTexImage3D(getTarget()), pixels),
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
            return setSubImagePaddingWorkaround(context, target, level, area, format, type, unpack,
                                                unpackBuffer, pixels);
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
    ASSERT(CompatibleTextureTarget(getTarget(), target));

    nativegl::TexImageFormat texImageFormat =
        nativegl::GetTexImageFormat(mFunctions, mWorkarounds, internalFormat, format, type);

    mStateManager->bindTexture(getTarget(), mTextureID);

    if (UseTexImage2D(getTarget()))
    {
        ASSERT(size.depth == 1);
        mFunctions->texImage2D(target, static_cast<GLint>(level), texImageFormat.internalFormat,
                               size.width, size.height, 0, texImageFormat.format,
                               texImageFormat.type, pixels);
    }
    else if (UseTexImage3D(getTarget()))
    {
        mFunctions->texImage3D(target, static_cast<GLint>(level), texImageFormat.internalFormat,
                               size.width, size.height, size.depth, 0, texImageFormat.format,
                               texImageFormat.type, pixels);
    }
    else
    {
        UNREACHABLE();
    }

    setLevelInfo(target, level, 1, GetLevelInfo(internalFormat, texImageFormat.internalFormat));
}

void TextureGL::reserveTexImageToBeFilled(GLenum target,
                                          size_t level,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          GLenum format,
                                          GLenum type)
{
    mStateManager->setPixelUnpackBuffer(nullptr);
    setImageHelper(target, level, internalFormat, size, format, type, nullptr);
}

gl::Error TextureGL::setSubImage(const gl::Context *context,
                                 GLenum target,
                                 size_t level,
                                 const gl::Box &area,
                                 GLenum format,
                                 GLenum type,
                                 const gl::PixelUnpackState &unpack,
                                 const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(getTarget(), target));
    const gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);

    nativegl::TexSubImageFormat texSubImageFormat =
        nativegl::GetTexSubImageFormat(mFunctions, mWorkarounds, format, type);

    ASSERT(getLevelInfo(target, level).lumaWorkaround.enabled ==
           GetLevelInfo(format, texSubImageFormat.format).lumaWorkaround.enabled);

    mStateManager->bindTexture(getTarget(), mTextureID);
    if (mWorkarounds.unpackOverlappingRowsSeparatelyUnpackBuffer && unpackBuffer &&
        unpack.rowLength != 0 && unpack.rowLength < area.width)
    {
        return setSubImageRowByRowWorkaround(context, target, level, area, format, type, unpack,
                                             unpackBuffer, pixels);
    }

    if (mWorkarounds.unpackLastRowSeparatelyForPaddingInclusion)
    {
        gl::Extents size(area.width, area.height, area.depth);

        bool apply;
        ANGLE_TRY_RESULT(
            ShouldApplyLastRowPaddingWorkaround(size, unpack, unpackBuffer, format, type,
                                                UseTexImage3D(getTarget()), pixels),
            apply);

        // The driver will think the pixel buffer doesn't have enough data, work around this bug
        // by uploading the last row (and last level if 3D) separately.
        if (apply)
        {
            return setSubImagePaddingWorkaround(context, target, level, area, format, type, unpack,
                                                unpackBuffer, pixels);
        }
    }

    if (UseTexImage2D(getTarget()))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        mFunctions->texSubImage2D(target, static_cast<GLint>(level), area.x, area.y, area.width,
                                  area.height, texSubImageFormat.format, texSubImageFormat.type,
                                  pixels);
    }
    else
    {
        ASSERT(UseTexImage3D(getTarget()));
        mFunctions->texSubImage3D(target, static_cast<GLint>(level), area.x, area.y, area.z,
                                  area.width, area.height, area.depth, texSubImageFormat.format,
                                  texSubImageFormat.type, pixels);
    }

    return gl::NoError();
}

gl::Error TextureGL::setSubImageRowByRowWorkaround(const gl::Context *context,
                                                   GLenum target,
                                                   size_t level,
                                                   const gl::Box &area,
                                                   GLenum format,
                                                   GLenum type,
                                                   const gl::PixelUnpackState &unpack,
                                                   const gl::Buffer *unpackBuffer,
                                                   const uint8_t *pixels)
{
    gl::PixelUnpackState directUnpack;
    directUnpack.alignment   = 1;
    mStateManager->setPixelUnpackState(directUnpack);
    mStateManager->setPixelUnpackBuffer(unpackBuffer);

    const gl::InternalFormat &glFormat   = gl::GetInternalFormatInfo(format, type);
    GLuint rowBytes                      = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, unpack.alignment, unpack.rowLength),
                     rowBytes);
    GLuint imageBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeDepthPitch(area.height, unpack.imageHeight, rowBytes),
                     imageBytes);
    bool useTexImage3D = UseTexImage3D(getTarget());
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
        ASSERT(UseTexImage2D(getTarget()));
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

gl::Error TextureGL::setSubImagePaddingWorkaround(const gl::Context *context,
                                                  GLenum target,
                                                  size_t level,
                                                  const gl::Box &area,
                                                  GLenum format,
                                                  GLenum type,
                                                  const gl::PixelUnpackState &unpack,
                                                  const gl::Buffer *unpackBuffer,
                                                  const uint8_t *pixels)
{
    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);
    GLuint rowBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeRowPitch(type, area.width, unpack.alignment, unpack.rowLength),
                     rowBytes);
    GLuint imageBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeDepthPitch(area.height, unpack.imageHeight, rowBytes),
                     imageBytes);
    bool useTexImage3D = UseTexImage3D(getTarget());
    GLuint skipBytes   = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, imageBytes, unpack, useTexImage3D),
                     skipBytes);

    mStateManager->setPixelUnpackState(unpack);
    mStateManager->setPixelUnpackBuffer(unpackBuffer);

    gl::PixelUnpackState directUnpack;
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
        ASSERT(UseTexImage2D(getTarget()));

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

    return gl::NoError();
}

gl::Error TextureGL::setCompressedImage(const gl::Context *context,
                                        GLenum target,
                                        size_t level,
                                        GLenum internalFormat,
                                        const gl::Extents &size,
                                        const gl::PixelUnpackState &unpack,
                                        size_t imageSize,
                                        const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(getTarget(), target));

    nativegl::CompressedTexImageFormat compressedTexImageFormat =
        nativegl::GetCompressedTexImageFormat(mFunctions, mWorkarounds, internalFormat);

    mStateManager->bindTexture(getTarget(), mTextureID);
    if (UseTexImage2D(getTarget()))
    {
        ASSERT(size.depth == 1);
        mFunctions->compressedTexImage2D(target, static_cast<GLint>(level),
                                         compressedTexImageFormat.internalFormat, size.width,
                                         size.height, 0, static_cast<GLsizei>(imageSize), pixels);
    }
    else if (UseTexImage3D(getTarget()))
    {
        mFunctions->compressedTexImage3D(
            target, static_cast<GLint>(level), compressedTexImageFormat.internalFormat, size.width,
            size.height, size.depth, 0, static_cast<GLsizei>(imageSize), pixels);
    }
    else
    {
        UNREACHABLE();
    }

    LevelInfoGL levelInfo = GetLevelInfo(internalFormat, compressedTexImageFormat.internalFormat);
    ASSERT(!levelInfo.lumaWorkaround.enabled);
    setLevelInfo(target, level, 1, levelInfo);

    return gl::NoError();
}

gl::Error TextureGL::setCompressedSubImage(const gl::Context *context,
                                           GLenum target,
                                           size_t level,
                                           const gl::Box &area,
                                           GLenum format,
                                           const gl::PixelUnpackState &unpack,
                                           size_t imageSize,
                                           const uint8_t *pixels)
{
    ASSERT(CompatibleTextureTarget(getTarget(), target));

    nativegl::CompressedTexSubImageFormat compressedTexSubImageFormat =
        nativegl::GetCompressedSubTexImageFormat(mFunctions, mWorkarounds, format);

    mStateManager->bindTexture(getTarget(), mTextureID);
    if (UseTexImage2D(getTarget()))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        mFunctions->compressedTexSubImage2D(
            target, static_cast<GLint>(level), area.x, area.y, area.width, area.height,
            compressedTexSubImageFormat.format, static_cast<GLsizei>(imageSize), pixels);
    }
    else if (UseTexImage3D(getTarget()))
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

    ASSERT(!getLevelInfo(target, level).lumaWorkaround.enabled &&
           !GetLevelInfo(format, compressedTexSubImageFormat.format).lumaWorkaround.enabled);

    return gl::NoError();
}

gl::Error TextureGL::copyImage(const gl::Context *context,
                               GLenum target,
                               size_t level,
                               const gl::Rectangle &origSourceArea,
                               GLenum internalFormat,
                               const gl::Framebuffer *source)
{
    GLenum type = source->getImplementationColorReadType(context);
    nativegl::CopyTexImageImageFormat copyTexImageFormat =
        nativegl::GetCopyTexImageImageFormat(mFunctions, mWorkarounds, internalFormat, type);

    mStateManager->bindTexture(getTarget(), mTextureID);

    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);
    gl::Extents fbSize = sourceFramebufferGL->getState().getReadAttachment()->getSize();

    // Did the read area go outside the framebuffer?
    bool outside = origSourceArea.x < 0 || origSourceArea.y < 0 ||
                   origSourceArea.x + origSourceArea.width > fbSize.width ||
                   origSourceArea.y + origSourceArea.height > fbSize.height;

    // In WebGL mode the area outside the framebuffer must be zeroed.
    // We just zero the whole thing before copying into the area that overlaps the framebuffer.
    if (outside && context->getExtensions().webglCompatibility)
    {
        // TODO(fjhenigman): When robust resource initialization is implemented, avoid redundant
        // clearing of the texture.
        GLuint pixelBytes =
            gl::GetInternalFormatInfo(copyTexImageFormat.internalFormat, type).pixelBytes;
        angle::MemoryBuffer *zero;
        ANGLE_TRY(context->getZeroFilledBuffer(
            origSourceArea.width * origSourceArea.height * pixelBytes, &zero));

        gl::PixelUnpackState unpack;
        unpack.alignment = 1;
        mStateManager->setPixelUnpackState(unpack);
        mStateManager->setPixelUnpackBuffer(nullptr);

        mFunctions->texImage2D(target, static_cast<GLint>(level), copyTexImageFormat.internalFormat,
                               origSourceArea.width, origSourceArea.height, 0,
                               gl::GetUnsizedFormat(copyTexImageFormat.internalFormat), type,
                               zero->data());
    }

    // Clip source area to framebuffer and copy if remaining area is not empty.
    gl::Rectangle sourceArea;
    if (ClipRectangle(origSourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                      &sourceArea))
    {
        LevelInfoGL levelInfo = GetLevelInfo(internalFormat, copyTexImageFormat.internalFormat);
        gl::Offset destOffset(sourceArea.x - origSourceArea.x, sourceArea.y - origSourceArea.y, 0);

        if (levelInfo.lumaWorkaround.enabled)
        {
            if (outside)
            {
                ANGLE_TRY(mBlitter->copySubImageToLUMAWorkaroundTexture(
                    context, mTextureID, getTarget(), target, levelInfo.sourceFormat, level,
                    destOffset, sourceArea, source));
            }
            else
            {
                ANGLE_TRY(mBlitter->copyImageToLUMAWorkaroundTexture(
                    context, mTextureID, getTarget(), target, levelInfo.sourceFormat, level,
                    sourceArea, copyTexImageFormat.internalFormat, source));
            }
        }
        else if (UseTexImage2D(getTarget()))
        {
            mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER,
                                           sourceFramebufferGL->getFramebufferID());
            if (outside)
            {
                mFunctions->copyTexSubImage2D(target, static_cast<GLint>(level), destOffset.x,
                                              destOffset.y, sourceArea.x, sourceArea.y,
                                              sourceArea.width, sourceArea.height);
            }
            else
            {
                mFunctions->copyTexImage2D(target, static_cast<GLint>(level),
                                           copyTexImageFormat.internalFormat, sourceArea.x,
                                           sourceArea.y, sourceArea.width, sourceArea.height, 0);
            }
        }
        else
        {
            UNREACHABLE();
        }

        setLevelInfo(target, level, 1, levelInfo);
    }

    return gl::NoError();
}

gl::Error TextureGL::copySubImage(const gl::Context *context,
                                  GLenum target,
                                  size_t level,
                                  const gl::Offset &origDestOffset,
                                  const gl::Rectangle &origSourceArea,
                                  const gl::Framebuffer *source)
{
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);

    // Clip source area to framebuffer.
    const gl::Extents fbSize = sourceFramebufferGL->getState().getReadAttachment()->getSize();
    gl::Rectangle sourceArea;
    if (!ClipRectangle(origSourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &sourceArea))
    {
        // nothing to do
        return gl::NoError();
    }
    gl::Offset destOffset(origDestOffset.x + sourceArea.x - origSourceArea.x,
                          origDestOffset.y + sourceArea.y - origSourceArea.y, origDestOffset.z);

    mStateManager->bindTexture(getTarget(), mTextureID);
    mStateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    const LevelInfoGL &levelInfo = getLevelInfo(target, level);
    if (levelInfo.lumaWorkaround.enabled)
    {
        gl::Error error = mBlitter->copySubImageToLUMAWorkaroundTexture(
            context, mTextureID, getTarget(), target, levelInfo.sourceFormat, level, destOffset,
            sourceArea, source);
        if (error.isError())
        {
            return error;
        }
    }
    else
    {
        if (UseTexImage2D(getTarget()))
        {
            ASSERT(destOffset.z == 0);
            mFunctions->copyTexSubImage2D(target, static_cast<GLint>(level), destOffset.x,
                                          destOffset.y, sourceArea.x, sourceArea.y,
                                          sourceArea.width, sourceArea.height);
        }
        else if (UseTexImage3D(getTarget()))
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

    return gl::NoError();
}

gl::Error TextureGL::copyTexture(const gl::Context *context,
                                 GLenum target,
                                 size_t level,
                                 GLenum internalFormat,
                                 GLenum type,
                                 size_t sourceLevel,
                                 bool unpackFlipY,
                                 bool unpackPremultiplyAlpha,
                                 bool unpackUnmultiplyAlpha,
                                 const gl::Texture *source)
{
    const TextureGL *sourceGL            = GetImplAs<TextureGL>(source);
    const gl::ImageDesc &sourceImageDesc =
        sourceGL->mState.getImageDesc(source->getTarget(), sourceLevel);
    gl::Rectangle sourceArea(0, 0, sourceImageDesc.size.width, sourceImageDesc.size.height);

    reserveTexImageToBeFilled(target, level, internalFormat, sourceImageDesc.size,
                              gl::GetUnsizedFormat(internalFormat), type);

    return copySubTextureHelper(context, target, level, gl::Offset(0, 0, 0), sourceLevel,
                                sourceArea, gl::GetUnsizedFormat(internalFormat), type, unpackFlipY,
                                unpackPremultiplyAlpha, unpackUnmultiplyAlpha, source);
}

gl::Error TextureGL::copySubTexture(const gl::Context *context,
                                    GLenum target,
                                    size_t level,
                                    const gl::Offset &destOffset,
                                    size_t sourceLevel,
                                    const gl::Rectangle &sourceArea,
                                    bool unpackFlipY,
                                    bool unpackPremultiplyAlpha,
                                    bool unpackUnmultiplyAlpha,
                                    const gl::Texture *source)
{
    const gl::InternalFormat &destFormatInfo = *mState.getImageDesc(target, level).format.info;
    return copySubTextureHelper(context, target, level, destOffset, sourceLevel, sourceArea,
                                destFormatInfo.format, destFormatInfo.type, unpackFlipY,
                                unpackPremultiplyAlpha, unpackUnmultiplyAlpha, source);
}

gl::Error TextureGL::copySubTextureHelper(const gl::Context *context,
                                          GLenum target,
                                          size_t level,
                                          const gl::Offset &destOffset,
                                          size_t sourceLevel,
                                          const gl::Rectangle &sourceArea,
                                          GLenum destFormat,
                                          GLenum destType,
                                          bool unpackFlipY,
                                          bool unpackPremultiplyAlpha,
                                          bool unpackUnmultiplyAlpha,
                                          const gl::Texture *source)
{
    TextureGL *sourceGL                  = GetImplAs<TextureGL>(source);
    const gl::ImageDesc &sourceImageDesc =
        sourceGL->mState.getImageDesc(source->getTarget(), sourceLevel);

    // Check is this is a simple copySubTexture that can be done with a copyTexSubImage
    ASSERT(sourceGL->getTarget() == GL_TEXTURE_2D);
    const LevelInfoGL &sourceLevelInfo = sourceGL->getLevelInfo(source->getTarget(), sourceLevel);
    bool needsLumaWorkaround           = sourceLevelInfo.lumaWorkaround.enabled;

    GLenum sourceFormat = sourceImageDesc.format.info->format;
    bool sourceFormatContainSupersetOfDestFormat =
        (sourceFormat == destFormat && sourceFormat != GL_BGRA_EXT) ||
        (sourceFormat == GL_RGBA && destFormat == GL_RGB);

    GLenum sourceComponentType = sourceImageDesc.format.info->componentType;
    const auto &destInternalFormatInfo = gl::GetInternalFormatInfo(destFormat, destType);
    GLenum destComponentType           = destInternalFormatInfo.componentType;
    bool destSRGB                      = destInternalFormatInfo.colorEncoding == GL_SRGB;
    if (source->getTarget() == GL_TEXTURE_2D && !unpackFlipY &&
        unpackPremultiplyAlpha == unpackUnmultiplyAlpha && !needsLumaWorkaround &&
        sourceFormatContainSupersetOfDestFormat && sourceComponentType == destComponentType &&
        !destSRGB)
    {
        return mBlitter->copyTexSubImage(sourceGL, sourceLevel, this, target, level, sourceArea,
                                         destOffset);
    }

    // Check if the destination is renderable and copy on the GPU
    const LevelInfoGL &destLevelInfo = getLevelInfo(target, level);
    if (!destSRGB &&
        nativegl::SupportsNativeRendering(mFunctions, target, destLevelInfo.nativeInternalFormat))
    {
        return mBlitter->copySubTexture(context, sourceGL, sourceLevel, sourceComponentType, this,
                                        target, level, destComponentType, sourceImageDesc.size,
                                        sourceArea, destOffset, needsLumaWorkaround,
                                        sourceLevelInfo.sourceFormat, unpackFlipY,
                                        unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
    }

    // Fall back to CPU-readback
    return mBlitter->copySubTextureCPUReadback(context, sourceGL, sourceLevel, sourceComponentType,
                                               this, target, level, destFormat, destType,
                                               sourceArea, destOffset, unpackFlipY,
                                               unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

gl::Error TextureGL::setStorage(const gl::Context *context,
                                GLenum target,
                                size_t levels,
                                GLenum internalFormat,
                                const gl::Extents &size)
{
    nativegl::TexStorageFormat texStorageFormat =
        nativegl::GetTexStorageFormat(mFunctions, mWorkarounds, internalFormat);

    mStateManager->bindTexture(getTarget(), mTextureID);
    if (UseTexImage2D(getTarget()))
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
            mStateManager->bindBuffer(gl::BufferBinding::PixelUnpack, 0);

            const gl::InternalFormat &internalFormatInfo =
                gl::GetSizedInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.sized);

            for (size_t level = 0; level < levels; level++)
            {
                gl::Extents levelSize(std::max(size.width >> level, 1),
                                      std::max(size.height >> level, 1),
                                      1);

                if (getTarget() == GL_TEXTURE_2D || getTarget() == GL_TEXTURE_RECTANGLE_ANGLE)
                {
                    if (internalFormatInfo.compressed)
                    {
                        nativegl::CompressedTexSubImageFormat compressedTexImageFormat =
                            nativegl::GetCompressedSubTexImageFormat(mFunctions, mWorkarounds,
                                                                     internalFormat);

                        GLuint dataSize = 0;
                        ANGLE_TRY_RESULT(internalFormatInfo.computeCompressedImageSize(levelSize),
                                         dataSize);
                        mFunctions->compressedTexImage2D(target, static_cast<GLint>(level),
                                                         compressedTexImageFormat.format,
                                                         levelSize.width, levelSize.height, 0,
                                                         static_cast<GLsizei>(dataSize), nullptr);
                    }
                    else
                    {
                        nativegl::TexImageFormat texImageFormat = nativegl::GetTexImageFormat(
                            mFunctions, mWorkarounds, internalFormat, internalFormatInfo.format,
                            internalFormatInfo.type);

                        mFunctions->texImage2D(target, static_cast<GLint>(level),
                                               texImageFormat.internalFormat, levelSize.width,
                                               levelSize.height, 0, texImageFormat.format,
                                               texImageFormat.type, nullptr);
                    }
                }
                else if (getTarget() == GL_TEXTURE_CUBE_MAP)
                {
                    for (GLenum face = gl::FirstCubeMapTextureTarget; face <= gl::LastCubeMapTextureTarget; face++)
                    {
                        if (internalFormatInfo.compressed)
                        {
                            nativegl::CompressedTexSubImageFormat compressedTexImageFormat =
                                nativegl::GetCompressedSubTexImageFormat(mFunctions, mWorkarounds,
                                                                         internalFormat);

                            GLuint dataSize = 0;
                            ANGLE_TRY_RESULT(internalFormatInfo.computeCompressedImageSize(levelSize),
                                             dataSize);
                            mFunctions->compressedTexImage2D(
                                face, static_cast<GLint>(level), compressedTexImageFormat.format,
                                levelSize.width, levelSize.height, 0,
                                static_cast<GLsizei>(dataSize), nullptr);
                        }
                        else
                        {
                            nativegl::TexImageFormat texImageFormat = nativegl::GetTexImageFormat(
                                mFunctions, mWorkarounds, internalFormat, internalFormatInfo.format,
                                internalFormatInfo.type);

                            mFunctions->texImage2D(face, static_cast<GLint>(level),
                                                   texImageFormat.internalFormat, levelSize.width,
                                                   levelSize.height, 0, texImageFormat.format,
                                                   texImageFormat.type, nullptr);
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
    else if (UseTexImage3D(getTarget()))
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
            mStateManager->bindBuffer(gl::BufferBinding::PixelUnpack, 0);

            const gl::InternalFormat &internalFormatInfo =
                gl::GetSizedInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.sized);

            for (GLsizei i = 0; i < static_cast<GLsizei>(levels); i++)
            {
                gl::Extents levelSize(
                    std::max(size.width >> i, 1), std::max(size.height >> i, 1),
                    getTarget() == GL_TEXTURE_3D ? std::max(size.depth >> i, 1) : size.depth);

                if (internalFormatInfo.compressed)
                {
                    nativegl::CompressedTexSubImageFormat compressedTexImageFormat =
                        nativegl::GetCompressedSubTexImageFormat(mFunctions, mWorkarounds,
                                                                 internalFormat);

                    GLuint dataSize = 0;
                    ANGLE_TRY_RESULT(
                        internalFormatInfo.computeCompressedImageSize(levelSize),
                        dataSize);
                    mFunctions->compressedTexImage3D(target, i, compressedTexImageFormat.format,
                                                     levelSize.width, levelSize.height,
                                                     levelSize.depth, 0,
                                                     static_cast<GLsizei>(dataSize), nullptr);
                }
                else
                {
                    nativegl::TexImageFormat texImageFormat = nativegl::GetTexImageFormat(
                        mFunctions, mWorkarounds, internalFormat, internalFormatInfo.format,
                        internalFormatInfo.type);

                    mFunctions->texImage3D(target, i, texImageFormat.internalFormat,
                                           levelSize.width, levelSize.height, levelSize.depth, 0,
                                           texImageFormat.format, texImageFormat.type, nullptr);
                }
            }
        }
    }
    else
    {
        UNREACHABLE();
    }

    setLevelInfo(target, 0, levels, GetLevelInfo(internalFormat, texStorageFormat.internalFormat));

    return gl::NoError();
}

gl::Error TextureGL::setStorageMultisample(const gl::Context *context,
                                           GLenum target,
                                           GLsizei samples,
                                           GLint internalFormat,
                                           const gl::Extents &size,
                                           bool fixedSampleLocations)
{
    nativegl::TexStorageFormat texStorageFormat =
        nativegl::GetTexStorageFormat(mFunctions, mWorkarounds, internalFormat);

    mStateManager->bindTexture(mState.mTarget, mTextureID);

    ASSERT(size.depth == 1);

    mFunctions->texStorage2DMultisample(target, samples, texStorageFormat.internalFormat,
                                        size.width, size.height,
                                        gl::ConvertToGLBoolean(fixedSampleLocations));

    setLevelInfo(target, 0, 1, GetLevelInfo(internalFormat, texStorageFormat.internalFormat));

    return gl::NoError();
}

gl::Error TextureGL::setImageExternal(const gl::Context *context,
                                      GLenum target,
                                      egl::Stream *stream,
                                      const egl::Stream::GLTextureDescription &desc)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error TextureGL::generateMipmap(const gl::Context *context)
{
    mStateManager->bindTexture(getTarget(), mTextureID);
    mFunctions->generateMipmap(getTarget());

    const GLuint effectiveBaseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel           = mState.getMipmapMaxLevel();

    setLevelInfo(getTarget(), effectiveBaseLevel, maxLevel - effectiveBaseLevel,
                 getBaseLevelInfo());

    return gl::NoError();
}

gl::Error TextureGL::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    ASSERT(getTarget() == GL_TEXTURE_2D || getTarget() == GL_TEXTURE_RECTANGLE_ANGLE);

    // Make sure this texture is bound
    mStateManager->bindTexture(getTarget(), mTextureID);

    setLevelInfo(getTarget(), 0, 1, LevelInfoGL());
    return gl::NoError();
}

gl::Error TextureGL::releaseTexImage(const gl::Context *context)
{
    // Not all Surface implementations reset the size of mip 0 when releasing, do it manually
    ASSERT(getTarget() == GL_TEXTURE_2D || getTarget() == GL_TEXTURE_RECTANGLE_ANGLE);

    mStateManager->bindTexture(getTarget(), mTextureID);
    if (UseTexImage2D(getTarget()))
    {
        mFunctions->texImage2D(getTarget(), 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               nullptr);
    }
    else
    {
        UNREACHABLE();
    }
    return gl::NoError();
}

gl::Error TextureGL::setEGLImageTarget(const gl::Context *context, GLenum target, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

void TextureGL::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
    if (dirtyBits.none() && mLocalDirtyBits.none())
    {
        return;
    }

    mStateManager->bindTexture(getTarget(), mTextureID);

    if (dirtyBits[gl::Texture::DIRTY_BIT_BASE_LEVEL] || dirtyBits[gl::Texture::DIRTY_BIT_MAX_LEVEL])
    {
        // Don't know if the previous base level was using any workarounds, always re-sync the
        // workaround dirty bits
        mLocalDirtyBits |= GetLevelWorkaroundDirtyBits();
    }

    for (auto dirtyBit : (dirtyBits | mLocalDirtyBits))
    {
        switch (dirtyBit)
        {
            case gl::Texture::DIRTY_BIT_MIN_FILTER:
                mAppliedSampler.minFilter = mState.getSamplerState().minFilter;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_MIN_FILTER,
                                          mAppliedSampler.minFilter);
                break;
            case gl::Texture::DIRTY_BIT_MAG_FILTER:
                mAppliedSampler.magFilter = mState.getSamplerState().magFilter;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_MAG_FILTER,
                                          mAppliedSampler.magFilter);
                break;
            case gl::Texture::DIRTY_BIT_WRAP_S:
                mAppliedSampler.wrapS = mState.getSamplerState().wrapS;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_WRAP_S, mAppliedSampler.wrapS);
                break;
            case gl::Texture::DIRTY_BIT_WRAP_T:
                mAppliedSampler.wrapT = mState.getSamplerState().wrapT;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_WRAP_T, mAppliedSampler.wrapT);
                break;
            case gl::Texture::DIRTY_BIT_WRAP_R:
                mAppliedSampler.wrapR = mState.getSamplerState().wrapR;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_WRAP_R, mAppliedSampler.wrapR);
                break;
            case gl::Texture::DIRTY_BIT_MAX_ANISOTROPY:
                mAppliedSampler.maxAnisotropy = mState.getSamplerState().maxAnisotropy;
                mFunctions->texParameterf(getTarget(), GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                          mAppliedSampler.maxAnisotropy);
                break;
            case gl::Texture::DIRTY_BIT_MIN_LOD:
                mAppliedSampler.minLod = mState.getSamplerState().minLod;
                mFunctions->texParameterf(getTarget(), GL_TEXTURE_MIN_LOD, mAppliedSampler.minLod);
                break;
            case gl::Texture::DIRTY_BIT_MAX_LOD:
                mAppliedSampler.maxLod = mState.getSamplerState().maxLod;
                mFunctions->texParameterf(getTarget(), GL_TEXTURE_MAX_LOD, mAppliedSampler.maxLod);
                break;
            case gl::Texture::DIRTY_BIT_COMPARE_MODE:
                mAppliedSampler.compareMode = mState.getSamplerState().compareMode;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_COMPARE_MODE,
                                          mAppliedSampler.compareMode);
                break;
            case gl::Texture::DIRTY_BIT_COMPARE_FUNC:
                mAppliedSampler.compareFunc = mState.getSamplerState().compareFunc;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_COMPARE_FUNC,
                                          mAppliedSampler.compareFunc);
                break;
            case gl::Texture::DIRTY_BIT_SRGB_DECODE:
                mAppliedSampler.sRGBDecode = mState.getSamplerState().sRGBDecode;
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_SRGB_DECODE_EXT,
                                          mAppliedSampler.sRGBDecode);
                break;

            // Texture state
            case gl::Texture::DIRTY_BIT_SWIZZLE_RED:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_R,
                                        mState.getSwizzleState().swizzleRed,
                                        &mAppliedSwizzle.swizzleRed);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_GREEN:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_G,
                                        mState.getSwizzleState().swizzleGreen,
                                        &mAppliedSwizzle.swizzleGreen);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_BLUE:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_B,
                                        mState.getSwizzleState().swizzleBlue,
                                        &mAppliedSwizzle.swizzleBlue);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA:
                syncTextureStateSwizzle(mFunctions, GL_TEXTURE_SWIZZLE_A,
                                        mState.getSwizzleState().swizzleAlpha,
                                        &mAppliedSwizzle.swizzleAlpha);
                break;
            case gl::Texture::DIRTY_BIT_BASE_LEVEL:
                mAppliedBaseLevel = mState.getEffectiveBaseLevel();
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_BASE_LEVEL, mAppliedBaseLevel);
                break;
            case gl::Texture::DIRTY_BIT_MAX_LEVEL:
                mAppliedMaxLevel = mState.getEffectiveMaxLevel();
                mFunctions->texParameteri(getTarget(), GL_TEXTURE_MAX_LEVEL, mAppliedMaxLevel);
                break;
            case gl::Texture::DIRTY_BIT_USAGE:
                break;
            case gl::Texture::DIRTY_BIT_LABEL:
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

gl::Error TextureGL::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    if (baseLevel != mAppliedBaseLevel)
    {
        mAppliedBaseLevel = baseLevel;
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_BASE_LEVEL);

        mStateManager->bindTexture(getTarget(), mTextureID);
        mFunctions->texParameteri(getTarget(), GL_TEXTURE_BASE_LEVEL, baseLevel);
    }
    return gl::NoError();
}

void TextureGL::setMinFilter(GLenum filter)
{
    if (filter != mAppliedSampler.minFilter)
    {
        mAppliedSampler.minFilter = filter;
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_MIN_FILTER);

        mStateManager->bindTexture(getTarget(), mTextureID);
        mFunctions->texParameteri(getTarget(), GL_TEXTURE_MIN_FILTER, filter);
    }
}
void TextureGL::setMagFilter(GLenum filter)
{
    if (filter != mAppliedSampler.magFilter)
    {
        mAppliedSampler.magFilter = filter;
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_MAG_FILTER);

        mStateManager->bindTexture(getTarget(), mTextureID);
        mFunctions->texParameteri(getTarget(), GL_TEXTURE_MAG_FILTER, filter);
    }
}

void TextureGL::setSwizzle(GLint swizzle[4])
{
    gl::SwizzleState resultingSwizzle =
        gl::SwizzleState(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

    if (resultingSwizzle != mAppliedSwizzle)
    {
        mAppliedSwizzle = resultingSwizzle;
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_RED);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_GREEN);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_BLUE);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA);

        mStateManager->bindTexture(getTarget(), mTextureID);
        mFunctions->texParameteriv(getTarget(), GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }
}

void TextureGL::syncTextureStateSwizzle(const FunctionsGL *functions,
                                        GLenum name,
                                        GLenum value,
                                        GLenum *outValue)
{
    const LevelInfoGL &levelInfo = getBaseLevelInfo();
    GLenum resultSwizzle         = value;
    if (levelInfo.lumaWorkaround.enabled || levelInfo.depthStencilWorkaround)
    {
        if (levelInfo.lumaWorkaround.enabled)
        {
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

    *outValue = resultSwizzle;
    functions->texParameteri(getTarget(), name, resultSwizzle);
}

void TextureGL::setLevelInfo(GLenum target,
                             size_t level,
                             size_t levelCount,
                             const LevelInfoGL &levelInfo)
{
    ASSERT(levelCount > 0);

    bool updateWorkarounds = levelInfo.depthStencilWorkaround || levelInfo.lumaWorkaround.enabled;

    for (size_t i = level; i < level + levelCount; i++)
    {
        if (target == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = gl::FirstCubeMapTextureTarget; face <= gl::LastCubeMapTextureTarget;
                 face++)
            {
                size_t index = GetLevelInfoIndex(face, level);
                ASSERT(index < mLevelInfo.size());
                auto &curLevelInfo = mLevelInfo[index];

                updateWorkarounds |= curLevelInfo.depthStencilWorkaround;
                updateWorkarounds |= curLevelInfo.lumaWorkaround.enabled;

                curLevelInfo = levelInfo;
            }
        }
        else
        {
            size_t index = GetLevelInfoIndex(target, level);
            ASSERT(index < mLevelInfo.size());
            auto &curLevelInfo = mLevelInfo[index];

            updateWorkarounds |= curLevelInfo.depthStencilWorkaround;
            updateWorkarounds |= curLevelInfo.lumaWorkaround.enabled;

            curLevelInfo = levelInfo;
        }
    }

    if (updateWorkarounds)
    {
        mLocalDirtyBits |= GetLevelWorkaroundDirtyBits();
    }
}

const LevelInfoGL &TextureGL::getLevelInfo(GLenum target, size_t level) const
{
    ASSERT(target != GL_TEXTURE_CUBE_MAP);
    return mLevelInfo[GetLevelInfoIndex(target, level)];
}

const LevelInfoGL &TextureGL::getBaseLevelInfo() const
{
    GLint effectiveBaseLevel = mState.getEffectiveBaseLevel();
    GLenum target =
        getTarget() == GL_TEXTURE_CUBE_MAP ? gl::FirstCubeMapTextureTarget : getTarget();
    return getLevelInfo(target, effectiveBaseLevel);
}

GLuint TextureGL::getTextureID() const
{
    return mTextureID;
}

GLenum TextureGL::getTarget() const
{
    return mState.mTarget;
}

gl::Error TextureGL::initializeContents(const gl::Context *context,
                                        const gl::ImageIndex &imageIndex)
{
    // UNIMPLEMENTED();
    return gl::NoError();
}

}  // namespace rx
