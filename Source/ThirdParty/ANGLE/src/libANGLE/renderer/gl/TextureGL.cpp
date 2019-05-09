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
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/ImageGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"
#include "libANGLE/renderer/gl/formatutilsgl.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

using angle::CheckedNumeric;

namespace rx
{

namespace
{

size_t GetLevelInfoIndex(gl::TextureTarget target, size_t level)
{
    return gl::IsCubeMapFaceTarget(target)
               ? ((level * gl::kCubeFaceCount) + gl::CubeMapTextureTargetToFaceIndex(target))
               : level;
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

size_t GetMaxLevelInfoCountForTextureType(gl::TextureType type)
{
    switch (type)
    {
        case gl::TextureType::CubeMap:
            return (gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1) * gl::kCubeFaceCount;

        case gl::TextureType::External:
            return 1;

        default:
            return gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS + 1;
    }
}

}  // anonymous namespace

LUMAWorkaroundGL::LUMAWorkaroundGL() : LUMAWorkaroundGL(false, GL_NONE) {}

LUMAWorkaroundGL::LUMAWorkaroundGL(bool enabled_, GLenum workaroundFormat_)
    : enabled(enabled_), workaroundFormat(workaroundFormat_)
{}

LevelInfoGL::LevelInfoGL() : LevelInfoGL(GL_NONE, GL_NONE, false, LUMAWorkaroundGL()) {}

LevelInfoGL::LevelInfoGL(GLenum sourceFormat_,
                         GLenum nativeInternalFormat_,
                         bool depthStencilWorkaround_,
                         const LUMAWorkaroundGL &lumaWorkaround_)
    : sourceFormat(sourceFormat_),
      nativeInternalFormat(nativeInternalFormat_),
      depthStencilWorkaround(depthStencilWorkaround_),
      lumaWorkaround(lumaWorkaround_)
{}

TextureGL::TextureGL(const gl::TextureState &state, GLuint id)
    : TextureImpl(state),
      mAppliedSwizzle(state.getSwizzleState()),
      mAppliedSampler(state.getSamplerState()),
      mAppliedBaseLevel(state.getEffectiveBaseLevel()),
      mAppliedMaxLevel(state.getEffectiveMaxLevel()),
      mTextureID(id)
{
    mLevelInfo.resize(GetMaxLevelInfoCountForTextureType(getType()));
}

TextureGL::~TextureGL()
{
    ASSERT(mTextureID == 0);
}

void TextureGL::onDestroy(const gl::Context *context)
{
    StateManagerGL *stateManager = GetStateManagerGL(context);
    stateManager->deleteTexture(mTextureID);
    mTextureID = 0;
}

angle::Result TextureGL::setImage(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  GLenum internalFormat,
                                  const gl::Extents &size,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  const uint8_t *pixels)
{
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    const gl::Buffer *unpackBuffer =
        context->getState().getTargetBuffer(gl::BufferBinding::PixelUnpack);

    gl::TextureTarget target = index.getTarget();
    size_t level             = static_cast<size_t>(index.getLevelIndex());

    if (workarounds.unpackOverlappingRowsSeparatelyUnpackBuffer && unpackBuffer &&
        unpack.rowLength != 0 && unpack.rowLength < size.width)
    {
        // The rows overlap in unpack memory. Upload the texture row by row to work around
        // driver bug.
        reserveTexImageToBeFilled(context, target, level, internalFormat, size, format, type);

        if (size.width == 0 || size.height == 0 || size.depth == 0)
        {
            return angle::Result::Continue;
        }

        gl::Box area(0, 0, 0, size.width, size.height, size.depth);
        return setSubImageRowByRowWorkaround(context, target, level, area, format, type, unpack,
                                             unpackBuffer, pixels);
    }

    if (workarounds.unpackLastRowSeparatelyForPaddingInclusion)
    {
        bool apply = false;
        ANGLE_TRY(ShouldApplyLastRowPaddingWorkaround(
            GetImplAs<ContextGL>(context), size, unpack, unpackBuffer, format, type,
            nativegl::UseTexImage3D(getType()), pixels, &apply));

        // The driver will think the pixel buffer doesn't have enough data, work around this bug
        // by uploading the last row (and last level if 3D) separately.
        if (apply)
        {
            reserveTexImageToBeFilled(context, target, level, internalFormat, size, format, type);

            if (size.width == 0 || size.height == 0 || size.depth == 0)
            {
                return angle::Result::Continue;
            }

            gl::Box area(0, 0, 0, size.width, size.height, size.depth);
            return setSubImagePaddingWorkaround(context, target, level, area, format, type, unpack,
                                                unpackBuffer, pixels);
        }
    }

    setImageHelper(context, target, level, internalFormat, size, format, type, pixels);

    return angle::Result::Continue;
}

void TextureGL::setImageHelper(const gl::Context *context,
                               gl::TextureTarget target,
                               size_t level,
                               GLenum internalFormat,
                               const gl::Extents &size,
                               GLenum format,
                               GLenum type,
                               const uint8_t *pixels)
{
    ASSERT(TextureTargetToType(target) == getType());

    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    nativegl::TexImageFormat texImageFormat =
        nativegl::GetTexImageFormat(functions, workarounds, internalFormat, format, type);

    stateManager->bindTexture(getType(), mTextureID);

    if (nativegl::UseTexImage2D(getType()))
    {
        ASSERT(size.depth == 1);
        functions->texImage2D(ToGLenum(target), static_cast<GLint>(level),
                              texImageFormat.internalFormat, size.width, size.height, 0,
                              texImageFormat.format, texImageFormat.type, pixels);
    }
    else
    {
        ASSERT(nativegl::UseTexImage3D(getType()));
        functions->texImage3D(ToGLenum(target), static_cast<GLint>(level),
                              texImageFormat.internalFormat, size.width, size.height, size.depth, 0,
                              texImageFormat.format, texImageFormat.type, pixels);
    }

    setLevelInfo(context, target, level, 1,
                 GetLevelInfo(internalFormat, texImageFormat.internalFormat));
}

void TextureGL::reserveTexImageToBeFilled(const gl::Context *context,
                                          gl::TextureTarget target,
                                          size_t level,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          GLenum format,
                                          GLenum type)
{
    StateManagerGL *stateManager = GetStateManagerGL(context);
    stateManager->setPixelUnpackBuffer(nullptr);
    setImageHelper(context, target, level, internalFormat, size, format, type, nullptr);
}

angle::Result TextureGL::setSubImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     const gl::Box &area,
                                     GLenum format,
                                     GLenum type,
                                     const gl::PixelUnpackState &unpack,
                                     gl::Buffer *unpackBuffer,
                                     const uint8_t *pixels)
{
    ASSERT(TextureTargetToType(index.getTarget()) == getType());

    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    nativegl::TexSubImageFormat texSubImageFormat =
        nativegl::GetTexSubImageFormat(functions, workarounds, format, type);

    gl::TextureTarget target = index.getTarget();
    size_t level             = static_cast<size_t>(index.getLevelIndex());

    ASSERT(getLevelInfo(target, level).lumaWorkaround.enabled ==
           GetLevelInfo(format, texSubImageFormat.format).lumaWorkaround.enabled);

    stateManager->bindTexture(getType(), mTextureID);
    if (workarounds.unpackOverlappingRowsSeparatelyUnpackBuffer && unpackBuffer &&
        unpack.rowLength != 0 && unpack.rowLength < area.width)
    {
        return setSubImageRowByRowWorkaround(context, target, level, area, format, type, unpack,
                                             unpackBuffer, pixels);
    }

    if (workarounds.unpackLastRowSeparatelyForPaddingInclusion)
    {
        gl::Extents size(area.width, area.height, area.depth);

        bool apply = false;
        ANGLE_TRY(ShouldApplyLastRowPaddingWorkaround(
            GetImplAs<ContextGL>(context), size, unpack, unpackBuffer, format, type,
            nativegl::UseTexImage3D(getType()), pixels, &apply));

        // The driver will think the pixel buffer doesn't have enough data, work around this bug
        // by uploading the last row (and last level if 3D) separately.
        if (apply)
        {
            return setSubImagePaddingWorkaround(context, target, level, area, format, type, unpack,
                                                unpackBuffer, pixels);
        }
    }

    if (nativegl::UseTexImage2D(getType()))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        functions->texSubImage2D(ToGLenum(target), static_cast<GLint>(level), area.x, area.y,
                                 area.width, area.height, texSubImageFormat.format,
                                 texSubImageFormat.type, pixels);
    }
    else
    {
        ASSERT(nativegl::UseTexImage3D(getType()));
        functions->texSubImage3D(ToGLenum(target), static_cast<GLint>(level), area.x, area.y,
                                 area.z, area.width, area.height, area.depth,
                                 texSubImageFormat.format, texSubImageFormat.type, pixels);
    }

