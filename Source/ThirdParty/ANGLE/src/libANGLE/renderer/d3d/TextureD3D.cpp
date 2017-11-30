//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureD3D.cpp: Implementations of the Texture interfaces shared betweeen the D3D backends.

#include "libANGLE/renderer/d3d/TextureD3D.h"

#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/Image.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/EGLImageD3D.h"
#include "libANGLE/renderer/d3d/ImageD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/SurfaceD3D.h"
#include "libANGLE/renderer/d3d/TextureStorage.h"

namespace rx
{

namespace
{

gl::Error GetUnpackPointer(const gl::Context *context,
                           const gl::PixelUnpackState &unpack,
                           gl::Buffer *unpackBuffer,
                           const uint8_t *pixels,
                           ptrdiff_t layerOffset,
                           const uint8_t **pointerOut)
{
    if (unpackBuffer)
    {
        // Do a CPU readback here, if we have an unpack buffer bound and the fast GPU path is not supported
        ptrdiff_t offset = reinterpret_cast<ptrdiff_t>(pixels);

        // TODO: this is the only place outside of renderer that asks for a buffers raw data.
        // This functionality should be moved into renderer and the getData method of BufferImpl removed.
        BufferD3D *bufferD3D = GetImplAs<BufferD3D>(unpackBuffer);
        ASSERT(bufferD3D);
        const uint8_t *bufferData = nullptr;
        ANGLE_TRY(bufferD3D->getData(context, &bufferData));
        *pointerOut = bufferData + offset;
    }
    else
    {
        *pointerOut = pixels;
    }

    // Offset the pointer for 2D array layer (if it's valid)
    if (*pointerOut != nullptr)
    {
        *pointerOut += layerOffset;
    }

    return gl::NoError();
}

bool IsRenderTargetUsage(GLenum usage)
{
    return (usage == GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
}

}

TextureD3D::TextureD3D(const gl::TextureState &state, RendererD3D *renderer)
    : TextureImpl(state),
      mRenderer(renderer),
      mDirtyImages(true),
      mImmutable(false),
      mTexStorage(nullptr),
      mBaseLevel(0)
{
}

TextureD3D::~TextureD3D()
{
    ASSERT(!mTexStorage);
}

gl::Error TextureD3D::getNativeTexture(const gl::Context *context, TextureStorage **outStorage)
{
    // ensure the underlying texture is created
    ANGLE_TRY(initializeStorage(context, false));

    if (mTexStorage)
    {
        ANGLE_TRY(updateStorage(context));
    }

    ASSERT(outStorage);

    *outStorage = mTexStorage;
    return gl::NoError();
}

gl::Error TextureD3D::getImageAndSyncFromStorage(const gl::Context *context,
                                                 const gl::ImageIndex &index,
                                                 ImageD3D **outImage)
{
    ImageD3D *image = getImage(index);
    if (mTexStorage && mTexStorage->isRenderTarget())
    {
        ANGLE_TRY(image->copyFromTexStorage(context, index, mTexStorage));
        mDirtyImages = true;
    }
    *outImage = image;
    return gl::NoError();
}

GLint TextureD3D::getLevelZeroWidth() const
{
    ASSERT(gl::CountLeadingZeros(static_cast<uint32_t>(getBaseLevelWidth())) > getBaseLevel());
    return getBaseLevelWidth() << mBaseLevel;
}

GLint TextureD3D::getLevelZeroHeight() const
{
    ASSERT(gl::CountLeadingZeros(static_cast<uint32_t>(getBaseLevelHeight())) > getBaseLevel());
    return getBaseLevelHeight() << mBaseLevel;
}

GLint TextureD3D::getLevelZeroDepth() const
{
    return getBaseLevelDepth();
}

GLint TextureD3D::getBaseLevelWidth() const
{
    const ImageD3D *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getWidth() : 0);
}

GLint TextureD3D::getBaseLevelHeight() const
{
    const ImageD3D *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getHeight() : 0);
}

GLint TextureD3D::getBaseLevelDepth() const
{
    const ImageD3D *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getDepth() : 0);
}

// Note: "base level image" is loosely defined to be any image from the base level,
// where in the base of 2D array textures and cube maps there are several. Don't use
// the base level image for anything except querying texture format and size.
GLenum TextureD3D::getBaseLevelInternalFormat() const
{
    const ImageD3D *baseImage = getBaseLevelImage();
    return (baseImage ? baseImage->getInternalFormat() : GL_NONE);
}

gl::Error TextureD3D::setStorage(const gl::Context *context,
                                 GLenum target,
                                 size_t levels,
                                 GLenum internalFormat,
                                 const gl::Extents &size)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D::setStorageMultisample(const gl::Context *context,
                                            GLenum target,
                                            GLsizei samples,
                                            GLint internalFormat,
                                            const gl::Extents &size,
                                            bool fixedSampleLocations)
{
    UNREACHABLE();
    return gl::InternalError();
}

bool TextureD3D::shouldUseSetData(const ImageD3D *image) const
{
    if (!mRenderer->getWorkarounds().setDataFasterThanImageUpload)
    {
        return false;
    }

    if (image->isDirty())
    {
        return false;
    }

    gl::InternalFormat internalFormat = gl::GetSizedInternalFormatInfo(image->getInternalFormat());

    // We can only handle full updates for depth-stencil textures, so to avoid complications
    // disable them entirely.
    if (internalFormat.depthBits > 0 || internalFormat.stencilBits > 0)
    {
        return false;
    }

    // TODO(jmadill): Handle compressed internal formats
    return (mTexStorage && !internalFormat.compressed);
}

gl::Error TextureD3D::setImageImpl(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   GLenum type,
                                   const gl::PixelUnpackState &unpack,
                                   const uint8_t *pixels,
                                   ptrdiff_t layerOffset)
{
    ImageD3D *image = getImage(index);
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    ASSERT(image);

    // No-op
    if (image->getWidth() == 0 || image->getHeight() == 0 || image->getDepth() == 0)
    {
        return gl::NoError();
    }

    // We no longer need the "GLenum format" parameter to TexImage to determine what data format "pixels" contains.
    // From our image internal format we know how many channels to expect, and "type" gives the format of pixel's components.
    const uint8_t *pixelData = nullptr;
    ANGLE_TRY(GetUnpackPointer(context, unpack, unpackBuffer, pixels, layerOffset, &pixelData));

    if (pixelData != nullptr)
    {
        if (shouldUseSetData(image))
        {
            ANGLE_TRY(
                mTexStorage->setData(context, index, image, nullptr, type, unpack, pixelData));
        }
        else
        {
            gl::Box fullImageArea(0, 0, 0, image->getWidth(), image->getHeight(), image->getDepth());
            ANGLE_TRY(
                image->loadData(context, fullImageArea, unpack, type, pixelData, index.is3D()));
        }

        mDirtyImages = true;
    }

    return gl::NoError();
}

gl::Error TextureD3D::subImage(const gl::Context *context,
                               const gl::ImageIndex &index,
                               const gl::Box &area,
                               GLenum format,
                               GLenum type,
                               const gl::PixelUnpackState &unpack,
                               const uint8_t *pixels,
                               ptrdiff_t layerOffset)
{
    // CPU readback & copy where direct GPU copy is not supported
    const uint8_t *pixelData = nullptr;
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    ANGLE_TRY(GetUnpackPointer(context, unpack, unpackBuffer, pixels, layerOffset, &pixelData));

    if (pixelData != nullptr)
    {
        ImageD3D *image = getImage(index);
        ASSERT(image);

        if (shouldUseSetData(image))
        {
            return mTexStorage->setData(context, index, image, &area, type, unpack, pixelData);
        }

        ANGLE_TRY(image->loadData(context, area, unpack, type, pixelData, index.is3D()));
        ANGLE_TRY(commitRegion(context, index, area));
        mDirtyImages = true;
    }

    return gl::NoError();
}

gl::Error TextureD3D::setCompressedImageImpl(const gl::Context *context,
                                             const gl::ImageIndex &index,
                                             const gl::PixelUnpackState &unpack,
                                             const uint8_t *pixels,
                                             ptrdiff_t layerOffset)
{
    ImageD3D *image = getImage(index);
    ASSERT(image);

    if (image->getWidth() == 0 || image->getHeight() == 0 || image->getDepth() == 0)
    {
        return gl::NoError();
    }

    // We no longer need the "GLenum format" parameter to TexImage to determine what data format "pixels" contains.
    // From our image internal format we know how many channels to expect, and "type" gives the format of pixel's components.
    const uint8_t *pixelData = nullptr;
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    ANGLE_TRY(GetUnpackPointer(context, unpack, unpackBuffer, pixels, layerOffset, &pixelData));

    if (pixelData != nullptr)
    {
        gl::Box fullImageArea(0, 0, 0, image->getWidth(), image->getHeight(), image->getDepth());
        ANGLE_TRY(image->loadCompressedData(context, fullImageArea, pixelData));

        mDirtyImages = true;
    }

    return gl::NoError();
}

gl::Error TextureD3D::subImageCompressed(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         const gl::Box &area,
                                         GLenum format,
                                         const gl::PixelUnpackState &unpack,
                                         const uint8_t *pixels,
                                         ptrdiff_t layerOffset)
{
    const uint8_t *pixelData = nullptr;
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    ANGLE_TRY(GetUnpackPointer(context, unpack, unpackBuffer, pixels, layerOffset, &pixelData));

    if (pixelData != nullptr)
    {
        ImageD3D *image = getImage(index);
        ASSERT(image);

        ANGLE_TRY(image->loadCompressedData(context, area, pixelData));

        mDirtyImages = true;
    }

    return gl::NoError();
}

bool TextureD3D::isFastUnpackable(const gl::Buffer *unpackBuffer, GLenum sizedInternalFormat)
{
    return unpackBuffer != nullptr &&
           mRenderer->supportsFastCopyBufferToTexture(sizedInternalFormat);
}

gl::Error TextureD3D::fastUnpackPixels(const gl::Context *context,
                                       const gl::PixelUnpackState &unpack,
                                       const uint8_t *pixels,
                                       const gl::Box &destArea,
                                       GLenum sizedInternalFormat,
                                       GLenum type,
                                       RenderTargetD3D *destRenderTarget)
{
    if (unpack.skipRows != 0 || unpack.skipPixels != 0 || unpack.imageHeight != 0 ||
        unpack.skipImages != 0)
    {
        // TODO(jmadill): additional unpack parameters
        UNIMPLEMENTED();
        return gl::InternalError() << "Unimplemented pixel store parameters in fastUnpackPixels";
    }

    // No-op
    if (destArea.width <= 0 && destArea.height <= 0 && destArea.depth <= 0)
    {
        return gl::NoError();
    }

    // In order to perform the fast copy through the shader, we must have the right format, and be able
    // to create a render target.
    ASSERT(mRenderer->supportsFastCopyBufferToTexture(sizedInternalFormat));

    uintptr_t offset = reinterpret_cast<uintptr_t>(pixels);

    ANGLE_TRY(mRenderer->fastCopyBufferToTexture(context, unpack, static_cast<unsigned int>(offset),
                                                 destRenderTarget, sizedInternalFormat, type,
                                                 destArea));

    return gl::NoError();
}

GLint TextureD3D::creationLevels(GLsizei width, GLsizei height, GLsizei depth) const
{
    if ((gl::isPow2(width) && gl::isPow2(height) && gl::isPow2(depth)) ||
        mRenderer->getNativeExtensions().textureNPOT)
    {
        // Maximum number of levels
        return gl::log2(std::max(std::max(width, height), depth)) + 1;
    }
    else
    {
        // OpenGL ES 2.0 without GL_OES_texture_npot does not permit NPOT mipmaps.
        return 1;
    }
}

TextureStorage *TextureD3D::getStorage()
{
    ASSERT(mTexStorage);
    return mTexStorage;
}

ImageD3D *TextureD3D::getBaseLevelImage() const
{
    if (mBaseLevel >= gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        return nullptr;
    }
    return getImage(getImageIndex(mBaseLevel, 0));
}

