//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureVk.cpp:
//    Implements the class methods for TextureVk.
//

#include "libANGLE/renderer/vulkan/TextureVk.h"

#include "common/debug.h"
#include "image_util/generatemip.inc"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Image.h"
#include "libANGLE/MemoryObject.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/ImageVk.h"
#include "libANGLE/renderer/vulkan/MemoryObjectVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/trace.h"

namespace rx
{
namespace
{
constexpr VkImageUsageFlags kDrawStagingImageFlags =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

constexpr VkImageUsageFlags kTransferStagingImageFlags =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

constexpr VkFormatFeatureFlags kBlitFeatureFlags =
    VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;

bool CanCopyWithTransfer(RendererVk *renderer,
                         const vk::Format &srcFormat,
                         const vk::Format &destFormat)
{
    // NOTE(syoussefi): technically, you can transfer between formats as long as they have the same
    // size and are compatible, but for now, let's just support same-format copies with transfer.
    return srcFormat.internalFormat == destFormat.internalFormat &&
           renderer->hasImageFormatFeatureBits(srcFormat.vkImageFormat,
                                               VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) &&
           renderer->hasImageFormatFeatureBits(destFormat.vkImageFormat,
                                               VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
}

bool CanCopyWithDraw(RendererVk *renderer,
                     const vk::Format &srcFormat,
                     const vk::Format &destFormat)
{
    return renderer->hasImageFormatFeatureBits(srcFormat.vkImageFormat,
                                               VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
           renderer->hasImageFormatFeatureBits(destFormat.vkImageFormat,
                                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
}

bool ForceCPUPathForCopy(RendererVk *renderer, const vk::ImageHelper &image)
{
    return image.getLayerCount() > 1 && renderer->getFeatures().forceCPUPathForCubeMapCopy.enabled;
}

uint32_t GetImageLayerCountForView(const vk::ImageHelper &image)
{
    // Depth > 1 means this is a 3D texture and depth is our layer count
    return image.getExtents().depth > 1 ? image.getExtents().depth : image.getLayerCount();
}

bool HasBothDepthAndStencilAspects(VkImageAspectFlags aspectFlags)
{
    constexpr VkImageAspectFlags kDepthStencilAspects =
        VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;
    return (aspectFlags & kDepthStencilAspects) == kDepthStencilAspects;
}
}  // anonymous namespace

TextureVk::TextureVkViews::TextureVkViews() {}
TextureVk::TextureVkViews::~TextureVkViews() {}

void TextureVk::TextureVkViews::release(ContextVk *contextVk, Serial currentSerial)
{
    contextVk->releaseObject(currentSerial, &mDrawBaseLevelImageView);
    contextVk->releaseObject(currentSerial, &mReadBaseLevelImageView);
    contextVk->releaseObject(currentSerial, &mReadMipmapImageView);
    contextVk->releaseObject(currentSerial, &mFetchBaseLevelImageView);
    contextVk->releaseObject(currentSerial, &mFetchMipmapImageView);
}

angle::Result TextureVk::generateMipmapLevelsWithCPU(ContextVk *contextVk,
                                                     const angle::Format &sourceFormat,
                                                     GLuint layer,
                                                     GLuint firstMipLevel,
                                                     GLuint maxMipLevel,
                                                     const size_t sourceWidth,
                                                     const size_t sourceHeight,
                                                     const size_t sourceRowPitch,
                                                     uint8_t *sourceData)
{
    size_t previousLevelWidth    = sourceWidth;
    size_t previousLevelHeight   = sourceHeight;
    uint8_t *previousLevelData   = sourceData;
    size_t previousLevelRowPitch = sourceRowPitch;

    for (GLuint currentMipLevel = firstMipLevel; currentMipLevel <= maxMipLevel; currentMipLevel++)
    {
        // Compute next level width and height.
        size_t mipWidth  = std::max<size_t>(1, previousLevelWidth >> 1);
        size_t mipHeight = std::max<size_t>(1, previousLevelHeight >> 1);

        // With the width and height of the next mip, we can allocate the next buffer we need.
        uint8_t *destData   = nullptr;
        size_t destRowPitch = mipWidth * sourceFormat.pixelBytes;

        size_t mipAllocationSize = destRowPitch * mipHeight;
        gl::Extents mipLevelExtents(static_cast<int>(mipWidth), static_cast<int>(mipHeight), 1);

        ANGLE_TRY(mImage->stageSubresourceUpdateAndGetData(
            contextVk, mipAllocationSize,
            gl::ImageIndex::MakeFromType(mState.getType(), currentMipLevel, layer), mipLevelExtents,
            gl::Offset(), &destData));
        onStagingBufferChange();

        // Generate the mipmap into that new buffer
        sourceFormat.mipGenerationFunction(previousLevelWidth, previousLevelHeight, 1,
                                           previousLevelData, previousLevelRowPitch, 0, destData,
                                           destRowPitch, 0);

        // Swap for the next iteration
        previousLevelWidth    = mipWidth;
        previousLevelHeight   = mipHeight;
        previousLevelData     = destData;
        previousLevelRowPitch = destRowPitch;
    }

    return angle::Result::Continue;
}

// TextureVk implementation.
TextureVk::TextureVk(const gl::TextureState &state, RendererVk *renderer)
    : TextureImpl(state),
      mOwnsImage(false),
      mImageNativeType(gl::TextureType::InvalidEnum),
      mImageLayerOffset(0),
      mImageLevelOffset(0),
      mImage(nullptr),
      mStagingBufferInitialSize(vk::kStagingBufferSize)
{}

TextureVk::~TextureVk() = default;

void TextureVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    releaseAndDeleteImage(contextVk);
    contextVk->releaseObject(contextVk->getCurrentQueueSerial(), &mSampler);
}

angle::Result TextureVk::setImage(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  GLenum internalFormat,
                                  const gl::Extents &size,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  const uint8_t *pixels)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat, type);

    return setImageImpl(context, index, formatInfo, size, type, unpack, pixels);
}

angle::Result TextureVk::setSubImage(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     const gl::Box &area,
                                     GLenum format,
                                     GLenum type,
                                     const gl::PixelUnpackState &unpack,
                                     gl::Buffer *unpackBuffer,
                                     const uint8_t *pixels)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(format, type);
    ContextVk *contextVk                 = vk::GetImpl(context);
    const gl::ImageDesc &levelDesc       = mState.getImageDesc(index);
    const vk::Format &vkFormat =
        contextVk->getRenderer()->getFormat(levelDesc.format.info->sizedInternalFormat);

    return setSubImageImpl(context, index, area, formatInfo, type, unpack, pixels, vkFormat);
}

angle::Result TextureVk::setCompressedImage(const gl::Context *context,
                                            const gl::ImageIndex &index,
                                            GLenum internalFormat,
                                            const gl::Extents &size,
                                            const gl::PixelUnpackState &unpack,
                                            size_t imageSize,
                                            const uint8_t *pixels)
{
    const gl::InternalFormat &formatInfo = gl::GetSizedInternalFormatInfo(internalFormat);

    return setImageImpl(context, index, formatInfo, size, GL_UNSIGNED_BYTE, unpack, pixels);
}

angle::Result TextureVk::setCompressedSubImage(const gl::Context *context,
                                               const gl::ImageIndex &index,
                                               const gl::Box &area,
                                               GLenum format,
                                               const gl::PixelUnpackState &unpack,
                                               size_t imageSize,
                                               const uint8_t *pixels)
{

    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(format, GL_UNSIGNED_BYTE);
    ContextVk *contextVk                 = vk::GetImpl(context);
    const gl::ImageDesc &levelDesc       = mState.getImageDesc(index);
    const vk::Format &vkFormat =
        contextVk->getRenderer()->getFormat(levelDesc.format.info->sizedInternalFormat);

    return setSubImageImpl(context, index, area, formatInfo, GL_UNSIGNED_BYTE, unpack, pixels,
                           vkFormat);
}