    return angle::Result::Continue;
}

angle::Result TextureGL::setSubImageRowByRowWorkaround(const gl::Context *context,
                                                       gl::TextureTarget target,
                                                       size_t level,
                                                       const gl::Box &area,
                                                       GLenum format,
                                                       GLenum type,
                                                       const gl::PixelUnpackState &unpack,
                                                       const gl::Buffer *unpackBuffer,
                                                       const uint8_t *pixels)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    gl::PixelUnpackState directUnpack;
    directUnpack.alignment = 1;
    stateManager->setPixelUnpackState(directUnpack);
    stateManager->setPixelUnpackBuffer(unpackBuffer);

    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);
    GLuint rowBytes                    = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeRowPitch(type, area.width, unpack.alignment,
                                                            unpack.rowLength, &rowBytes));
    GLuint imageBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeDepthPitch(area.height, unpack.imageHeight,
                                                              rowBytes, &imageBytes));

    bool useTexImage3D = nativegl::UseTexImage3D(getType());
    GLuint skipBytes   = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeSkipBytes(type, rowBytes, imageBytes, unpack,
                                                             useTexImage3D, &skipBytes));

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
                functions->texSubImage3D(ToGLenum(target), static_cast<GLint>(level), area.x,
                                         row + area.y, image + area.z, area.width, 1, 1, format,
                                         type, rowPixels);
            }
        }
    }
    else
    {
        ASSERT(nativegl::UseTexImage2D(getType()));
        for (GLint row = 0; row < area.height; ++row)
        {
            GLint byteOffset         = row * rowBytes;
            const GLubyte *rowPixels = pixelsWithSkip + byteOffset;
            functions->texSubImage2D(ToGLenum(target), static_cast<GLint>(level), area.x,
                                     row + area.y, area.width, 1, format, type, rowPixels);
        }
    }
    return angle::Result::Continue;
}

angle::Result TextureGL::setSubImagePaddingWorkaround(const gl::Context *context,
                                                      gl::TextureTarget target,
                                                      size_t level,
                                                      const gl::Box &area,
                                                      GLenum format,
                                                      GLenum type,
                                                      const gl::PixelUnpackState &unpack,
                                                      const gl::Buffer *unpackBuffer,
                                                      const uint8_t *pixels)
{
    ContextGL *contextGL         = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);
    GLuint rowBytes                    = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeRowPitch(type, area.width, unpack.alignment,
                                                            unpack.rowLength, &rowBytes));
    GLuint imageBytes = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeDepthPitch(area.height, unpack.imageHeight,
                                                              rowBytes, &imageBytes));
    bool useTexImage3D = nativegl::UseTexImage3D(getType());
    GLuint skipBytes   = 0;
    ANGLE_CHECK_GL_MATH(contextGL, glFormat.computeSkipBytes(type, rowBytes, imageBytes, unpack,
                                                             useTexImage3D, &skipBytes));

    stateManager->setPixelUnpackState(unpack);
    stateManager->setPixelUnpackBuffer(unpackBuffer);

    gl::PixelUnpackState directUnpack;
    directUnpack.alignment = 1;

    if (useTexImage3D)
    {
        // Upload all but the last slice
        if (area.depth > 1)
        {
            functions->texSubImage3D(ToGLenum(target), static_cast<GLint>(level), area.x, area.y,
                                     area.z, area.width, area.height, area.depth - 1, format, type,
                                     pixels);
        }

        // Upload the last slice but its last row
        if (area.height > 1)
        {
            // Do not include skipBytes in the last image pixel start offset as it will be done by
            // the driver
            GLint lastImageOffset          = (area.depth - 1) * imageBytes;
            const GLubyte *lastImagePixels = pixels + lastImageOffset;
            functions->texSubImage3D(ToGLenum(target), static_cast<GLint>(level), area.x, area.y,
                                     area.z + area.depth - 1, area.width, area.height - 1, 1,
                                     format, type, lastImagePixels);
        }

        // Upload the last row of the last slice "manually"
        stateManager->setPixelUnpackState(directUnpack);

        GLint lastRowOffset =
            skipBytes + (area.depth - 1) * imageBytes + (area.height - 1) * rowBytes;
        const GLubyte *lastRowPixels = pixels + lastRowOffset;
        functions->texSubImage3D(ToGLenum(target), static_cast<GLint>(level), area.x,
                                 area.y + area.height - 1, area.z + area.depth - 1, area.width, 1,
                                 1, format, type, lastRowPixels);
    }
    else
    {
        ASSERT(nativegl::UseTexImage2D(getType()));

        // Upload all but the last row
        if (area.height > 1)
        {
            functions->texSubImage2D(ToGLenum(target), static_cast<GLint>(level), area.x, area.y,
                                     area.width, area.height - 1, format, type, pixels);
        }

        // Upload the last row "manually"
        stateManager->setPixelUnpackState(directUnpack);

        GLint lastRowOffset          = skipBytes + (area.height - 1) * rowBytes;
        const GLubyte *lastRowPixels = pixels + lastRowOffset;
        functions->texSubImage2D(ToGLenum(target), static_cast<GLint>(level), area.x,
                                 area.y + area.height - 1, area.width, 1, format, type,
                                 lastRowPixels);
    }

    return angle::Result::Continue;
}