gl::Error TextureD3D::setImageExternal(const gl::Context *context,
                                       GLenum target,
                                       egl::Stream *stream,
                                       const egl::Stream::GLTextureDescription &desc)
{
    // Only external images can accept external textures
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D::generateMipmap(const gl::Context *context)
{
    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel = mState.getMipmapMaxLevel();
    ASSERT(maxLevel > baseLevel);  // Should be checked before calling this.

    if (mTexStorage && mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        // Switch to using the mipmapped texture.
        TextureStorage *textureStorage = nullptr;
        ANGLE_TRY(getNativeTexture(context, &textureStorage));
        ANGLE_TRY(textureStorage->useLevelZeroWorkaroundTexture(context, false));
    }

    // Set up proper mipmap chain in our Image array.
    ANGLE_TRY(initMipmapImages(context));

    if (mTexStorage && mTexStorage->supportsNativeMipmapFunction())
    {
        ANGLE_TRY(updateStorage(context));

        // Generate the mipmap chain using the ad-hoc DirectX function.
        ANGLE_TRY(mRenderer->generateMipmapUsingD3D(context, mTexStorage, mState));
    }
    else
    {
        // Generate the mipmap chain, one level at a time.
        ANGLE_TRY(generateMipmapUsingImages(context, maxLevel));
    }

    return gl::NoError();
}

gl::Error TextureD3D::generateMipmapUsingImages(const gl::Context *context, const GLuint maxLevel)
{
    // We know that all layers have the same dimension, for the texture to be complete
    GLint layerCount = static_cast<GLint>(getLayerCount(mBaseLevel));

    // When making mipmaps with the setData workaround enabled, the texture storage has
    // the image data already. For non-render-target storage, we have to pull it out into
    // an image layer.
    if (mRenderer->getWorkarounds().setDataFasterThanImageUpload && mTexStorage)
    {
        if (!mTexStorage->isRenderTarget())
        {
            // Copy from the storage mip 0 to Image mip 0
            for (GLint layer = 0; layer < layerCount; ++layer)
            {
                gl::ImageIndex srcIndex = getImageIndex(mBaseLevel, layer);

                ImageD3D *image = getImage(srcIndex);
                ANGLE_TRY(image->copyFromTexStorage(context, srcIndex, mTexStorage));
            }
        }
        else
        {
            ANGLE_TRY(updateStorage(context));
        }
    }

    // TODO: Decouple this from zeroMaxLodWorkaround. This is a 9_3 restriction, unrelated to zeroMaxLodWorkaround.
    // The restriction is because Feature Level 9_3 can't create SRVs on individual levels of the texture.
    // As a result, even if the storage is a rendertarget, we can't use the GPU to generate the mipmaps without further work.
    // The D3D9 renderer works around this by copying each level of the texture into its own single-layer GPU texture (in Blit9::boxFilter).
    // Feature Level 9_3 could do something similar, or it could continue to use CPU-side mipmap generation, or something else.
    bool renderableStorage = (mTexStorage && mTexStorage->isRenderTarget() && !(mRenderer->getWorkarounds().zeroMaxLodWorkaround));

    for (GLint layer = 0; layer < layerCount; ++layer)
    {
        for (GLuint mip = mBaseLevel + 1; mip <= maxLevel; ++mip)
        {
            ASSERT(getLayerCount(mip) == layerCount);

            gl::ImageIndex sourceIndex = getImageIndex(mip - 1, layer);
            gl::ImageIndex destIndex = getImageIndex(mip, layer);

            if (renderableStorage)
            {
                // GPU-side mipmapping
                ANGLE_TRY(mTexStorage->generateMipmap(context, sourceIndex, destIndex));
            }
            else
            {
                // CPU-side mipmapping
                ANGLE_TRY(
                    mRenderer->generateMipmap(context, getImage(destIndex), getImage(sourceIndex)));
            }
        }
    }

    mDirtyImages = true;

    if (mTexStorage)
    {
        ANGLE_TRY(updateStorage(context));
    }

    return gl::NoError();
}

bool TextureD3D::isBaseImageZeroSize() const
{
    ImageD3D *baseImage = getBaseLevelImage();

    if (!baseImage || baseImage->getWidth() <= 0)
    {
        return true;
    }

    if (!gl::IsCubeMapTextureTarget(baseImage->getTarget()) && baseImage->getHeight() <= 0)
    {
        return true;
    }

    if (baseImage->getTarget() == GL_TEXTURE_3D && baseImage->getDepth() <= 0)
    {
        return true;
    }

    if (baseImage->getTarget() == GL_TEXTURE_2D_ARRAY && getLayerCount(getBaseLevel()) <= 0)
    {
        return true;
    }

    return false;
}

gl::Error TextureD3D::ensureRenderTarget(const gl::Context *context)
{
    ANGLE_TRY(initializeStorage(context, true));

    // initializeStorage can fail with NoError if the texture is not complete. This is not
    // an error for incomplete sampling, but it is a big problem for rendering.
    if (!mTexStorage)
    {
        UNREACHABLE();
        return gl::InternalError() << "Cannot render to incomplete texture.";
    }

    if (!isBaseImageZeroSize())
    {
        ASSERT(mTexStorage);
        if (!mTexStorage->isRenderTarget())
        {
            TexStoragePointer newRenderTargetStorage(context);
            ANGLE_TRY(createCompleteStorage(true, &newRenderTargetStorage));

            ANGLE_TRY(mTexStorage->copyToStorage(context, newRenderTargetStorage.get()));
            ANGLE_TRY(setCompleteTexStorage(context, newRenderTargetStorage.get()));
            newRenderTargetStorage.release();
        }
    }

    return gl::NoError();
}

bool TextureD3D::canCreateRenderTargetForImage(const gl::ImageIndex &index) const
{
    if (index.type == GL_TEXTURE_2D_MULTISAMPLE)
        return true;

    ImageD3D *image = getImage(index);
    ASSERT(image);
    bool levelsComplete = (isImageComplete(index) && isImageComplete(getImageIndex(0, 0)));
    return (image->isRenderableFormat() && levelsComplete);
}

gl::Error TextureD3D::commitRegion(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Box &region)
{
    if (mTexStorage)
    {
        ASSERT(isValidIndex(index));
        ImageD3D *image = getImage(index);
        ANGLE_TRY(image->copyToStorage(context, mTexStorage, index, region));
        image->markClean();
    }

    return gl::NoError();
}

gl::Error TextureD3D::getAttachmentRenderTarget(const gl::Context *context,
                                                GLenum /*binding*/,
                                                const gl::ImageIndex &imageIndex,
                                                FramebufferAttachmentRenderTarget **rtOut)
{
    RenderTargetD3D *rtD3D = nullptr;
    gl::Error error        = getRenderTarget(context, imageIndex, &rtD3D);
    *rtOut = static_cast<FramebufferAttachmentRenderTarget *>(rtD3D);
    return error;
}

gl::Error TextureD3D::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    const int oldStorageWidth  = std::max(1, getLevelZeroWidth());
    const int oldStorageHeight = std::max(1, getLevelZeroHeight());
    const int oldStorageDepth  = std::max(1, getLevelZeroDepth());
    const int oldStorageFormat = getBaseLevelInternalFormat();
    mBaseLevel                 = baseLevel;

    // When the base level changes, the texture storage might not be valid anymore, since it could
    // have been created based on the dimensions of the previous specified level range.
    const int newStorageWidth  = std::max(1, getLevelZeroWidth());
    const int newStorageHeight = std::max(1, getLevelZeroHeight());
    const int newStorageDepth = std::max(1, getLevelZeroDepth());
    const int newStorageFormat = getBaseLevelInternalFormat();
    if (mTexStorage &&
        (newStorageWidth != oldStorageWidth || newStorageHeight != oldStorageHeight ||
         newStorageDepth != oldStorageDepth || newStorageFormat != oldStorageFormat))
    {
        markAllImagesDirty();
        ANGLE_TRY(releaseTexStorage(context));
    }

    return gl::NoError();
}

void TextureD3D::syncState(const gl::Texture::DirtyBits &dirtyBits)
{
    // TODO(geofflang): Use dirty bits
}

gl::Error TextureD3D::releaseTexStorage(const gl::Context *context)
{
    if (!mTexStorage)
    {
        return gl::NoError();
    }
    auto err = mTexStorage->onDestroy(context);
    SafeDelete(mTexStorage);
    return err;
}

gl::Error TextureD3D::onDestroy(const gl::Context *context)
{
    return releaseTexStorage(context);
}

gl::Error TextureD3D::initializeContents(const gl::Context *context,
                                         const gl::ImageIndex &imageIndexIn)
{
    gl::ImageIndex imageIndex = imageIndexIn;

    // Special case for D3D11 3D textures. We can't create render targets for individual layers of a
    // 3D texture, so force the clear to the entire mip. There shouldn't ever be a case where we
    // would lose existing data.
    if (imageIndex.type == GL_TEXTURE_3D)
    {
        imageIndex.layerIndex = gl::ImageIndex::ENTIRE_LEVEL;
    }
    else if (imageIndex.type == GL_TEXTURE_2D_ARRAY &&
             imageIndex.layerIndex == gl::ImageIndex::ENTIRE_LEVEL)
    {
        GLsizei layerCount = getLayerCount(imageIndex.mipIndex);
        for (imageIndex.layerIndex = 0; imageIndex.layerIndex < layerCount; ++imageIndex.layerIndex)
        {
            ANGLE_TRY(initializeContents(context, imageIndex));
        }
        return gl::NoError();
    }

    // Force image clean.
    ImageD3D *image = getImage(imageIndex);
    if (image)
    {
        image->markClean();
    }

    // Fast path: can use a render target clear.
    if (canCreateRenderTargetForImage(imageIndex))
    {
        ANGLE_TRY(ensureRenderTarget(context));
        ASSERT(mTexStorage);
        RenderTargetD3D *renderTarget = nullptr;
        ANGLE_TRY(mTexStorage->getRenderTarget(context, imageIndex, &renderTarget));
        ANGLE_TRY(mRenderer->initRenderTarget(renderTarget));
        return gl::NoError();
    }

    // Slow path: non-renderable texture or the texture levels aren't set up.
    const auto &formatInfo = gl::GetSizedInternalFormatInfo(image->getInternalFormat());

    size_t imageBytes = 0;
    ANGLE_TRY_RESULT(formatInfo.computeRowPitch(formatInfo.type, image->getWidth(), 1, 0),
                     imageBytes);
    imageBytes *= image->getHeight() * image->getDepth();

    gl::PixelUnpackState defaultUnpackState;

    angle::MemoryBuffer *zeroBuffer = nullptr;
    ANGLE_TRY(context->getZeroFilledBuffer(imageBytes, &zeroBuffer));
    if (shouldUseSetData(image))
    {
        ANGLE_TRY(mTexStorage->setData(context, imageIndex, image, nullptr, formatInfo.type,
                                       defaultUnpackState, zeroBuffer->data()));
    }
    else
    {
        gl::Box fullImageArea(0, 0, 0, image->getWidth(), image->getHeight(), image->getDepth());
        ANGLE_TRY(image->loadData(context, fullImageArea, defaultUnpackState, formatInfo.type,
                                  zeroBuffer->data(), false));

        // Force an update to the tex storage so we avoid problems with subImage and dirty regions.
        if (mTexStorage)
        {
            ANGLE_TRY(commitRegion(context, imageIndex, fullImageArea));
            image->markClean();
        }
        else
        {
            mDirtyImages = true;
        }
    }
    return gl::NoError();
}

TextureD3D_2D::TextureD3D_2D(const gl::TextureState &state, RendererD3D *renderer)
    : TextureD3D(state, renderer)
{
    mEGLImageTarget = false;
    for (auto &image : mImageArray)
    {
        image.reset(renderer->createImage());
    }
}

gl::Error TextureD3D_2D::onDestroy(const gl::Context *context)
{
    // Delete the Images before the TextureStorage. Images might be relying on the TextureStorage
    // for some of their data. If TextureStorage is deleted before the Images, then their data will
    // be wastefully copied back from the GPU before we delete the Images.
    for (auto &image : mImageArray)
    {
        image.reset();
    }
    return TextureD3D::onDestroy(context);
}

TextureD3D_2D::~TextureD3D_2D()
{
}

ImageD3D *TextureD3D_2D::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer == 0);
    return mImageArray[level].get();
}

ImageD3D *TextureD3D_2D::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(!index.hasLayer());
    ASSERT(index.type == GL_TEXTURE_2D);
    return mImageArray[index.mipIndex].get();
}

GLsizei TextureD3D_2D::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return 1;
}

GLsizei TextureD3D_2D::getWidth(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getWidth();
    else
        return 0;
}

GLsizei TextureD3D_2D::getHeight(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getHeight();
    else
        return 0;
}

GLenum TextureD3D_2D::getInternalFormat(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getInternalFormat();
    else
        return GL_NONE;
}

bool TextureD3D_2D::isDepth(GLint level) const
{
    return gl::GetSizedInternalFormatInfo(getInternalFormat(level)).depthBits > 0;
}

bool TextureD3D_2D::isSRGB(GLint level) const
{
    return gl::GetSizedInternalFormatInfo(getInternalFormat(level)).colorEncoding == GL_SRGB;
}

gl::Error TextureD3D_2D::setImage(const gl::Context *context,
                                  GLenum target,
                                  size_t imageLevel,
                                  GLenum internalFormat,
                                  const gl::Extents &size,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && size.depth == 1);

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);

    bool fastUnpacked = false;
    GLint level       = static_cast<GLint>(imageLevel);

    ANGLE_TRY(redefineImage(context, level, internalFormatInfo.sizedInternalFormat, size, false));

    gl::ImageIndex index = gl::ImageIndex::Make2D(level);

    // Attempt a fast gpu copy of the pixel data to the surface
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    if (isFastUnpackable(unpackBuffer, internalFormatInfo.sizedInternalFormat) &&
        isLevelComplete(level))
    {
        // Will try to create RT storage if it does not exist
        RenderTargetD3D *destRenderTarget = nullptr;
        ANGLE_TRY(getRenderTarget(context, index, &destRenderTarget));

        gl::Box destArea(0, 0, 0, getWidth(level), getHeight(level), 1);

        ANGLE_TRY(fastUnpackPixels(context, unpack, pixels, destArea,
                                   internalFormatInfo.sizedInternalFormat, type, destRenderTarget));

        // Ensure we don't overwrite our newly initialized data
        mImageArray[level]->markClean();

        fastUnpacked = true;
    }

    if (!fastUnpacked)
    {
        ANGLE_TRY(setImageImpl(context, index, type, unpack, pixels, 0));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::setSubImage(const gl::Context *context,
                                     GLenum target,
                                     size_t imageLevel,
                                     const gl::Box &area,
                                     GLenum format,
                                     GLenum type,
                                     const gl::PixelUnpackState &unpack,
                                     const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && area.depth == 1 && area.z == 0);

    GLint level          = static_cast<GLint>(imageLevel);
    gl::ImageIndex index = gl::ImageIndex::Make2D(level);

    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    if (isFastUnpackable(unpackBuffer, getInternalFormat(level)) && isLevelComplete(level))
    {
        RenderTargetD3D *renderTarget = nullptr;
        ANGLE_TRY(getRenderTarget(context, index, &renderTarget));
        ASSERT(!mImageArray[level]->isDirty());

        return fastUnpackPixels(context, unpack, pixels, area, getInternalFormat(level), type,
                                renderTarget);
    }
    else
    {
        return TextureD3D::subImage(context, index, area, format, type, unpack, pixels, 0);
    }
}

gl::Error TextureD3D_2D::setCompressedImage(const gl::Context *context,
                                            GLenum target,
                                            size_t imageLevel,
                                            GLenum internalFormat,
                                            const gl::Extents &size,
                                            const gl::PixelUnpackState &unpack,
                                            size_t imageSize,
                                            const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && size.depth == 1);
    GLint level = static_cast<GLint>(imageLevel);

    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    ANGLE_TRY(redefineImage(context, level, internalFormat, size, false));

    return setCompressedImageImpl(context, gl::ImageIndex::Make2D(level), unpack, pixels, 0);
}

gl::Error TextureD3D_2D::setCompressedSubImage(const gl::Context *context,
                                               GLenum target,
                                               size_t level,
                                               const gl::Box &area,
                                               GLenum format,
                                               const gl::PixelUnpackState &unpack,
                                               size_t imageSize,
                                               const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D && area.depth == 1 && area.z == 0);

    gl::ImageIndex index = gl::ImageIndex::Make2D(static_cast<GLint>(level));
    ANGLE_TRY(TextureD3D::subImageCompressed(context, index, area, format, unpack, pixels, 0));

    return commitRegion(context, index, area);
}