angle::Result TextureVk::setImageImpl(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      const gl::InternalFormat &formatInfo,
                                      const gl::Extents &size,
                                      GLenum type,
                                      const gl::PixelUnpackState &unpack,
                                      const uint8_t *pixels)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    const vk::Format &vkFormat = renderer->getFormat(formatInfo.sizedInternalFormat);

    ANGLE_TRY(redefineImage(context, index, vkFormat, size));

    // Early-out on empty textures, don't create a zero-sized storage.
    if (size.empty())
    {
        return angle::Result::Continue;
    }

    return setSubImageImpl(context, index, gl::Box(0, 0, 0, size.width, size.height, size.depth),
                           formatInfo, type, unpack, pixels, vkFormat);
}

angle::Result TextureVk::setSubImageImpl(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         const gl::Box &area,
                                         const gl::InternalFormat &formatInfo,
                                         GLenum type,
                                         const gl::PixelUnpackState &unpack,
                                         const uint8_t *pixels,
                                         const vk::Format &vkFormat)
{
    ContextVk *contextVk     = vk::GetImpl(context);
    const gl::State &glState = contextVk->getState();
    gl::Buffer *unpackBuffer = glState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    if (unpackBuffer)
    {
        BufferVk *unpackBufferVk = vk::GetImpl(unpackBuffer);
        void *mapPtr             = nullptr;
        ANGLE_TRY(unpackBufferVk->mapImpl(contextVk, &mapPtr));
        const uint8_t *source =
            static_cast<const uint8_t *>(mapPtr) + reinterpret_cast<ptrdiff_t>(pixels);

        ANGLE_TRY(mImage->stageSubresourceUpdate(
            contextVk, getNativeImageIndex(index), gl::Extents(area.width, area.height, area.depth),
            gl::Offset(area.x, area.y, area.z), formatInfo, unpack, type, source, vkFormat));

        unpackBufferVk->unmapImpl(contextVk);

        onStagingBufferChange();
    }
    else if (pixels)
    {
        ANGLE_TRY(mImage->stageSubresourceUpdate(
            contextVk, getNativeImageIndex(index), gl::Extents(area.width, area.height, area.depth),
            gl::Offset(area.x, area.y, area.z), formatInfo, unpack, type, pixels, vkFormat));
        onStagingBufferChange();
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::copyImage(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Rectangle &sourceArea,
                                   GLenum internalFormat,
                                   gl::Framebuffer *source)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();

    gl::Extents newImageSize(sourceArea.width, sourceArea.height, 1);
    const gl::InternalFormat &internalFormatInfo =
        gl::GetInternalFormatInfo(internalFormat, GL_UNSIGNED_BYTE);
    const vk::Format &vkFormat = renderer->getFormat(internalFormatInfo.sizedInternalFormat);

    ANGLE_TRY(redefineImage(context, index, vkFormat, newImageSize));
    return copySubImageImpl(context, index, gl::Offset(0, 0, 0), sourceArea, internalFormatInfo,
                            source);
}

angle::Result TextureVk::copySubImage(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      const gl::Offset &destOffset,
                                      const gl::Rectangle &sourceArea,
                                      gl::Framebuffer *source)
{
    const gl::InternalFormat &currentFormat = *mState.getImageDesc(index).format.info;
    return copySubImageImpl(context, index, destOffset, sourceArea, currentFormat, source);
}

angle::Result TextureVk::copyTexture(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     GLenum internalFormat,
                                     GLenum type,
                                     size_t sourceLevel,
                                     bool unpackFlipY,
                                     bool unpackPremultiplyAlpha,
                                     bool unpackUnmultiplyAlpha,
                                     const gl::Texture *source)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();

    TextureVk *sourceVk = vk::GetImpl(source);
    const gl::ImageDesc &sourceImageDesc =
        sourceVk->mState.getImageDesc(NonCubeTextureTypeToTarget(source->getType()), sourceLevel);
    gl::Rectangle sourceArea(0, 0, sourceImageDesc.size.width, sourceImageDesc.size.height);

    const gl::InternalFormat &destFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    const vk::Format &destVkFormat = renderer->getFormat(destFormatInfo.sizedInternalFormat);

    ANGLE_TRY(redefineImage(context, index, destVkFormat, sourceImageDesc.size));

    return copySubTextureImpl(vk::GetImpl(context), index, gl::kOffsetZero, destFormatInfo,
                              sourceLevel, sourceArea, unpackFlipY, unpackPremultiplyAlpha,
                              unpackUnmultiplyAlpha, sourceVk);
}

angle::Result TextureVk::copySubTexture(const gl::Context *context,
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
    return copySubTextureImpl(vk::GetImpl(context), index, destOffset, destFormatInfo, sourceLevel,
                              sourceBox.toRect(), unpackFlipY, unpackPremultiplyAlpha,
                              unpackUnmultiplyAlpha, vk::GetImpl(source));
}

angle::Result TextureVk::copyCompressedTexture(const gl::Context *context,
                                               const gl::Texture *source)
{
    ContextVk *contextVk = vk::GetImpl(context);
    TextureVk *sourceVk  = vk::GetImpl(source);

    gl::TextureTarget sourceTarget = NonCubeTextureTypeToTarget(source->getType());
    constexpr GLint sourceLevel    = 0;
    constexpr GLint destLevel      = 0;

    const gl::InternalFormat &internalFormat = *source->getFormat(sourceTarget, sourceLevel).info;
    const vk::Format &vkFormat =
        contextVk->getRenderer()->getFormat(internalFormat.sizedInternalFormat);
    const gl::Extents size(static_cast<int>(source->getWidth(sourceTarget, sourceLevel)),
                           static_cast<int>(source->getHeight(sourceTarget, sourceLevel)), 1);
    const gl::ImageIndex destIndex = gl::ImageIndex::MakeFromTarget(sourceTarget, destLevel, 1);

    ANGLE_TRY(redefineImage(context, destIndex, vkFormat, size));

    ANGLE_TRY(sourceVk->ensureImageInitialized(contextVk));

    return copySubImageImplWithTransfer(
        contextVk, destIndex, gl::Offset(0, 0, 0), vkFormat, sourceLevel, 0,
        gl::Rectangle(0, 0, size.width, size.height), &sourceVk->getImage());
}

angle::Result TextureVk::copySubImageImpl(const gl::Context *context,
                                          const gl::ImageIndex &index,
                                          const gl::Offset &destOffset,
                                          const gl::Rectangle &sourceArea,
                                          const gl::InternalFormat &internalFormat,
                                          gl::Framebuffer *source)
{
    gl::Extents fbSize = source->getReadColorAttachment()->getSize();
    gl::Rectangle clippedSourceArea;
    if (!ClipRectangle(sourceArea, gl::Rectangle(0, 0, fbSize.width, fbSize.height),
                       &clippedSourceArea))
    {
        return angle::Result::Continue;
    }

    ContextVk *contextVk         = vk::GetImpl(context);
    RendererVk *renderer         = contextVk->getRenderer();
    FramebufferVk *framebufferVk = vk::GetImpl(source);

    const gl::ImageIndex offsetImageIndex = getNativeImageIndex(index);

    // If negative offsets are given, clippedSourceArea ensures we don't read from those offsets.
    // However, that changes the sourceOffset->destOffset mapping.  Here, destOffset is shifted by
    // the same amount as clipped to correct the error.
    VkImageType imageType = gl_vk::GetImageType(mState.getType());
    int zOffset           = (imageType == VK_IMAGE_TYPE_3D) ? destOffset.z : 0;
    const gl::Offset modifiedDestOffset(destOffset.x + clippedSourceArea.x - sourceArea.x,
                                        destOffset.y + clippedSourceArea.y - sourceArea.y, zOffset);

    RenderTargetVk *colorReadRT = framebufferVk->getColorReadRenderTarget();

    const vk::Format &srcFormat  = colorReadRT->getImageFormat();
    const vk::Format &destFormat = renderer->getFormat(internalFormat.sizedInternalFormat);

    bool isViewportFlipY = contextVk->isViewportFlipEnabledForReadFBO();

    // If it's possible to perform the copy with a transfer, that's the best option.
    if (!isViewportFlipY && CanCopyWithTransfer(renderer, srcFormat, destFormat))
    {
        return copySubImageImplWithTransfer(contextVk, offsetImageIndex, modifiedDestOffset,
                                            destFormat, colorReadRT->getLevelIndex(),
                                            colorReadRT->getLayerIndex(), clippedSourceArea,
                                            &colorReadRT->getImage());
    }

    bool forceCPUPath = ForceCPUPathForCopy(renderer, *mImage);

    // If it's possible to perform the copy with a draw call, do that.
    if (CanCopyWithDraw(renderer, srcFormat, destFormat) && !forceCPUPath)
    {
        // Layer count can only be 1 as the source is a framebuffer.
        ASSERT(offsetImageIndex.getLayerCount() == 1);

        return copySubImageImplWithDraw(contextVk, offsetImageIndex, modifiedDestOffset, destFormat,
                                        0, clippedSourceArea, isViewportFlipY, false, false, false,
                                        &colorReadRT->getImage(), colorReadRT->getFetchImageView());
    }

    // Do a CPU readback that does the conversion, and then stage the change to the pixel buffer.
    ANGLE_TRY(mImage->stageSubresourceUpdateFromFramebuffer(
        context, offsetImageIndex, clippedSourceArea, modifiedDestOffset,
        gl::Extents(clippedSourceArea.width, clippedSourceArea.height, 1), internalFormat,
        framebufferVk));
    onStagingBufferChange();

    return angle::Result::Continue;
}