angle::Result TextureGL::setCompressedImage(const gl::Context *context,
                                            const gl::ImageIndex &index,
                                            GLenum internalFormat,
                                            const gl::Extents &size,
                                            const gl::PixelUnpackState &unpack,
                                            size_t imageSize,
                                            const uint8_t *pixels)
{
    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    gl::TextureTarget target = index.getTarget();
    size_t level             = static_cast<size_t>(index.getLevelIndex());
    ASSERT(TextureTargetToType(target) == getType());

    nativegl::CompressedTexImageFormat compressedTexImageFormat =
        nativegl::GetCompressedTexImageFormat(functions, workarounds, internalFormat);

    stateManager->bindTexture(getType(), mTextureID);
    if (nativegl::UseTexImage2D(getType()))
    {
        ASSERT(size.depth == 1);
        functions->compressedTexImage2D(ToGLenum(target), static_cast<GLint>(level),
                                        compressedTexImageFormat.internalFormat, size.width,
                                        size.height, 0, static_cast<GLsizei>(imageSize), pixels);
    }
    else
    {
        ASSERT(nativegl::UseTexImage3D(getType()));
        functions->compressedTexImage3D(
            ToGLenum(target), static_cast<GLint>(level), compressedTexImageFormat.internalFormat,
            size.width, size.height, size.depth, 0, static_cast<GLsizei>(imageSize), pixels);
    }

    LevelInfoGL levelInfo = GetLevelInfo(internalFormat, compressedTexImageFormat.internalFormat);
    ASSERT(!levelInfo.lumaWorkaround.enabled);
    setLevelInfo(context, target, level, 1, levelInfo);

    return angle::Result::Continue;
}

angle::Result TextureGL::setCompressedSubImage(const gl::Context *context,
                                               const gl::ImageIndex &index,
                                               const gl::Box &area,
                                               GLenum format,
                                               const gl::PixelUnpackState &unpack,
                                               size_t imageSize,
                                               const uint8_t *pixels)
{
    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    gl::TextureTarget target = index.getTarget();
    size_t level             = static_cast<size_t>(index.getLevelIndex());
    ASSERT(TextureTargetToType(target) == getType());

    nativegl::CompressedTexSubImageFormat compressedTexSubImageFormat =
        nativegl::GetCompressedSubTexImageFormat(functions, workarounds, format);

    stateManager->bindTexture(getType(), mTextureID);
    if (nativegl::UseTexImage2D(getType()))
    {
        ASSERT(area.z == 0 && area.depth == 1);
        functions->compressedTexSubImage2D(
            ToGLenum(target), static_cast<GLint>(level), area.x, area.y, area.width, area.height,
            compressedTexSubImageFormat.format, static_cast<GLsizei>(imageSize), pixels);
    }
    else
    {
        ASSERT(nativegl::UseTexImage3D(getType()));
        functions->compressedTexSubImage3D(ToGLenum(target), static_cast<GLint>(level), area.x,
                                           area.y, area.z, area.width, area.height, area.depth,
                                           compressedTexSubImageFormat.format,
                                           static_cast<GLsizei>(imageSize), pixels);
    }

    ASSERT(!getLevelInfo(target, level).lumaWorkaround.enabled &&
           !GetLevelInfo(format, compressedTexSubImageFormat.format).lumaWorkaround.enabled);

    return angle::Result::Continue;
}

angle::Result TextureGL::copyImage(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Rectangle &sourceArea,
                                   GLenum internalFormat,
                                   gl::Framebuffer *source)
{
    ContextGL *contextGL             = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    gl::TextureTarget target = index.getTarget();
    size_t level             = static_cast<size_t>(index.getLevelIndex());
    GLenum type              = GL_NONE;
    ANGLE_TRY(source->getImplementationColorReadType(context, &type));
    nativegl::CopyTexImageImageFormat copyTexImageFormat =
        nativegl::GetCopyTexImageImageFormat(functions, workarounds, internalFormat, type);

    stateManager->bindTexture(getType(), mTextureID);

    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);
    gl::Extents fbSize = sourceFramebufferGL->getState().getReadAttachment()->getSize();

    // Did the read area go outside the framebuffer?
    bool outside = sourceArea.x < 0 || sourceArea.y < 0 ||
                   sourceArea.x + sourceArea.width > fbSize.width ||
                   sourceArea.y + sourceArea.height > fbSize.height;

    // TODO: Find a way to initialize the texture entirely in the gl level with ensureInitialized.
    // Right now there is no easy way to pre-fill the texture when it is being redefined with
    // partially uninitialized data.
    bool requiresInitialization =
        outside && (context->isRobustResourceInitEnabled() || context->isWebGL());

    // When robust resource initialization is enabled, the area outside the framebuffer must be
    // zeroed. We just zero the whole thing before copying into the area that overlaps the
    // framebuffer.
    if (requiresInitialization)
    {
        GLuint pixelBytes =
            gl::GetInternalFormatInfo(copyTexImageFormat.internalFormat, type).pixelBytes;
        angle::MemoryBuffer *zero;
        ANGLE_CHECK_GL_ALLOC(
            contextGL,
            context->getZeroFilledBuffer(sourceArea.width * sourceArea.height * pixelBytes, &zero));

        gl::PixelUnpackState unpack;
        unpack.alignment = 1;
        stateManager->setPixelUnpackState(unpack);
        stateManager->setPixelUnpackBuffer(nullptr);

        functions->texImage2D(
            ToGLenum(target), static_cast<GLint>(level), copyTexImageFormat.internalFormat,
            sourceArea.width, sourceArea.height, 0,
            gl::GetUnsizedFormat(copyTexImageFormat.internalFormat), type, zero->data());
    }

    // Clip source area to framebuffer and copy if remaining area is not empty.
    gl::Rectangle clippedArea;
    if (ClipRectangle(sourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height), &clippedArea))
    {
        LevelInfoGL levelInfo = GetLevelInfo(internalFormat, copyTexImageFormat.internalFormat);
        gl::Offset destOffset(clippedArea.x - sourceArea.x, clippedArea.y - sourceArea.y, 0);

        if (levelInfo.lumaWorkaround.enabled)
        {
            BlitGL *blitter = GetBlitGL(context);

            if (requiresInitialization)
            {
                ANGLE_TRY(blitter->copySubImageToLUMAWorkaroundTexture(
                    context, mTextureID, getType(), target, levelInfo.sourceFormat, level,
                    destOffset, clippedArea, source));
            }
            else
            {
                ANGLE_TRY(blitter->copyImageToLUMAWorkaroundTexture(
                    context, mTextureID, getType(), target, levelInfo.sourceFormat, level,
                    clippedArea, copyTexImageFormat.internalFormat, source));
            }
        }
        else
        {
            ASSERT(nativegl::UseTexImage2D(getType()));
            stateManager->bindFramebuffer(GL_READ_FRAMEBUFFER,
                                          sourceFramebufferGL->getFramebufferID());
            if (requiresInitialization)
            {
                functions->copyTexSubImage2D(ToGLenum(target), static_cast<GLint>(level),
                                             destOffset.x, destOffset.y, clippedArea.x,
                                             clippedArea.y, clippedArea.width, clippedArea.height);
            }
            else
            {
                functions->copyTexImage2D(ToGLenum(target), static_cast<GLint>(level),
                                          copyTexImageFormat.internalFormat, clippedArea.x,
                                          clippedArea.y, clippedArea.width, clippedArea.height, 0);
            }
        }

        setLevelInfo(context, target, level, 1, levelInfo);
    }

    return angle::Result::Continue;
}