gl::Error TextureD3D_2D::copyImage(const gl::Context *context,
                                   GLenum target,
                                   size_t imageLevel,
                                   const gl::Rectangle &origSourceArea,
                                   GLenum internalFormat,
                                   const gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_2D);

    GLint level = static_cast<GLint>(imageLevel);
    const gl::InternalFormat &internalFormatInfo =
        gl::GetInternalFormatInfo(internalFormat, GL_UNSIGNED_BYTE);
    gl::Extents sourceExtents(origSourceArea.width, origSourceArea.height, 1);
    ANGLE_TRY(redefineImage(context, level, internalFormatInfo.sizedInternalFormat, sourceExtents,
                            false));

    gl::Extents fbSize = source->getReadColorbuffer()->getSize();

    // Does the read area extend beyond the framebuffer?
    bool outside = origSourceArea.x < 0 || origSourceArea.y < 0 ||
                   origSourceArea.x + origSourceArea.width > fbSize.width ||
                   origSourceArea.y + origSourceArea.height > fbSize.height;

    // In WebGL mode we need to zero the texture outside the framebuffer.
    // If we have robust resource init, it was already zeroed by redefineImage() above, otherwise
    // zero it explicitly.
    // TODO(fjhenigman): When robust resource is fully implemented look into making it a
    // prerequisite for WebGL and deleting this code.
    if (outside &&
        (context->getExtensions().webglCompatibility || context->isRobustResourceInitEnabled()))
    {
        angle::MemoryBuffer *zero;
        ANGLE_TRY(context->getZeroFilledBuffer(
            origSourceArea.width * origSourceArea.height * internalFormatInfo.pixelBytes, &zero));
        gl::PixelUnpackState unpack;
        unpack.alignment = 1;
        ANGLE_TRY(setImage(context, target, imageLevel, internalFormat, sourceExtents,
                           internalFormatInfo.format, internalFormatInfo.type, unpack,
                           zero->data()));
    }

    gl::Rectangle sourceArea;
    if (!ClipRectangle(origSourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &sourceArea))
    {
        // Empty source area, nothing to do.
        return gl::NoError();
    }

    gl::ImageIndex index = gl::ImageIndex::Make2D(level);
    gl::Offset destOffset(sourceArea.x - origSourceArea.x, sourceArea.y - origSourceArea.y, 0);

    // If the zero max LOD workaround is active, then we can't sample from individual layers of the framebuffer in shaders,
    // so we should use the non-rendering copy path.
    if (!canCreateRenderTargetForImage(index) || mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ANGLE_TRY(mImageArray[level]->copyFromFramebuffer(context, destOffset, sourceArea, source));
        mDirtyImages = true;
    }
    else
    {
        ANGLE_TRY(ensureRenderTarget(context));

        if (sourceArea.width != 0 && sourceArea.height != 0 && isValidLevel(level))
        {
            ANGLE_TRY(updateStorageLevel(context, level));
            ANGLE_TRY(mRenderer->copyImage2D(context, source, sourceArea, internalFormat,
                                             destOffset, mTexStorage, level));
        }
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::copySubImage(const gl::Context *context,
                                      GLenum target,
                                      size_t imageLevel,
                                      const gl::Offset &origDestOffset,
                                      const gl::Rectangle &origSourceArea,
                                      const gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_2D && origDestOffset.z == 0);

    gl::Extents fbSize = source->getReadColorbuffer()->getSize();
    gl::Rectangle sourceArea;
    if (!ClipRectangle(origSourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &sourceArea))
    {
        return gl::NoError();
    }
    const gl::Offset destOffset(origDestOffset.x + sourceArea.x - origSourceArea.x,
                                origDestOffset.y + sourceArea.y - origSourceArea.y, 0);

    // can only make our texture storage to a render target if level 0 is defined (with a width & height) and
    // the current level we're copying to is defined (with appropriate format, width & height)

    GLint level          = static_cast<GLint>(imageLevel);
    gl::ImageIndex index = gl::ImageIndex::Make2D(level);

    // If the zero max LOD workaround is active, then we can't sample from individual layers of the framebuffer in shaders,
    // so we should use the non-rendering copy path.
    if (!canCreateRenderTargetForImage(index) || mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ANGLE_TRY(mImageArray[level]->copyFromFramebuffer(context, destOffset, sourceArea, source));
        mDirtyImages = true;
    }
    else
    {
        ANGLE_TRY(ensureRenderTarget(context));

        if (isValidLevel(level))
        {
            ANGLE_TRY(updateStorageLevel(context, level));
            ANGLE_TRY(mRenderer->copyImage2D(context, source, sourceArea,
                                             gl::GetUnsizedFormat(getBaseLevelInternalFormat()),
                                             destOffset, mTexStorage, level));
        }
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::copyTexture(const gl::Context *context,
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
    ASSERT(target == GL_TEXTURE_2D);

    GLenum sourceTarget = source->getTarget();

    GLint destLevel = static_cast<GLint>(level);

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    gl::Extents size(static_cast<int>(source->getWidth(sourceTarget, sourceLevel)),
                     static_cast<int>(source->getHeight(sourceTarget, sourceLevel)), 1);
    ANGLE_TRY(
        redefineImage(context, destLevel, internalFormatInfo.sizedInternalFormat, size, false));

    gl::Rectangle sourceRect(0, 0, size.width, size.height);
    gl::Offset destOffset(0, 0, 0);

    if (!isSRGB(destLevel) && canCreateRenderTargetForImage(gl::ImageIndex::Make2D(destLevel)))
    {
        ANGLE_TRY(ensureRenderTarget(context));
        ASSERT(isValidLevel(destLevel));
        ANGLE_TRY(updateStorageLevel(context, destLevel));

        ANGLE_TRY(mRenderer->copyTexture(context, source, static_cast<GLint>(sourceLevel),
                                         sourceRect, internalFormatInfo.format, destOffset,
                                         mTexStorage, target, destLevel, unpackFlipY,
                                         unpackPremultiplyAlpha, unpackUnmultiplyAlpha));
    }
    else
    {
        gl::ImageIndex sourceImageIndex = gl::ImageIndex::Make2D(static_cast<GLint>(sourceLevel));
        TextureD3D *sourceD3D           = GetImplAs<TextureD3D>(source);
        ImageD3D *sourceImage           = nullptr;
        ANGLE_TRY(sourceD3D->getImageAndSyncFromStorage(context, sourceImageIndex, &sourceImage));

        gl::ImageIndex destImageIndex = gl::ImageIndex::Make2D(static_cast<GLint>(destLevel));
        ImageD3D *destImage           = nullptr;
        ANGLE_TRY(getImageAndSyncFromStorage(context, destImageIndex, &destImage));

        ANGLE_TRY(mRenderer->copyImage(context, destImage, sourceImage, sourceRect, destOffset,
                                       unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha));

        mDirtyImages = true;

        gl::Box destRegion(destOffset, size);
        ANGLE_TRY(commitRegion(context, destImageIndex, destRegion));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::copySubTexture(const gl::Context *context,
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
    ASSERT(target == GL_TEXTURE_2D);

    GLint destLevel = static_cast<GLint>(level);

    if (!isSRGB(destLevel) && canCreateRenderTargetForImage(gl::ImageIndex::Make2D(destLevel)))
    {
        ANGLE_TRY(ensureRenderTarget(context));
        ASSERT(isValidLevel(destLevel));
        ANGLE_TRY(updateStorageLevel(context, destLevel));

        ANGLE_TRY(mRenderer->copyTexture(
            context, source, static_cast<GLint>(sourceLevel), sourceArea,
            gl::GetUnsizedFormat(getInternalFormat(destLevel)), destOffset, mTexStorage, target,
            destLevel, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha));
    }
    else
    {
        gl::ImageIndex sourceImageIndex = gl::ImageIndex::Make2D(static_cast<GLint>(sourceLevel));
        TextureD3D *sourceD3D           = GetImplAs<TextureD3D>(source);
        ImageD3D *sourceImage           = nullptr;
        ANGLE_TRY(sourceD3D->getImageAndSyncFromStorage(context, sourceImageIndex, &sourceImage));

        gl::ImageIndex destImageIndex = gl::ImageIndex::Make2D(static_cast<GLint>(destLevel));
        ImageD3D *destImage           = nullptr;
        ANGLE_TRY(getImageAndSyncFromStorage(context, destImageIndex, &destImage));

        ANGLE_TRY(mRenderer->copyImage(context, destImage, sourceImage, sourceArea, destOffset,
                                       unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha));

        mDirtyImages = true;

        gl::Box destRegion(destOffset.x, destOffset.y, 0, sourceArea.width, sourceArea.height, 1);
        ANGLE_TRY(commitRegion(context, destImageIndex, destRegion));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::copyCompressedTexture(const gl::Context *context,
                                               const gl::Texture *source)
{
    GLenum sourceTarget = source->getTarget();
    GLint sourceLevel   = 0;

    GLint destLevel = 0;

    GLenum sizedInternalFormat =
        source->getFormat(sourceTarget, sourceLevel).info->sizedInternalFormat;
    gl::Extents size(static_cast<int>(source->getWidth(sourceTarget, sourceLevel)),
                     static_cast<int>(source->getHeight(sourceTarget, sourceLevel)), 1);
    ANGLE_TRY(redefineImage(context, destLevel, sizedInternalFormat, size, false));

    ANGLE_TRY(initializeStorage(context, false));
    ASSERT(mTexStorage);

    ANGLE_TRY(
        mRenderer->copyCompressedTexture(context, source, sourceLevel, mTexStorage, destLevel));

    return gl::NoError();
}

gl::Error TextureD3D_2D::setStorage(const gl::Context *context,
                                    GLenum target,
                                    size_t levels,
                                    GLenum internalFormat,
                                    const gl::Extents &size)
{
    ASSERT(GL_TEXTURE_2D && size.depth == 1);

    for (size_t level = 0; level < levels; level++)
    {
        gl::Extents levelSize(std::max(1, size.width >> level),
                              std::max(1, size.height >> level),
                              1);
        ANGLE_TRY(redefineImage(context, level, internalFormat, levelSize, true));
    }

    for (size_t level = levels; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        ANGLE_TRY(redefineImage(context, level, GL_NONE, gl::Extents(0, 0, 1), true));
    }

    // TODO(geofflang): Verify storage creation had no errors
    bool renderTarget = IsRenderTargetUsage(mState.getUsage());
    TexStoragePointer storage(context);
    storage.reset(mRenderer->createTextureStorage2D(internalFormat, renderTarget, size.width,
                                                    size.height, static_cast<int>(levels), false));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ANGLE_TRY(updateStorage(context));

    mImmutable = true;

    return gl::NoError();
}

gl::Error TextureD3D_2D::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    GLenum internalformat = surface->getConfig()->renderTargetFormat;

    gl::Extents size(surface->getWidth(), surface->getHeight(), 1);
    ANGLE_TRY(redefineImage(context, 0, internalformat, size, true));

    ANGLE_TRY(releaseTexStorage(context));

    SurfaceD3D *surfaceD3D = GetImplAs<SurfaceD3D>(surface);
    ASSERT(surfaceD3D);

    mTexStorage = mRenderer->createTextureStorage2D(surfaceD3D->getSwapChain());
    mEGLImageTarget = false;

    mDirtyImages = false;
    mImageArray[0]->markClean();

    return gl::NoError();
}

gl::Error TextureD3D_2D::releaseTexImage(const gl::Context *context)
{
    if (mTexStorage)
    {
        ANGLE_TRY(releaseTexStorage(context));
    }

    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        ANGLE_TRY(redefineImage(context, i, GL_NONE, gl::Extents(0, 0, 1), true));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::setEGLImageTarget(const gl::Context *context,
                                           GLenum target,
                                           egl::Image *image)
{
    EGLImageD3D *eglImaged3d = GetImplAs<EGLImageD3D>(image);

    // Set the properties of the base mip level from the EGL image
    const auto &format = image->getFormat();
    gl::Extents size(static_cast<int>(image->getWidth()), static_cast<int>(image->getHeight()), 1);
    ANGLE_TRY(redefineImage(context, 0, format.info->sizedInternalFormat, size, true));

    // Clear all other images.
    for (size_t level = 1; level < mImageArray.size(); level++)
    {
        ANGLE_TRY(redefineImage(context, level, GL_NONE, gl::Extents(0, 0, 1), true));
    }

    ANGLE_TRY(releaseTexStorage(context));
    mImageArray[0]->markClean();

    // Pass in the RenderTargetD3D here: createTextureStorage can't generate an error.
    RenderTargetD3D *renderTargetD3D = nullptr;
    ANGLE_TRY(eglImaged3d->getRenderTarget(context, &renderTargetD3D));

    mTexStorage     = mRenderer->createTextureStorageEGLImage(eglImaged3d, renderTargetD3D);
    mEGLImageTarget = true;

    return gl::NoError();
}

gl::Error TextureD3D_2D::initMipmapImages(const gl::Context *context)
{
    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();
    // Purge array levels baseLevel + 1 through q and reset them to represent the generated mipmap
    // levels.
    for (GLuint level = baseLevel + 1; level <= maxLevel; level++)
    {
        gl::Extents levelSize(std::max(getLevelZeroWidth() >> level, 1),
                              std::max(getLevelZeroHeight() >> level, 1), 1);

        ANGLE_TRY(redefineImage(context, level, getBaseLevelInternalFormat(), levelSize, false));
    }
    return gl::NoError();
}

gl::Error TextureD3D_2D::getRenderTarget(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         RenderTargetD3D **outRT)
{
    ASSERT(!index.hasLayer());

    // ensure the underlying texture is created
    ANGLE_TRY(ensureRenderTarget(context));
    ANGLE_TRY(updateStorageLevel(context, index.mipIndex));

    return mTexStorage->getRenderTarget(context, index, outRT);
}

bool TextureD3D_2D::isValidLevel(int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : false);
}

bool TextureD3D_2D::isLevelComplete(int level) const
{
    if (isImmutable())
    {
        return true;
    }

    GLsizei width  = getLevelZeroWidth();
    GLsizei height = getLevelZeroHeight();

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    // The base image level is complete if the width and height are positive
    if (level == static_cast<int>(getBaseLevel()))
    {
        return true;
    }

    ASSERT(level >= 0 && level <= static_cast<int>(mImageArray.size()) &&
           mImageArray[level] != nullptr);
    ImageD3D *image = mImageArray[level].get();

    if (image->getInternalFormat() != getBaseLevelInternalFormat())
    {
        return false;
    }

    if (image->getWidth() != std::max(1, width >> level))
    {
        return false;
    }

    if (image->getHeight() != std::max(1, height >> level))
    {
        return false;
    }

    return true;
}

bool TextureD3D_2D::isImageComplete(const gl::ImageIndex &index) const
{
    return isLevelComplete(index.mipIndex);
}

// Constructs a native texture resource from the texture images
gl::Error TextureD3D_2D::initializeStorage(const gl::Context *context, bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return gl::NoError();
    }

    // do not attempt to create storage for nonexistant data
    if (!isLevelComplete(getBaseLevel()))
    {
        return gl::NoError();
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mState.getUsage()));

    TexStoragePointer storage(context);
    ANGLE_TRY(createCompleteStorage(createRenderTarget, &storage));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ASSERT(mTexStorage);

    // flush image data to the storage
    ANGLE_TRY(updateStorage(context));

    return gl::NoError();
}

gl::Error TextureD3D_2D::createCompleteStorage(bool renderTarget,
                                               TexStoragePointer *outStorage) const
{
    GLsizei width         = getLevelZeroWidth();
    GLsizei height        = getLevelZeroHeight();
    GLenum internalFormat = getBaseLevelInternalFormat();

    ASSERT(width > 0 && height > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(width, height, 1));

    bool hintLevelZeroOnly = false;
    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        // If any of the CPU images (levels >= 1) are dirty, then the textureStorage2D should use the mipped texture to begin with.
        // Otherwise, it should use the level-zero-only texture.
        hintLevelZeroOnly = true;
        for (int level = 1; level < levels && hintLevelZeroOnly; level++)
        {
            hintLevelZeroOnly = !(mImageArray[level]->isDirty() && isLevelComplete(level));
        }
    }

    // TODO(geofflang): Determine if the texture creation succeeded
    outStorage->reset(mRenderer->createTextureStorage2D(internalFormat, renderTarget, width, height,
                                                        levels, hintLevelZeroOnly));

    return gl::NoError();
}

gl::Error TextureD3D_2D::setCompleteTexStorage(const gl::Context *context,
                                               TextureStorage *newCompleteTexStorage)
{
    if (newCompleteTexStorage && newCompleteTexStorage->isManaged())
    {
        for (int level = 0; level < newCompleteTexStorage->getLevelCount(); level++)
        {
            ANGLE_TRY(
                mImageArray[level]->setManagedSurface2D(context, newCompleteTexStorage, level));
        }
    }

    ANGLE_TRY(releaseTexStorage(context));
    mTexStorage = newCompleteTexStorage;

    mDirtyImages = true;

    return gl::NoError();
}

gl::Error TextureD3D_2D::updateStorage(const gl::Context *context)
{
    if (!mDirtyImages)
    {
        return gl::NoError();
    }

    ASSERT(mTexStorage != nullptr);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int level = 0; level < storageLevels; level++)
    {
        if (mImageArray[level]->isDirty() && isLevelComplete(level))
        {
            ANGLE_TRY(updateStorageLevel(context, level));
        }
    }

    mDirtyImages = false;
    return gl::NoError();
}

gl::Error TextureD3D_2D::updateStorageLevel(const gl::Context *context, int level)
{
    ASSERT(level <= static_cast<int>(mImageArray.size()) && mImageArray[level] != nullptr);
    ASSERT(isLevelComplete(level));

    if (mImageArray[level]->isDirty())
    {
        gl::ImageIndex index = gl::ImageIndex::Make2D(level);
        gl::Box region(0, 0, 0, getWidth(level), getHeight(level), 1);
        ANGLE_TRY(commitRegion(context, index, region));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2D::redefineImage(const gl::Context *context,
                                       size_t level,
                                       GLenum internalformat,
                                       const gl::Extents &size,
                                       bool forceRelease)
{
    ASSERT(size.depth == 1);

    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth     = std::max(1, getLevelZeroWidth() >> level);
    const int storageHeight    = std::max(1, getLevelZeroHeight() >> level);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    mImageArray[level]->redefine(GL_TEXTURE_2D, internalformat, size, forceRelease);
    mDirtyImages = mDirtyImages || mImageArray[level]->isDirty();

    if (mTexStorage)
    {
        const size_t storageLevels = mTexStorage->getLevelCount();

        // If the storage was from an EGL image, copy it back into local images to preserve it
        // while orphaning
        if (level != 0 && mEGLImageTarget)
        {
            ANGLE_TRY(mImageArray[0]->copyFromTexStorage(context, gl::ImageIndex::Make2D(0),
                                                         mTexStorage));
        }

        if ((level >= storageLevels && storageLevels != 0) || size.width != storageWidth ||
            size.height != storageHeight ||
            internalformat != storageFormat)  // Discard mismatched storage
        {
            ANGLE_TRY(releaseTexStorage(context));
            markAllImagesDirty();
        }
    }

    // Can't be an EGL image target after being redefined
    mEGLImageTarget = false;

    return gl::NoError();
}

gl::ImageIndexIterator TextureD3D_2D::imageIterator() const
{
    return gl::ImageIndexIterator::Make2D(0, mTexStorage->getLevelCount());
}

gl::ImageIndex TextureD3D_2D::getImageIndex(GLint mip, GLint /*layer*/) const
{
    // "layer" does not apply to 2D Textures.
    return gl::ImageIndex::Make2D(mip);
}

bool TextureD3D_2D::isValidIndex(const gl::ImageIndex &index) const
{
    return (mTexStorage && index.type == GL_TEXTURE_2D &&
            index.mipIndex >= 0 && index.mipIndex < mTexStorage->getLevelCount());
}

void TextureD3D_2D::markAllImagesDirty()
{
    for (size_t i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mImageArray[i]->markDirty();
    }
    mDirtyImages = true;
}

TextureD3D_Cube::TextureD3D_Cube(const gl::TextureState &state, RendererD3D *renderer)
    : TextureD3D(state, renderer)
{
    for (auto &face : mImageArray)
    {
        for (auto &image : face)
        {
            image.reset(renderer->createImage());
        }
    }
}

gl::Error TextureD3D_Cube::onDestroy(const gl::Context *context)
{
    // Delete the Images before the TextureStorage. Images might be relying on the TextureStorage
    // for some of their data. If TextureStorage is deleted before the Images, then their data will
    // be wastefully copied back from the GPU before we delete the Images.
    for (auto &face : mImageArray)
    {
        for (auto &image : face)
        {
            image.reset();
        }
    }
    return TextureD3D::onDestroy(context);
}

TextureD3D_Cube::~TextureD3D_Cube()
{
}

ImageD3D *TextureD3D_Cube::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer >= 0 && layer < 6);
    return mImageArray[layer][level].get();
}

ImageD3D *TextureD3D_Cube::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(index.layerIndex >= 0 && index.layerIndex < 6);
    return mImageArray[index.layerIndex][index.mipIndex].get();
}