angle::Result TextureVk::copySubTextureImpl(ContextVk *contextVk,
                                            const gl::ImageIndex &index,
                                            const gl::Offset &destOffset,
                                            const gl::InternalFormat &destFormat,
                                            size_t sourceLevel,
                                            const gl::Rectangle &sourceArea,
                                            bool unpackFlipY,
                                            bool unpackPremultiplyAlpha,
                                            bool unpackUnmultiplyAlpha,
                                            TextureVk *source)
{
    RendererVk *renderer = contextVk->getRenderer();

    ANGLE_TRY(source->ensureImageInitialized(contextVk));

    const vk::Format &sourceVkFormat = source->getImage().getFormat();
    const vk::Format &destVkFormat   = renderer->getFormat(destFormat.sizedInternalFormat);

    const gl::ImageIndex offsetImageIndex = getNativeImageIndex(index);

    // If it's possible to perform the copy with a transfer, that's the best option.
    if (!unpackFlipY && !unpackPremultiplyAlpha && !unpackUnmultiplyAlpha &&
        CanCopyWithTransfer(renderer, sourceVkFormat, destVkFormat))
    {
        return copySubImageImplWithTransfer(contextVk, offsetImageIndex, destOffset, destVkFormat,
                                            sourceLevel, 0, sourceArea, &source->getImage());
    }

    bool forceCPUPath = ForceCPUPathForCopy(renderer, *mImage);

    // If it's possible to perform the copy with a draw call, do that.
    if (CanCopyWithDraw(renderer, sourceVkFormat, destVkFormat) && !forceCPUPath)
    {
        return copySubImageImplWithDraw(contextVk, offsetImageIndex, destOffset, destVkFormat,
                                        sourceLevel, sourceArea, false, unpackFlipY,
                                        unpackPremultiplyAlpha, unpackUnmultiplyAlpha,
                                        &source->getImage(), &source->getFetchImageView());
    }

    if (sourceLevel != 0)
    {
        WARN() << "glCopyTextureCHROMIUM with sourceLevel != 0 not implemented.";
        return angle::Result::Stop;
    }

    // Read back the requested region of the source texture
    uint8_t *sourceData = nullptr;
    ANGLE_TRY(source->copyImageDataToBuffer(contextVk, sourceLevel, 1, sourceArea, &sourceData));

    const angle::Format &sourceTextureFormat = sourceVkFormat.imageFormat();
    const angle::Format &destTextureFormat   = destVkFormat.imageFormat();
    size_t destinationAllocationSize =
        sourceArea.width * sourceArea.height * destTextureFormat.pixelBytes;

    // Allocate memory in the destination texture for the copy/conversion
    uint8_t *destData = nullptr;
    ANGLE_TRY(mImage->stageSubresourceUpdateAndGetData(
        contextVk, destinationAllocationSize, offsetImageIndex,
        gl::Extents(sourceArea.width, sourceArea.height, 1), destOffset, &destData));
    onStagingBufferChange();

    // Source and dest data is tightly packed
    GLuint sourceDataRowPitch = sourceArea.width * sourceTextureFormat.pixelBytes;
    GLuint destDataRowPitch   = sourceArea.width * destTextureFormat.pixelBytes;

    rx::PixelReadFunction pixelReadFunction   = sourceTextureFormat.pixelReadFunction;
    rx::PixelWriteFunction pixelWriteFunction = destTextureFormat.pixelWriteFunction;

    // Fix up the read/write functions for the sake of luminance/alpha that are emulated with
    // formats whose channels don't correspond to the original format (alpha is emulated with red,
    // and luminance/alpha is emulated with red/green).
    if (sourceVkFormat.angleFormat().isLUMA())
    {
        pixelReadFunction = sourceVkFormat.angleFormat().pixelReadFunction;
    }
    if (destVkFormat.angleFormat().isLUMA())
    {
        pixelWriteFunction = destVkFormat.angleFormat().pixelWriteFunction;
    }

    CopyImageCHROMIUM(sourceData, sourceDataRowPitch, sourceTextureFormat.pixelBytes, 0,
                      pixelReadFunction, destData, destDataRowPitch, destTextureFormat.pixelBytes,
                      0, pixelWriteFunction, destFormat.format, destFormat.componentType,
                      sourceArea.width, sourceArea.height, 1, unpackFlipY, unpackPremultiplyAlpha,
                      unpackUnmultiplyAlpha);

    return angle::Result::Continue;
}