angle::Result TextureGL::copySubImage(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      const gl::Offset &destOffset,
                                      const gl::Rectangle &sourceArea,
                                      gl::Framebuffer *source)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    gl::TextureTarget target                 = index.getTarget();
    size_t level                             = static_cast<size_t>(index.getLevelIndex());
    const FramebufferGL *sourceFramebufferGL = GetImplAs<FramebufferGL>(source);

    // Clip source area to framebuffer.
    const gl::Extents fbSize = sourceFramebufferGL->getState().getReadAttachment()->getSize();
    gl::Rectangle clippedArea;
    if (!ClipRectangle(sourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height), &clippedArea))
    {
        // nothing to do
        return angle::Result::Continue;
    }
    gl::Offset clippedOffset(destOffset.x + clippedArea.x - sourceArea.x,
                             destOffset.y + clippedArea.y - sourceArea.y, destOffset.z);

    stateManager->bindTexture(getType(), mTextureID);
    stateManager->bindFramebuffer(GL_READ_FRAMEBUFFER, sourceFramebufferGL->getFramebufferID());

    const LevelInfoGL &levelInfo = getLevelInfo(target, level);
    if (levelInfo.lumaWorkaround.enabled)
    {
        BlitGL *blitter = GetBlitGL(context);
        ANGLE_TRY(blitter->copySubImageToLUMAWorkaroundTexture(
            context, mTextureID, getType(), target, levelInfo.sourceFormat, level, clippedOffset,
            clippedArea, source));
    }
    else
    {
        if (nativegl::UseTexImage2D(getType()))
        {
            ASSERT(clippedOffset.z == 0);
            functions->copyTexSubImage2D(ToGLenum(target), static_cast<GLint>(level),
                                         clippedOffset.x, clippedOffset.y, clippedArea.x,
                                         clippedArea.y, clippedArea.width, clippedArea.height);
        }
        else
        {
            ASSERT(nativegl::UseTexImage3D(getType()));
            functions->copyTexSubImage3D(ToGLenum(target), static_cast<GLint>(level),
                                         clippedOffset.x, clippedOffset.y, clippedOffset.z,
                                         clippedArea.x, clippedArea.y, clippedArea.width,
                                         clippedArea.height);
        }
    }

    return angle::Result::Continue;
}

angle::Result TextureGL::copyTexture(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     GLenum internalFormat,
                                     GLenum type,
                                     size_t sourceLevel,
                                     bool unpackFlipY,
                                     bool unpackPremultiplyAlpha,
                                     bool unpackUnmultiplyAlpha,
                                     const gl::Texture *source)
{
    gl::TextureTarget target  = index.getTarget();
    size_t level              = static_cast<size_t>(index.getLevelIndex());
    const TextureGL *sourceGL = GetImplAs<TextureGL>(source);
    const gl::ImageDesc &sourceImageDesc =
        sourceGL->mState.getImageDesc(NonCubeTextureTypeToTarget(source->getType()), sourceLevel);
    gl::Rectangle sourceArea(0, 0, sourceImageDesc.size.width, sourceImageDesc.size.height);

    reserveTexImageToBeFilled(context, target, level, internalFormat, sourceImageDesc.size,
                              gl::GetUnsizedFormat(internalFormat), type);

    const gl::InternalFormat &destFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    return copySubTextureHelper(context, target, level, gl::Offset(0, 0, 0), sourceLevel,
                                sourceArea, destFormatInfo, unpackFlipY, unpackPremultiplyAlpha,
                                unpackUnmultiplyAlpha, source);
}

angle::Result TextureGL::copySubTexture(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const gl::Offset &destOffset,
                                        size_t sourceLevel,
                                        const gl::Box &sourceBox,
                                        bool unpackFlipY,
                                        bool unpackPremultiplyAlpha,
                                        bool unpackUnmultiplyAlpha,
                                        const gl::Texture *source)
{
    gl::TextureTarget target                 = index.getTarget();
    size_t level                             = static_cast<size_t>(index.getLevelIndex());
    const gl::InternalFormat &destFormatInfo = *mState.getImageDesc(target, level).format.info;
    return copySubTextureHelper(context, target, level, destOffset, sourceLevel, sourceBox.toRect(),
                                destFormatInfo, unpackFlipY, unpackPremultiplyAlpha,
                                unpackUnmultiplyAlpha, source);
}