GLsizei TextureD3D_Cube::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return 6;
}

GLenum TextureD3D_Cube::getInternalFormat(GLint level, GLint layer) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[layer][level]->getInternalFormat();
    else
        return GL_NONE;
}

bool TextureD3D_Cube::isDepth(GLint level, GLint layer) const
{
    return gl::GetSizedInternalFormatInfo(getInternalFormat(level, layer)).depthBits > 0;
}

bool TextureD3D_Cube::isSRGB(GLint level, GLint layer) const
{
    return gl::GetSizedInternalFormatInfo(getInternalFormat(level, layer)).colorEncoding == GL_SRGB;
}

gl::Error TextureD3D_Cube::setEGLImageTarget(const gl::Context *context,
                                             GLenum target,
                                             egl::Image *image)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_Cube::setImage(const gl::Context *context,
                                    GLenum target,
                                    size_t level,
                                    GLenum internalFormat,
                                    const gl::Extents &size,
                                    GLenum format,
                                    GLenum type,
                                    const gl::PixelUnpackState &unpack,
                                    const uint8_t *pixels)
{
    ASSERT(size.depth == 1);

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    gl::ImageIndex index       = gl::ImageIndex::MakeCube(target, static_cast<GLint>(level));

    ANGLE_TRY(redefineImage(context, index.layerIndex, static_cast<GLint>(level),
                            internalFormatInfo.sizedInternalFormat, size, false));

    return setImageImpl(context, index, type, unpack, pixels, 0);
}

gl::Error TextureD3D_Cube::setSubImage(const gl::Context *context,
                                       GLenum target,
                                       size_t level,
                                       const gl::Box &area,
                                       GLenum format,
                                       GLenum type,
                                       const gl::PixelUnpackState &unpack,
                                       const uint8_t *pixels)
{
    ASSERT(area.depth == 1 && area.z == 0);

    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, static_cast<GLint>(level));
    return TextureD3D::subImage(context, index, area, format, type, unpack, pixels, 0);
}

gl::Error TextureD3D_Cube::setCompressedImage(const gl::Context *context,
                                              GLenum target,
                                              size_t level,
                                              GLenum internalFormat,
                                              const gl::Extents &size,
                                              const gl::PixelUnpackState &unpack,
                                              size_t imageSize,
                                              const uint8_t *pixels)
{
    ASSERT(size.depth == 1);

    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    size_t faceIndex = gl::CubeMapTextureTargetToLayerIndex(target);

    ANGLE_TRY(redefineImage(context, static_cast<int>(faceIndex), static_cast<GLint>(level),
                            internalFormat, size, false));

    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, static_cast<GLint>(level));
    return setCompressedImageImpl(context, index, unpack, pixels, 0);
}

gl::Error TextureD3D_Cube::setCompressedSubImage(const gl::Context *context,
                                                 GLenum target,
                                                 size_t level,
                                                 const gl::Box &area,
                                                 GLenum format,
                                                 const gl::PixelUnpackState &unpack,
                                                 size_t imageSize,
                                                 const uint8_t *pixels)
{
    ASSERT(area.depth == 1 && area.z == 0);

    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, static_cast<GLint>(level));

    ANGLE_TRY(TextureD3D::subImageCompressed(context, index, area, format, unpack, pixels, 0));
    return commitRegion(context, index, area);
}

gl::Error TextureD3D_Cube::copyImage(const gl::Context *context,
                                     GLenum target,
                                     size_t imageLevel,
                                     const gl::Rectangle &origSourceArea,
                                     GLenum internalFormat,
                                     const gl::Framebuffer *source)
{
    int faceIndex              = static_cast<int>(gl::CubeMapTextureTargetToLayerIndex(target));
    const gl::InternalFormat &internalFormatInfo =
        gl::GetInternalFormatInfo(internalFormat, GL_UNSIGNED_BYTE);

    GLint level = static_cast<GLint>(imageLevel);

    gl::Extents size(origSourceArea.width, origSourceArea.height, 1);
    ANGLE_TRY(redefineImage(context, static_cast<int>(faceIndex), level,
                            internalFormatInfo.sizedInternalFormat, size, false));

    gl::Extents fbSize = source->getReadColorbuffer()->getSize();

    // Does the read area extend beyond the framebuffer?
    bool outside = origSourceArea.x < 0 || origSourceArea.y < 0 ||
                   origSourceArea.x + origSourceArea.width > fbSize.width ||
                   origSourceArea.y + origSourceArea.height > fbSize.height;

    // In WebGL mode we need to zero the texture outside the framebuffer.
    // If we have robust resource init, it was already zeroed by redefineImage() above, otherwise
    // zero it explicitly.
    // TODO(fjhenigman): When robust resource is fully implemented look into making it a
    // prerequisite for WebGL and deleting this code.
    if (outside && context->getExtensions().webglCompatibility &&
        !context->isRobustResourceInitEnabled())
    {
        angle::MemoryBuffer *zero;
        ANGLE_TRY(context->getZeroFilledBuffer(
            origSourceArea.width * origSourceArea.height * internalFormatInfo.pixelBytes, &zero));
        gl::PixelUnpackState unpack;
        unpack.alignment = 1;
        ANGLE_TRY(setImage(context, target, imageLevel, internalFormat, size,
                           internalFormatInfo.format, internalFormatInfo.type, unpack,
                           zero->data()));
    }

    gl::Rectangle sourceArea;
    if (!ClipRectangle(origSourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &sourceArea))
    {
        // Empty source area, nothing to do.
        return gl::NoError();
    }

    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, level);
    gl::Offset destOffset(sourceArea.x - origSourceArea.x, sourceArea.y - origSourceArea.y, 0);

    // If the zero max LOD workaround is active, then we can't sample from individual layers of the framebuffer in shaders,
    // so we should use the non-rendering copy path.
    if (!canCreateRenderTargetForImage(index) || mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ANGLE_TRY(mImageArray[faceIndex][level]->copyFromFramebuffer(context, destOffset,
                                                                     sourceArea, source));
        mDirtyImages = true;
    }
    else
    {
        ANGLE_TRY(ensureRenderTarget(context));

        ASSERT(size.width == size.height);

        if (size.width > 0 && isValidFaceLevel(faceIndex, level))
        {
            ANGLE_TRY(updateStorageFaceLevel(context, faceIndex, level));
            ANGLE_TRY(mRenderer->copyImageCube(context, source, sourceArea, internalFormat,
                                               destOffset, mTexStorage, target, level));
        }
    }

    return gl::NoError();
}

gl::Error TextureD3D_Cube::copySubImage(const gl::Context *context,
                                        GLenum target,
                                        size_t imageLevel,
                                        const gl::Offset &origDestOffset,
                                        const gl::Rectangle &origSourceArea,
                                        const gl::Framebuffer *source)
{
    gl::Extents fbSize = source->getReadColorbuffer()->getSize();
    gl::Rectangle sourceArea;
    if (!ClipRectangle(origSourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &sourceArea))
    {
        return gl::NoError();
    }
    const gl::Offset destOffset(origDestOffset.x + sourceArea.x - origSourceArea.x,
                                origDestOffset.y + sourceArea.y - origSourceArea.y, 0);

    int faceIndex = static_cast<int>(gl::CubeMapTextureTargetToLayerIndex(target));

    GLint level          = static_cast<GLint>(imageLevel);
    gl::ImageIndex index = gl::ImageIndex::MakeCube(target, level);

    // If the zero max LOD workaround is active, then we can't sample from individual layers of the framebuffer in shaders,
    // so we should use the non-rendering copy path.
    if (!canCreateRenderTargetForImage(index) || mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        ANGLE_TRY(mImageArray[faceIndex][level]->copyFromFramebuffer(context, destOffset,
                                                                     sourceArea, source));
        mDirtyImages = true;
    }
    else
    {
        ANGLE_TRY(ensureRenderTarget(context));
        if (isValidFaceLevel(faceIndex, level))
        {
            ANGLE_TRY(updateStorageFaceLevel(context, faceIndex, level));
            ANGLE_TRY(mRenderer->copyImageCube(context, source, sourceArea,
                                               gl::GetUnsizedFormat(getBaseLevelInternalFormat()),
                                               destOffset, mTexStorage, target, level));
        }
    }

    return gl::NoError();
}

gl::Error TextureD3D_Cube::copyTexture(const gl::Context *context,
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
    ASSERT(gl::IsCubeMapTextureTarget(target));

    GLenum sourceTarget = source->getTarget();

    GLint destLevel = static_cast<GLint>(level);
    int faceIndex   = static_cast<int>(gl::CubeMapTextureTargetToLayerIndex(target));

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    gl::Extents size(static_cast<int>(source->getWidth(sourceTarget, sourceLevel)),
                     static_cast<int>(source->getHeight(sourceTarget, sourceLevel)), 1);
    ANGLE_TRY(redefineImage(context, faceIndex, destLevel, internalFormatInfo.sizedInternalFormat,
                            size, false));

    gl::Rectangle sourceRect(0, 0, size.width, size.height);
    gl::Offset destOffset(0, 0, 0);

    if (!isSRGB(destLevel, faceIndex) &&
        canCreateRenderTargetForImage(gl::ImageIndex::MakeCube(target, destLevel)))
    {
        ANGLE_TRY(ensureRenderTarget(context));
        ASSERT(isValidFaceLevel(faceIndex, destLevel));
        ANGLE_TRY(updateStorageFaceLevel(context, faceIndex, destLevel));

        ANGLE_TRY(mRenderer->copyTexture(context, source, static_cast<GLint>(sourceLevel),
                                         sourceRect, internalFormatInfo.format, destOffset,
                                         mTexStorage, target, destLevel, unpackFlipY,
                                         unpackPremultiplyAlpha, unpackUnmultiplyAlpha));
    }
    else
    {
        gl::ImageIndex sourceImageIndex = gl::ImageIndex::Make2D(static_cast<GLint>(sourceLevel));
        TextureD3D *sourceD3D           = GetImplAs<TextureD3D>(source);
        ImageD3D *sourceImage           = nullptr;
        ANGLE_TRY(sourceD3D->getImageAndSyncFromStorage(context, sourceImageIndex, &sourceImage));

        gl::ImageIndex destImageIndex =
            gl::ImageIndex::MakeCube(target, static_cast<GLint>(destLevel));
        ImageD3D *destImage = nullptr;
        ANGLE_TRY(getImageAndSyncFromStorage(context, destImageIndex, &destImage));

        ANGLE_TRY(mRenderer->copyImage(context, destImage, sourceImage, sourceRect, destOffset,
                                       unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha));

        mDirtyImages = true;

        gl::Box destRegion(destOffset, size);
        ANGLE_TRY(commitRegion(context, destImageIndex, destRegion));
    }

    return gl::NoError();
}