angle::Result TextureVk::copySubImageImplWithTransfer(ContextVk *contextVk,
                                                      const gl::ImageIndex &index,
                                                      const gl::Offset &destOffset,
                                                      const vk::Format &destFormat,
                                                      size_t sourceLevel,
                                                      size_t sourceLayer,
                                                      const gl::Rectangle &sourceArea,
                                                      vk::ImageHelper *srcImage)
{
    RendererVk *renderer = contextVk->getRenderer();

    uint32_t level       = index.getLevelIndex();
    uint32_t baseLayer   = index.hasLayer() ? index.getLayerIndex() : 0;
    uint32_t layerCount  = index.getLayerCount();
    gl::Offset srcOffset = {sourceArea.x, sourceArea.y, 0};
    gl::Extents extents  = {sourceArea.width, sourceArea.height, 1};

    // Change source layout if necessary
    if (srcImage->isLayoutChangeNecessary(vk::ImageLayout::TransferSrc))
    {
        vk::CommandBuffer *srcLayoutChange;
        ANGLE_TRY(srcImage->recordCommands(contextVk, &srcLayoutChange));
        srcImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferSrc,
                               srcLayoutChange);
    }

    VkImageSubresourceLayers srcSubresource = {};
    srcSubresource.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    srcSubresource.mipLevel                 = static_cast<uint32_t>(sourceLevel);
    srcSubresource.baseArrayLayer           = static_cast<uint32_t>(sourceLayer);
    srcSubresource.layerCount               = layerCount;

    // If destination is valid, copy the source directly into it.
    if (mImage->valid())
    {
        // Make sure any updates to the image are already flushed.
        ANGLE_TRY(ensureImageInitialized(contextVk));

        vk::CommandBuffer *commandBuffer;
        ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));

        // Change the image layout before the transfer
        mImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst,
                             commandBuffer);

        // Source's layout change should happen before the copy
        srcImage->addReadDependency(mImage);

        VkImageSubresourceLayers destSubresource = srcSubresource;
        destSubresource.mipLevel                 = level;
        destSubresource.baseArrayLayer           = baseLayer;

        VkImageType imageType = gl_vk::GetImageType(mState.getType());
        if (imageType == VK_IMAGE_TYPE_3D)
        {
            destSubresource.baseArrayLayer = 0;
            destSubresource.layerCount     = 1;
        }

        vk::ImageHelper::Copy(srcImage, mImage, srcOffset, destOffset, extents, srcSubresource,
                              destSubresource, commandBuffer);
    }
    else
    {
        std::unique_ptr<vk::ImageHelper> stagingImage;

        // Create a temporary image to stage the copy
        stagingImage = std::make_unique<vk::ImageHelper>();

        ANGLE_TRY(stagingImage->init2DStaging(contextVk, renderer->getMemoryProperties(),
                                              gl::Extents(sourceArea.width, sourceArea.height, 1),
                                              destFormat, kTransferStagingImageFlags, layerCount));

        vk::CommandBuffer *commandBuffer;
        ANGLE_TRY(stagingImage->recordCommands(contextVk, &commandBuffer));

        // Change the image layout before the transfer
        stagingImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst,
                                   commandBuffer);

        // Source's layout change should happen before the copy
        srcImage->addReadDependency(stagingImage.get());

        VkImageSubresourceLayers destSubresource = srcSubresource;
        destSubresource.mipLevel                 = 0;
        destSubresource.baseArrayLayer           = 0;

        vk::ImageHelper::Copy(srcImage, stagingImage.get(), srcOffset, gl::Offset(), extents,
                              srcSubresource, destSubresource, commandBuffer);

        // Stage the copy for when the image storage is actually created.
        VkImageType imageType = gl_vk::GetImageType(mState.getType());
        mImage->stageSubresourceUpdateFromImage(stagingImage.release(), index, destOffset, extents,
                                                imageType);
        onStagingBufferChange();
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::copySubImageImplWithDraw(ContextVk *contextVk,
                                                  const gl::ImageIndex &index,
                                                  const gl::Offset &destOffset,
                                                  const vk::Format &destFormat,
                                                  size_t sourceLevel,
                                                  const gl::Rectangle &sourceArea,
                                                  bool isSrcFlipY,
                                                  bool unpackFlipY,
                                                  bool unpackPremultiplyAlpha,
                                                  bool unpackUnmultiplyAlpha,
                                                  vk::ImageHelper *srcImage,
                                                  const vk::ImageView *srcView)
{
    RendererVk *renderer      = contextVk->getRenderer();
    UtilsVk &utilsVk          = contextVk->getUtils();
    Serial currentQueueSerial = contextVk->getCurrentQueueSerial();

    UtilsVk::CopyImageParameters params;
    params.srcOffset[0]        = sourceArea.x;
    params.srcOffset[1]        = sourceArea.y;
    params.srcExtents[0]       = sourceArea.width;
    params.srcExtents[1]       = sourceArea.height;
    params.destOffset[0]       = destOffset.x;
    params.destOffset[1]       = destOffset.y;
    params.srcMip              = static_cast<uint32_t>(sourceLevel);
    params.srcHeight           = srcImage->getExtents().height;
    params.srcPremultiplyAlpha = unpackPremultiplyAlpha && !unpackUnmultiplyAlpha;
    params.srcUnmultiplyAlpha  = unpackUnmultiplyAlpha && !unpackPremultiplyAlpha;
    params.srcFlipY            = isSrcFlipY;
    params.destFlipY           = unpackFlipY;

    uint32_t level      = index.getLevelIndex();
    uint32_t baseLayer  = index.hasLayer() ? index.getLayerIndex() : 0;
    uint32_t layerCount = index.getLayerCount();

    // If destination is valid, copy the source directly into it.
    if (mImage->valid())
    {
        // Make sure any updates to the image are already flushed.
        ANGLE_TRY(ensureImageInitialized(contextVk));

        for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            params.srcLayer = layerIndex;

            const vk::ImageView *destView;
            ANGLE_TRY(
                getLayerLevelDrawImageView(contextVk, baseLayer + layerIndex, level, &destView));

            ANGLE_TRY(utilsVk.copyImage(contextVk, mImage, destView, srcImage, srcView, params));
        }
    }
    else
    {
        std::unique_ptr<vk::ImageHelper> stagingImage;

        GLint samples                      = srcImage->getSamples();
        gl::TextureType stagingTextureType = vk::Get2DTextureType(layerCount, samples);

        // Create a temporary image to stage the copy
        stagingImage = std::make_unique<vk::ImageHelper>();

        ANGLE_TRY(stagingImage->init2DStaging(contextVk, renderer->getMemoryProperties(),
                                              gl::Extents(sourceArea.width, sourceArea.height, 1),
                                              destFormat, kDrawStagingImageFlags, layerCount));

        params.destOffset[0] = 0;
        params.destOffset[1] = 0;

        for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            params.srcLayer = layerIndex;

            // Create a temporary view for this layer.
            vk::ImageView stagingView;
            ANGLE_TRY(stagingImage->initLayerImageView(
                contextVk, stagingTextureType, VK_IMAGE_ASPECT_COLOR_BIT, gl::SwizzleState(),
                &stagingView, 0, 1, layerIndex, 1));

            ANGLE_TRY(utilsVk.copyImage(contextVk, stagingImage.get(), &stagingView, srcImage,
                                        srcView, params));

            // Queue the resource for cleanup as soon as the copy above is finished.  There's no
            // need to keep it around.
            contextVk->releaseObject(currentQueueSerial, &stagingView);
        }

        // Stage the copy for when the image storage is actually created.
        VkImageType imageType = gl_vk::GetImageType(mState.getType());
        mImage->stageSubresourceUpdateFromImage(stagingImage.release(), index, destOffset,
                                                gl::Extents(sourceArea.width, sourceArea.height, 1),
                                                imageType);
        onStagingBufferChange();
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::setStorage(const gl::Context *context,
                                    gl::TextureType type,
                                    size_t levels,
                                    GLenum internalFormat,
                                    const gl::Extents &size)
{
    ContextVk *contextVk = GetAs<ContextVk>(context->getImplementation());
    RendererVk *renderer = contextVk->getRenderer();

    if (!mOwnsImage)
    {
        releaseAndDeleteImage(contextVk);
    }

    const vk::Format &format = renderer->getFormat(internalFormat);
    ANGLE_TRY(ensureImageAllocated(contextVk, format));

    if (mImage->valid())
    {
        releaseImage(contextVk);
    }

    gl::Format glFormat(internalFormat);
    ANGLE_TRY(
        initImage(contextVk, format, glFormat.info->sized, size, static_cast<uint32_t>(levels)));
    return angle::Result::Continue;
}

angle::Result TextureVk::setStorageExternalMemory(const gl::Context *context,
                                                  gl::TextureType type,
                                                  size_t levels,
                                                  GLenum internalFormat,
                                                  const gl::Extents &size,
                                                  gl::MemoryObject *memoryObject,
                                                  GLuint64 offset)
{
    ContextVk *contextVk           = vk::GetImpl(context);
    RendererVk *renderer           = contextVk->getRenderer();
    MemoryObjectVk *memoryObjectVk = vk::GetImpl(memoryObject);

    releaseAndDeleteImage(contextVk);

    const vk::Format &format = renderer->getFormat(internalFormat);

    setImageHelper(contextVk, new vk::ImageHelper(), mState.getType(), format, 0, 0, true);

    ANGLE_TRY(
        memoryObjectVk->createImage(context, type, levels, internalFormat, size, offset, mImage));

    gl::Format glFormat(internalFormat);
    ANGLE_TRY(initImageViews(contextVk, format, glFormat.info->sized, static_cast<uint32_t>(levels),
                             mImage->getLayerCount()));

    // TODO(spang): This needs to be reworked when semaphores are added.
    // http://anglebug.com/3289
    uint32_t rendererQueueFamilyIndex = renderer->getQueueFamilyIndex();
    if (mImage->isQueueChangeNeccesary(rendererQueueFamilyIndex))
    {
        vk::CommandBuffer *commandBuffer = nullptr;
        ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));
        mImage->changeLayoutAndQueue(VK_IMAGE_ASPECT_COLOR_BIT,
                                     vk::ImageLayout::AllGraphicsShadersReadOnly,
                                     rendererQueueFamilyIndex, commandBuffer);
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::setEGLImageTarget(const gl::Context *context,
                                           gl::TextureType type,
                                           egl::Image *image)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    releaseAndDeleteImage(contextVk);

    const vk::Format &format = renderer->getFormat(image->getFormat().info->sizedInternalFormat);

    ImageVk *imageVk = vk::GetImpl(image);
    setImageHelper(contextVk, imageVk->getImage(), imageVk->getImageTextureType(), format,
                   imageVk->getImageLevel(), imageVk->getImageLayer(), false);

    ASSERT(type != gl::TextureType::CubeMap);
    ANGLE_TRY(initImageViews(contextVk, format, image->getFormat().info->sized, 1, 1));

    // Transfer the image to this queue if needed
    uint32_t rendererQueueFamilyIndex = renderer->getQueueFamilyIndex();
    if (mImage->isQueueChangeNeccesary(rendererQueueFamilyIndex))
    {
        vk::CommandBuffer *commandBuffer = nullptr;
        ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));
        mImage->changeLayoutAndQueue(VK_IMAGE_ASPECT_COLOR_BIT,
                                     vk::ImageLayout::AllGraphicsShadersReadOnly,
                                     rendererQueueFamilyIndex, commandBuffer);
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::setImageExternal(const gl::Context *context,
                                          gl::TextureType type,
                                          egl::Stream *stream,
                                          const egl::Stream::GLTextureDescription &desc)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