angle::Result TextureGL::copySubTextureHelper(const gl::Context *context,
                                              gl::TextureTarget target,
                                              size_t level,
                                              const gl::Offset &destOffset,
                                              size_t sourceLevel,
                                              const gl::Rectangle &sourceArea,
                                              const gl::InternalFormat &destFormat,
                                              bool unpackFlipY,
                                              bool unpackPremultiplyAlpha,
                                              bool unpackUnmultiplyAlpha,
                                              const gl::Texture *source)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    BlitGL *blitter              = GetBlitGL(context);

    TextureGL *sourceGL = GetImplAs<TextureGL>(source);
    const gl::ImageDesc &sourceImageDesc =
        sourceGL->mState.getImageDesc(NonCubeTextureTypeToTarget(source->getType()), sourceLevel);

    // Check is this is a simple copySubTexture that can be done with a copyTexSubImage
    ASSERT(sourceGL->getType() == gl::TextureType::_2D);
    const LevelInfoGL &sourceLevelInfo =
        sourceGL->getLevelInfo(NonCubeTextureTypeToTarget(source->getType()), sourceLevel);
    bool needsLumaWorkaround = sourceLevelInfo.lumaWorkaround.enabled;

    GLenum sourceFormat = sourceImageDesc.format.info->format;
    bool sourceFormatContainSupersetOfDestFormat =
        (sourceFormat == destFormat.format && sourceFormat != GL_BGRA_EXT) ||
        (sourceFormat == GL_RGBA && destFormat.format == GL_RGB);

    GLenum sourceComponentType = sourceImageDesc.format.info->componentType;
    GLenum destComponentType   = destFormat.componentType;
    bool destSRGB              = destFormat.colorEncoding == GL_SRGB;
    if (!unpackFlipY && unpackPremultiplyAlpha == unpackUnmultiplyAlpha && !needsLumaWorkaround &&
        sourceFormatContainSupersetOfDestFormat && sourceComponentType == destComponentType &&
        !destSRGB)
    {
        bool copySucceeded = false;
        ANGLE_TRY(blitter->copyTexSubImage(sourceGL, sourceLevel, this, target, level, sourceArea,
                                           destOffset, &copySucceeded));
        if (copySucceeded)
        {
            return angle::Result::Continue;
        }
    }

    // Check if the destination is renderable and copy on the GPU
    const LevelInfoGL &destLevelInfo = getLevelInfo(target, level);
    if (!destSRGB &&
        nativegl::SupportsNativeRendering(functions, getType(), destLevelInfo.nativeInternalFormat))
    {
        bool copySucceeded = false;
        ANGLE_TRY(blitter->copySubTexture(
            context, sourceGL, sourceLevel, sourceComponentType, this, target, level,
            destComponentType, sourceImageDesc.size, sourceArea, destOffset, needsLumaWorkaround,
            sourceLevelInfo.sourceFormat, unpackFlipY, unpackPremultiplyAlpha,
            unpackUnmultiplyAlpha, &copySucceeded));
        if (copySucceeded)
        {
            return angle::Result::Continue;
        }
    }

    // Fall back to CPU-readback
    return blitter->copySubTextureCPUReadback(context, sourceGL, sourceLevel, sourceComponentType,
                                              this, target, level, destFormat.format,
                                              destFormat.type, sourceArea, destOffset, unpackFlipY,
                                              unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

angle::Result TextureGL::setStorage(const gl::Context *context,
                                    gl::TextureType type,
                                    size_t levels,
                                    GLenum internalFormat,
                                    const gl::Extents &size)
{
    ContextGL *contextGL             = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    nativegl::TexStorageFormat texStorageFormat =
        nativegl::GetTexStorageFormat(functions, workarounds, internalFormat);

    stateManager->bindTexture(getType(), mTextureID);
    if (nativegl::UseTexImage2D(getType()))
    {
        ASSERT(size.depth == 1);
        if (functions->texStorage2D)
        {
            functions->texStorage2D(ToGLenum(type), static_cast<GLsizei>(levels),
                                    texStorageFormat.internalFormat, size.width, size.height);
        }
        else
        {
            // Make sure no pixel unpack buffer is bound
            stateManager->bindBuffer(gl::BufferBinding::PixelUnpack, 0);

            const gl::InternalFormat &internalFormatInfo =
                gl::GetSizedInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.sized);

            for (size_t level = 0; level < levels; level++)
            {
                gl::Extents levelSize(std::max(size.width >> level, 1),
                                      std::max(size.height >> level, 1), 1);

                if (getType() == gl::TextureType::_2D || getType() == gl::TextureType::Rectangle)
                {
                    if (internalFormatInfo.compressed)
                    {
                        nativegl::CompressedTexSubImageFormat compressedTexImageFormat =
                            nativegl::GetCompressedSubTexImageFormat(functions, workarounds,
                                                                     internalFormat);

                        GLuint dataSize = 0;
                        ANGLE_CHECK_GL_MATH(
                            contextGL,
                            internalFormatInfo.computeCompressedImageSize(levelSize, &dataSize));
                        functions->compressedTexImage2D(ToGLenum(type), static_cast<GLint>(level),
                                                        compressedTexImageFormat.format,
                                                        levelSize.width, levelSize.height, 0,
                                                        static_cast<GLsizei>(dataSize), nullptr);
                    }
                    else
                    {
                        nativegl::TexImageFormat texImageFormat = nativegl::GetTexImageFormat(
                            functions, workarounds, internalFormat, internalFormatInfo.format,
                            internalFormatInfo.type);

                        functions->texImage2D(ToGLenum(type), static_cast<GLint>(level),
                                              texImageFormat.internalFormat, levelSize.width,
                                              levelSize.height, 0, texImageFormat.format,
                                              texImageFormat.type, nullptr);
                    }
                }
                else
                {
                    ASSERT(getType() == gl::TextureType::CubeMap);
                    for (gl::TextureTarget face : gl::AllCubeFaceTextureTargets())
                    {
                        if (internalFormatInfo.compressed)
                        {
                            nativegl::CompressedTexSubImageFormat compressedTexImageFormat =
                                nativegl::GetCompressedSubTexImageFormat(functions, workarounds,
                                                                         internalFormat);

                            GLuint dataSize = 0;
                            ANGLE_CHECK_GL_MATH(contextGL,
                                                internalFormatInfo.computeCompressedImageSize(
                                                    levelSize, &dataSize));
                            functions->compressedTexImage2D(
                                ToGLenum(face), static_cast<GLint>(level),
                                compressedTexImageFormat.format, levelSize.width, levelSize.height,
                                0, static_cast<GLsizei>(dataSize), nullptr);
                        }
                        else
                        {
                            nativegl::TexImageFormat texImageFormat = nativegl::GetTexImageFormat(
                                functions, workarounds, internalFormat, internalFormatInfo.format,
                                internalFormatInfo.type);

                            functions->texImage2D(ToGLenum(face), static_cast<GLint>(level),
                                                  texImageFormat.internalFormat, levelSize.width,
                                                  levelSize.height, 0, texImageFormat.format,
                                                  texImageFormat.type, nullptr);
                        }
                    }
                }
            }
        }
    }
    else
    {
        ASSERT(nativegl::UseTexImage3D(getType()));
        if (functions->texStorage3D)
        {
            functions->texStorage3D(ToGLenum(type), static_cast<GLsizei>(levels),
                                    texStorageFormat.internalFormat, size.width, size.height,
                                    size.depth);
        }
        else
        {
            // Make sure no pixel unpack buffer is bound
            stateManager->bindBuffer(gl::BufferBinding::PixelUnpack, 0);

            const gl::InternalFormat &internalFormatInfo =
                gl::GetSizedInternalFormatInfo(internalFormat);

            // Internal format must be sized
            ASSERT(internalFormatInfo.sized);

            for (GLsizei i = 0; i < static_cast<GLsizei>(levels); i++)
            {
                gl::Extents levelSize(
                    std::max(size.width >> i, 1), std::max(size.height >> i, 1),
                    getType() == gl::TextureType::_3D ? std::max(size.depth >> i, 1) : size.depth);

                if (internalFormatInfo.compressed)
                {
                    nativegl::CompressedTexSubImageFormat compressedTexImageFormat =
                        nativegl::GetCompressedSubTexImageFormat(functions, workarounds,
                                                                 internalFormat);

                    GLuint dataSize = 0;
                    ANGLE_CHECK_GL_MATH(contextGL, internalFormatInfo.computeCompressedImageSize(
                                                       levelSize, &dataSize));
                    functions->compressedTexImage3D(
                        ToGLenum(type), i, compressedTexImageFormat.format, levelSize.width,
                        levelSize.height, levelSize.depth, 0, static_cast<GLsizei>(dataSize),
                        nullptr);
                }
                else
                {
                    nativegl::TexImageFormat texImageFormat = nativegl::GetTexImageFormat(
                        functions, workarounds, internalFormat, internalFormatInfo.format,
                        internalFormatInfo.type);

                    functions->texImage3D(ToGLenum(type), i, texImageFormat.internalFormat,
                                          levelSize.width, levelSize.height, levelSize.depth, 0,
                                          texImageFormat.format, texImageFormat.type, nullptr);
                }
            }
        }
    }

    setLevelInfo(context, type, 0, levels,
                 GetLevelInfo(internalFormat, texStorageFormat.internalFormat));

    return angle::Result::Continue;
}