gl::Error TextureD3D_Cube::copySubTexture(const gl::Context *context,
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
    ASSERT(gl::IsCubeMapTextureTarget(target));

    GLint destLevel = static_cast<GLint>(level);
    int faceIndex   = static_cast<int>(gl::CubeMapTextureTargetToLayerIndex(target));

    if (!isSRGB(destLevel, faceIndex) &&
        canCreateRenderTargetForImage(gl::ImageIndex::MakeCube(target, destLevel)))
    {
        ANGLE_TRY(ensureRenderTarget(context));
        ASSERT(isValidFaceLevel(faceIndex, destLevel));
        ANGLE_TRY(updateStorageFaceLevel(context, faceIndex, destLevel));

        ANGLE_TRY(mRenderer->copyTexture(
            context, source, static_cast<GLint>(sourceLevel), sourceArea,
            gl::GetUnsizedFormat(getInternalFormat(destLevel, faceIndex)), destOffset, mTexStorage,
            target, destLevel, unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha));
    }
    else
    {
        gl::ImageIndex sourceImageIndex = gl::ImageIndex::Make2D(static_cast<GLint>(sourceLevel));
        TextureD3D *sourceD3D           = GetImplAs<TextureD3D>(source);
        ImageD3D *sourceImage           = nullptr;
        ANGLE_TRY(sourceD3D->getImageAndSyncFromStorage(context, sourceImageIndex, &sourceImage));

        gl::ImageIndex destImageIndex =
            gl::ImageIndex::MakeCube(target, static_cast<GLint>(destLevel));
        ImageD3D *destImage = nullptr;
        ANGLE_TRY(getImageAndSyncFromStorage(context, destImageIndex, &destImage));

        ANGLE_TRY(mRenderer->copyImage(context, destImage, sourceImage, sourceArea, destOffset,
                                       unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha));

        mDirtyImages = true;

        gl::Box destRegion(destOffset.x, destOffset.y, 0, sourceArea.width, sourceArea.height, 1);
        ANGLE_TRY(commitRegion(context, destImageIndex, destRegion));
    }

    return gl::NoError();
}

gl::Error TextureD3D_Cube::setStorage(const gl::Context *context,
                                      GLenum target,
                                      size_t levels,
                                      GLenum internalFormat,
                                      const gl::Extents &size)
{
    ASSERT(size.width == size.height);
    ASSERT(size.depth == 1);

    for (size_t level = 0; level < levels; level++)
    {
        GLsizei mipSize = std::max(1, size.width >> level);
        for (int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            mImageArray[faceIndex][level]->redefine(GL_TEXTURE_CUBE_MAP, internalFormat, gl::Extents(mipSize, mipSize, 1), true);
        }
    }

    for (size_t level = levels; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        for (int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            mImageArray[faceIndex][level]->redefine(GL_TEXTURE_CUBE_MAP, GL_NONE, gl::Extents(0, 0, 0), true);
        }
    }

    // TODO(geofflang): Verify storage creation had no errors
    bool renderTarget = IsRenderTargetUsage(mState.getUsage());

    TexStoragePointer storage(context);
    storage.reset(mRenderer->createTextureStorageCube(internalFormat, renderTarget, size.width,
                                                      static_cast<int>(levels), false));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ANGLE_TRY(updateStorage(context));

    mImmutable = true;

    return gl::NoError();
}

// Tests for cube texture completeness. [OpenGL ES 2.0.24] section 3.7.10 page 81.
bool TextureD3D_Cube::isCubeComplete() const
{
    int    baseWidth  = getBaseLevelWidth();
    int    baseHeight = getBaseLevelHeight();
    GLenum baseFormat = getBaseLevelInternalFormat();

    if (baseWidth <= 0 || baseWidth != baseHeight)
    {
        return false;
    }

    for (int faceIndex = 1; faceIndex < 6; faceIndex++)
    {
        const ImageD3D &faceBaseImage = *mImageArray[faceIndex][getBaseLevel()];

        if (faceBaseImage.getWidth()          != baseWidth  ||
            faceBaseImage.getHeight()         != baseHeight ||
            faceBaseImage.getInternalFormat() != baseFormat )
        {
            return false;
        }
    }

    return true;
}

gl::Error TextureD3D_Cube::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_Cube::releaseTexImage(const gl::Context *context)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_Cube::initMipmapImages(const gl::Context *context)
{
    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();
    // Purge array levels baseLevel + 1 through q and reset them to represent the generated mipmap
    // levels.
    for (int faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        for (GLuint level = baseLevel + 1; level <= maxLevel; level++)
        {
            int faceLevelSize =
                (std::max(mImageArray[faceIndex][baseLevel]->getWidth() >> (level - baseLevel), 1));
            ANGLE_TRY(redefineImage(context, faceIndex, level,
                                    mImageArray[faceIndex][baseLevel]->getInternalFormat(),
                                    gl::Extents(faceLevelSize, faceLevelSize, 1), false));
        }
    }
    return gl::NoError();
}

gl::Error TextureD3D_Cube::getRenderTarget(const gl::Context *context,
                                           const gl::ImageIndex &index,
                                           RenderTargetD3D **outRT)
{
    ASSERT(gl::IsCubeMapTextureTarget(index.type));

    // ensure the underlying texture is created
    ANGLE_TRY(ensureRenderTarget(context));
    ANGLE_TRY(updateStorageFaceLevel(context, index.layerIndex, index.mipIndex));

    return mTexStorage->getRenderTarget(context, index, outRT);
}

gl::Error TextureD3D_Cube::initializeStorage(const gl::Context *context, bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return gl::NoError();
    }

    // do not attempt to create storage for nonexistant data
    if (!isFaceLevelComplete(0, getBaseLevel()))
    {
        return gl::NoError();
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mState.getUsage()));

    TexStoragePointer storage(context);
    ANGLE_TRY(createCompleteStorage(createRenderTarget, &storage));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ASSERT(mTexStorage);

    // flush image data to the storage
    ANGLE_TRY(updateStorage(context));

    return gl::NoError();
}

gl::Error TextureD3D_Cube::createCompleteStorage(bool renderTarget,
                                                 TexStoragePointer *outStorage) const
{
    GLsizei size = getLevelZeroWidth();

    ASSERT(size > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(size, size, 1));

    bool hintLevelZeroOnly = false;
    if (mRenderer->getWorkarounds().zeroMaxLodWorkaround)
    {
        // If any of the CPU images (levels >= 1) are dirty, then the textureStorage should use the mipped texture to begin with.
        // Otherwise, it should use the level-zero-only texture.
        hintLevelZeroOnly = true;
        for (int faceIndex = 0; faceIndex < 6 && hintLevelZeroOnly; faceIndex++)
        {
            for (int level = 1; level < levels && hintLevelZeroOnly; level++)
            {
                hintLevelZeroOnly = !(mImageArray[faceIndex][level]->isDirty() && isFaceLevelComplete(faceIndex, level));
            }
        }
    }

    // TODO (geofflang): detect if storage creation succeeded
    outStorage->reset(mRenderer->createTextureStorageCube(
        getBaseLevelInternalFormat(), renderTarget, size, levels, hintLevelZeroOnly));

    return gl::NoError();
}

gl::Error TextureD3D_Cube::setCompleteTexStorage(const gl::Context *context,
                                                 TextureStorage *newCompleteTexStorage)
{
    if (newCompleteTexStorage && newCompleteTexStorage->isManaged())
    {
        for (int faceIndex = 0; faceIndex < 6; faceIndex++)
        {
            for (int level = 0; level < newCompleteTexStorage->getLevelCount(); level++)
            {
                ANGLE_TRY(mImageArray[faceIndex][level]->setManagedSurfaceCube(
                    context, newCompleteTexStorage, faceIndex, level));
            }
        }
    }

    ANGLE_TRY(releaseTexStorage(context));
    mTexStorage = newCompleteTexStorage;

    mDirtyImages = true;
    return gl::NoError();
}

gl::Error TextureD3D_Cube::updateStorage(const gl::Context *context)
{
    if (!mDirtyImages)
    {
        return gl::NoError();
    }

    ASSERT(mTexStorage != nullptr);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int face = 0; face < 6; face++)
    {
        for (int level = 0; level < storageLevels; level++)
        {
            if (mImageArray[face][level]->isDirty() && isFaceLevelComplete(face, level))
            {
                ANGLE_TRY(updateStorageFaceLevel(context, face, level));
            }
        }
    }

    mDirtyImages = false;
    return gl::NoError();
}

bool TextureD3D_Cube::isValidFaceLevel(int faceIndex, int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : 0);
}

bool TextureD3D_Cube::isFaceLevelComplete(int faceIndex, int level) const
{
    if (getBaseLevel() >= gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        return false;
    }
    ASSERT(level >= 0 && faceIndex < 6 && level < static_cast<int>(mImageArray[faceIndex].size()) &&
           mImageArray[faceIndex][level] != nullptr);

    if (isImmutable())
    {
        return true;
    }

    int levelZeroSize = getLevelZeroWidth();

    if (levelZeroSize <= 0)
    {
        return false;
    }

    // "isCubeComplete" checks for base level completeness and we must call that
    // to determine if any face at level 0 is complete. We omit that check here
    // to avoid re-checking cube-completeness for every face at level 0.
    if (level == 0)
    {
        return true;
    }

    // Check that non-zero levels are consistent with the base level.
    const ImageD3D *faceLevelImage = mImageArray[faceIndex][level].get();

    if (faceLevelImage->getInternalFormat() != getBaseLevelInternalFormat())
    {
        return false;
    }

    if (faceLevelImage->getWidth() != std::max(1, levelZeroSize >> level))
    {
        return false;
    }

    return true;
}

bool TextureD3D_Cube::isImageComplete(const gl::ImageIndex &index) const
{
    return isFaceLevelComplete(index.layerIndex, index.mipIndex);
}

gl::Error TextureD3D_Cube::updateStorageFaceLevel(const gl::Context *context,
                                                  int faceIndex,
                                                  int level)
{
    ASSERT(level >= 0 && faceIndex < 6 && level < static_cast<int>(mImageArray[faceIndex].size()) &&
           mImageArray[faceIndex][level] != nullptr);
    ImageD3D *image = mImageArray[faceIndex][level].get();

    if (image->isDirty())
    {
        GLenum faceTarget = gl::LayerIndexToCubeMapTextureTarget(faceIndex);
        gl::ImageIndex index = gl::ImageIndex::MakeCube(faceTarget, level);
        gl::Box region(0, 0, 0, image->getWidth(), image->getHeight(), 1);
        ANGLE_TRY(commitRegion(context, index, region));
    }

    return gl::NoError();
}

gl::Error TextureD3D_Cube::redefineImage(const gl::Context *context,
                                         int faceIndex,
                                         GLint level,
                                         GLenum internalformat,
                                         const gl::Extents &size,
                                         bool forceRelease)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth     = std::max(1, getLevelZeroWidth() >> level);
    const int storageHeight    = std::max(1, getLevelZeroHeight() >> level);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    mImageArray[faceIndex][level]->redefine(GL_TEXTURE_CUBE_MAP, internalformat, size,
                                            forceRelease);
    mDirtyImages = mDirtyImages || mImageArray[faceIndex][level]->isDirty();

    if (mTexStorage)
    {
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) || size.width != storageWidth ||
            size.height != storageHeight ||
            internalformat != storageFormat)  // Discard mismatched storage
        {
            markAllImagesDirty();
            ANGLE_TRY(releaseTexStorage(context));
        }
    }

    return gl::NoError();
}

gl::ImageIndexIterator TextureD3D_Cube::imageIterator() const
{
    return gl::ImageIndexIterator::MakeCube(0, mTexStorage->getLevelCount());
}

gl::ImageIndex TextureD3D_Cube::getImageIndex(GLint mip, GLint layer) const
{
    // The "layer" of the image index corresponds to the cube face
    return gl::ImageIndex::MakeCube(gl::LayerIndexToCubeMapTextureTarget(layer), mip);
}

bool TextureD3D_Cube::isValidIndex(const gl::ImageIndex &index) const
{
    return (mTexStorage && gl::IsCubeMapTextureTarget(index.type) &&
            index.mipIndex >= 0 && index.mipIndex < mTexStorage->getLevelCount());
}

void TextureD3D_Cube::markAllImagesDirty()
{
    for (int dirtyLevel = 0; dirtyLevel < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; dirtyLevel++)
    {
        for (int dirtyFace = 0; dirtyFace < 6; dirtyFace++)
        {
            mImageArray[dirtyFace][dirtyLevel]->markDirty();
        }
    }
    mDirtyImages = true;
}

TextureD3D_3D::TextureD3D_3D(const gl::TextureState &state, RendererD3D *renderer)
    : TextureD3D(state, renderer)
{
    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++i)
    {
        mImageArray[i].reset(renderer->createImage());
    }
}

gl::Error TextureD3D_3D::onDestroy(const gl::Context *context)
{
    // Delete the Images before the TextureStorage. Images might be relying on the TextureStorage
    // for some of their data. If TextureStorage is deleted before the Images, then their data will
    // be wastefully copied back from the GPU before we delete the Images.
    for (auto &image : mImageArray)
    {
        image.reset();
    }
    return TextureD3D::onDestroy(context);
}

TextureD3D_3D::~TextureD3D_3D()
{
}

ImageD3D *TextureD3D_3D::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(layer == 0);
    return mImageArray[level].get();
}

ImageD3D *TextureD3D_3D::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(!index.hasLayer());
    ASSERT(index.type == GL_TEXTURE_3D);
    return mImageArray[index.mipIndex].get();
}

GLsizei TextureD3D_3D::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return 1;
}

GLsizei TextureD3D_3D::getWidth(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getWidth();
    else
        return 0;
}

GLsizei TextureD3D_3D::getHeight(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getHeight();
    else
        return 0;
}