gl::ImageIndex TextureVk::getNativeImageIndex(const gl::ImageIndex &inputImageIndex) const
{
    // The input index can be a specific layer (for cube maps, 2d arrays, etc) or mImageLayerOffset
    // can be non-zero but both of these cannot be true at the same time.  EGL images can source
    // from a cube map or 3D texture but can only be a 2D destination.
    ASSERT(!(inputImageIndex.hasLayer() && mImageLayerOffset > 0));

    // handle the special-case where image index can represent a whole level of a texture
    GLint resultImageLayer = inputImageIndex.getLayerIndex();
    if (inputImageIndex.getType() != mImageNativeType)
    {
        ASSERT(!inputImageIndex.hasLayer());
        resultImageLayer = mImageLayerOffset;
    }

    return gl::ImageIndex::MakeFromType(mImageNativeType,
                                        getNativeImageLevel(inputImageIndex.getLevelIndex()),
                                        resultImageLayer, inputImageIndex.getLayerCount());
}

uint32_t TextureVk::getNativeImageLevel(uint32_t frontendLevel) const
{
    return mImageLevelOffset + frontendLevel;
}

uint32_t TextureVk::getNativeImageLayer(uint32_t frontendLayer) const
{
    return mImageLayerOffset + frontendLayer;
}

void TextureVk::releaseAndDeleteImage(ContextVk *contextVk)
{
    if (mImage)
    {
        releaseImage(contextVk);
        releaseStagingBuffer(contextVk);
        SafeDelete(mImage);
    }
}

angle::Result TextureVk::ensureImageAllocated(ContextVk *contextVk, const vk::Format &format)
{
    if (mImage == nullptr)
    {
        setImageHelper(contextVk, new vk::ImageHelper(), mState.getType(), format, 0, 0, true);
    }
    else
    {
        updateImageHelper(contextVk, format);
    }

    return angle::Result::Continue;
}

void TextureVk::setImageHelper(ContextVk *contextVk,
                               vk::ImageHelper *imageHelper,
                               gl::TextureType imageType,
                               const vk::Format &format,
                               uint32_t imageLevelOffset,
                               uint32_t imageLayerOffset,
                               bool selfOwned)
{
    ASSERT(mImage == nullptr);

    mOwnsImage        = selfOwned;
    mImageNativeType  = imageType;
    mImageLevelOffset = imageLevelOffset;
    mImageLayerOffset = imageLayerOffset;
    mImage            = imageHelper;
    mImage->initStagingBuffer(contextVk->getRenderer(), format, vk::kStagingBufferFlags,
                              mStagingBufferInitialSize);

    mRenderTarget.init(mImage, &mDefaultViews.mDrawBaseLevelImageView,
                       &mDefaultViews.mFetchBaseLevelImageView, getNativeImageLevel(0),
                       getNativeImageLayer(0));

    // Force re-creation of layered render targets next time they are needed
    mCubeMapRenderTargets.clear();
    m3DRenderTargets.clear();

    mSerial = contextVk->generateTextureSerial();
}

void TextureVk::updateImageHelper(ContextVk *contextVk, const vk::Format &format)
{
    ASSERT(mImage != nullptr);
    mImage->initStagingBuffer(contextVk->getRenderer(), format, vk::kStagingBufferFlags,
                              mStagingBufferInitialSize);
}

angle::Result TextureVk::redefineImage(const gl::Context *context,
                                       const gl::ImageIndex &index,
                                       const vk::Format &format,
                                       const gl::Extents &size)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (!mOwnsImage)
    {
        releaseAndDeleteImage(contextVk);
    }

    if (mImage != nullptr)
    {
        // If there is any staged changes for this index, we can remove them since we're going to
        // override them with this call.
        mImage->removeStagedUpdates(contextVk, index);

        if (mImage->valid())
        {
            // Calculate the expected size for the index we are defining. If the size is different
            // from the given size, or the format is different, we are redefining the image so we
            // must release it.
            if (mImage->getFormat() != format || size != mImage->getSize(index))
            {
                releaseImage(contextVk);
            }
        }
    }

    if (!size.empty())
    {
        ANGLE_TRY(ensureImageAllocated(contextVk, format));
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::copyImageDataToBuffer(ContextVk *contextVk,
                                               size_t sourceLevel,
                                               uint32_t layerCount,
                                               const gl::Rectangle &sourceArea,
                                               uint8_t **outDataPtr)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "TextureVk::copyImageDataToBuffer");
    // Make sure the source is initialized and it's images are flushed.
    ANGLE_TRY(ensureImageInitialized(contextVk));

    const angle::Format &imageFormat = getImage().getFormat().imageFormat();
    size_t sourceCopyAllocationSize =
        sourceArea.width * sourceArea.height * imageFormat.pixelBytes * layerCount;

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));

    // Requirement of the copyImageToBuffer, the source image must be in SRC_OPTIMAL layout.
    mImage->changeLayout(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferSrc, commandBuffer);

    // Allocate enough memory to copy the sourceArea region of the source texture into its pixel
    // buffer.
    VkBuffer copyBufferHandle     = VK_NULL_HANDLE;
    VkDeviceSize sourceCopyOffset = 0;
    ANGLE_TRY(mImage->allocateStagingMemory(contextVk, sourceCopyAllocationSize, outDataPtr,
                                            &copyBufferHandle, &sourceCopyOffset, nullptr));

    VkBufferImageCopy region               = {};
    region.bufferOffset                    = sourceCopyOffset;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageExtent.width               = sourceArea.width;
    region.imageExtent.height              = sourceArea.height;
    region.imageExtent.depth               = 1;
    region.imageOffset.x                   = sourceArea.x;
    region.imageOffset.y                   = sourceArea.y;
    region.imageOffset.z                   = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = layerCount;
    region.imageSubresource.mipLevel       = static_cast<uint32_t>(sourceLevel);

    commandBuffer->copyImageToBuffer(mImage->getImage(), mImage->getCurrentLayout(),
                                     copyBufferHandle, 1, &region);

    // Explicitly finish. If new use cases arise where we don't want to block we can change this.
    ANGLE_TRY(contextVk->finishImpl());

    return angle::Result::Continue;
}

angle::Result TextureVk::generateMipmapsWithCPU(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    const VkExtent3D baseLevelExtents = mImage->getExtents();
    uint32_t imageLayerCount          = mImage->getLayerCount();

    uint8_t *imageData = nullptr;
    gl::Rectangle imageArea(0, 0, baseLevelExtents.width, baseLevelExtents.height);
    ANGLE_TRY(copyImageDataToBuffer(contextVk, mState.getEffectiveBaseLevel(), imageLayerCount,
                                    imageArea, &imageData));

    const angle::Format &angleFormat = mImage->getFormat().imageFormat();
    GLuint sourceRowPitch            = baseLevelExtents.width * angleFormat.pixelBytes;
    size_t baseLevelAllocationSize   = sourceRowPitch * baseLevelExtents.height;

    // We now have the base level available to be manipulated in the imageData pointer. Generate all
    // the missing mipmaps with the slow path. For each layer, use the copied data to generate all
    // the mips.
    for (GLuint layer = 0; layer < imageLayerCount; layer++)
    {
        size_t bufferOffset = layer * baseLevelAllocationSize;

        ANGLE_TRY(generateMipmapLevelsWithCPU(
            contextVk, angleFormat, layer, mState.getEffectiveBaseLevel() + 1,
            mState.getMipmapMaxLevel(), baseLevelExtents.width, baseLevelExtents.height,
            sourceRowPitch, imageData + bufferOffset));
    }

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));
    return mImage->flushStagedUpdates(contextVk, getNativeImageLevel(0), mImage->getLevelCount(),
                                      getNativeImageLayer(0), mImage->getLayerCount(),
                                      commandBuffer);
}