angle::Result TextureGL::setStorageMultisample(const gl::Context *context,
                                               gl::TextureType type,
                                               GLsizei samples,
                                               GLint internalformat,
                                               const gl::Extents &size,
                                               bool fixedSampleLocations)
{
    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    nativegl::TexStorageFormat texStorageFormat =
        nativegl::GetTexStorageFormat(functions, workarounds, internalformat);

    stateManager->bindTexture(getType(), mTextureID);

    if (nativegl::UseTexImage2D(getType()))
    {
        ASSERT(size.depth == 1);
        if (functions->texStorage2DMultisample)
        {
            functions->texStorage2DMultisample(
                ToGLenum(type), samples, texStorageFormat.internalFormat, size.width, size.height,
                gl::ConvertToGLBoolean(fixedSampleLocations));
        }
        else
        {
            // texImage2DMultisample is similar to texStorage2DMultisample of es 3.1 core feature,
            // On macos and some old drivers which doesn't support OpenGL ES 3.1, the function can
            // be supported by ARB_texture_multisample or OpenGL 3.2 core feature.
            functions->texImage2DMultisample(
                ToGLenum(type), samples, texStorageFormat.internalFormat, size.width, size.height,
                gl::ConvertToGLBoolean(fixedSampleLocations));
        }
    }
    else
    {
        ASSERT(nativegl::UseTexImage3D(getType()));
        functions->texStorage3DMultisample(ToGLenum(type), samples, texStorageFormat.internalFormat,
                                           size.width, size.height, size.depth,
                                           gl::ConvertToGLBoolean(fixedSampleLocations));
    }

    setLevelInfo(context, type, 0, 1,
                 GetLevelInfo(internalformat, texStorageFormat.internalFormat));

    return angle::Result::Continue;
}

angle::Result TextureGL::setStorageExternalMemory(const gl::Context *context,
                                                  gl::TextureType type,
                                                  size_t levels,
                                                  GLenum internalFormat,
                                                  const gl::Extents &size,
                                                  gl::MemoryObject *memoryObject,
                                                  GLuint64 offset)
{
    ANGLE_GL_UNREACHABLE(GetImplAs<ContextGL>(context));
    return angle::Result::Stop;
}

angle::Result TextureGL::setImageExternal(const gl::Context *context,
                                          gl::TextureType type,
                                          egl::Stream *stream,
                                          const egl::Stream::GLTextureDescription &desc)
{
    ANGLE_GL_UNREACHABLE(GetImplAs<ContextGL>(context));
    return angle::Result::Stop;
}

angle::Result TextureGL::generateMipmap(const gl::Context *context)
{
    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindTexture(getType(), mTextureID);
    functions->generateMipmap(ToGLenum(getType()));

    const GLuint effectiveBaseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel           = mState.getMipmapMaxLevel();

    setLevelInfo(context, getType(), effectiveBaseLevel, maxLevel - effectiveBaseLevel,
                 getBaseLevelInfo());

    return angle::Result::Continue;
}

angle::Result TextureGL::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    ASSERT(getType() == gl::TextureType::_2D || getType() == gl::TextureType::Rectangle);

    StateManagerGL *stateManager = GetStateManagerGL(context);

    // Make sure this texture is bound
    stateManager->bindTexture(getType(), mTextureID);

    setLevelInfo(context, getType(), 0, 1, LevelInfoGL());
    return angle::Result::Continue;
}

angle::Result TextureGL::releaseTexImage(const gl::Context *context)
{
    // Not all Surface implementations reset the size of mip 0 when releasing, do it manually
    ASSERT(getType() == gl::TextureType::_2D || getType() == gl::TextureType::Rectangle);

    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindTexture(getType(), mTextureID);
    ASSERT(nativegl::UseTexImage2D(getType()));
    functions->texImage2D(ToGLenum(getType()), 0, GL_RGBA, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          nullptr);
    return angle::Result::Continue;
}

angle::Result TextureGL::setEGLImageTarget(const gl::Context *context,
                                           gl::TextureType type,
                                           egl::Image *image)
{
    ImageGL *imageGL = GetImplAs<ImageGL>(image);

    GLenum imageNativeInternalFormat = GL_NONE;
    ANGLE_TRY(imageGL->setTexture2D(context, type, this, &imageNativeInternalFormat));

    setLevelInfo(context, type, 0, 1,
                 GetLevelInfo(image->getFormat().info->internalFormat, imageNativeInternalFormat));

    return angle::Result::Continue;
}