GLsizei TextureD3D_3D::getDepth(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getDepth();
    else
        return 0;
}

GLenum TextureD3D_3D::getInternalFormat(GLint level) const
{
    if (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
        return mImageArray[level]->getInternalFormat();
    else
        return GL_NONE;
}

bool TextureD3D_3D::isDepth(GLint level) const
{
    return gl::GetSizedInternalFormatInfo(getInternalFormat(level)).depthBits > 0;
}

gl::Error TextureD3D_3D::setEGLImageTarget(const gl::Context *context,
                                           GLenum target,
                                           egl::Image *image)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_3D::setImage(const gl::Context *context,
                                  GLenum target,
                                  size_t imageLevel,
                                  GLenum internalFormat,
                                  const gl::Extents &size,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);
    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);

    GLint level = static_cast<GLint>(imageLevel);
    ANGLE_TRY(redefineImage(context, level, internalFormatInfo.sizedInternalFormat, size, false));

    bool fastUnpacked = false;

    gl::ImageIndex index = gl::ImageIndex::Make3D(level);

    // Attempt a fast gpu copy of the pixel data to the surface if the app bound an unpack buffer
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    if (isFastUnpackable(unpackBuffer, internalFormatInfo.sizedInternalFormat) && !size.empty() &&
        isLevelComplete(level))
    {
        // Will try to create RT storage if it does not exist
        RenderTargetD3D *destRenderTarget = nullptr;
        ANGLE_TRY(getRenderTarget(context, index, &destRenderTarget));

        gl::Box destArea(0, 0, 0, getWidth(level), getHeight(level), getDepth(level));

        ANGLE_TRY(fastUnpackPixels(context, unpack, pixels, destArea,
                                   internalFormatInfo.sizedInternalFormat, type, destRenderTarget));

        // Ensure we don't overwrite our newly initialized data
        mImageArray[level]->markClean();

        fastUnpacked = true;
    }

    if (!fastUnpacked)
    {
        ANGLE_TRY(setImageImpl(context, index, type, unpack, pixels, 0));
    }

    return gl::NoError();
}

gl::Error TextureD3D_3D::setSubImage(const gl::Context *context,
                                     GLenum target,
                                     size_t imageLevel,
                                     const gl::Box &area,
                                     GLenum format,
                                     GLenum type,
                                     const gl::PixelUnpackState &unpack,
                                     const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);

    GLint level          = static_cast<GLint>(imageLevel);
    gl::ImageIndex index = gl::ImageIndex::Make3D(level);

    // Attempt a fast gpu copy of the pixel data to the surface if the app bound an unpack buffer
    gl::Buffer *unpackBuffer =
        context->getGLState().getTargetBuffer(gl::BufferBinding::PixelUnpack);
    if (isFastUnpackable(unpackBuffer, getInternalFormat(level)) && isLevelComplete(level))
    {
        RenderTargetD3D *destRenderTarget = nullptr;
        ANGLE_TRY(getRenderTarget(context, index, &destRenderTarget));
        ASSERT(!mImageArray[level]->isDirty());

        return fastUnpackPixels(context, unpack, pixels, area, getInternalFormat(level), type,
                                destRenderTarget);
    }
    else
    {
        return TextureD3D::subImage(context, index, area, format, type, unpack, pixels, 0);
    }
}

gl::Error TextureD3D_3D::setCompressedImage(const gl::Context *context,
                                            GLenum target,
                                            size_t imageLevel,
                                            GLenum internalFormat,
                                            const gl::Extents &size,
                                            const gl::PixelUnpackState &unpack,
                                            size_t imageSize,
                                            const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);

    GLint level = static_cast<GLint>(imageLevel);
    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    ANGLE_TRY(redefineImage(context, level, internalFormat, size, false));

    gl::ImageIndex index = gl::ImageIndex::Make3D(level);
    return setCompressedImageImpl(context, index, unpack, pixels, 0);
}

gl::Error TextureD3D_3D::setCompressedSubImage(const gl::Context *context,
                                               GLenum target,
                                               size_t level,
                                               const gl::Box &area,
                                               GLenum format,
                                               const gl::PixelUnpackState &unpack,
                                               size_t imageSize,
                                               const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_3D);

    gl::ImageIndex index = gl::ImageIndex::Make3D(static_cast<GLint>(level));
    ANGLE_TRY(TextureD3D::subImageCompressed(context, index, area, format, unpack, pixels, 0));
    return commitRegion(context, index, area);
}

gl::Error TextureD3D_3D::copyImage(const gl::Context *context,
                                   GLenum target,
                                   size_t level,
                                   const gl::Rectangle &sourceArea,
                                   GLenum internalFormat,
                                   const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "Copying 3D textures is unimplemented.";
}

gl::Error TextureD3D_3D::copySubImage(const gl::Context *context,
                                      GLenum target,
                                      size_t imageLevel,
                                      const gl::Offset &destOffset,
                                      const gl::Rectangle &sourceArea,
                                      const gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_3D);

    GLint level = static_cast<GLint>(imageLevel);

    gl::Extents fbSize = source->getReadColorbuffer()->getSize();
    gl::Rectangle clippedSourceArea;
    if (!ClipRectangle(sourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &clippedSourceArea))
    {
        return gl::NoError();
    }
    const gl::Offset clippedDestOffset(destOffset.x + clippedSourceArea.x - sourceArea.x,
                                       destOffset.y + clippedSourceArea.y - sourceArea.y,
                                       destOffset.z);

    // Currently, copying directly to the storage is not possible because it's not possible to
    // create an SRV from a single layer of a 3D texture.  Instead, make sure the image is up to
    // date before the copy and then copy back to the storage afterwards if needed.
    // TODO: Investigate 3D blits in D3D11.

    bool syncTexStorage = mTexStorage && isLevelComplete(level);
    if (syncTexStorage)
    {
        gl::ImageIndex index = gl::ImageIndex::Make3D(level);
        ANGLE_TRY(mImageArray[level]->copyFromTexStorage(context, index, mTexStorage));
    }
    ANGLE_TRY(mImageArray[level]->copyFromFramebuffer(context, clippedDestOffset, clippedSourceArea,
                                                      source));
    mDirtyImages = true;

    if (syncTexStorage)
    {
        ANGLE_TRY(updateStorageLevel(context, level));
    }

    return gl::NoError();
}

gl::Error TextureD3D_3D::setStorage(const gl::Context *context,
                                    GLenum target,
                                    size_t levels,
                                    GLenum internalFormat,
                                    const gl::Extents &size)
{
    ASSERT(target == GL_TEXTURE_3D);

    for (size_t level = 0; level < levels; level++)
    {
        gl::Extents levelSize(std::max(1, size.width >> level),
                              std::max(1, size.height >> level),
                              std::max(1, size.depth >> level));
        mImageArray[level]->redefine(GL_TEXTURE_3D, internalFormat, levelSize, true);
    }

    for (size_t level = levels; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        mImageArray[level]->redefine(GL_TEXTURE_3D, GL_NONE, gl::Extents(0, 0, 0), true);
    }

    // TODO(geofflang): Verify storage creation had no errors
    bool renderTarget = IsRenderTargetUsage(mState.getUsage());
    TexStoragePointer storage(context);
    storage.reset(mRenderer->createTextureStorage3D(internalFormat, renderTarget, size.width,
                                                    size.height, size.depth,
                                                    static_cast<int>(levels)));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ANGLE_TRY(updateStorage(context));

    mImmutable = true;

    return gl::NoError();
}

gl::Error TextureD3D_3D::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_3D::releaseTexImage(const gl::Context *context)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_3D::initMipmapImages(const gl::Context *context)
{
    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();
    // Purge array levels baseLevel + 1 through q and reset them to represent the generated mipmap
    // levels.
    for (GLuint level = baseLevel + 1; level <= maxLevel; level++)
    {
        gl::Extents levelSize(std::max(getLevelZeroWidth() >> level, 1),
                              std::max(getLevelZeroHeight() >> level, 1),
                              std::max(getLevelZeroDepth() >> level, 1));
        ANGLE_TRY(redefineImage(context, level, getBaseLevelInternalFormat(), levelSize, false));
    }

    return gl::NoError();
}

gl::Error TextureD3D_3D::getRenderTarget(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         RenderTargetD3D **outRT)
{
    // ensure the underlying texture is created
    ANGLE_TRY(ensureRenderTarget(context));

    if (index.hasLayer())
    {
        ANGLE_TRY(updateStorage(context));
    }
    else
    {
        ANGLE_TRY(updateStorageLevel(context, index.mipIndex));
    }

    return mTexStorage->getRenderTarget(context, index, outRT);
}

gl::Error TextureD3D_3D::initializeStorage(const gl::Context *context, bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return gl::NoError();
    }

    // do not attempt to create storage for nonexistant data
    if (!isLevelComplete(getBaseLevel()))
    {
        return gl::NoError();
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mState.getUsage()));

    TexStoragePointer storage(context);
    ANGLE_TRY(createCompleteStorage(createRenderTarget, &storage));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ASSERT(mTexStorage);

    // flush image data to the storage
    ANGLE_TRY(updateStorage(context));

    return gl::NoError();
}

gl::Error TextureD3D_3D::createCompleteStorage(bool renderTarget,
                                               TexStoragePointer *outStorage) const
{
    GLsizei width         = getLevelZeroWidth();
    GLsizei height        = getLevelZeroHeight();
    GLsizei depth         = getLevelZeroDepth();
    GLenum internalFormat = getBaseLevelInternalFormat();

    ASSERT(width > 0 && height > 0 && depth > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(width, height, depth));

    // TODO: Verify creation of the storage succeeded
    outStorage->reset(mRenderer->createTextureStorage3D(internalFormat, renderTarget, width, height,
                                                        depth, levels));

    return gl::NoError();
}

gl::Error TextureD3D_3D::setCompleteTexStorage(const gl::Context *context,
                                               TextureStorage *newCompleteTexStorage)
{
    ANGLE_TRY(releaseTexStorage(context));
    mTexStorage = newCompleteTexStorage;
    mDirtyImages = true;

    // We do not support managed 3D storage, as that is D3D9/ES2-only
    ASSERT(!mTexStorage->isManaged());

    return gl::NoError();
}

gl::Error TextureD3D_3D::updateStorage(const gl::Context *context)
{
    if (!mDirtyImages)
    {
        return gl::NoError();
    }

    ASSERT(mTexStorage != nullptr);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int level = 0; level < storageLevels; level++)
    {
        if (mImageArray[level]->isDirty() && isLevelComplete(level))
        {
            ANGLE_TRY(updateStorageLevel(context, level));
        }
    }

    mDirtyImages = false;
    return gl::NoError();
}

bool TextureD3D_3D::isValidLevel(int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : 0);
}

bool TextureD3D_3D::isLevelComplete(int level) const
{
    ASSERT(level >= 0 && level < static_cast<int>(mImageArray.size()) &&
           mImageArray[level] != nullptr);

    if (isImmutable())
    {
        return true;
    }

    GLsizei width  = getLevelZeroWidth();
    GLsizei height = getLevelZeroHeight();
    GLsizei depth  = getLevelZeroDepth();

    if (width <= 0 || height <= 0 || depth <= 0)
    {
        return false;
    }

    if (level == static_cast<int>(getBaseLevel()))
    {
        return true;
    }

    ImageD3D *levelImage = mImageArray[level].get();

    if (levelImage->getInternalFormat() != getBaseLevelInternalFormat())
    {
        return false;
    }

    if (levelImage->getWidth() != std::max(1, width >> level))
    {
        return false;
    }

    if (levelImage->getHeight() != std::max(1, height >> level))
    {
        return false;
    }

    if (levelImage->getDepth() != std::max(1, depth >> level))
    {
        return false;
    }

    return true;
}

bool TextureD3D_3D::isImageComplete(const gl::ImageIndex &index) const
{
    return isLevelComplete(index.mipIndex);
}

gl::Error TextureD3D_3D::updateStorageLevel(const gl::Context *context, int level)
{
    ASSERT(level >= 0 && level < static_cast<int>(mImageArray.size()) &&
           mImageArray[level] != nullptr);
    ASSERT(isLevelComplete(level));

    if (mImageArray[level]->isDirty())
    {
        gl::ImageIndex index = gl::ImageIndex::Make3D(level);
        gl::Box region(0, 0, 0, getWidth(level), getHeight(level), getDepth(level));
        ANGLE_TRY(commitRegion(context, index, region));
    }

    return gl::NoError();
}

gl::Error TextureD3D_3D::redefineImage(const gl::Context *context,
                                       GLint level,
                                       GLenum internalformat,
                                       const gl::Extents &size,
                                       bool forceRelease)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth  = std::max(1, getLevelZeroWidth() >> level);
    const int storageHeight = std::max(1, getLevelZeroHeight() >> level);
    const int storageDepth  = std::max(1, getLevelZeroDepth() >> level);
    const GLenum storageFormat = getBaseLevelInternalFormat();

    mImageArray[level]->redefine(GL_TEXTURE_3D, internalformat, size, forceRelease);
    mDirtyImages = mDirtyImages || mImageArray[level]->isDirty();

    if (mTexStorage)
    {
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) || size.width != storageWidth ||
            size.height != storageHeight || size.depth != storageDepth ||
            internalformat != storageFormat)  // Discard mismatched storage
        {
            markAllImagesDirty();
            ANGLE_TRY(releaseTexStorage(context));
        }
    }

    return gl::NoError();
}

gl::ImageIndexIterator TextureD3D_3D::imageIterator() const
{
    return gl::ImageIndexIterator::Make3D(0, mTexStorage->getLevelCount(),
                                          gl::ImageIndex::ENTIRE_LEVEL, gl::ImageIndex::ENTIRE_LEVEL);
}

gl::ImageIndex TextureD3D_3D::getImageIndex(GLint mip, GLint /*layer*/) const
{
    // The "layer" here does not apply to 3D images. We use one Image per mip.
    return gl::ImageIndex::Make3D(mip);
}

bool TextureD3D_3D::isValidIndex(const gl::ImageIndex &index) const
{
    return (mTexStorage && index.type == GL_TEXTURE_3D &&
            index.mipIndex >= 0 && index.mipIndex < mTexStorage->getLevelCount());
}

void TextureD3D_3D::markAllImagesDirty()
{
    for (int i = 0; i < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; i++)
    {
        mImageArray[i]->markDirty();
    }
    mDirtyImages = true;
}

GLint TextureD3D_3D::getLevelZeroDepth() const
{
    ASSERT(gl::CountLeadingZeros(static_cast<uint32_t>(getBaseLevelDepth())) > getBaseLevel());
    return getBaseLevelDepth() << getBaseLevel();
}

TextureD3D_2DArray::TextureD3D_2DArray(const gl::TextureState &state, RendererD3D *renderer)
    : TextureD3D(state, renderer)
{
    for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++level)
    {
        mLayerCounts[level] = 0;
        mImageArray[level]  = nullptr;
    }
}