angle::Result TextureVk::generateMipmap(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Some data is pending, or the image has not been defined at all yet
    if (!mImage->valid())
    {
        // Let's initialize the image so we can generate the next levels.
        if (mImage->hasStagedUpdates())
        {
            ANGLE_TRY(ensureImageInitialized(contextVk));
            ASSERT(mImage->valid());
        }
        else
        {
            // There is nothing to generate if there is nothing uploaded so far.
            return angle::Result::Continue;
        }
    }

    RendererVk *renderer = contextVk->getRenderer();

    // Check if the image supports blit. If it does, we can do the mipmap generation on the gpu
    // only.
    if (renderer->hasImageFormatFeatureBits(mImage->getFormat().vkImageFormat, kBlitFeatureFlags))
    {
        ANGLE_TRY(ensureImageInitialized(contextVk));
        ANGLE_TRY(mImage->generateMipmapsWithBlit(contextVk, mState.getMipmapMaxLevel()));
    }
    else
    {
        ANGLE_TRY(generateMipmapsWithCPU(context));
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result TextureVk::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    releaseAndDeleteImage(contextVk);

    GLenum internalFormat    = surface->getConfig()->renderTargetFormat;
    const vk::Format &format = renderer->getFormat(internalFormat);

    // eglBindTexImage can only be called with pbuffer (offscreen) surfaces
    OffscreenSurfaceVk *offscreenSurface = GetImplAs<OffscreenSurfaceVk>(surface);
    setImageHelper(contextVk, offscreenSurface->getColorAttachmentImage(), mState.getType(), format,
                   surface->getMipmapLevel(), 0, false);

    ASSERT(mImage->getLayerCount() == 1);
    gl::Format glFormat(internalFormat);
    return initImageViews(contextVk, format, glFormat.info->sized, 1, 1);
}

angle::Result TextureVk::releaseTexImage(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    releaseImage(contextVk);

    return angle::Result::Continue;
}

angle::Result TextureVk::getAttachmentRenderTarget(const gl::Context *context,
                                                   GLenum binding,
                                                   const gl::ImageIndex &imageIndex,
                                                   GLsizei samples,
                                                   FramebufferAttachmentRenderTarget **rtOut)
{
    // Non-zero mip level attachments are an ES 3.0 feature.
    ASSERT(imageIndex.getLevelIndex() == 0);

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(ensureImageInitialized(contextVk));

    switch (imageIndex.getType())
    {
        case gl::TextureType::_2D:
            *rtOut = &mRenderTarget;
            break;
        case gl::TextureType::_2DArray:
        case gl::TextureType::_3D:
            ANGLE_TRY(init3DRenderTargets(contextVk));
            *rtOut = &m3DRenderTargets[imageIndex.getLayerIndex()];
            break;
        case gl::TextureType::CubeMap:
            ANGLE_TRY(initCubeMapRenderTargets(contextVk));
            *rtOut = &mCubeMapRenderTargets[imageIndex.cubeMapFaceIndex()];
            break;
        default:
            UNREACHABLE();
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::ensureImageInitialized(ContextVk *contextVk)
{
    const gl::ImageDesc &baseLevelDesc  = mState.getBaseLevelDesc();
    const gl::Extents &baseLevelExtents = baseLevelDesc.size;
    const uint32_t levelCount           = getLevelCount();

    const vk::Format &format =
        contextVk->getRenderer()->getFormat(baseLevelDesc.format.info->sizedInternalFormat);

    return ensureImageInitializedImpl(contextVk, baseLevelExtents, levelCount, format);
}

angle::Result TextureVk::ensureImageInitializedImpl(ContextVk *contextVk,
                                                    const gl::Extents &baseLevelExtents,
                                                    uint32_t levelCount,
                                                    const vk::Format &format)
{
    if (mImage->valid() && !mImage->hasStagedUpdates())
    {
        return angle::Result::Continue;
    }

    if (!mImage->valid())
    {
        const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();

        ANGLE_TRY(initImage(contextVk, format, baseLevelDesc.format.info->sized, baseLevelExtents,
                            levelCount));
    }

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));
    return mImage->flushStagedUpdates(contextVk, getNativeImageLevel(0), mImage->getLevelCount(),
                                      getNativeImageLayer(0), mImage->getLayerCount(),
                                      commandBuffer);
}

angle::Result TextureVk::init3DRenderTargets(ContextVk *contextVk)
{
    // Lazy init. Check if already initialized.
    if (!m3DRenderTargets.empty())
        return angle::Result::Continue;

    uint32_t layerCount = GetImageLayerCountForView(*mImage);
    const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();

    mLayerFetchImageView.resize(layerCount);
    m3DRenderTargets.resize(layerCount);

    for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        const vk::ImageView *drawView;
        ANGLE_TRY(getLayerLevelDrawImageView(contextVk, layerIndex, 0, &drawView));

        // Users of the render target expect the views to directly view the desired layer, so we
        // need create a fetch view for each layer as well.
        gl::SwizzleState mappedSwizzle;
        MapSwizzleState(contextVk, mImage->getFormat(), baseLevelDesc.format.info->sized,
                        mState.getSwizzleState(), &mappedSwizzle);
        gl::TextureType arrayType = vk::Get2DTextureType(layerCount, mImage->getSamples());
        ANGLE_TRY(mImage->initLayerImageView(contextVk, arrayType, mImage->getAspectFlags(),
                                             mappedSwizzle, &mLayerFetchImageView[layerIndex],
                                             getNativeImageLevel(0), 1,
                                             getNativeImageLayer(layerIndex), 1));

        m3DRenderTargets[layerIndex].init(mImage, drawView, &mLayerFetchImageView[layerIndex],
                                          getNativeImageLevel(0), getNativeImageLayer(layerIndex));
    }
    return angle::Result::Continue;
}

angle::Result TextureVk::initCubeMapRenderTargets(ContextVk *contextVk)
{
    // Lazy init. Check if already initialized.
    if (!mCubeMapRenderTargets.empty())
        return angle::Result::Continue;

    const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();

    mLayerFetchImageView.resize(gl::kCubeFaceCount);
    mCubeMapRenderTargets.resize(gl::kCubeFaceCount);
    for (uint32_t cubeMapFaceIndex = 0; cubeMapFaceIndex < gl::kCubeFaceCount; ++cubeMapFaceIndex)
    {
        const vk::ImageView *drawView;
        ANGLE_TRY(getLayerLevelDrawImageView(contextVk, cubeMapFaceIndex, 0, &drawView));

        // Users of the render target expect the views to directly view the desired layer, so we
        // need create a fetch view for each layer as well.
        gl::SwizzleState mappedSwizzle;
        MapSwizzleState(contextVk, mImage->getFormat(), baseLevelDesc.format.info->sized,
                        mState.getSwizzleState(), &mappedSwizzle);
        gl::TextureType arrayType = vk::Get2DTextureType(gl::kCubeFaceCount, mImage->getSamples());
        ANGLE_TRY(mImage->initLayerImageView(contextVk, arrayType, mImage->getAspectFlags(),
                                             mappedSwizzle, &mLayerFetchImageView[cubeMapFaceIndex],
                                             getNativeImageLevel(0), 1,
                                             getNativeImageLayer(cubeMapFaceIndex), 1));

        mCubeMapRenderTargets[cubeMapFaceIndex].init(
            mImage, drawView, &mLayerFetchImageView[cubeMapFaceIndex], getNativeImageLevel(0),
            getNativeImageLayer(cubeMapFaceIndex));
    }
    return angle::Result::Continue;
}