angle::Result TextureGL::syncState(const gl::Context *context,
                                   const gl::Texture::DirtyBits &dirtyBits)
{
    if (dirtyBits.none() && mLocalDirtyBits.none())
    {
        return angle::Result::Continue;
    }

    const FunctionsGL *functions = GetFunctionsGL(context);
    StateManagerGL *stateManager = GetStateManagerGL(context);

    stateManager->bindTexture(getType(), mTextureID);

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
                mAppliedSampler.setMinFilter(mState.getSamplerState().getMinFilter());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_MIN_FILTER,
                                         mAppliedSampler.getMinFilter());
                break;
            case gl::Texture::DIRTY_BIT_MAG_FILTER:
                mAppliedSampler.setMagFilter(mState.getSamplerState().getMagFilter());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_MAG_FILTER,
                                         mAppliedSampler.getMagFilter());
                break;
            case gl::Texture::DIRTY_BIT_WRAP_S:
                mAppliedSampler.setWrapS(mState.getSamplerState().getWrapS());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_WRAP_S,
                                         mAppliedSampler.getWrapS());
                break;
            case gl::Texture::DIRTY_BIT_WRAP_T:
                mAppliedSampler.setWrapT(mState.getSamplerState().getWrapT());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_WRAP_T,
                                         mAppliedSampler.getWrapT());
                break;
            case gl::Texture::DIRTY_BIT_WRAP_R:
                mAppliedSampler.setWrapR(mState.getSamplerState().getWrapR());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_WRAP_R,
                                         mAppliedSampler.getWrapR());
                break;
            case gl::Texture::DIRTY_BIT_MAX_ANISOTROPY:
                mAppliedSampler.setMaxAnisotropy(mState.getSamplerState().getMaxAnisotropy());
                functions->texParameterf(ToGLenum(getType()), GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                         mAppliedSampler.getMaxAnisotropy());
                break;
            case gl::Texture::DIRTY_BIT_MIN_LOD:
                mAppliedSampler.setMinLod(mState.getSamplerState().getMinLod());
                functions->texParameterf(ToGLenum(getType()), GL_TEXTURE_MIN_LOD,
                                         mAppliedSampler.getMinLod());
                break;
            case gl::Texture::DIRTY_BIT_MAX_LOD:
                mAppliedSampler.setMaxLod(mState.getSamplerState().getMaxLod());
                functions->texParameterf(ToGLenum(getType()), GL_TEXTURE_MAX_LOD,
                                         mAppliedSampler.getMaxLod());
                break;
            case gl::Texture::DIRTY_BIT_COMPARE_MODE:
                mAppliedSampler.setCompareMode(mState.getSamplerState().getCompareMode());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_COMPARE_MODE,
                                         mAppliedSampler.getCompareMode());
                break;
            case gl::Texture::DIRTY_BIT_COMPARE_FUNC:
                mAppliedSampler.setCompareFunc(mState.getSamplerState().getCompareFunc());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_COMPARE_FUNC,
                                         mAppliedSampler.getCompareFunc());
                break;
            case gl::Texture::DIRTY_BIT_SRGB_DECODE:
                mAppliedSampler.setSRGBDecode(mState.getSamplerState().getSRGBDecode());
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_SRGB_DECODE_EXT,
                                         mAppliedSampler.getSRGBDecode());
                break;
            case gl::Texture::DIRTY_BIT_BORDER_COLOR:
            {
                const angle::ColorGeneric &borderColor(mState.getSamplerState().getBorderColor());
                mAppliedSampler.setBorderColor(borderColor);
                switch (borderColor.type)
                {
                    case angle::ColorGeneric::Type::Float:
                        functions->texParameterfv(ToGLenum(getType()), GL_TEXTURE_BORDER_COLOR,
                                                  &borderColor.colorF.red);
                        break;
                    case angle::ColorGeneric::Type::Int:
                        functions->texParameterIiv(ToGLenum(getType()), GL_TEXTURE_BORDER_COLOR,
                                                   &borderColor.colorI.red);
                        break;
                    case angle::ColorGeneric::Type::UInt:
                        functions->texParameterIuiv(ToGLenum(getType()), GL_TEXTURE_BORDER_COLOR,
                                                    &borderColor.colorUI.red);
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
                break;
            }

            // Texture state
            case gl::Texture::DIRTY_BIT_SWIZZLE_RED:
                syncTextureStateSwizzle(functions, GL_TEXTURE_SWIZZLE_R,
                                        mState.getSwizzleState().swizzleRed,
                                        &mAppliedSwizzle.swizzleRed);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_GREEN:
                syncTextureStateSwizzle(functions, GL_TEXTURE_SWIZZLE_G,
                                        mState.getSwizzleState().swizzleGreen,
                                        &mAppliedSwizzle.swizzleGreen);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_BLUE:
                syncTextureStateSwizzle(functions, GL_TEXTURE_SWIZZLE_B,
                                        mState.getSwizzleState().swizzleBlue,
                                        &mAppliedSwizzle.swizzleBlue);
                break;
            case gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA:
                syncTextureStateSwizzle(functions, GL_TEXTURE_SWIZZLE_A,
                                        mState.getSwizzleState().swizzleAlpha,
                                        &mAppliedSwizzle.swizzleAlpha);
                break;
            case gl::Texture::DIRTY_BIT_BASE_LEVEL:
                mAppliedBaseLevel = mState.getEffectiveBaseLevel();
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_BASE_LEVEL,
                                         mAppliedBaseLevel);
                break;
            case gl::Texture::DIRTY_BIT_MAX_LEVEL:
                mAppliedMaxLevel = mState.getEffectiveMaxLevel();
                functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_MAX_LEVEL,
                                         mAppliedMaxLevel);
                break;
            case gl::Texture::DIRTY_BIT_DEPTH_STENCIL_TEXTURE_MODE:
            {
                GLenum mDepthStencilTextureMode = mState.getDepthStencilTextureMode();
                functions->texParameteri(ToGLenum(getType()), GL_DEPTH_STENCIL_TEXTURE_MODE,
                                         mDepthStencilTextureMode);
                break;
            }
            case gl::Texture::DIRTY_BIT_USAGE:
                break;
            case gl::Texture::DIRTY_BIT_LABEL:
                break;

            case gl::Texture::DIRTY_BIT_IMPLEMENTATION:
                // This special dirty bit is used to signal the front-end that the implementation
                // has local dirty bits. The real dirty bits are in mLocalDirty bits.
                break;

            default:
                UNREACHABLE();
        }
    }

    mLocalDirtyBits.reset();
    return angle::Result::Continue;
}

bool TextureGL::hasAnyDirtyBit() const
{
    return mLocalDirtyBits.any();
}

angle::Result TextureGL::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    if (baseLevel != mAppliedBaseLevel)
    {
        const FunctionsGL *functions = GetFunctionsGL(context);
        StateManagerGL *stateManager = GetStateManagerGL(context);

        mAppliedBaseLevel = baseLevel;
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_BASE_LEVEL);

        // Signal to the GL layer that the Impl has dirty bits.
        onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);

        stateManager->bindTexture(getType(), mTextureID);
        functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_BASE_LEVEL, baseLevel);
    }
    return angle::Result::Continue;
}

void TextureGL::setMinFilter(const gl::Context *context, GLenum filter)
{
    if (filter != mAppliedSampler.getMinFilter())
    {
        const FunctionsGL *functions = GetFunctionsGL(context);
        StateManagerGL *stateManager = GetStateManagerGL(context);

        mAppliedSampler.setMinFilter(filter);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_MIN_FILTER);

        // Signal to the GL layer that the Impl has dirty bits.
        onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);

        stateManager->bindTexture(getType(), mTextureID);
        functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_MIN_FILTER, filter);
    }
}
void TextureGL::setMagFilter(const gl::Context *context, GLenum filter)
{
    if (filter != mAppliedSampler.getMagFilter())
    {
        const FunctionsGL *functions = GetFunctionsGL(context);
        StateManagerGL *stateManager = GetStateManagerGL(context);

        mAppliedSampler.setMagFilter(filter);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_MAG_FILTER);

        // Signal to the GL layer that the Impl has dirty bits.
        onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);

        stateManager->bindTexture(getType(), mTextureID);
        functions->texParameteri(ToGLenum(getType()), GL_TEXTURE_MAG_FILTER, filter);
    }
}

void TextureGL::setSwizzle(const gl::Context *context, GLint swizzle[4])
{
    gl::SwizzleState resultingSwizzle =
        gl::SwizzleState(swizzle[0], swizzle[1], swizzle[2], swizzle[3]);

    if (resultingSwizzle != mAppliedSwizzle)
    {
        const FunctionsGL *functions = GetFunctionsGL(context);
        StateManagerGL *stateManager = GetStateManagerGL(context);

        mAppliedSwizzle = resultingSwizzle;
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_RED);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_GREEN);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_BLUE);
        mLocalDirtyBits.set(gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA);

        // Signal to the GL layer that the Impl has dirty bits.
        onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);

        stateManager->bindTexture(getType(), mTextureID);
        functions->texParameteriv(ToGLenum(getType()), GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }
}