gl::Error TextureD3D_2DArray::onDestroy(const gl::Context *context)
{
    // Delete the Images before the TextureStorage. Images might be relying on the TextureStorage
    // for some of their data. If TextureStorage is deleted before the Images, then their data will
    // be wastefully copied back from the GPU before we delete the Images.
    deleteImages();
    return TextureD3D::onDestroy(context);
}

TextureD3D_2DArray::~TextureD3D_2DArray()
{
}

ImageD3D *TextureD3D_2DArray::getImage(int level, int layer) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT((layer == 0 && mLayerCounts[level] == 0) ||
           layer < mLayerCounts[level]);
    return (mImageArray[level] ? mImageArray[level][layer] : nullptr);
}

ImageD3D *TextureD3D_2DArray::getImage(const gl::ImageIndex &index) const
{
    ASSERT(index.mipIndex < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    ASSERT(index.layerIndex != gl::ImageIndex::ENTIRE_LEVEL);
    ASSERT((index.layerIndex == 0 && mLayerCounts[index.mipIndex] == 0) ||
           index.layerIndex < mLayerCounts[index.mipIndex]);
    ASSERT(index.type == GL_TEXTURE_2D_ARRAY);
    return (mImageArray[index.mipIndex] ? mImageArray[index.mipIndex][index.layerIndex] : nullptr);
}

GLsizei TextureD3D_2DArray::getLayerCount(int level) const
{
    ASSERT(level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS);
    return mLayerCounts[level];
}

GLsizei TextureD3D_2DArray::getWidth(GLint level) const
{
    return (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS && mLayerCounts[level] > 0) ? mImageArray[level][0]->getWidth() : 0;
}

GLsizei TextureD3D_2DArray::getHeight(GLint level) const
{
    return (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS && mLayerCounts[level] > 0) ? mImageArray[level][0]->getHeight() : 0;
}

GLenum TextureD3D_2DArray::getInternalFormat(GLint level) const
{
    return (level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS && mLayerCounts[level] > 0) ? mImageArray[level][0]->getInternalFormat() : GL_NONE;
}

bool TextureD3D_2DArray::isDepth(GLint level) const
{
    return gl::GetSizedInternalFormatInfo(getInternalFormat(level)).depthBits > 0;
}

gl::Error TextureD3D_2DArray::setEGLImageTarget(const gl::Context *context,
                                                GLenum target,
                                                egl::Image *image)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DArray::setImage(const gl::Context *context,
                                       GLenum target,
                                       size_t imageLevel,
                                       GLenum internalFormat,
                                       const gl::Extents &size,
                                       GLenum format,
                                       GLenum type,
                                       const gl::PixelUnpackState &unpack,
                                       const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat, type);

    GLint level = static_cast<GLint>(imageLevel);
    ANGLE_TRY(redefineImage(context, level, formatInfo.sizedInternalFormat, size, false));

    GLsizei inputDepthPitch              = 0;
    ANGLE_TRY_RESULT(formatInfo.computeDepthPitch(type, size.width, size.height, unpack.alignment,
                                                  unpack.rowLength, unpack.imageHeight),
                     inputDepthPitch);

    for (int i = 0; i < size.depth; i++)
    {
        const ptrdiff_t layerOffset = (inputDepthPitch * i);
        gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, i);
        ANGLE_TRY(setImageImpl(context, index, type, unpack, pixels, layerOffset));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::setSubImage(const gl::Context *context,
                                          GLenum target,
                                          size_t imageLevel,
                                          const gl::Box &area,
                                          GLenum format,
                                          GLenum type,
                                          const gl::PixelUnpackState &unpack,
                                          const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);
    GLint level                          = static_cast<GLint>(imageLevel);
    const gl::InternalFormat &formatInfo =
        gl::GetInternalFormatInfo(getInternalFormat(level), type);
    GLsizei inputDepthPitch              = 0;
    ANGLE_TRY_RESULT(formatInfo.computeDepthPitch(type, area.width, area.height, unpack.alignment,
                                                  unpack.rowLength, unpack.imageHeight),
                     inputDepthPitch);

    for (int i = 0; i < area.depth; i++)
    {
        int layer = area.z + i;
        const ptrdiff_t layerOffset = (inputDepthPitch * i);

        gl::Box layerArea(area.x, area.y, 0, area.width, area.height, 1);

        gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, layer);
        ANGLE_TRY(TextureD3D::subImage(context, index, layerArea, format, type, unpack, pixels,
                                       layerOffset));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::setCompressedImage(const gl::Context *context,
                                                 GLenum target,
                                                 size_t imageLevel,
                                                 GLenum internalFormat,
                                                 const gl::Extents &size,
                                                 const gl::PixelUnpackState &unpack,
                                                 size_t imageSize,
                                                 const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    GLint level = static_cast<GLint>(imageLevel);
    // compressed formats don't have separate sized internal formats-- we can just use the compressed format directly
    ANGLE_TRY(redefineImage(context, level, internalFormat, size, false));

    const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(internalFormat);
    GLsizei inputDepthPitch              = 0;
    ANGLE_TRY_RESULT(
        formatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, size.width, size.height, 1, 0, 0),
        inputDepthPitch);

    for (int i = 0; i < size.depth; i++)
    {
        const ptrdiff_t layerOffset = (inputDepthPitch * i);

        gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, i);
        ANGLE_TRY(setCompressedImageImpl(context, index, unpack, pixels, layerOffset));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::setCompressedSubImage(const gl::Context *context,
                                                    GLenum target,
                                                    size_t level,
                                                    const gl::Box &area,
                                                    GLenum format,
                                                    const gl::PixelUnpackState &unpack,
                                                    size_t imageSize,
                                                    const uint8_t *pixels)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(format);
    GLsizei inputDepthPitch              = 0;
    ANGLE_TRY_RESULT(
        formatInfo.computeDepthPitch(GL_UNSIGNED_BYTE, area.width, area.height, 1, 0, 0),
        inputDepthPitch);

    for (int i = 0; i < area.depth; i++)
    {
        int layer = area.z + i;
        const ptrdiff_t layerOffset = (inputDepthPitch * i);

        gl::Box layerArea(area.x, area.y, 0, area.width, area.height, 1);

        gl::ImageIndex index = gl::ImageIndex::Make2DArray(static_cast<GLint>(level), layer);
        ANGLE_TRY(TextureD3D::subImageCompressed(context, index, layerArea, format, unpack, pixels,
                                                 layerOffset));
        ANGLE_TRY(commitRegion(context, index, layerArea));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::copyImage(const gl::Context *context,
                                        GLenum target,
                                        size_t level,
                                        const gl::Rectangle &sourceArea,
                                        GLenum internalFormat,
                                        const gl::Framebuffer *source)
{
    UNIMPLEMENTED();
    return gl::InternalError() << "Copying 2D array textures is unimplemented.";
}

gl::Error TextureD3D_2DArray::copySubImage(const gl::Context *context,
                                           GLenum target,
                                           size_t imageLevel,
                                           const gl::Offset &destOffset,
                                           const gl::Rectangle &sourceArea,
                                           const gl::Framebuffer *source)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    GLint level          = static_cast<GLint>(imageLevel);
    gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, destOffset.z);

    gl::Extents fbSize = source->getReadColorbuffer()->getSize();
    gl::Rectangle clippedSourceArea;
    if (!ClipRectangle(sourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &clippedSourceArea))
    {
        return gl::NoError();
    }
    const gl::Offset clippedDestOffset(destOffset.x + clippedSourceArea.x - sourceArea.x,
                                       destOffset.y + clippedSourceArea.y - sourceArea.y,
                                       destOffset.z);

    if (!canCreateRenderTargetForImage(index))
    {
        gl::Offset destLayerOffset(clippedDestOffset.x, clippedDestOffset.y, 0);
        ANGLE_TRY(mImageArray[level][clippedDestOffset.z]->copyFromFramebuffer(
            context, destLayerOffset, clippedSourceArea, source));
        mDirtyImages = true;
    }
    else
    {
        ANGLE_TRY(ensureRenderTarget(context));

        if (isValidLevel(level))
        {
            ANGLE_TRY(updateStorageLevel(context, level));
            ANGLE_TRY(
                mRenderer->copyImage2DArray(context, source, clippedSourceArea,
                                            gl::GetUnsizedFormat(getInternalFormat(getBaseLevel())),
                                            clippedDestOffset, mTexStorage, level));
        }
    }
    return gl::NoError();
}