angle::Result TextureVk::syncState(const gl::Context *context,
                                   const gl::Texture::DirtyBits &dirtyBits)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Initialize the image storage and flush the pixel buffer.
    ANGLE_TRY(ensureImageInitialized(contextVk));

    if (dirtyBits.none() && mSampler.valid())
    {
        return angle::Result::Continue;
    }

    RendererVk *renderer = contextVk->getRenderer();
    if (mSampler.valid())
    {
        contextVk->releaseObject(contextVk->getCurrentQueueSerial(), &mSampler);
    }

    if (dirtyBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_RED) ||
        dirtyBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_GREEN) ||
        dirtyBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_BLUE) ||
        dirtyBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA))
    {
        if (mImage && mImage->valid())
        {
            // We use a special layer count here to handle EGLImages. They might only be
            // looking at one layer of a cube or 2D array texture.
            uint32_t layerCount =
                mState.getType() == gl::TextureType::_2D ? 1 : mImage->getLayerCount();

            releaseImageViews(contextVk);
            const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();

            ANGLE_TRY(initImageViews(contextVk, mImage->getFormat(),
                                     baseLevelDesc.format.info->sized, mImage->getLevelCount(),
                                     layerCount));
        }
    }

    const gl::Extensions &extensions     = renderer->getNativeExtensions();
    const gl::SamplerState &samplerState = mState.getSamplerState();

    float maxAnisotropy   = samplerState.getMaxAnisotropy();
    bool anisotropyEnable = extensions.textureFilterAnisotropic && maxAnisotropy > 1.0f;
    bool compareEnable    = samplerState.getCompareMode() == GL_COMPARE_REF_TO_TEXTURE;
    VkCompareOp compareOp = gl_vk::GetCompareOp(samplerState.getCompareFunc());
    // When sampling from stencil, deqp tests expect texture compare to have no effect
    // dEQP - GLES31.functional.stencil_texturing.misc.compare_mode_effect
    // states: NOTE: Texture compare mode has no effect when reading stencil values.
    if (mState.isStencilMode())
    {
        compareEnable = VK_FALSE;
        compareOp     = VK_COMPARE_OP_ALWAYS;
    }

    // Create a simple sampler. Force basic parameter settings.
    VkSamplerCreateInfo samplerInfo     = {};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.flags                   = 0;
    samplerInfo.magFilter               = gl_vk::GetFilter(samplerState.getMagFilter());
    samplerInfo.minFilter               = gl_vk::GetFilter(samplerState.getMinFilter());
    samplerInfo.mipmapMode              = gl_vk::GetSamplerMipmapMode(samplerState.getMinFilter());
    samplerInfo.addressModeU            = gl_vk::GetSamplerAddressMode(samplerState.getWrapS());
    samplerInfo.addressModeV            = gl_vk::GetSamplerAddressMode(samplerState.getWrapT());
    samplerInfo.addressModeW            = gl_vk::GetSamplerAddressMode(samplerState.getWrapR());
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.anisotropyEnable        = anisotropyEnable;
    samplerInfo.maxAnisotropy           = maxAnisotropy;
    samplerInfo.compareEnable           = compareEnable;
    samplerInfo.compareOp               = compareOp;
    samplerInfo.minLod                  = samplerState.getMinLod();
    samplerInfo.maxLod                  = samplerState.getMaxLod();
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    ANGLE_VK_TRY(contextVk, mSampler.init(contextVk->getDevice(), samplerInfo));

    // Regenerate the serial on a sampler change.
    mSerial = contextVk->generateTextureSerial();

    return angle::Result::Continue;
}

angle::Result TextureVk::setStorageMultisample(const gl::Context *context,
                                               gl::TextureType type,
                                               GLsizei samples,
                                               GLint internalformat,
                                               const gl::Extents &size,
                                               bool fixedSampleLocations)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result TextureVk::initializeContents(const gl::Context *context,
                                            const gl::ImageIndex &imageIndex)
{
    const gl::ImageDesc &desc = mState.getImageDesc(imageIndex);
    const vk::Format &format =
        vk::GetImpl(context)->getRenderer()->getFormat(desc.format.info->sizedInternalFormat);

    mImage->stageSubresourceRobustClear(imageIndex, format.angleFormat());

    // Note that we cannot ensure the image is initialized because we might be calling subImage
    // on a non-complete cube map.
    return angle::Result::Continue;
}

void TextureVk::releaseOwnershipOfImage(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    mOwnsImage = false;
    releaseAndDeleteImage(contextVk);
}

const TextureVk::TextureVkViews *TextureVk::getTextureViews() const
{
    VkImageAspectFlags aspectFlags = mImage->getAspectFlags();
    if (HasBothDepthAndStencilAspects(aspectFlags) && mState.isStencilMode())
    {
        return &mStencilViews;
    }
    return &mDefaultViews;
}

const vk::ImageView &TextureVk::getReadImageView() const
{
    ASSERT(mImage->valid());
    const TextureVkViews *activeView = getTextureViews();

    if (!gl::IsMipmapFiltered(mState.getSamplerState()))
    {
        return activeView->mReadBaseLevelImageView;
    }

    return activeView->mReadMipmapImageView;
}

const vk::ImageView &TextureVk::getFetchImageView() const
{

    if (!mDefaultViews.mFetchBaseLevelImageView.valid())
    {
        return getReadImageView();
    }

    ASSERT(mImage->valid());
    const TextureVkViews *activeView = getTextureViews();

    if (!gl::IsMipmapFiltered(mState.getSamplerState()))
    {
        return activeView->mFetchBaseLevelImageView;
    }

    return activeView->mFetchMipmapImageView;
}

vk::ImageView *TextureVk::getLayerLevelImageViewImpl(vk::LayerLevelImageViewVector *imageViews,
                                                     size_t layer,
                                                     size_t level)
{
    ASSERT(mImage->valid());
    ASSERT(!mImage->getFormat().imageFormat().isBlock);

    uint32_t layerCount = GetImageLayerCountForView(*mImage);

    // Lazily allocate the storage for image views
    if (imageViews->empty())
    {
        imageViews->resize(layerCount);
    }
    ASSERT(imageViews->size() > layer);

    return getLevelImageViewImpl(&(*imageViews)[layer], level);
}

vk::ImageView *TextureVk::getLevelImageViewImpl(vk::ImageViewVector *imageViews, size_t level)
{
    // Lazily allocate the storage for image views
    if (imageViews->empty())
    {
        imageViews->resize(mImage->getLevelCount());
    }
    ASSERT(imageViews->size() > level);

    return &(*imageViews)[level];
}

angle::Result TextureVk::getLayerLevelDrawImageView(vk::Context *context,
                                                    size_t layer,
                                                    size_t level,
                                                    const vk::ImageView **imageViewOut)
{
    vk::ImageView *imageView = getLayerLevelImageViewImpl(&mLayerLevelDrawImageViews, layer, level);
    *imageViewOut            = imageView;
    if (imageView->valid())
    {
        return angle::Result::Continue;
    }

    uint32_t layerCount = GetImageLayerCountForView(*mImage);

    // Lazily allocate the image view itself.
    // Note that these views are specifically made to be used as color attachments, and therefore
    // don't have swizzle.
    gl::TextureType viewType = vk::Get2DTextureType(layerCount, mImage->getSamples());
    return mImage->initLayerImageView(context, viewType, mImage->getAspectFlags(),
                                      gl::SwizzleState(), imageView,
                                      getNativeImageLevel(static_cast<uint32_t>(level)), 1,
                                      getNativeImageLayer(static_cast<uint32_t>(layer)), 1);
}