GLenum TextureGL::getNativeInternalFormat(const gl::ImageIndex &index) const
{
    return getLevelInfo(index.getTarget(), index.getLevelIndex()).nativeInternalFormat;
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
                        // Texture is backed by a RED or RG texture, point all color channels at the
                        // red channel.
                        ASSERT(levelInfo.lumaWorkaround.workaroundFormat == GL_RED ||
                               levelInfo.lumaWorkaround.workaroundFormat == GL_RG);
                        resultSwizzle = GL_RED;
                    }
                    else
                    {
                        ASSERT(levelInfo.sourceFormat == GL_ALPHA);
                        // Color channels are not supposed to exist, make them always sample 0.
                        resultSwizzle = GL_ZERO;
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
                    else
                    {
                        ASSERT(levelInfo.sourceFormat == GL_LUMINANCE_ALPHA);
                        // Texture is backed by an RG texture, point the alpha channel at the green
                        // channel.
                        ASSERT(levelInfo.lumaWorkaround.workaroundFormat == GL_RG);
                        resultSwizzle = GL_GREEN;
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
        else
        {
            ASSERT(levelInfo.depthStencilWorkaround);
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
    }

    *outValue = resultSwizzle;
    functions->texParameteri(ToGLenum(getType()), name, resultSwizzle);
}

void TextureGL::setLevelInfo(const gl::Context *context,
                             gl::TextureTarget target,
                             size_t level,
                             size_t levelCount,
                             const LevelInfoGL &levelInfo)
{
    ASSERT(levelCount > 0);

    bool updateWorkarounds = levelInfo.depthStencilWorkaround || levelInfo.lumaWorkaround.enabled;

    for (size_t i = level; i < level + levelCount; i++)
    {
        size_t index = GetLevelInfoIndex(target, i);
        ASSERT(index < mLevelInfo.size());
        auto &curLevelInfo = mLevelInfo[index];

        updateWorkarounds |= curLevelInfo.depthStencilWorkaround;
        updateWorkarounds |= curLevelInfo.lumaWorkaround.enabled;

        curLevelInfo = levelInfo;
    }

    if (updateWorkarounds)
    {
        mLocalDirtyBits |= GetLevelWorkaroundDirtyBits();
        onStateChange(context, angle::SubjectMessage::DEPENDENT_DIRTY_BITS);
    }
}

void TextureGL::setLevelInfo(const gl::Context *context,
                             gl::TextureType type,
                             size_t level,
                             size_t levelCount,
                             const LevelInfoGL &levelInfo)
{
    if (type == gl::TextureType::CubeMap)
    {
        for (gl::TextureTarget target : gl::AllCubeFaceTextureTargets())
        {
            setLevelInfo(context, target, level, levelCount, levelInfo);
        }
    }
    else
    {
        setLevelInfo(context, NonCubeTextureTypeToTarget(type), level, levelCount, levelInfo);
    }
}

const LevelInfoGL &TextureGL::getLevelInfo(gl::TextureTarget target, size_t level) const
{
    return mLevelInfo[GetLevelInfoIndex(target, level)];
}

const LevelInfoGL &TextureGL::getBaseLevelInfo() const
{
    GLint effectiveBaseLevel = mState.getEffectiveBaseLevel();
    gl::TextureTarget target = getType() == gl::TextureType::CubeMap
                                   ? gl::kCubeMapTextureTargetMin
                                   : gl::NonCubeTextureTypeToTarget(getType());
    return getLevelInfo(target, effectiveBaseLevel);
}

gl::TextureType TextureGL::getType() const
{
    return mState.mType;
}

angle::Result TextureGL::initializeContents(const gl::Context *context,
                                            const gl::ImageIndex &imageIndex)
{
    ContextGL *contextGL             = GetImplAs<ContextGL>(context);
    const FunctionsGL *functions     = GetFunctionsGL(context);
    StateManagerGL *stateManager     = GetStateManagerGL(context);
    const WorkaroundsGL &workarounds = GetWorkaroundsGL(context);

    GLenum nativeInternalFormat =
        getLevelInfo(imageIndex.getTarget(), imageIndex.getLevelIndex()).nativeInternalFormat;
    if (nativegl::SupportsNativeRendering(functions, mState.getType(), nativeInternalFormat))
    {
        BlitGL *blitter = GetBlitGL(context);

        int levelDepth = mState.getImageDesc(imageIndex).size.depth;

        bool clearSucceeded = false;
        ANGLE_TRY(blitter->clearRenderableTexture(this, nativeInternalFormat, levelDepth,
                                                  imageIndex, &clearSucceeded));
        if (clearSucceeded)
        {
            return angle::Result::Continue;
        }
    }

    // Either the texture is not renderable or was incomplete when clearing, fall back to a data
    // upload
    const gl::ImageDesc &desc                    = mState.getImageDesc(imageIndex);
    const gl::InternalFormat &internalFormatInfo = *desc.format.info;

    gl::PixelUnpackState unpackState;
    unpackState.alignment = 1;
    stateManager->setPixelUnpackState(unpackState);

    if (internalFormatInfo.compressed)
    {
        nativegl::CompressedTexSubImageFormat nativeSubImageFormat =
            nativegl::GetCompressedSubTexImageFormat(functions, workarounds,
                                                     internalFormatInfo.internalFormat);

        GLuint imageSize = 0;
        ANGLE_CHECK_GL_MATH(contextGL,
                            internalFormatInfo.computeCompressedImageSize(desc.size, &imageSize));

        angle::MemoryBuffer *zero;
        ANGLE_CHECK_GL_ALLOC(contextGL, context->getZeroFilledBuffer(imageSize, &zero));

        // WebGL spec requires that zero data is uploaded to compressed textures even if it might
        // not result in zero color data.
        if (nativegl::UseTexImage2D(getType()))
        {
            functions->compressedTexSubImage2D(
                ToGLenum(imageIndex.getTarget()), imageIndex.getLevelIndex(), 0, 0, desc.size.width,
                desc.size.height, nativeSubImageFormat.format, imageSize, zero->data());
        }
        else
        {
            ASSERT(nativegl::UseTexImage3D(getType()));
            functions->compressedTexSubImage3D(
                ToGLenum(imageIndex.getTarget()), imageIndex.getLevelIndex(), 0, 0, 0,
                desc.size.width, desc.size.height, desc.size.depth, nativeSubImageFormat.format,
                imageSize, zero->data());
        }
    }
    else
    {
        nativegl::TexSubImageFormat nativeSubImageFormat = nativegl::GetTexSubImageFormat(
            functions, workarounds, internalFormatInfo.format, internalFormatInfo.type);

        GLuint imageSize = 0;
        ANGLE_CHECK_GL_MATH(contextGL, internalFormatInfo.computePackUnpackEndByte(
                                           nativeSubImageFormat.type, desc.size, unpackState,
                                           nativegl::UseTexImage3D(getType()), &imageSize));

        angle::MemoryBuffer *zero;
        ANGLE_CHECK_GL_ALLOC(contextGL, context->getZeroFilledBuffer(imageSize, &zero));

        if (nativegl::UseTexImage2D(getType()))
        {
            functions->texSubImage2D(ToGLenum(imageIndex.getTarget()), imageIndex.getLevelIndex(),
                                     0, 0, desc.size.width, desc.size.height,
                                     nativeSubImageFormat.format, nativeSubImageFormat.type,
                                     zero->data());
        }
        else
        {
            ASSERT(nativegl::UseTexImage3D(getType()));
            functions->texSubImage3D(ToGLenum(imageIndex.getTarget()), imageIndex.getLevelIndex(),
                                     0, 0, 0, desc.size.width, desc.size.height, desc.size.depth,
                                     nativeSubImageFormat.format, nativeSubImageFormat.type,
                                     zero->data());
        }
    }

    return angle::Result::Continue;
}

}  // namespace rx