gl::Error TextureD3D_2DArray::setStorage(const gl::Context *context,
                                         GLenum target,
                                         size_t levels,
                                         GLenum internalFormat,
                                         const gl::Extents &size)
{
    ASSERT(target == GL_TEXTURE_2D_ARRAY);

    deleteImages();

    for (size_t level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; level++)
    {
        gl::Extents levelLayerSize(std::max(1, size.width >> level),
                                   std::max(1, size.height >> level),
                                   1);

        mLayerCounts[level] = (level < levels ? size.depth : 0);

        if (mLayerCounts[level] > 0)
        {
            // Create new images for this level
            mImageArray[level] = new ImageD3D*[mLayerCounts[level]];

            for (int layer = 0; layer < mLayerCounts[level]; layer++)
            {
                mImageArray[level][layer] = mRenderer->createImage();
                mImageArray[level][layer]->redefine(GL_TEXTURE_2D_ARRAY, internalFormat, levelLayerSize, true);
            }
        }
    }

    // TODO(geofflang): Verify storage creation had no errors
    bool renderTarget = IsRenderTargetUsage(mState.getUsage());
    TexStoragePointer storage(context);
    storage.reset(mRenderer->createTextureStorage2DArray(internalFormat, renderTarget, size.width,
                                                         size.height, size.depth,
                                                         static_cast<int>(levels)));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ANGLE_TRY(updateStorage(context));

    mImmutable = true;

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DArray::releaseTexImage(const gl::Context *context)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DArray::initMipmapImages(const gl::Context *context)
{
    const GLuint baseLevel = mState.getEffectiveBaseLevel();
    const GLuint maxLevel  = mState.getMipmapMaxLevel();
    int baseWidth     = getLevelZeroWidth();
    int baseHeight    = getLevelZeroHeight();
    int baseDepth     = getLayerCount(getBaseLevel());
    GLenum baseFormat = getBaseLevelInternalFormat();

    // Purge array levels baseLevel + 1 through q and reset them to represent the generated mipmap
    // levels.
    for (GLuint level = baseLevel + 1u; level <= maxLevel; level++)
    {
        ASSERT((baseWidth >> level) > 0 || (baseHeight >> level) > 0);
        gl::Extents levelLayerSize(std::max(baseWidth >> level, 1),
                                   std::max(baseHeight >> level, 1),
                                   baseDepth);
        ANGLE_TRY(redefineImage(context, level, baseFormat, levelLayerSize, false));
    }

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::getRenderTarget(const gl::Context *context,
                                              const gl::ImageIndex &index,
                                              RenderTargetD3D **outRT)
{
    // ensure the underlying texture is created
    ANGLE_TRY(ensureRenderTarget(context));
    ANGLE_TRY(updateStorageLevel(context, index.mipIndex));
    return mTexStorage->getRenderTarget(context, index, outRT);
}

gl::Error TextureD3D_2DArray::initializeStorage(const gl::Context *context, bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return gl::NoError();
    }

    // do not attempt to create storage for nonexistant data
    if (!isLevelComplete(getBaseLevel()))
    {
        return gl::NoError();
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mState.getUsage()));

    TexStoragePointer storage(context);
    ANGLE_TRY(createCompleteStorage(createRenderTarget, &storage));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ASSERT(mTexStorage);

    // flush image data to the storage
    ANGLE_TRY(updateStorage(context));

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::createCompleteStorage(bool renderTarget,
                                                    TexStoragePointer *outStorage) const
{
    GLsizei width         = getLevelZeroWidth();
    GLsizei height        = getLevelZeroHeight();
    GLsizei depth         = getLayerCount(getBaseLevel());
    GLenum internalFormat = getBaseLevelInternalFormat();

    ASSERT(width > 0 && height > 0 && depth > 0);

    // use existing storage level count, when previously specified by TexStorage*D
    GLint levels = (mTexStorage ? mTexStorage->getLevelCount() : creationLevels(width, height, 1));

    // TODO(geofflang): Verify storage creation succeeds
    outStorage->reset(mRenderer->createTextureStorage2DArray(internalFormat, renderTarget, width,
                                                             height, depth, levels));

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::setCompleteTexStorage(const gl::Context *context,
                                                    TextureStorage *newCompleteTexStorage)
{
    ANGLE_TRY(releaseTexStorage(context));
    mTexStorage = newCompleteTexStorage;
    mDirtyImages = true;

    // We do not support managed 2D array storage, as managed storage is ES2/D3D9 only
    ASSERT(!mTexStorage->isManaged());

    return gl::NoError();
}

gl::Error TextureD3D_2DArray::updateStorage(const gl::Context *context)
{
    if (!mDirtyImages)
    {
        return gl::NoError();
    }

    ASSERT(mTexStorage != nullptr);
    GLint storageLevels = mTexStorage->getLevelCount();
    for (int level = 0; level < storageLevels; level++)
    {
        if (isLevelComplete(level))
        {
            ANGLE_TRY(updateStorageLevel(context, level));
        }
    }

    mDirtyImages = false;
    return gl::NoError();
}

bool TextureD3D_2DArray::isValidLevel(int level) const
{
    return (mTexStorage ? (level >= 0 && level < mTexStorage->getLevelCount()) : 0);
}

bool TextureD3D_2DArray::isLevelComplete(int level) const
{
    ASSERT(level >= 0 && level < (int)ArraySize(mImageArray));

    if (isImmutable())
    {
        return true;
    }

    GLsizei width  = getLevelZeroWidth();
    GLsizei height = getLevelZeroHeight();

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    // Layers check needs to happen after the above checks, otherwise out-of-range base level may be
    // queried.
    GLsizei layers = getLayerCount(getBaseLevel());

    if (layers <= 0)
    {
        return false;
    }

    if (level == static_cast<int>(getBaseLevel()))
    {
        return true;
    }

    if (getInternalFormat(level) != getInternalFormat(getBaseLevel()))
    {
        return false;
    }

    if (getWidth(level) != std::max(1, width >> level))
    {
        return false;
    }

    if (getHeight(level) != std::max(1, height >> level))
    {
        return false;
    }

    if (getLayerCount(level) != layers)
    {
        return false;
    }

    return true;
}

bool TextureD3D_2DArray::isImageComplete(const gl::ImageIndex &index) const
{
    return isLevelComplete(index.mipIndex);
}

gl::Error TextureD3D_2DArray::updateStorageLevel(const gl::Context *context, int level)
{
    ASSERT(level >= 0 && level < static_cast<int>(ArraySize(mLayerCounts)));
    ASSERT(isLevelComplete(level));

    for (int layer = 0; layer < mLayerCounts[level]; layer++)
    {
        ASSERT(mImageArray[level] != nullptr && mImageArray[level][layer] != nullptr);
        if (mImageArray[level][layer]->isDirty())
        {
            gl::ImageIndex index = gl::ImageIndex::Make2DArray(level, layer);
            gl::Box region(0, 0, 0, getWidth(level), getHeight(level), 1);
            ANGLE_TRY(commitRegion(context, index, region));
        }
    }

    return gl::NoError();
}

void TextureD3D_2DArray::deleteImages()
{
    for (int level = 0; level < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; ++level)
    {
        for (int layer = 0; layer < mLayerCounts[level]; ++layer)
        {
            delete mImageArray[level][layer];
        }
        delete[] mImageArray[level];
        mImageArray[level]  = nullptr;
        mLayerCounts[level] = 0;
    }
}

gl::Error TextureD3D_2DArray::redefineImage(const gl::Context *context,
                                            GLint level,
                                            GLenum internalformat,
                                            const gl::Extents &size,
                                            bool forceRelease)
{
    // If there currently is a corresponding storage texture image, it has these parameters
    const int storageWidth  = std::max(1, getLevelZeroWidth() >> level);
    const int storageHeight = std::max(1, getLevelZeroHeight() >> level);
    const GLuint baseLevel  = getBaseLevel();
    int storageDepth = 0;
    if (baseLevel < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS)
    {
        storageDepth = getLayerCount(baseLevel);
    }

    // Only reallocate the layers if the size doesn't match
    if (size.depth != mLayerCounts[level])
    {
        for (int layer = 0; layer < mLayerCounts[level]; layer++)
        {
            SafeDelete(mImageArray[level][layer]);
        }
        SafeDeleteArray(mImageArray[level]);
        mLayerCounts[level] = size.depth;

        if (size.depth > 0)
        {
            mImageArray[level] = new ImageD3D*[size.depth];
            for (int layer = 0; layer < mLayerCounts[level]; layer++)
            {
                mImageArray[level][layer] = mRenderer->createImage();
            }
        }
    }

    if (size.depth > 0)
    {
        for (int layer = 0; layer < mLayerCounts[level]; layer++)
        {
            mImageArray[level][layer]->redefine(GL_TEXTURE_2D_ARRAY, internalformat,
                                                gl::Extents(size.width, size.height, 1),
                                                forceRelease);
            mDirtyImages = mDirtyImages || mImageArray[level][layer]->isDirty();
        }
    }

    if (mTexStorage)
    {
        const GLenum storageFormat = getBaseLevelInternalFormat();
        const int storageLevels = mTexStorage->getLevelCount();

        if ((level >= storageLevels && storageLevels != 0) || size.width != storageWidth ||
            size.height != storageHeight || size.depth != storageDepth ||
            internalformat != storageFormat)  // Discard mismatched storage
        {
            markAllImagesDirty();
            ANGLE_TRY(releaseTexStorage(context));
        }
    }

    return gl::NoError();
}

gl::ImageIndexIterator TextureD3D_2DArray::imageIterator() const
{
    return gl::ImageIndexIterator::Make2DArray(0, mTexStorage->getLevelCount(), mLayerCounts);
}

gl::ImageIndex TextureD3D_2DArray::getImageIndex(GLint mip, GLint layer) const
{
    return gl::ImageIndex::Make2DArray(mip, layer);
}

bool TextureD3D_2DArray::isValidIndex(const gl::ImageIndex &index) const
{
    // Check for having a storage and the right type of index
    if (!mTexStorage || index.type != GL_TEXTURE_2D_ARRAY)
    {
        return false;
    }

    // Check the mip index
    if (index.mipIndex < 0 || index.mipIndex >= mTexStorage->getLevelCount())
    {
        return false;
    }

    // Check the layer index
    return (!index.hasLayer() || (index.layerIndex >= 0 && index.layerIndex < mLayerCounts[index.mipIndex]));
}

void TextureD3D_2DArray::markAllImagesDirty()
{
    for (int dirtyLevel = 0; dirtyLevel < gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS; dirtyLevel++)
    {
        for (int dirtyLayer = 0; dirtyLayer < mLayerCounts[dirtyLevel]; dirtyLayer++)
        {
            mImageArray[dirtyLevel][dirtyLayer]->markDirty();
        }
    }
    mDirtyImages = true;
}

TextureD3D_External::TextureD3D_External(const gl::TextureState &state, RendererD3D *renderer)
    : TextureD3D(state, renderer)
{
}

TextureD3D_External::~TextureD3D_External()
{
}

ImageD3D *TextureD3D_External::getImage(const gl::ImageIndex &index) const
{
    UNREACHABLE();
    return nullptr;
}

GLsizei TextureD3D_External::getLayerCount(int level) const
{
    return 1;
}

gl::Error TextureD3D_External::setImage(const gl::Context *context,
                                        GLenum target,
                                        size_t imageLevel,
                                        GLenum internalFormat,
                                        const gl::Extents &size,
                                        GLenum format,
                                        GLenum type,
                                        const gl::PixelUnpackState &unpack,
                                        const uint8_t *pixels)
{
    // Image setting is not supported for external images
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::setSubImage(const gl::Context *context,
                                           GLenum target,
                                           size_t imageLevel,
                                           const gl::Box &area,
                                           GLenum format,
                                           GLenum type,
                                           const gl::PixelUnpackState &unpack,
                                           const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::setCompressedImage(const gl::Context *context,
                                                  GLenum target,
                                                  size_t imageLevel,
                                                  GLenum internalFormat,
                                                  const gl::Extents &size,
                                                  const gl::PixelUnpackState &unpack,
                                                  size_t imageSize,
                                                  const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::setCompressedSubImage(const gl::Context *context,
                                                     GLenum target,
                                                     size_t level,
                                                     const gl::Box &area,
                                                     GLenum format,
                                                     const gl::PixelUnpackState &unpack,
                                                     size_t imageSize,
                                                     const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::copyImage(const gl::Context *context,
                                         GLenum target,
                                         size_t imageLevel,
                                         const gl::Rectangle &sourceArea,
                                         GLenum internalFormat,
                                         const gl::Framebuffer *source)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::copySubImage(const gl::Context *context,
                                            GLenum target,
                                            size_t imageLevel,
                                            const gl::Offset &destOffset,
                                            const gl::Rectangle &sourceArea,
                                            const gl::Framebuffer *source)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::setStorage(const gl::Context *context,
                                          GLenum target,
                                          size_t levels,
                                          GLenum internalFormat,
                                          const gl::Extents &size)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::setImageExternal(const gl::Context *context,
                                                GLenum target,
                                                egl::Stream *stream,
                                                const egl::Stream::GLTextureDescription &desc)
{
    ASSERT(target == GL_TEXTURE_EXTERNAL_OES);

    ANGLE_TRY(releaseTexStorage(context));

    // If the stream is null, the external image is unbound and we release the storage
    if (stream != nullptr)
    {
        mTexStorage = mRenderer->createTextureStorageExternal(stream, desc);
    }

    return gl::NoError();
}

gl::Error TextureD3D_External::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::releaseTexImage(const gl::Context *context)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::setEGLImageTarget(const gl::Context *context,
                                                 GLenum target,
                                                 egl::Image *image)
{
    EGLImageD3D *eglImaged3d = GetImplAs<EGLImageD3D>(image);

    // Pass in the RenderTargetD3D here: createTextureStorage can't generate an error.
    RenderTargetD3D *renderTargetD3D = nullptr;
    ANGLE_TRY(eglImaged3d->getRenderTarget(context, &renderTargetD3D));

    ANGLE_TRY(releaseTexStorage(context));
    mTexStorage = mRenderer->createTextureStorageEGLImage(eglImaged3d, renderTargetD3D);

    return gl::NoError();
}

gl::Error TextureD3D_External::initMipmapImages(const gl::Context *context)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_External::getRenderTarget(const gl::Context *context,
                                               const gl::ImageIndex &index,
                                               RenderTargetD3D **outRT)
{
    UNREACHABLE();
    return gl::InternalError();
}

bool TextureD3D_External::isImageComplete(const gl::ImageIndex &index) const
{
    return (index.mipIndex == 0) ? (mTexStorage != nullptr) : false;
}

gl::Error TextureD3D_External::initializeStorage(const gl::Context *context, bool renderTarget)
{
    // Texture storage is created when an external image is bound
    ASSERT(mTexStorage);
    return gl::NoError();
}

gl::Error TextureD3D_External::createCompleteStorage(bool renderTarget,
                                                     TexStoragePointer *outStorage) const
{
    UNREACHABLE();
    return gl::NoError();
}

gl::Error TextureD3D_External::setCompleteTexStorage(const gl::Context *context,
                                                     TextureStorage *newCompleteTexStorage)
{
    UNREACHABLE();
    return gl::NoError();
}

gl::Error TextureD3D_External::updateStorage(const gl::Context *context)
{
    // Texture storage does not need to be updated since it is already loaded with the latest
    // external image
    ASSERT(mTexStorage);
    return gl::NoError();
}

gl::ImageIndexIterator TextureD3D_External::imageIterator() const
{
    return gl::ImageIndexIterator::Make2D(0, mTexStorage->getLevelCount());
}

gl::ImageIndex TextureD3D_External::getImageIndex(GLint mip, GLint /*layer*/) const
{
    // "layer" does not apply to 2D Textures.
    return gl::ImageIndex::Make2D(mip);
}

bool TextureD3D_External::isValidIndex(const gl::ImageIndex &index) const
{
    return (mTexStorage && index.type == GL_TEXTURE_EXTERNAL_OES && index.mipIndex == 0);
}

void TextureD3D_External::markAllImagesDirty()
{
    UNREACHABLE();
}

TextureD3D_2DMultisample::TextureD3D_2DMultisample(const gl::TextureState &state,
                                                   RendererD3D *renderer)
    : TextureD3D(state, renderer)
{
}

TextureD3D_2DMultisample::~TextureD3D_2DMultisample()
{
}

ImageD3D *TextureD3D_2DMultisample::getImage(const gl::ImageIndex &index) const
{
    return nullptr;
}

gl::Error TextureD3D_2DMultisample::setImage(const gl::Context *context,
                                             GLenum target,
                                             size_t level,
                                             GLenum internalFormat,
                                             const gl::Extents &size,
                                             GLenum format,
                                             GLenum type,
                                             const gl::PixelUnpackState &unpack,
                                             const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::setSubImage(const gl::Context *context,
                                                GLenum target,
                                                size_t level,
                                                const gl::Box &area,
                                                GLenum format,
                                                GLenum type,
                                                const gl::PixelUnpackState &unpack,
                                                const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::setCompressedImage(const gl::Context *context,
                                                       GLenum target,
                                                       size_t level,
                                                       GLenum internalFormat,
                                                       const gl::Extents &size,
                                                       const gl::PixelUnpackState &unpack,
                                                       size_t imageSize,
                                                       const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::setCompressedSubImage(const gl::Context *context,
                                                          GLenum target,
                                                          size_t level,
                                                          const gl::Box &area,
                                                          GLenum format,
                                                          const gl::PixelUnpackState &unpack,
                                                          size_t imageSize,
                                                          const uint8_t *pixels)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::copyImage(const gl::Context *context,
                                              GLenum target,
                                              size_t level,
                                              const gl::Rectangle &sourceArea,
                                              GLenum internalFormat,
                                              const gl::Framebuffer *source)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::copySubImage(const gl::Context *context,
                                                 GLenum target,
                                                 size_t level,
                                                 const gl::Offset &destOffset,
                                                 const gl::Rectangle &sourceArea,
                                                 const gl::Framebuffer *source)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::setStorageMultisample(const gl::Context *context,
                                                          GLenum target,
                                                          GLsizei samples,
                                                          GLint internalFormat,
                                                          const gl::Extents &size,
                                                          bool fixedSampleLocations)
{
    ASSERT(target == GL_TEXTURE_2D_MULTISAMPLE && size.depth == 1);

    TexStoragePointer storage(context);
    storage.reset(mRenderer->createTextureStorage2DMultisample(internalFormat, size.width,
                                                               size.height, static_cast<int>(0),
                                                               samples, fixedSampleLocations));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ANGLE_TRY(updateStorage(context));

    mImmutable = false;

    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    UNREACHABLE();
    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::releaseTexImage(const gl::Context *context)
{
    UNREACHABLE();
    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::setEGLImageTarget(const gl::Context *context,
                                                      GLenum target,
                                                      egl::Image *image)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error TextureD3D_2DMultisample::getRenderTarget(const gl::Context *context,
                                                    const gl::ImageIndex &index,
                                                    RenderTargetD3D **outRT)
{
    ASSERT(!index.hasLayer());

    // ensure the underlying texture is created
    ANGLE_TRY(ensureRenderTarget(context));

    return mTexStorage->getRenderTarget(context, index, outRT);
}

gl::ImageIndexIterator TextureD3D_2DMultisample::imageIterator() const
{
    return gl::ImageIndexIterator::Make2DMultisample();
}

gl::ImageIndex TextureD3D_2DMultisample::getImageIndex(GLint mip, GLint layer) const
{
    return gl::ImageIndex::Make2DMultisample();
}

bool TextureD3D_2DMultisample::isValidIndex(const gl::ImageIndex &index) const
{
    return (mTexStorage && index.type == GL_TEXTURE_2D_MULTISAMPLE && index.mipIndex == 0);
}

GLsizei TextureD3D_2DMultisample::getLayerCount(int level) const
{
    return 1;
}

void TextureD3D_2DMultisample::markAllImagesDirty()
{
}

gl::Error TextureD3D_2DMultisample::initializeStorage(const gl::Context *context, bool renderTarget)
{
    // Only initialize the first time this texture is used as a render target or shader resource
    if (mTexStorage)
    {
        return gl::NoError();
    }

    bool createRenderTarget = (renderTarget || IsRenderTargetUsage(mState.getUsage()));

    TexStoragePointer storage(context);
    ANGLE_TRY(createCompleteStorage(createRenderTarget, &storage));

    ANGLE_TRY(setCompleteTexStorage(context, storage.get()));
    storage.release();

    ASSERT(mTexStorage);

    // flush image data to the storage
    ANGLE_TRY(updateStorage(context));

    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::createCompleteStorage(bool renderTarget,
                                                          TexStoragePointer *outStorage) const
{
    outStorage->reset(mTexStorage);

    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::setCompleteTexStorage(const gl::Context *context,
                                                          TextureStorage *newCompleteTexStorage)
{
    ANGLE_TRY(releaseTexStorage(context));
    mTexStorage = newCompleteTexStorage;

    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::updateStorage(const gl::Context *context)
{
    return gl::NoError();
}

gl::Error TextureD3D_2DMultisample::initMipmapImages(const gl::Context *context)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

bool TextureD3D_2DMultisample::isImageComplete(const gl::ImageIndex &index) const
{
    return true;
}

}  // namespace rx