angle::Result TextureVk::getLayerLevelStorageImageView(ContextVk *contextVk,
                                                       bool allLayers,
                                                       size_t singleLayer,
                                                       size_t level,
                                                       const vk::ImageView **imageViewOut)
{
    gl::TextureType viewType = mState.getType();
    uint32_t nativeLevel     = getNativeImageLevel(static_cast<uint32_t>(level));
    uint32_t nativeLayer     = getNativeImageLayer(static_cast<uint32_t>(singleLayer));
    uint32_t layerCount      = 1;

    vk::ImageView *imageView = nullptr;

    if (allLayers)
    {
        // Ignore the layer parameter and create a view with all layers of the level.
        imageView = getLevelImageViewImpl(&mLevelStorageImageViews, level);

        // If layered, the view has the same type as the texture.
        nativeLayer = getNativeImageLayer(0);
        layerCount  = mImage->getLayerCount();
    }
    else
    {
        // Create a view of the selected layer.
        imageView = getLayerLevelImageViewImpl(&mLayerLevelStorageImageViews, singleLayer, level);

        // If viewing a single layer, the image is always 2D.  Note that GLES doesn't support
        // multisampled storage images.
        viewType = gl::TextureType::_2D;
    }

    *imageViewOut = imageView;
    if (imageView->valid())
    {
        return angle::Result::Continue;
    }

    // Create the view.  Note that storage images are not affected by swizzle parameters.
    return mImage->initLayerImageView(contextVk, viewType, mImage->getAspectFlags(),
                                      gl::SwizzleState(), imageView, nativeLevel, 1, nativeLayer,
                                      layerCount);
}

const vk::Sampler &TextureVk::getSampler() const
{
    ASSERT(mSampler.valid());
    return mSampler;
}

angle::Result TextureVk::initImage(ContextVk *contextVk,
                                   const vk::Format &format,
                                   const bool sized,
                                   const gl::Extents &extents,
                                   const uint32_t levelCount)
{
    RendererVk *renderer = contextVk->getRenderer();

    VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT;

    // If the image has depth/stencil support, add those as possible usage.
    if (renderer->hasImageFormatFeatureBits(format.vkImageFormat,
                                            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        imageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else if (renderer->hasImageFormatFeatureBits(format.vkImageFormat,
                                                 VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        imageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    // If the image has storage support, add it for ES3.1 image support.
    if (renderer->hasImageFormatFeatureBits(format.vkImageFormat,
                                            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
    {
        imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    VkExtent3D vkExtent;
    uint32_t layerCount;
    gl_vk::GetExtentsAndLayerCount(mState.getType(), extents, &vkExtent, &layerCount);

    ANGLE_TRY(mImage->init(contextVk, mState.getType(), vkExtent, format, 1, imageUsageFlags,
                           levelCount, layerCount));

    const VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    ANGLE_TRY(mImage->initMemory(contextVk, renderer->getMemoryProperties(), flags));

    ANGLE_TRY(initImageViews(contextVk, format, sized, levelCount, layerCount));

    // If the image has an emulated channel, always clear it.  These channels will be masked out in
    // future writes, and shouldn't contain uninitialized values.
    if (format.hasEmulatedImageChannels())
    {
        uint32_t levelCount = mImage->getLevelCount();

        for (uint32_t level = 0; level < levelCount; ++level)
        {
            gl::ImageIndex index = gl::ImageIndex::Make2DArrayRange(level, 0, layerCount);
            mImage->stageSubresourceEmulatedClear(index, format.angleFormat());
            onStagingBufferChange();
        }
    }

    mSerial = contextVk->generateTextureSerial();

    return angle::Result::Continue;
}

angle::Result TextureVk::initImageViewImpl(ContextVk *contextVk,
                                           const vk::Format &format,
                                           uint32_t levelCount,
                                           uint32_t layerCount,
                                           TextureVkViews *view,
                                           VkImageAspectFlags aspectFlags,
                                           gl::SwizzleState mappedSwizzle)
{
    // TODO: Support non-zero base level for ES 3.0 by passing it to getNativeImageLevel.
    // http://anglebug.com/3148
    uint32_t baseLevel = getNativeImageLevel(0);
    uint32_t baseLayer = getNativeImageLayer(0);

    ANGLE_TRY(mImage->initLayerImageView(contextVk, mState.getType(), aspectFlags, mappedSwizzle,
                                         &view->mReadMipmapImageView, baseLevel, levelCount,
                                         baseLayer, layerCount));
    ANGLE_TRY(mImage->initLayerImageView(contextVk, mState.getType(), aspectFlags, mappedSwizzle,
                                         &view->mReadBaseLevelImageView, baseLevel, 1, baseLayer,
                                         layerCount));
    if (mState.getType() == gl::TextureType::CubeMap ||
        mState.getType() == gl::TextureType::_2DArray ||
        mState.getType() == gl::TextureType::_2DMultisampleArray)
    {
        gl::TextureType arrayType = vk::Get2DTextureType(layerCount, mImage->getSamples());

        ANGLE_TRY(mImage->initLayerImageView(contextVk, arrayType, aspectFlags, mappedSwizzle,
                                             &view->mFetchMipmapImageView, baseLevel, levelCount,
                                             baseLayer, layerCount));
        ANGLE_TRY(mImage->initLayerImageView(contextVk, arrayType, aspectFlags, mappedSwizzle,
                                             &view->mFetchBaseLevelImageView, baseLevel, 1,
                                             baseLayer, layerCount));
    }
    if (!format.imageFormat().isBlock)
    {
        ANGLE_TRY(mImage->initLayerImageView(contextVk, mState.getType(), aspectFlags,
                                             gl::SwizzleState(), &view->mDrawBaseLevelImageView,
                                             baseLevel, 1, baseLayer, layerCount));
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::initImageViews(ContextVk *contextVk,
                                        const vk::Format &format,
                                        const bool sized,
                                        uint32_t levelCount,
                                        uint32_t layerCount)
{
    ASSERT(mImage != nullptr);

    gl::SwizzleState mappedSwizzle;
    MapSwizzleState(contextVk, format, sized, mState.getSwizzleState(), &mappedSwizzle);

    VkImageAspectFlags aspectFlags = vk::GetFormatAspectFlags(format.angleFormat());
    if (HasBothDepthAndStencilAspects(aspectFlags))
    {
        ANGLE_TRY(initImageViewImpl(contextVk, format, levelCount, layerCount, &mStencilViews,
                                    VK_IMAGE_ASPECT_STENCIL_BIT, mappedSwizzle));
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    ANGLE_TRY(initImageViewImpl(contextVk, format, levelCount, layerCount, &mDefaultViews,
                                aspectFlags, mappedSwizzle));

    return angle::Result::Continue;
}

void TextureVk::releaseImage(ContextVk *contextVk)
{
    if (mImage)
    {
        if (mOwnsImage)
        {
            mImage->releaseImage(contextVk);
        }
        else
        {
            mImage = nullptr;
        }
    }

    releaseImageViews(contextVk);

    mCubeMapRenderTargets.clear();
    m3DRenderTargets.clear();

    onStagingBufferChange();
}

void TextureVk::releaseImageViews(ContextVk *contextVk)
{
    Serial currentSerial = contextVk->getCurrentQueueSerial();

    mDefaultViews.release(contextVk, currentSerial);

    mStencilViews.release(contextVk, currentSerial);

    for (vk::ImageViewVector &layerViews : mLayerLevelDrawImageViews)
    {
        for (vk::ImageView &imageView : layerViews)
        {
            contextVk->releaseObject(currentSerial, &imageView);
        }
    }
    mLayerLevelDrawImageViews.clear();
    for (vk::ImageView &imageView : mLayerFetchImageView)
    {
        contextVk->releaseObject(currentSerial, &imageView);
    }
    mLayerFetchImageView.clear();
    for (vk::ImageView &imageView : mLevelStorageImageViews)
    {
        contextVk->releaseObject(currentSerial, &imageView);
    }
    mLevelStorageImageViews.clear();
    for (vk::ImageViewVector &layerViews : mLayerLevelStorageImageViews)
    {
        for (vk::ImageView &imageView : layerViews)
        {
            contextVk->releaseObject(currentSerial, &imageView);
        }
    }
    mLayerLevelStorageImageViews.clear();
}

void TextureVk::releaseStagingBuffer(ContextVk *contextVk)
{
    if (mImage)
    {
        mImage->releaseStagingBuffer(contextVk);
    }
}

uint32_t TextureVk::getLevelCount() const
{
    ASSERT(mState.getEffectiveBaseLevel() == 0);

    // getMipmapMaxLevel will be 0 here if mipmaps are not used, so the levelCount is always +1.
    return mState.getMipmapMaxLevel() + 1;
}
}  // namespace rx
