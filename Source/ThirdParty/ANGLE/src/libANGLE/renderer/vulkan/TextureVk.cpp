//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureVk.cpp:
//    Implements the class methods for TextureVk.
//

#include "libANGLE/renderer/vulkan/TextureVk.h"
#include <vulkan/vulkan.h>

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
constexpr VkImageUsageFlags kDrawStagingImageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;

constexpr VkImageUsageFlags kTransferStagingImageFlags =
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

constexpr VkFormatFeatureFlags kBlitFeatureFlags =
    VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;

constexpr angle::SubjectIndex kTextureImageSubjectIndex = 0;

// Test whether a texture level is within the range of levels for which the current image is
// allocated.  This is used to ensure out-of-range updates are staged in the image, and not
// attempted to be directly applied.
bool IsTextureLevelInAllocatedImage(const vk::ImageHelper &image, uint32_t textureLevelIndexGL)
{
    uint32_t imageBaseLevel = image.getBaseLevel();
    if (textureLevelIndexGL < imageBaseLevel)
    {
        return false;
    }

    uint32_t imageLevelIndexVK = textureLevelIndexGL - imageBaseLevel;
    return imageLevelIndexVK < image.getLevelCount();
}

// Test whether a redefined texture level is compatible with the currently allocated image.  Returns
// true if the given size and format match the corresponding mip in the allocated image (taking
// base level into account).  This could return false when:
//
// - Defining a texture level that is outside the range of the image levels.  In this case, changes
//   to this level should remain staged until the texture is redefined to include this level.
// - Redefining a texture level that is within the range of the image levels, but has a different
//   size or format.  In this case too, changes to this level should remain staged as the texture
//   is no longer complete as is.
bool IsTextureLevelDefinitionCompatibleWithImage(const vk::ImageHelper &image,
                                                 uint32_t textureLevelIndexGL,
                                                 const gl::Extents &size,
                                                 const vk::Format &format)
{
    ASSERT(IsTextureLevelInAllocatedImage(image, textureLevelIndexGL));

    uint32_t imageLevelIndexVK = textureLevelIndexGL - image.getBaseLevel();
    return size == image.getLevelExtents(imageLevelIndexVK) && format == image.getFormat();
}

ANGLE_INLINE bool FormatHasNecessaryFeature(RendererVk *renderer,
                                            VkFormat format,
                                            VkImageTiling tilingMode,
                                            VkFormatFeatureFlags featureBits)
{
    return (tilingMode == VK_IMAGE_TILING_OPTIMAL)
               ? renderer->hasImageFormatFeatureBits(format, featureBits)
               : renderer->hasLinearImageFormatFeatureBits(format, featureBits);
}

bool CanCopyWithTransfer(RendererVk *renderer,
                         const vk::Format &srcFormat,
                         VkImageTiling srcTilingMode,
                         const vk::Format &destFormat,
                         VkImageTiling destTilingMode)
{
    // NOTE(syoussefi): technically, you can transfer between formats as long as they have the same
    // size and are compatible, but for now, let's just support same-format copies with transfer.
    bool isFormatCompatible           = srcFormat.internalFormat == destFormat.internalFormat;
    bool isTilingCompatible           = srcTilingMode == destTilingMode;
    bool srcFormatHasNecessaryFeature = FormatHasNecessaryFeature(
        renderer, srcFormat.vkImageFormat, srcTilingMode, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
    bool dstFormatHasNecessaryFeature = FormatHasNecessaryFeature(
        renderer, destFormat.vkImageFormat, destTilingMode, VK_FORMAT_FEATURE_TRANSFER_DST_BIT);

    return isFormatCompatible && isTilingCompatible && srcFormatHasNecessaryFeature &&
           dstFormatHasNecessaryFeature;
}

bool CanCopyWithDraw(RendererVk *renderer,
                     const vk::Format &srcFormat,
                     VkImageTiling srcTilingMode,
                     const vk::Format &destFormat,
                     VkImageTiling destTilingMode)
{
    bool srcFormatHasNecessaryFeature = FormatHasNecessaryFeature(
        renderer, srcFormat.vkImageFormat, srcTilingMode, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    bool dstFormatHasNecessaryFeature = FormatHasNecessaryFeature(
        renderer, destFormat.vkImageFormat, destTilingMode, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

    return srcFormatHasNecessaryFeature && dstFormatHasNecessaryFeature;
}

bool ForceCPUPathForCopy(RendererVk *renderer, const vk::ImageHelper &image)
{
    return image.getLayerCount() > 1 && renderer->getFeatures().forceCPUPathForCubeMapCopy.enabled;
}

bool CanGenerateMipmapWithCompute(RendererVk *renderer,
                                  VkImageType imageType,
                                  const vk::Format &format,
                                  GLint samples)
{
    const angle::Format &angleFormat = format.actualImageFormat();

    if (!renderer->getFeatures().allowGenerateMipmapWithCompute.enabled)
    {
        return false;
    }

    // Format must have STORAGE support.
    const bool hasStorageSupport = renderer->hasImageFormatFeatureBits(
        format.vkImageFormat, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

    // No support for sRGB formats yet.
    const bool isSRGB =
        gl::GetSizedInternalFormatInfo(format.internalFormat).colorEncoding == GL_SRGB;

    // No support for integer formats yet.
    const bool isInt = angleFormat.isInt();

    // Only 2D images are supported.
    const bool is2D = imageType == VK_IMAGE_TYPE_2D;

    // No support for multisampled images yet.
    const bool isMultisampled = samples > 1;

    // Only color formats are supported.
    const bool isColorFormat = !angleFormat.hasDepthOrStencilBits();

    return hasStorageSupport && !isSRGB && !isInt && is2D && !isMultisampled && isColorFormat;
}

void GetRenderTargetLayerCountAndIndex(vk::ImageHelper *image,
                                       const gl::ImageIndex &index,
                                       GLuint *layerCount,
                                       GLuint *layerIndex)
{
    switch (index.getType())
    {
        case gl::TextureType::_2D:
        case gl::TextureType::_2DMultisample:
            *layerIndex = 0;
            *layerCount = 1;
            return;

        case gl::TextureType::CubeMap:
            *layerIndex = index.cubeMapFaceIndex();
            *layerCount = gl::kCubeFaceCount;
            return;

        case gl::TextureType::_3D:
            *layerIndex = index.hasLayer() ? index.getLayerIndex() : 0;
            *layerCount = image->getExtents().depth;
            return;

        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisampleArray:
            *layerIndex = index.hasLayer() ? index.getLayerIndex() : 0;
            *layerCount = image->getLayerCount();
            return;

        default:
            UNREACHABLE();
    }
}

void Set3DBaseArrayLayerAndLayerCount(VkImageSubresourceLayers *Subresource)
{
    // If the srcImage/dstImage parameters are of VkImageType VK_IMAGE_TYPE_3D, the baseArrayLayer
    // and layerCount members of the corresponding subresource must be 0 and 1, respectively.
    Subresource->baseArrayLayer = 0;
    Subresource->layerCount     = 1;
}
}  // anonymous namespace

// TextureVk implementation.
TextureVk::TextureVk(const gl::TextureState &state, RendererVk *renderer)
    : TextureImpl(state),
      mOwnsImage(false),
      mRequiresSRGBViews(false),
      mImageNativeType(gl::TextureType::InvalidEnum),
      mImageLayerOffset(0),
      mImageLevelOffset(0),
      mImage(nullptr),
      mStagingBufferInitialSize(vk::kStagingBufferSize),
      mImageUsageFlags(0),
      mImageCreateFlags(0),
      mImageObserverBinding(this, kTextureImageSubjectIndex)
{}

TextureVk::~TextureVk() = default;

void TextureVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    releaseAndDeleteImage(contextVk);
    mSampler.reset();
}

angle::Result TextureVk::setImage(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  GLenum internalFormat,
                                  const gl::Extents &size,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  gl::Buffer *unpackBuffer,
                                  const uint8_t *pixels)
{
    const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat, type);

    return setImageImpl(context, index, formatInfo, size, type, unpack, unpackBuffer, pixels);
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

    return setSubImageImpl(context, index, area, formatInfo, type, unpack, unpackBuffer, pixels,
                           vkFormat);
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

    const gl::State &glState = context->getState();
    gl::Buffer *unpackBuffer = glState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    return setImageImpl(context, index, formatInfo, size, GL_UNSIGNED_BYTE, unpack, unpackBuffer,
                        pixels);
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
    const gl::State &glState = contextVk->getState();
    gl::Buffer *unpackBuffer = glState.getTargetBuffer(gl::BufferBinding::PixelUnpack);

    return setSubImageImpl(context, index, area, formatInfo, GL_UNSIGNED_BYTE, unpack, unpackBuffer,
                           pixels, vkFormat);
}

angle::Result TextureVk::setImageImpl(const gl::Context *context,
                                      const gl::ImageIndex &index,
                                      const gl::InternalFormat &formatInfo,
                                      const gl::Extents &size,
                                      GLenum type,
                                      const gl::PixelUnpackState &unpack,
                                      gl::Buffer *unpackBuffer,
                                      const uint8_t *pixels)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    const vk::Format &vkFormat = renderer->getFormat(formatInfo.sizedInternalFormat);

    ANGLE_TRY(redefineLevel(context, index, vkFormat, size));

    // Early-out on empty textures, don't create a zero-sized storage.
    if (size.empty())
    {
        return angle::Result::Continue;
    }

    return setSubImageImpl(context, index, gl::Box(gl::kOffsetZero, size), formatInfo, type, unpack,
                           unpackBuffer, pixels, vkFormat);
}

bool TextureVk::isFastUnpackPossible(const vk::Format &vkFormat, size_t offset) const
{
    // Conditions to determine if fast unpacking is possible
    // 1. Image must be well defined to unpack directly to it
    //    TODO(http://anglebug.com/4222) Create and stage a temp image instead
    // 2. Can't perform a fast copy for emulated formats
    // 3. vkCmdCopyBufferToImage requires byte offset to be a multiple of 4
    return mImage->valid() && vkFormat.intendedFormatID == vkFormat.actualImageFormatID &&
           (offset & (kBufferOffsetMultiple - 1)) == 0;
}

bool TextureVk::shouldUpdateBeStaged(uint32_t textureLevelIndexGL) const
{
    ASSERT(mImage);

    // If update is outside the range of image levels, it must be staged.
    if (!IsTextureLevelInAllocatedImage(*mImage, textureLevelIndexGL))
    {
        return true;
    }

    uint32_t imageLevelIndexVK = textureLevelIndexGL - mImage->getBaseLevel();

    // Can't have more than 32 mips for the foreseeable future.
    ASSERT(imageLevelIndexVK < 32);

    // Otherwise, it can only be directly applied to the image if the level is not previously
    // incompatibly redefined.
    return mRedefinedLevels.test(imageLevelIndexVK);
}

angle::Result TextureVk::setSubImageImpl(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         const gl::Box &area,
                                         const gl::InternalFormat &formatInfo,
                                         GLenum type,
                                         const gl::PixelUnpackState &unpack,
                                         gl::Buffer *unpackBuffer,
                                         const uint8_t *pixels,
                                         const vk::Format &vkFormat)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (unpackBuffer)
    {
        BufferVk *unpackBufferVk       = vk::GetImpl(unpackBuffer);
        vk::BufferHelper &bufferHelper = unpackBufferVk->getBuffer();
        uintptr_t offset               = reinterpret_cast<uintptr_t>(pixels);
        GLuint inputRowPitch           = 0;
        GLuint inputDepthPitch         = 0;
        GLuint inputSkipBytes          = 0;

        ANGLE_TRY(mImage->CalculateBufferInfo(
            contextVk, gl::Extents(area.width, area.height, area.depth), formatInfo, unpack, type,
            index.usesTex3D(), &inputRowPitch, &inputDepthPitch, &inputSkipBytes));

        size_t offsetBytes = static_cast<size_t>(offset + inputSkipBytes);

        if (!shouldUpdateBeStaged(index.getLevelIndex()) &&
            isFastUnpackPossible(vkFormat, offsetBytes))
        {
            GLuint pixelSize   = formatInfo.pixelBytes;
            GLuint blockWidth  = formatInfo.compressedBlockWidth;
            GLuint blockHeight = formatInfo.compressedBlockHeight;
            if (!formatInfo.compressed)
            {
                pixelSize   = formatInfo.computePixelBytes(type);
                blockWidth  = 1;
                blockHeight = 1;
            }
            ASSERT(pixelSize != 0 && inputRowPitch != 0 && blockWidth != 0 && blockHeight != 0);

            GLuint rowLengthPixels   = inputRowPitch / pixelSize * blockWidth;
            GLuint imageHeightPixels = inputDepthPitch / inputRowPitch * blockHeight;

            ANGLE_TRY(copyBufferDataToImage(contextVk, &bufferHelper, index, rowLengthPixels,
                                            imageHeightPixels, area, offsetBytes));
        }
        else
        {
            void *mapPtr = nullptr;

            ANGLE_TRY(unpackBufferVk->mapImpl(contextVk, &mapPtr));

            const uint8_t *source =
                static_cast<const uint8_t *>(mapPtr) + reinterpret_cast<ptrdiff_t>(pixels);

            ANGLE_TRY(mImage->stageSubresourceUpdateImpl(
                contextVk, getNativeImageIndex(index),
                gl::Extents(area.width, area.height, area.depth),
                gl::Offset(area.x, area.y, area.z), formatInfo, unpack, type, source, vkFormat,
                inputRowPitch, inputDepthPitch, inputSkipBytes));

            ANGLE_TRY(unpackBufferVk->unmapImpl(contextVk));
        }
    }
    else if (pixels)
    {
        ANGLE_TRY(mImage->stageSubresourceUpdate(
            contextVk, getNativeImageIndex(index), gl::Extents(area.width, area.height, area.depth),
            gl::Offset(area.x, area.y, area.z), formatInfo, unpack, type, pixels, vkFormat));
    }

    if (!mOwnsImage)
    {
        ANGLE_TRY(mImage->flushAllStagedUpdates(contextVk));
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

    ANGLE_TRY(redefineLevel(context, index, vkFormat, newImageSize));

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
                                     size_t sourceLevelGL,
                                     bool unpackFlipY,
                                     bool unpackPremultiplyAlpha,
                                     bool unpackUnmultiplyAlpha,
                                     const gl::Texture *source)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();

    TextureVk *sourceVk = vk::GetImpl(source);
    const gl::ImageDesc &sourceImageDesc =
        sourceVk->mState.getImageDesc(NonCubeTextureTypeToTarget(source->getType()), sourceLevelGL);
    gl::Box sourceBox(gl::kOffsetZero, sourceImageDesc.size);

    const gl::InternalFormat &destFormatInfo = gl::GetInternalFormatInfo(internalFormat, type);
    const vk::Format &destVkFormat = renderer->getFormat(destFormatInfo.sizedInternalFormat);

    ANGLE_TRY(redefineLevel(context, index, destVkFormat, sourceImageDesc.size));

    return copySubTextureImpl(vk::GetImpl(context), index, gl::kOffsetZero, destFormatInfo,
                              sourceLevelGL, sourceBox, unpackFlipY, unpackPremultiplyAlpha,
                              unpackUnmultiplyAlpha, sourceVk);
}

angle::Result TextureVk::copySubTexture(const gl::Context *context,
                                        const gl::ImageIndex &index,
                                        const gl::Offset &destOffset,
                                        size_t sourceLevelGL,
                                        const gl::Box &sourceBox,
                                        bool unpackFlipY,
                                        bool unpackPremultiplyAlpha,
                                        bool unpackUnmultiplyAlpha,
                                        const gl::Texture *source)
{
    gl::TextureTarget target                 = index.getTarget();
    size_t level                             = static_cast<size_t>(index.getLevelIndex());
    const gl::InternalFormat &destFormatInfo = *mState.getImageDesc(target, level).format.info;
    return copySubTextureImpl(vk::GetImpl(context), index, destOffset, destFormatInfo,
                              sourceLevelGL, sourceBox, unpackFlipY, unpackPremultiplyAlpha,
                              unpackUnmultiplyAlpha, vk::GetImpl(source));
}

angle::Result TextureVk::copyCompressedTexture(const gl::Context *context,
                                               const gl::Texture *source)
{
    ContextVk *contextVk = vk::GetImpl(context);
    TextureVk *sourceVk  = vk::GetImpl(source);

    gl::TextureTarget sourceTarget = NonCubeTextureTypeToTarget(source->getType());
    constexpr GLint sourceLevelGL  = 0;
    constexpr GLint destLevelGL    = 0;

    const gl::InternalFormat &internalFormat = *source->getFormat(sourceTarget, sourceLevelGL).info;
    const vk::Format &vkFormat =
        contextVk->getRenderer()->getFormat(internalFormat.sizedInternalFormat);
    const gl::Extents size(static_cast<int>(source->getWidth(sourceTarget, sourceLevelGL)),
                           static_cast<int>(source->getHeight(sourceTarget, sourceLevelGL)),
                           static_cast<int>(source->getDepth(sourceTarget, sourceLevelGL)));
    const gl::ImageIndex destIndex = gl::ImageIndex::MakeFromTarget(sourceTarget, destLevelGL, 1);

    ANGLE_TRY(redefineLevel(context, destIndex, vkFormat, size));

    ANGLE_TRY(sourceVk->ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

    return copySubImageImplWithTransfer(contextVk, destIndex, gl::kOffsetZero, vkFormat,
                                        sourceLevelGL, 0, gl::Box(gl::kOffsetZero, size),
                                        &sourceVk->getImage());
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
    VkImageTiling srcTilingMode  = colorReadRT->getImageForCopy().getTilingMode();
    const vk::Format &destFormat = renderer->getFormat(internalFormat.sizedInternalFormat);
    VkImageTiling destTilingMode = getTilingMode();

    bool isViewportFlipY = contextVk->isViewportFlipEnabledForReadFBO();

    gl::Box clippedSourceBox(clippedSourceArea.x, clippedSourceArea.y, colorReadRT->getLayerIndex(),
                             clippedSourceArea.width, clippedSourceArea.height, 1);

    // If it's possible to perform the copy with a transfer, that's the best option.
    if (!isViewportFlipY &&
        CanCopyWithTransfer(renderer, srcFormat, srcTilingMode, destFormat, destTilingMode))
    {
        return copySubImageImplWithTransfer(contextVk, offsetImageIndex, modifiedDestOffset,
                                            destFormat, colorReadRT->getLevelIndex(),
                                            colorReadRT->getLayerIndex(), clippedSourceBox,
                                            &colorReadRT->getImageForCopy());
    }

    bool forceCPUPath = ForceCPUPathForCopy(renderer, *mImage);

    // If it's possible to perform the copy with a draw call, do that.
    if (CanCopyWithDraw(renderer, srcFormat, srcTilingMode, destFormat, destTilingMode) &&
        !forceCPUPath)
    {
        // Layer count can only be 1 as the source is a framebuffer.
        ASSERT(offsetImageIndex.getLayerCount() == 1);

        const vk::ImageView *copyImageView = nullptr;
        ANGLE_TRY(colorReadRT->getAndRetainCopyImageView(contextVk, &copyImageView));

        return copySubImageImplWithDraw(contextVk, offsetImageIndex, modifiedDestOffset, destFormat,
                                        colorReadRT->getLevelIndex(), clippedSourceBox,
                                        isViewportFlipY, false, false, false,
                                        &colorReadRT->getImageForCopy(), copyImageView,
                                        contextVk->getRotationReadFramebuffer());
    }

    // Do a CPU readback that does the conversion, and then stage the change to the pixel buffer.
    ANGLE_TRY(mImage->stageSubresourceUpdateFromFramebuffer(
        context, offsetImageIndex, clippedSourceArea, modifiedDestOffset,
        gl::Extents(clippedSourceArea.width, clippedSourceArea.height, 1), internalFormat,
        framebufferVk));

    return angle::Result::Continue;
}

angle::Result TextureVk::copySubTextureImpl(ContextVk *contextVk,
                                            const gl::ImageIndex &index,
                                            const gl::Offset &destOffset,
                                            const gl::InternalFormat &destFormat,
                                            size_t sourceLevelGL,
                                            const gl::Box &sourceBox,
                                            bool unpackFlipY,
                                            bool unpackPremultiplyAlpha,
                                            bool unpackUnmultiplyAlpha,
                                            TextureVk *source)
{
    RendererVk *renderer = contextVk->getRenderer();

    ANGLE_TRY(source->ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

    const vk::Format &sourceVkFormat = source->getImage().getFormat();
    VkImageTiling srcTilingMode      = source->getImage().getTilingMode();
    const vk::Format &destVkFormat   = renderer->getFormat(destFormat.sizedInternalFormat);
    VkImageTiling destTilingMode     = getTilingMode();

    const gl::ImageIndex offsetImageIndex = getNativeImageIndex(index);

    // If it's possible to perform the copy with a transfer, that's the best option.
    if (!unpackFlipY && !unpackPremultiplyAlpha && !unpackUnmultiplyAlpha &&
        CanCopyWithTransfer(renderer, sourceVkFormat, srcTilingMode, destVkFormat, destTilingMode))
    {
        return copySubImageImplWithTransfer(contextVk, offsetImageIndex, destOffset, destVkFormat,
                                            sourceLevelGL, sourceBox.z, sourceBox,
                                            &source->getImage());
    }

    bool forceCPUPath = ForceCPUPathForCopy(renderer, *mImage);

    // If it's possible to perform the copy with a draw call, do that.
    if (CanCopyWithDraw(renderer, sourceVkFormat, srcTilingMode, destVkFormat, destTilingMode) &&
        !forceCPUPath)
    {
        return copySubImageImplWithDraw(
            contextVk, offsetImageIndex, destOffset, destVkFormat, sourceLevelGL, sourceBox, false,
            unpackFlipY, unpackPremultiplyAlpha, unpackUnmultiplyAlpha, &source->getImage(),
            &source->getCopyImageViewAndRecordUse(contextVk), SurfaceRotation::Identity);
    }

    if (sourceLevelGL != 0)
    {
        WARN() << "glCopyTextureCHROMIUM with sourceLevel != 0 not implemented.";
        return angle::Result::Stop;
    }

    // Read back the requested region of the source texture
    uint8_t *sourceData = nullptr;
    ANGLE_TRY(source->copyImageDataToBufferAndGetData(contextVk, sourceLevelGL, sourceBox.depth,
                                                      sourceBox, &sourceData));

    const angle::Format &sourceTextureFormat = sourceVkFormat.actualImageFormat();
    const angle::Format &destTextureFormat   = destVkFormat.actualImageFormat();
    size_t destinationAllocationSize =
        sourceBox.width * sourceBox.height * sourceBox.depth * destTextureFormat.pixelBytes;

    // Allocate memory in the destination texture for the copy/conversion
    uint32_t stagingBaseLayer =
        offsetImageIndex.hasLayer() ? offsetImageIndex.getLayerIndex() : destOffset.z;
    uint32_t stagingLayerCount = sourceBox.depth;
    gl::Offset stagingOffset   = destOffset;
    gl::Extents stagingExtents(sourceBox.width, sourceBox.height, sourceBox.depth);
    bool is3D = gl_vk::GetImageType(mState.getType()) == VK_IMAGE_TYPE_3D;

    if (is3D)
    {
        stagingBaseLayer  = 0;
        stagingLayerCount = 1;
    }
    else
    {
        stagingOffset.z      = 0;
        stagingExtents.depth = 1;
    }

    const gl::ImageIndex stagingIndex = gl::ImageIndex::Make2DArrayRange(
        offsetImageIndex.getLevelIndex(), stagingBaseLayer, stagingLayerCount);

    uint8_t *destData = nullptr;
    ANGLE_TRY(mImage->stageSubresourceUpdateAndGetData(contextVk, destinationAllocationSize,
                                                       stagingIndex, stagingExtents, stagingOffset,
                                                       &destData));

    // Source and dest data is tightly packed
    GLuint sourceDataRowPitch = sourceBox.width * sourceTextureFormat.pixelBytes;
    GLuint destDataRowPitch   = sourceBox.width * destTextureFormat.pixelBytes;

    GLuint sourceDataDepthPitch = sourceDataRowPitch * sourceBox.height;
    GLuint destDataDepthPitch   = destDataRowPitch * sourceBox.height;

    rx::PixelReadFunction pixelReadFunction   = sourceTextureFormat.pixelReadFunction;
    rx::PixelWriteFunction pixelWriteFunction = destTextureFormat.pixelWriteFunction;

    // Fix up the read/write functions for the sake of luminance/alpha that are emulated with
    // formats whose channels don't correspond to the original format (alpha is emulated with red,
    // and luminance/alpha is emulated with red/green).
    if (sourceVkFormat.intendedFormat().isLUMA())
    {
        pixelReadFunction = sourceVkFormat.intendedFormat().pixelReadFunction;
    }
    if (destVkFormat.intendedFormat().isLUMA())
    {
        pixelWriteFunction = destVkFormat.intendedFormat().pixelWriteFunction;
    }

    CopyImageCHROMIUM(sourceData, sourceDataRowPitch, sourceTextureFormat.pixelBytes,
                      sourceDataDepthPitch, pixelReadFunction, destData, destDataRowPitch,
                      destTextureFormat.pixelBytes, destDataDepthPitch, pixelWriteFunction,
                      destFormat.format, destFormat.componentType, sourceBox.width,
                      sourceBox.height, sourceBox.depth, unpackFlipY, unpackPremultiplyAlpha,
                      unpackUnmultiplyAlpha);

    return angle::Result::Continue;
}

angle::Result TextureVk::copySubImageImplWithTransfer(ContextVk *contextVk,
                                                      const gl::ImageIndex &index,
                                                      const gl::Offset &destOffset,
                                                      const vk::Format &destFormat,
                                                      size_t sourceLevelGL,
                                                      size_t sourceLayer,
                                                      const gl::Box &sourceBox,
                                                      vk::ImageHelper *srcImage)
{
    RendererVk *renderer = contextVk->getRenderer();

    uint32_t level      = index.getLevelIndex();
    uint32_t baseLayer  = index.hasLayer() ? index.getLayerIndex() : destOffset.z;
    uint32_t layerCount = sourceBox.depth;

    ASSERT(layerCount == static_cast<uint32_t>(gl::ImageIndex::kEntireLevel) ||
           layerCount == static_cast<uint32_t>(sourceBox.depth));

    gl::Offset srcOffset = {sourceBox.x, sourceBox.y, sourceBox.z};
    gl::Extents extents  = {sourceBox.width, sourceBox.height, sourceBox.depth};

    // Change source layout if necessary
    ANGLE_TRY(
        contextVk->onImageRead(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferSrc, srcImage));

    VkImageSubresourceLayers srcSubresource = {};
    srcSubresource.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    srcSubresource.mipLevel       = static_cast<uint32_t>(sourceLevelGL) - srcImage->getBaseLevel();
    srcSubresource.baseArrayLayer = static_cast<uint32_t>(sourceLayer);
    srcSubresource.layerCount     = layerCount;

    bool isSrc3D  = srcImage->getExtents().depth > 1;
    bool isDest3D = gl_vk::GetImageType(mState.getType()) == VK_IMAGE_TYPE_3D;

    if (isSrc3D)
    {
        Set3DBaseArrayLayerAndLayerCount(&srcSubresource);
    }
    else
    {
        ASSERT(srcSubresource.baseArrayLayer == static_cast<uint32_t>(srcOffset.z));
        srcOffset.z = 0;
    }

    gl::Offset destOffsetModified = destOffset;
    if (!isDest3D)
    {
        // If destination is not 3D, destination offset must be 0.
        destOffsetModified.z = 0;
    }

    // Perform self-copies through a staging buffer.
    // TODO: optimize to copy directly if possible.  http://anglebug.com/4719
    bool isSelfCopy = mImage == srcImage;

    // If destination is valid, copy the source directly into it.
    if (mImage->valid() && !shouldUpdateBeStaged(level) && !isSelfCopy)
    {
        // Make sure any updates to the image are already flushed.
        ANGLE_TRY(ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

        vk::CommandBuffer *commandBuffer;
        ANGLE_TRY(contextVk->onImageWrite(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst,
                                          mImage));
        ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

        VkImageSubresourceLayers destSubresource = srcSubresource;
        destSubresource.mipLevel                 = level;
        destSubresource.baseArrayLayer           = baseLayer;
        destSubresource.layerCount               = layerCount;

        if (isDest3D)
        {
            Set3DBaseArrayLayerAndLayerCount(&destSubresource);
        }
        else if (!isSrc3D)
        {
            // extents.depth should be set to layer count if any of the source or destination is a
            // 2D Array.  If both are 2D Array, it should be set to 1.
            extents.depth = 1;
        }

        vk::ImageHelper::Copy(srcImage, mImage, srcOffset, destOffsetModified, extents,
                              srcSubresource, destSubresource, commandBuffer);
    }
    else
    {
        std::unique_ptr<vk::ImageHelper> stagingImage;

        // Create a temporary image to stage the copy
        stagingImage = std::make_unique<vk::ImageHelper>();

        ANGLE_TRY(stagingImage->init2DStaging(contextVk, renderer->getMemoryProperties(),
                                              gl::Extents(sourceBox.width, sourceBox.height, 1),
                                              destFormat, kTransferStagingImageFlags, layerCount));

        vk::CommandBuffer *commandBuffer;
        ANGLE_TRY(contextVk->onImageWrite(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst,
                                          stagingImage.get()));
        ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

        VkImageSubresourceLayers destSubresource = srcSubresource;
        destSubresource.mipLevel                 = 0;
        destSubresource.baseArrayLayer           = 0;
        destSubresource.layerCount               = layerCount;

        if (!isSrc3D)
        {
            // extents.depth should be set to layer count if any of the source or destination is a
            // 2D Array.  If both are 2D Array, it should be set to 1.
            extents.depth = 1;
        }

        vk::ImageHelper::Copy(srcImage, stagingImage.get(), srcOffset, gl::kOffsetZero, extents,
                              srcSubresource, destSubresource, commandBuffer);

        // Stage the copy for when the image storage is actually created.
        VkImageType imageType = gl_vk::GetImageType(mState.getType());
        const gl::ImageIndex stagingIndex =
            gl::ImageIndex::Make2DArrayRange(level, baseLayer, layerCount);
        mImage->stageSubresourceUpdateFromImage(stagingImage.release(), stagingIndex,
                                                destOffsetModified, extents, imageType);
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::copySubImageImplWithDraw(ContextVk *contextVk,
                                                  const gl::ImageIndex &index,
                                                  const gl::Offset &destOffset,
                                                  const vk::Format &destFormat,
                                                  size_t sourceLevelGL,
                                                  const gl::Box &sourceBox,
                                                  bool isSrcFlipY,
                                                  bool unpackFlipY,
                                                  bool unpackPremultiplyAlpha,
                                                  bool unpackUnmultiplyAlpha,
                                                  vk::ImageHelper *srcImage,
                                                  const vk::ImageView *srcView,
                                                  SurfaceRotation srcFramebufferRotation)
{
    RendererVk *renderer = contextVk->getRenderer();
    UtilsVk &utilsVk     = contextVk->getUtils();

    // Potentially make adjustments for pre-rotatation.
    gl::Box rotatedSourceBox = sourceBox;
    gl::Extents srcExtents   = srcImage->getLevelExtents2D(0);
    switch (srcFramebufferRotation)
    {
        case SurfaceRotation::Identity:
            // No adjustments needed
            break;
        case SurfaceRotation::Rotated90Degrees:
            // Turn off y-flip for 90 degrees, as we don't want it affecting the
            // shaderParams.srcOffset calculation done in UtilsVk::copyImage().
            ASSERT(isSrcFlipY);
            isSrcFlipY = false;
            std::swap(rotatedSourceBox.x, rotatedSourceBox.y);
            std::swap(rotatedSourceBox.width, rotatedSourceBox.height);
            std::swap(srcExtents.width, srcExtents.height);
            break;
        case SurfaceRotation::Rotated180Degrees:
            ASSERT(isSrcFlipY);
            rotatedSourceBox.x = srcExtents.width - sourceBox.x - sourceBox.width - 1;
            rotatedSourceBox.y = srcExtents.height - sourceBox.y - sourceBox.height - 1;
            break;
        case SurfaceRotation::Rotated270Degrees:
            // Turn off y-flip for 270 degrees, as we don't want it affecting the
            // shaderParams.srcOffset calculation done in UtilsVk::copyImage().  It is needed
            // within the shader (when it will affect how the shader looks-up the source pixel),
            // and so shaderParams.flipY is turned on at the right time within
            // UtilsVk::copyImage().
            ASSERT(isSrcFlipY);
            isSrcFlipY         = false;
            rotatedSourceBox.x = srcExtents.height - sourceBox.y - sourceBox.height - 1;
            rotatedSourceBox.y = srcExtents.width - sourceBox.x - sourceBox.width - 1;
            std::swap(rotatedSourceBox.width, rotatedSourceBox.height);
            std::swap(srcExtents.width, srcExtents.height);
            break;
        default:
            UNREACHABLE();
            break;
    }

    UtilsVk::CopyImageParameters params;
    params.srcOffset[0]        = rotatedSourceBox.x;
    params.srcOffset[1]        = rotatedSourceBox.y;
    params.srcExtents[0]       = rotatedSourceBox.width;
    params.srcExtents[1]       = rotatedSourceBox.height;
    params.destOffset[0]       = destOffset.x;
    params.destOffset[1]       = destOffset.y;
    params.srcMip              = static_cast<uint32_t>(sourceLevelGL) - srcImage->getBaseLevel();
    params.srcHeight           = srcExtents.height;
    params.srcPremultiplyAlpha = unpackPremultiplyAlpha && !unpackUnmultiplyAlpha;
    params.srcUnmultiplyAlpha  = unpackUnmultiplyAlpha && !unpackPremultiplyAlpha;
    params.srcFlipY            = isSrcFlipY;
    params.destFlipY           = unpackFlipY;
    params.srcRotation         = srcFramebufferRotation;

    uint32_t level      = index.getLevelIndex();
    uint32_t baseLayer  = index.hasLayer() ? index.getLayerIndex() : destOffset.z;
    uint32_t layerCount = sourceBox.depth;

    ASSERT(layerCount == static_cast<uint32_t>(gl::ImageIndex::kEntireLevel) ||
           layerCount == static_cast<uint32_t>(sourceBox.depth));

    gl::Extents extents = {sourceBox.width, sourceBox.height, sourceBox.depth};

    bool isSrc3D  = srcImage->getExtents().depth > 1;
    bool isDest3D = gl_vk::GetImageType(mState.getType()) == VK_IMAGE_TYPE_3D;

    // Perform self-copies through a staging buffer.
    // TODO: optimize to copy directly if possible.  http://anglebug.com/4719
    bool isSelfCopy = mImage == srcImage;

    // If destination is valid, copy the source directly into it.
    if (mImage->valid() && !shouldUpdateBeStaged(level) && !isSelfCopy)
    {
        // Make sure any updates to the image are already flushed.
        ANGLE_TRY(ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

        for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            params.srcLayer = layerIndex + sourceBox.z;

            const vk::ImageView *destView;
            ANGLE_TRY(getLevelLayerImageView(contextVk, level, baseLayer + layerIndex, &destView));

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
                                              gl::Extents(sourceBox.width, sourceBox.height, 1),
                                              destFormat, kDrawStagingImageFlags, layerCount));

        params.destOffset[0] = 0;
        params.destOffset[1] = 0;

        for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
        {
            params.srcLayer = layerIndex + sourceBox.z;

            // Create a temporary view for this layer.
            vk::ImageView stagingView;
            ANGLE_TRY(stagingImage->initLayerImageView(
                contextVk, stagingTextureType, VK_IMAGE_ASPECT_COLOR_BIT, gl::SwizzleState(),
                &stagingView, 0, 1, layerIndex, 1));

            ANGLE_TRY(utilsVk.copyImage(contextVk, stagingImage.get(), &stagingView, srcImage,
                                        srcView, params));

            // Queue the resource for cleanup as soon as the copy above is finished.  There's no
            // need to keep it around.
            contextVk->addGarbage(&stagingView);
        }

        if (!isSrc3D)
        {
            // extents.depth should be set to layer count if any of the source or destination is a
            // 2D Array.  If both are 2D Array, it should be set to 1.
            extents.depth = 1;
        }

        gl::Offset destOffsetModified = destOffset;
        if (!isDest3D)
        {
            // If destination is not 3D, destination offset must be 0.
            destOffsetModified.z = 0;
        }

        // Stage the copy for when the image storage is actually created.
        VkImageType imageType = gl_vk::GetImageType(mState.getType());
        const gl::ImageIndex stagingIndex =
            gl::ImageIndex::Make2DArrayRange(level, baseLayer, layerCount);
        mImage->stageSubresourceUpdateFromImage(stagingImage.release(), stagingIndex,
                                                destOffsetModified, extents, imageType);
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::setStorage(const gl::Context *context,
                                    gl::TextureType type,
                                    size_t levels,
                                    GLenum internalFormat,
                                    const gl::Extents &size)
{
    return setStorageMultisample(context, type, 1, internalFormat, size, true);
}

angle::Result TextureVk::setStorageMultisample(const gl::Context *context,
                                               gl::TextureType type,
                                               GLsizei samples,
                                               GLint internalformat,
                                               const gl::Extents &size,
                                               bool fixedSampleLocations)
{
    ContextVk *contextVk = GetAs<ContextVk>(context->getImplementation());
    RendererVk *renderer = contextVk->getRenderer();

    if (!mOwnsImage)
    {
        releaseAndDeleteImage(contextVk);
    }

    const vk::Format &format = renderer->getFormat(internalformat);
    ANGLE_TRY(ensureImageAllocated(contextVk, format));

    if (mImage->valid())
    {
        releaseImage(contextVk);
    }
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

    setImageHelper(contextVk, new vk::ImageHelper(), mState.getType(), format, 0, 0, 0, true);

    ANGLE_TRY(
        memoryObjectVk->createImage(contextVk, type, levels, internalFormat, size, offset, mImage));

    gl::Format glFormat(internalFormat);
    ANGLE_TRY(initImageViews(contextVk, format, glFormat.info->sized, static_cast<uint32_t>(levels),
                             mImage->getLayerCount()));

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
                   imageVk->getImageLevel(), imageVk->getImageLayer(),
                   mState.getEffectiveBaseLevel(), false);

    ASSERT(type != gl::TextureType::CubeMap);
    ANGLE_TRY(initImageViews(contextVk, format, image->getFormat().info->sized, 1, 1));

    // Transfer the image to this queue if needed
    uint32_t rendererQueueFamilyIndex = renderer->getQueueFamilyIndex();
    if (mImage->isQueueChangeNeccesary(rendererQueueFamilyIndex))
    {
        vk::CommandBuffer *commandBuffer = nullptr;
        vk::ImageLayout newLayout        = vk::ImageLayout::AllGraphicsShadersReadWrite;
        if (mImage->getUsage() & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            newLayout = vk::ImageLayout::ColorAttachment;
        }
        else if (mImage->getUsage() & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            newLayout = vk::ImageLayout::DepthStencilAttachment;
        }
        else if (mImage->getUsage() &
                 (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
        {
            newLayout = vk::ImageLayout::AllGraphicsShadersReadOnly;
        }

        ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));
        mImage->changeLayoutAndQueue(mImage->getAspectFlags(), newLayout, rendererQueueFamilyIndex,
                                     commandBuffer);
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
        mImageObserverBinding.bind(nullptr);
        SafeDelete(mImage);
    }
    mRedefinedLevels.reset();
}

angle::Result TextureVk::ensureImageAllocated(ContextVk *contextVk, const vk::Format &format)
{
    if (mImage == nullptr)
    {
        setImageHelper(contextVk, new vk::ImageHelper(), mState.getType(), format, 0, 0, 0, true);
    }
    else
    {
        // Note: one possible path here is when an image level is being redefined to a different
        // format.  In that case, staged updates with the new format should succeed, but otherwise
        // the format should not affect the currently allocated image.  The following function only
        // takes the alignment requirement to make sure the format is not accidentally used for any
        // other purpose.
        updateImageHelper(contextVk, format.getImageCopyBufferAlignment());
    }

    mImageUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;

    // If the image has depth/stencil support, add those as possible usage.
    if (contextVk->getRenderer()->hasImageFormatFeatureBits(
            format.vkImageFormat, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        mImageUsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    else if (contextVk->getRenderer()->hasImageFormatFeatureBits(
                 format.vkImageFormat, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        mImageUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    return angle::Result::Continue;
}

void TextureVk::setImageHelper(ContextVk *contextVk,
                               vk::ImageHelper *imageHelper,
                               gl::TextureType imageType,
                               const vk::Format &format,
                               uint32_t imageLevelOffset,
                               uint32_t imageLayerOffset,
                               uint32_t imageBaseLevel,
                               bool selfOwned)
{
    ASSERT(mImage == nullptr);

    mImageObserverBinding.bind(imageHelper);

    mOwnsImage        = selfOwned;
    mImageNativeType  = imageType;
    mImageLevelOffset = imageLevelOffset;
    mImageLayerOffset = imageLayerOffset;
    mImage            = imageHelper;
    updateImageHelper(contextVk, format.getImageCopyBufferAlignment());

    // Force re-creation of render targets next time they are needed
    for (RenderTargetVector &renderTargetLevels : mRenderTargets)
    {
        renderTargetLevels.clear();
    }
    mRenderTargets.clear();

    mSerial = contextVk->generateTextureSerial();
}

void TextureVk::updateImageHelper(ContextVk *contextVk, size_t imageCopyBufferAlignment)
{
    ASSERT(mImage != nullptr);
    mImage->initStagingBuffer(contextVk->getRenderer(), imageCopyBufferAlignment,
                              vk::kStagingBufferFlags, mStagingBufferInitialSize);
}

angle::Result TextureVk::redefineLevel(const gl::Context *context,
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
        uint32_t levelIndexGL = index.getLevelIndex();
        uint32_t layerIndex   = index.hasLayer() ? index.getLayerIndex() : 0;
        mImage->removeSingleSubresourceStagedUpdates(contextVk, levelIndexGL, layerIndex);

        if (mImage->valid())
        {
            // If the level that's being redefined is outside the level range of the allocated
            // image, the application is free to use any size or format.  Any data uploaded to it
            // will live in staging area until the texture base/max level is adjusted to include
            // this level, at which point the image will be recreated.
            //
            // Otherwise, if the level that's being redefined has a different format or size,
            // only release the image if it's single-mip, and keep the uploaded data staged.
            // Otherwise the image is mip-incomplete anyway and will be eventually recreated when
            // needed.  Only exception to this latter is if all the levels of the texture are
            // redefined such that the image becomes mip-complete in the end.
            // mRedefinedLevels is used during syncState to support this use-case.
            //
            // Note that if the image has multiple mips, there could be a copy from one mip
            // happening to the other, which means the image cannot be released.
            //
            // In summary:
            //
            // - If the image has a single level, and that level is being redefined, release the
            //   image.
            // - Otherwise keep the image intact (another mip may be the source of a copy), and
            //   make sure any updates to this level are staged.
            bool isInAllocatedImage = IsTextureLevelInAllocatedImage(*mImage, levelIndexGL);
            bool isCompatibleRedefinition =
                isInAllocatedImage &&
                IsTextureLevelDefinitionCompatibleWithImage(*mImage, levelIndexGL, size, format);

            // Mark the level as incompatibly redefined if that's the case.  Note that if the level
            // was previously incompatibly defined, then later redefined to be compatible, the
            // corresponding bit should clear.
            if (isInAllocatedImage)
            {
                mRedefinedLevels.set(levelIndexGL - mImage->getBaseLevel(),
                                     !isCompatibleRedefinition);
            }

            bool isUpdateToSingleLevelImage =
                mImage->getLevelCount() == 1 && mImage->getBaseLevel() == levelIndexGL;

            // If incompatible, and redefining the single-level image, release it so it can be
            // recreated immediately.  This is an optimization to avoid an extra copy.
            if (!isCompatibleRedefinition && isUpdateToSingleLevelImage)
            {
                releaseImage(contextVk);
            }
        }
    }

    // If image is not released due to an out-of-range or incompatible level definition, the image
    // is still valid and we shouldn't redefine it to use the new format.  In that case,
    // ensureImageAllocated will only use the format to update the staging buffer's alignment to
    // support both the previous and the new formats.
    if (!size.empty())
    {
        ANGLE_TRY(ensureImageAllocated(contextVk, format));
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::copyImageDataToBufferAndGetData(ContextVk *contextVk,
                                                         size_t sourceLevelGL,
                                                         uint32_t layerCount,
                                                         const gl::Box &sourceArea,
                                                         uint8_t **outDataPtr)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "TextureVk::copyImageDataToBufferAndGetData");

    // Make sure the source is initialized and it's images are flushed.
    ANGLE_TRY(ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

    vk::BufferHelper *copyBuffer                   = nullptr;
    vk::StagingBufferOffsetArray sourceCopyOffsets = {0, 0};
    size_t bufferSize                              = 0;

    gl::Box modifiedSourceArea = sourceArea;

    bool is3D = mImage->getExtents().depth > 1;
    if (is3D)
    {
        layerCount = 1;
    }
    else
    {
        modifiedSourceArea.depth = 1;
    }

    ANGLE_TRY(mImage->copyImageDataToBuffer(contextVk, sourceLevelGL, layerCount, 0,
                                            modifiedSourceArea, &copyBuffer, &bufferSize,
                                            &sourceCopyOffsets, outDataPtr));

    // Explicitly finish. If new use cases arise where we don't want to block we can change this.
    ANGLE_TRY(contextVk->finishImpl());

    return angle::Result::Continue;
}

angle::Result TextureVk::copyBufferDataToImage(ContextVk *contextVk,
                                               vk::BufferHelper *srcBuffer,
                                               const gl::ImageIndex index,
                                               uint32_t rowLength,
                                               uint32_t imageHeight,
                                               const gl::Box &sourceArea,
                                               size_t offset)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "TextureVk::copyBufferDataToImage");

    // Vulkan Spec requires the bufferOffset to be a multiple of 4 for vkCmdCopyBufferToImage.
    ASSERT((offset & (kBufferOffsetMultiple - 1)) == 0);

    GLuint layerCount = 0;
    GLuint layerIndex = 0;
    GetRenderTargetLayerCountAndIndex(mImage, index, &layerCount, &layerIndex);

    // Make sure the source is initialized and its images are flushed.
    ANGLE_TRY(ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(contextVk->onBufferTransferRead(srcBuffer));
    ANGLE_TRY(
        contextVk->onImageWrite(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst, mImage));
    ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

    VkBufferImageCopy region               = {};
    region.bufferOffset                    = offset;
    region.bufferRowLength                 = rowLength;
    region.bufferImageHeight               = imageHeight;
    region.imageExtent.width               = sourceArea.width;
    region.imageExtent.height              = sourceArea.height;
    region.imageExtent.depth               = sourceArea.depth;
    region.imageOffset.x                   = sourceArea.x;
    region.imageOffset.y                   = sourceArea.y;
    region.imageOffset.z                   = sourceArea.z;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = layerIndex;
    region.imageSubresource.layerCount     = 1;
    region.imageSubresource.mipLevel       = static_cast<uint32_t>(index.getLevelIndex());

    if (index.getType() == gl::TextureType::_2DArray)
    {
        region.imageExtent.depth           = 1;
        region.imageSubresource.layerCount = sourceArea.depth;
    }

    commandBuffer->copyBufferToImage(srcBuffer->getBuffer().getHandle(), mImage->getImage(),
                                     mImage->getCurrentLayout(), 1, &region);

    return angle::Result::Continue;
}

angle::Result TextureVk::generateMipmapsWithCompute(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    // Requires that the image:
    //
    // - is not sRGB
    // - is not integer
    // - is 2D or 2D array
    // - is single sample
    // - is color image
    //
    // Support for the first two can be added easily.  Supporting 3D textures, MSAA and
    // depth/stencil would be more involved.
    ASSERT(gl::GetSizedInternalFormatInfo(mImage->getFormat().internalFormat).colorEncoding !=
           GL_SRGB);
    ASSERT(!mImage->getFormat().actualImageFormat().isInt());
    ASSERT(mImage->getType() == VK_IMAGE_TYPE_2D);
    ASSERT(mImage->getSamples() == 1);
    ASSERT(mImage->getAspectFlags() == VK_IMAGE_ASPECT_COLOR_BIT);

    // Create the appropriate sampler.
    GLenum filter = CalculateGenerateMipmapFilter(contextVk, mImage->getFormat());

    gl::SamplerState samplerState;
    samplerState.setMinFilter(filter);
    samplerState.setMagFilter(filter);
    samplerState.setWrapS(GL_CLAMP_TO_EDGE);
    samplerState.setWrapT(GL_CLAMP_TO_EDGE);
    samplerState.setWrapR(GL_CLAMP_TO_EDGE);

    vk::BindingPointer<vk::Sampler> sampler;
    vk::SamplerDesc samplerDesc(samplerState, false, 0);
    ANGLE_TRY(renderer->getSamplerCache().getSampler(contextVk, samplerDesc, &sampler));

    // If the image has more levels than supported, generate as many mips as possible at a time.
    const uint32_t maxGenerateLevels = UtilsVk::GetGenerateMipmapMaxLevels(contextVk);

    for (uint32_t destBaseLevel = 1; destBaseLevel < mImage->getLevelCount();
         destBaseLevel += maxGenerateLevels)
    {
        ANGLE_TRY(contextVk->onImageWrite(VK_IMAGE_ASPECT_COLOR_BIT,
                                          vk::ImageLayout::ComputeShaderWrite, mImage));

        // Generate mipmaps for every layer separately.
        for (uint32_t layer = 0; layer < mImage->getLayerCount(); ++layer)
        {
            // Create the necessary views.
            const vk::ImageView *srcView                         = nullptr;
            UtilsVk::GenerateMipmapDestLevelViews destLevelViews = {};

            const uint32_t srcLevel = destBaseLevel - 1;
            ANGLE_TRY(mImageViews.getLevelLayerDrawImageView(contextVk, *mImage, srcLevel, layer,
                                                             &srcView));

            uint32_t destLevelCount = maxGenerateLevels;
            for (uint32_t level = 0; level < maxGenerateLevels; ++level)
            {
                uint32_t destLevel = destBaseLevel + level;

                // If fewer levels left than maxGenerateLevels, cut the loop short.
                if (destLevel >= mImage->getLevelCount())
                {
                    destLevelCount = level;
                    break;
                }

                ANGLE_TRY(mImageViews.getLevelLayerDrawImageView(contextVk, *mImage, destLevel,
                                                                 layer, &destLevelViews[level]));
            }

            // If the image has fewer than maximum levels, fill the last views with a dummy view.
            ASSERT(destLevelCount > 0);
            for (uint32_t level = destLevelCount; level < UtilsVk::kGenerateMipmapMaxLevels;
                 ++level)
            {
                destLevelViews[level] = destLevelViews[level - 1];
            }

            // Generate mipmaps.
            UtilsVk::GenerateMipmapParameters params = {};
            params.srcLevel                          = srcLevel;
            params.destLevelCount                    = destLevelCount;

            ANGLE_TRY(contextVk->getUtils().generateMipmap(contextVk, mImage, srcView, mImage,
                                                           destLevelViews, sampler.get(), params));
        }
    }

    return angle::Result::Continue;
}

angle::Result TextureVk::generateMipmapsWithCPU(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    const VkExtent3D baseLevelExtents = mImage->getExtents();
    uint32_t imageLayerCount          = mImage->getLayerCount();

    uint8_t *imageData = nullptr;
    gl::Box imageArea(0, 0, 0, baseLevelExtents.width, baseLevelExtents.height,
                      baseLevelExtents.depth);

    ANGLE_TRY(copyImageDataToBufferAndGetData(contextVk, mState.getEffectiveBaseLevel(),
                                              imageLayerCount, imageArea, &imageData));

    const angle::Format &angleFormat = mImage->getFormat().actualImageFormat();
    GLuint sourceRowPitch            = baseLevelExtents.width * angleFormat.pixelBytes;
    GLuint sourceDepthPitch          = sourceRowPitch * baseLevelExtents.height;
    size_t baseLevelAllocationSize   = sourceDepthPitch * baseLevelExtents.depth;

    // We now have the base level available to be manipulated in the imageData pointer. Generate all
    // the missing mipmaps with the slow path. For each layer, use the copied data to generate all
    // the mips.
    for (GLuint layer = 0; layer < imageLayerCount; layer++)
    {
        size_t bufferOffset = layer * baseLevelAllocationSize;

        ANGLE_TRY(generateMipmapLevelsWithCPU(
            contextVk, angleFormat, layer, mState.getEffectiveBaseLevel() + 1,
            mState.getMipmapMaxLevel(), baseLevelExtents.width, baseLevelExtents.height,
            baseLevelExtents.depth, sourceRowPitch, sourceDepthPitch, imageData + bufferOffset));
    }

    ASSERT(!mRedefinedLevels.any());
    return flushImageStagedUpdates(contextVk);
}

angle::Result TextureVk::generateMipmap(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    // The image should already be allocated by a prior syncState.
    ASSERT(mImage->valid());

    // If base level has changed, the front-end should have called syncState already.
    ASSERT(mImage->getBaseLevel() == mState.getEffectiveBaseLevel());

    // Only staged update here is the robust resource init if any.
    ANGLE_TRY(ensureImageInitialized(contextVk, ImageMipLevels::FullMipChain));

    uint32_t maxLevel = mState.getMipmapMaxLevel() - mState.getEffectiveBaseLevel();
    ASSERT(maxLevel != 0);

    // If it's possible to generate mipmap in compute, that would give the best possible
    // performance on some hardware.
    if (CanGenerateMipmapWithCompute(renderer, mImage->getType(), mImage->getFormat(),
                                     mImage->getSamples()))
    {
        ASSERT((mImageUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) != 0);

        mImage->retain(&contextVk->getResourceUseList());
        mImageViews.retain(&contextVk->getResourceUseList());

        return generateMipmapsWithCompute(contextVk);
    }
    else if (renderer->hasImageFormatFeatureBits(mImage->getFormat().vkImageFormat,
                                                 kBlitFeatureFlags))
    {
        // Otherwise, use blit if possible.
        return mImage->generateMipmapsWithBlit(contextVk, maxLevel);
    }

    // If not possible to generate mipmaps on the GPU, do it on the CPU for conformance.
    return generateMipmapsWithCPU(context);
}

angle::Result TextureVk::copyAndStageImageSubresource(ContextVk *contextVk,
                                                      bool ignoreLayerCount,
                                                      uint32_t currentLayer,
                                                      uint32_t srcLevelVk,
                                                      uint32_t dstLevelGL)
{
    const gl::Extents &baseLevelExtents = mImage->getLevelExtents(srcLevelVk);

    VkExtent3D updatedExtents;
    VkOffset3D offset = {};
    uint32_t layerCount;
    gl_vk::GetExtentsAndLayerCount(mState.getType(), baseLevelExtents, &updatedExtents,
                                   &layerCount);
    gl::Box area(offset.x, offset.y, offset.z, updatedExtents.width, updatedExtents.height,
                 updatedExtents.depth);
    // TODO: Refactor TextureVk::respecifyImageAttributesAndLevels() to avoid this workaround.
    if (ignoreLayerCount)
    {
        layerCount = 1;
    }

    // Copy from the base level image to the staging buffer
    vk::BufferHelper *stagingBuffer                   = nullptr;
    vk::StagingBufferOffsetArray stagingBufferOffsets = {0, 0};
    size_t bufferSize                                 = 0;
    ANGLE_TRY(mImage->copyImageDataToBuffer(contextVk, srcLevelVk + mImage->getBaseLevel(),
                                            layerCount, currentLayer, area, &stagingBuffer,
                                            &bufferSize, &stagingBufferOffsets, nullptr));

    // Stage an update to the new image
    ASSERT(stagingBuffer);
    uint32_t bufferRowLength   = updatedExtents.width;
    uint32_t bufferImageHeight = updatedExtents.height;
    const gl::InternalFormat &formatInfo =
        gl::GetSizedInternalFormatInfo(mImage->getFormat().internalFormat);
    if (formatInfo.compressed)
    {
        // In the case of a compressed texture, bufferRowLength can never be smaller than the
        // compressed format's compressed block width, and bufferImageHeight can never be smaller
        // than the compressed block height.
        bufferRowLength   = std::max(bufferRowLength, formatInfo.compressedBlockWidth);
        bufferImageHeight = std::max(bufferImageHeight, formatInfo.compressedBlockHeight);
    }
    ANGLE_TRY(mImage->stageSubresourceUpdateFromBuffer(
        contextVk, bufferSize, dstLevelGL, currentLayer, layerCount, bufferRowLength,
        bufferImageHeight, updatedExtents, offset, stagingBuffer, stagingBufferOffsets));

    return angle::Result::Continue;
}

angle::Result TextureVk::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    return angle::Result::Continue;
}

angle::Result TextureVk::updateBaseMaxLevels(ContextVk *contextVk,
                                             GLuint baseLevel,
                                             GLuint maxLevel)
{
    if (!mImage)
    {
        return angle::Result::Continue;
    }

    // Track the previous levels for use in update loop below
    uint32_t previousBaseLevel = mImage->getBaseLevel();
    uint32_t previousMaxLevel  = mImage->getMaxLevel();

    ASSERT(baseLevel <= maxLevel);
    bool baseLevelChanged = baseLevel != previousBaseLevel;
    bool maxLevelChanged  = (previousMaxLevel != maxLevel);

    if (!(baseLevelChanged || maxLevelChanged))
    {
        // This scenario is a noop, most likely maxLevel has been lowered to a level that already
        // reflects the current state of the image
        return angle::Result::Continue;
    }

    if (!mImage->valid())
    {
        // Track the levels in our ImageHelper
        mImage->setBaseAndMaxLevels(baseLevel, maxLevel);

        // No further work to do, let staged updates handle the new levels
        return angle::Result::Continue;
    }

    // With a valid image, check if only changing the maxLevel to a subset of the texture's actual
    // number of mip levels
    if (!baseLevelChanged && (maxLevel < baseLevel + mImage->getLevelCount()))
    {
        // Don't need to respecify the texture; just redo the texture's vkImageViews
        ASSERT(maxLevelChanged);

        // Release the current vkImageViews
        RendererVk *renderer = contextVk->getRenderer();
        mImageViews.release(renderer);

        // Track the levels in our ImageHelper
        mImage->setBaseAndMaxLevels(baseLevel, maxLevel);

        // Update the texture's serial so that the descriptor set is updated correctly
        mSerial = contextVk->generateTextureSerial();

        // Change the vkImageViews
        const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();
        // We use a special layer count here to handle EGLImages. They might only be
        // looking at one layer of a cube or 2D array texture.
        uint32_t layerCount =
            mState.getType() == gl::TextureType::_2D ? 1 : mImage->getLayerCount();
        return initImageViews(contextVk, mImage->getFormat(), baseLevelDesc.format.info->sized,
                              maxLevel - baseLevel + 1, layerCount);
    }

    return respecifyImageAttributesAndLevels(contextVk, previousBaseLevel, baseLevel, maxLevel);
}

angle::Result TextureVk::respecifyImageAttributes(ContextVk *contextVk)
{
    return respecifyImageAttributesAndLevels(contextVk, mImage->getBaseLevel(),
                                             mState.getEffectiveBaseLevel(),
                                             mState.getEffectiveMaxLevel());
}

angle::Result TextureVk::respecifyImageAttributesAndLevels(ContextVk *contextVk,
                                                           GLuint previousBaseLevel,
                                                           GLuint baseLevel,
                                                           GLuint maxLevel)
{
    // Recreate the image to reflect new base or max levels.
    // First, flush any pending updates so we have good data in the existing vkImage
    if (mImage->valid() && mImage->hasStagedUpdates())
    {
        ANGLE_TRY(flushImageStagedUpdates(contextVk));
    }

    // After flushing, track the new levels (they are used in the flush, hence the wait)
    mImage->setBaseAndMaxLevels(baseLevel, maxLevel);

    if (!mImage->valid())
    {
        releaseImage(contextVk);
        return angle::Result::Continue;
    }

    // Next, back up any data we need to preserve by staging it as updates to the new image.

    // Preserve the data in the Vulkan image.  GL texture's staged updates that correspond to levels
    // outside the range of the Vulkan image will remain intact.

    // The staged updates won't be applied until the image has the requisite mip levels
    for (uint32_t layer = 0; layer < mImage->getLayerCount(); layer++)
    {
        for (uint32_t levelVK = 0; levelVK < mImage->getLevelCount(); levelVK++)
        {
            // Vulkan level 0 previously aligned with whatever the base level was.
            uint32_t levelGL = levelVK + previousBaseLevel;

            if (mRedefinedLevels.test(levelVK))
            {
                // Note: if this level is incompatibly redefined, there will necessarily be a staged
                // update, and the contents of the image are to be thrown away.
                ASSERT(mImage->isUpdateStaged(levelGL, layer));
                continue;
            }

            ASSERT(!mImage->isUpdateStaged(levelGL, layer));

            // Pull data from the current image and stage it as an update for the new image

            // First we populate the staging buffer with current level data
            ANGLE_TRY(copyAndStageImageSubresource(contextVk, true, layer, levelVK, levelGL));
        }
    }

    // Now that we've staged all the updates, release the current image so that it will be
    // recreated with the correct number of mip levels, base level, and max level.
    releaseImage(contextVk);

    mImage->retain(&contextVk->getResourceUseList());

    return angle::Result::Continue;
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
                   surface->getMipmapLevel(), 0, mState.getEffectiveBaseLevel(), false);

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
    ASSERT(imageIndex.getLevelIndex() >= 0);

    ContextVk *contextVk = vk::GetImpl(context);

    if (!mImage->valid())
    {
        const gl::ImageDesc &baseLevelDesc  = mState.getBaseLevelDesc();
        const gl::Extents &baseLevelExtents = baseLevelDesc.size;
        const uint32_t levelCount           = getMipLevelCount(ImageMipLevels::EnabledLevels);
        const vk::Format &format            = getBaseLevelFormat(contextVk->getRenderer());

        ANGLE_TRY(initImage(contextVk, format, baseLevelDesc.format.info->sized, baseLevelExtents,
                            levelCount));
    }

    // Don't flush staged updates here. We'll handle that in FramebufferVk so it can defer clears.

    GLuint layerIndex = 0, layerCount = 0;
    GetRenderTargetLayerCountAndIndex(mImage, imageIndex, &layerCount, &layerIndex);

    ANGLE_TRY(initRenderTargets(contextVk, layerCount, imageIndex.getLevelIndex()));

    ASSERT(imageIndex.getLevelIndex() < static_cast<int32_t>(mRenderTargets.size()));
    *rtOut = &mRenderTargets[imageIndex.getLevelIndex()][layerIndex];

    return angle::Result::Continue;
}

angle::Result TextureVk::ensureImageInitialized(ContextVk *contextVk, ImageMipLevels mipLevels)
{
    if (mImage->valid() && !mImage->hasStagedUpdates())
    {
        return angle::Result::Continue;
    }

    if (!mImage->valid())
    {
        ASSERT(!mRedefinedLevels.any());

        const gl::ImageDesc &baseLevelDesc  = mState.getBaseLevelDesc();
        const gl::Extents &baseLevelExtents = baseLevelDesc.size;
        const uint32_t levelCount           = getMipLevelCount(mipLevels);
        const vk::Format &format            = getBaseLevelFormat(contextVk->getRenderer());

        ANGLE_TRY(initImage(contextVk, format, baseLevelDesc.format.info->sized, baseLevelExtents,
                            levelCount));

        if (mipLevels == ImageMipLevels::FullMipChain)
        {
            // Remove staged updates to non-base mips when generating mipmaps.  These can only be
            // emulated format init clears that are staged in initImage.
            mImage->removeStagedUpdates(contextVk, mState.getEffectiveBaseLevel() + 1,
                                        mState.getMipmapMaxLevel());
        }
    }

    return flushImageStagedUpdates(contextVk);
}

angle::Result TextureVk::flushImageStagedUpdates(ContextVk *contextVk)
{
    ASSERT(mImage->valid());

    vk::CommandBuffer *commandBuffer = nullptr;
    ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));
    return mImage->flushStagedUpdates(contextVk, getNativeImageLevel(0), mImage->getLevelCount(),
                                      getNativeImageLayer(0), mImage->getLayerCount(),
                                      mRedefinedLevels, commandBuffer);
}

angle::Result TextureVk::initRenderTargets(ContextVk *contextVk,
                                           GLuint layerCount,
                                           GLuint levelIndex)
{
    if (mRenderTargets.size() <= levelIndex)
    {
        mRenderTargets.resize(levelIndex + 1);
    }

    // Lazy init. Check if already initialized.
    if (!mRenderTargets[levelIndex].empty())
    {
        return angle::Result::Continue;
    }

    mRenderTargets[levelIndex].resize(layerCount);

    for (uint32_t layerIndex = 0; layerIndex < layerCount; ++layerIndex)
    {
        mRenderTargets[levelIndex][layerIndex].init(
            mImage, &mImageViews, getNativeImageLevel(levelIndex), getNativeImageLayer(layerIndex));
    }
    return angle::Result::Continue;
}

void TextureVk::prepareForGenerateMipmap(ContextVk *contextVk)
{
    // Remove staged updates to the range that's being respecified (which is all the mips except
    // mip 0).
    uint32_t baseLevel = mState.getEffectiveBaseLevel() + 1;
    uint32_t maxLevel  = mState.getMipmapMaxLevel();

    mImage->removeStagedUpdates(contextVk, baseLevel, maxLevel);

    // These levels are no longer incompatibly defined if they previously were.  The
    // corresponding bits in mRedefinedLevels should be cleared.  Note that the texture may be
    // simultaneously rebased, so mImage->getBaseLevel() and getEffectiveBaseLevel() may be
    // different.
    static_assert(gl::IMPLEMENTATION_MAX_TEXTURE_LEVELS < 32,
                  "levels mask assumes 32-bits is enough");
    gl::TexLevelMask::value_type levelsMask = angle::Bit<uint32_t>(maxLevel + 1 - baseLevel) - 1;

    uint32_t imageBaseLevel = mImage->getBaseLevel();
    if (imageBaseLevel > baseLevel)
    {
        levelsMask >>= imageBaseLevel - baseLevel;
    }
    else
    {
        levelsMask <<= baseLevel - imageBaseLevel;
    }

    mRedefinedLevels &= gl::TexLevelMask(~levelsMask);

    // If generating mipmap and base level is incompatibly redefined, the image is going to be
    // recreated.  Don't try to preserve the other mips.
    if (mRedefinedLevels.test(0))
    {
        releaseImage(contextVk);
    }

    const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();
    VkImageType imageType              = gl_vk::GetImageType(mState.getType());
    const vk::Format &format           = getBaseLevelFormat(contextVk->getRenderer());
    const GLint samples                = baseLevelDesc.samples ? baseLevelDesc.samples : 1;

    // If the compute path is to be used to generate mipmaps, add the STORAGE usage.
    if (CanGenerateMipmapWithCompute(contextVk->getRenderer(), imageType, format, samples))
    {
        mImageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
}

angle::Result TextureVk::syncState(const gl::Context *context,
                                   const gl::Texture::DirtyBits &dirtyBits,
                                   gl::TextureCommand source)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    VkImageUsageFlags oldUsageFlags   = mImageUsageFlags;
    VkImageCreateFlags oldCreateFlags = mImageCreateFlags;

    // Create a new image if the storage state is enabled for the first time.
    if (dirtyBits.test(gl::Texture::DIRTY_BIT_BOUND_AS_IMAGE))
    {
        mImageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        mImageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    if (dirtyBits.test(gl::Texture::DIRTY_BIT_SRGB_OVERRIDE))
    {
        if (mState.getSRGBOverride() != gl::SrgbOverride::Default)
        {
            mImageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            mRequiresSRGBViews = true;
        }
    }

    // Before redefining the image for any reason, check to see if it's about to go through mipmap
    // generation.  In that case, drop every staged change for the subsequent mips after base, and
    // make sure the image is created with the complete mipchain.
    bool isGenerateMipmap = source == gl::TextureCommand::GenerateMipmap;
    if (isGenerateMipmap)
    {
        prepareForGenerateMipmap(contextVk);
    }

    // Set base and max level before initializing the image
    if (dirtyBits.test(gl::Texture::DIRTY_BIT_MAX_LEVEL) ||
        dirtyBits.test(gl::Texture::DIRTY_BIT_BASE_LEVEL))
    {
        ANGLE_TRY(updateBaseMaxLevels(contextVk, mState.getEffectiveBaseLevel(),
                                      mState.getEffectiveMaxLevel()));
    }

    // It is possible for the image to have a single level (because it doesn't use mipmapping),
    // then have more levels defined in it and mipmapping enabled.  In that case, the image needs
    // to be recreated.
    bool isMipmapEnabledByMinFilter = false;
    if (!isGenerateMipmap && mImage->valid() && dirtyBits.test(gl::Texture::DIRTY_BIT_MIN_FILTER))
    {
        isMipmapEnabledByMinFilter =
            mImage->getLevelCount() < getMipLevelCount(ImageMipLevels::EnabledLevels);
    }

    // If generating mipmaps and the image needs to be recreated (not full-mip already, or changed
    // usage flags), make sure it's recreated.
    if (isGenerateMipmap && mImage->valid() &&
        (oldUsageFlags != mImageUsageFlags ||
         mImage->getLevelCount() != getMipLevelCount(ImageMipLevels::FullMipChain)))
    {
        ASSERT(mOwnsImage);

        // Flush staged updates to the base level of the image.  Note that updates to the rest of
        // the levels have already been discarded through the |removeStagedUpdates| call above.
        ANGLE_TRY(flushImageStagedUpdates(contextVk));

        mImage->stageSelfForBaseLevel();

        // Release views and render targets created for the old image.
        releaseImage(contextVk);
    }

    // Respecify the image if it's changed in usage, or if any of its levels are redefined and no
    // update to base/max levels were done (otherwise the above call would have already taken care
    // of this).  Note that if both base/max and image usage are changed, the image is recreated
    // twice, which incurs unncessary copies.  This is not expected to be happening in real
    // applications.
    if (oldUsageFlags != mImageUsageFlags || oldCreateFlags != mImageCreateFlags ||
        mRedefinedLevels.any() || isMipmapEnabledByMinFilter)
    {
        ANGLE_TRY(respecifyImageAttributes(contextVk));
    }

    // Initialize the image storage and flush the pixel buffer.
    ANGLE_TRY(ensureImageInitialized(contextVk, isGenerateMipmap ? ImageMipLevels::FullMipChain
                                                                 : ImageMipLevels::EnabledLevels));

    // Mask out the IMPLEMENTATION dirty bit to avoid unnecessary syncs.
    gl::Texture::DirtyBits localBits = dirtyBits;
    localBits.reset(gl::Texture::DIRTY_BIT_IMPLEMENTATION);
    localBits.reset(gl::Texture::DIRTY_BIT_BASE_LEVEL);
    localBits.reset(gl::Texture::DIRTY_BIT_MAX_LEVEL);

    if (localBits.none() && mSampler.valid())
    {
        return angle::Result::Continue;
    }

    if (mSampler.valid())
    {
        mSampler.reset();
    }

    if (localBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_RED) ||
        localBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_GREEN) ||
        localBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_BLUE) ||
        localBits.test(gl::Texture::DIRTY_BIT_SWIZZLE_ALPHA) ||
        localBits.test(gl::Texture::DIRTY_BIT_SRGB_OVERRIDE))
    {
        // We use a special layer count here to handle EGLImages. They might only be
        // looking at one layer of a cube or 2D array texture.
        uint32_t layerCount =
            mState.getType() == gl::TextureType::_2D ? 1 : mImage->getLayerCount();

        mImageViews.release(renderer);
        const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();

        ANGLE_TRY(initImageViews(contextVk, mImage->getFormat(), baseLevelDesc.format.info->sized,
                                 mImage->getLevelCount(), layerCount));
    }

    vk::SamplerDesc samplerDesc(mState.getSamplerState(), mState.isStencilMode(),
                                mImage->getExternalFormat());
    ANGLE_TRY(renderer->getSamplerCache().getSampler(contextVk, samplerDesc, &mSampler));

    // Regenerate the serial on a sampler change.
    mSerial = contextVk->generateTextureSerial();

    return angle::Result::Continue;
}

angle::Result TextureVk::initializeContents(const gl::Context *context,
                                            const gl::ImageIndex &imageIndex)
{
    ContextVk *contextVk      = vk::GetImpl(context);
    const gl::ImageDesc &desc = mState.getImageDesc(imageIndex);
    const vk::Format &format =
        contextVk->getRenderer()->getFormat(desc.format.info->sizedInternalFormat);

    ASSERT(mImage);
    // Note that we cannot ensure the image is initialized because we might be calling subImage
    // on a non-complete cube map.
    return mImage->stageRobustResourceClearWithFormat(contextVk, imageIndex, desc.size, format);
}

void TextureVk::releaseOwnershipOfImage(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    mOwnsImage = false;
    releaseAndDeleteImage(contextVk);
}

const vk::ImageView &TextureVk::getReadImageViewAndRecordUse(ContextVk *contextVk) const
{
    ASSERT(mImage->valid());

    mImageViews.retain(&contextVk->getResourceUseList());

    if (mState.isStencilMode() && mImageViews.hasStencilReadImageView())
    {
        return mImageViews.getStencilReadImageView();
    }

    if (mState.getSRGBOverride() == gl::SrgbOverride::Enabled)
    {
        ASSERT(mImageViews.getNonLinearReadImageView().valid());
        return mImageViews.getNonLinearReadImageView();
    }

    return mImageViews.getReadImageView();
}

const vk::ImageView &TextureVk::getFetchImageViewAndRecordUse(ContextVk *contextVk) const
{
    ASSERT(mImage->valid());

    mImageViews.retain(&contextVk->getResourceUseList());

    // We don't currently support fetch for depth/stencil cube map textures.
    ASSERT(!mImageViews.hasStencilReadImageView() || !mImageViews.hasFetchImageView());

    if (mState.getSRGBOverride() == gl::SrgbOverride::Enabled)
    {
        return (mImageViews.hasFetchImageView() ? mImageViews.getNonLinearFetchImageView()
                                                : mImageViews.getNonLinearReadImageView());
    }

    return (mImageViews.hasFetchImageView() ? mImageViews.getFetchImageView()
                                            : mImageViews.getReadImageView());
}

const vk::ImageView &TextureVk::getCopyImageViewAndRecordUse(ContextVk *contextVk) const
{
    ASSERT(mImage->valid());

    mImageViews.retain(&contextVk->getResourceUseList());

    if (mState.getSRGBOverride() == gl::SrgbOverride::Enabled)
    {
        return mImageViews.getNonLinearCopyImageView();
    }

    return mImageViews.getCopyImageView();
}

angle::Result TextureVk::getLevelLayerImageView(ContextVk *contextVk,
                                                size_t level,
                                                size_t layer,
                                                const vk::ImageView **imageViewOut)
{
    ASSERT(mImage && mImage->valid());

    uint32_t levelGL     = getNativeImageLevel(static_cast<uint32_t>(level));
    uint32_t nativeLayer = getNativeImageLayer(static_cast<uint32_t>(layer));

    uint32_t levelVK = levelGL - mImage->getBaseLevel();
    return mImageViews.getLevelLayerDrawImageView(contextVk, *mImage, levelVK, nativeLayer,
                                                  imageViewOut);
}

angle::Result TextureVk::getStorageImageView(ContextVk *contextVk,
                                             const gl::ImageUnit &binding,
                                             const vk::ImageView **imageViewOut)
{
    angle::FormatID formatID = angle::Format::InternalFormatToID(binding.format);
    const vk::Format &format = contextVk->getRenderer()->getFormat(formatID);

    if (binding.layered != GL_TRUE)
    {
        return getLevelLayerImageView(contextVk, binding.level, binding.layer, imageViewOut);
    }

    uint32_t nativeLevel = getNativeImageLevel(static_cast<uint32_t>(binding.level));
    uint32_t nativeLayer = getNativeImageLayer(0);
    return mImageViews.getLevelDrawImageView(
        contextVk, mState.getType(), *mImage, nativeLevel, nativeLayer,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, format.vkImageFormat,
        imageViewOut);
}

angle::Result TextureVk::initImage(ContextVk *contextVk,
                                   const vk::Format &format,
                                   const bool sized,
                                   const gl::Extents &extents,
                                   const uint32_t levelCount)
{
    RendererVk *renderer = contextVk->getRenderer();

    VkExtent3D vkExtent;
    uint32_t layerCount;
    gl_vk::GetExtentsAndLayerCount(mState.getType(), extents, &vkExtent, &layerCount);
    GLint samples = mState.getBaseLevelDesc().samples ? mState.getBaseLevelDesc().samples : 1;

    ANGLE_TRY(mImage->initExternal(
        contextVk, mState.getType(), vkExtent, format, samples, mImageUsageFlags, mImageCreateFlags,
        rx::vk::ImageLayout::Undefined, nullptr, mState.getEffectiveBaseLevel(),
        mState.getEffectiveMaxLevel(), levelCount, layerCount));

    const VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    ANGLE_TRY(mImage->initMemory(contextVk, renderer->getMemoryProperties(), flags));

    ANGLE_TRY(initImageViews(contextVk, format, sized, levelCount, layerCount));

    mSerial = contextVk->generateTextureSerial();

    return angle::Result::Continue;
}

angle::Result TextureVk::initImageViews(ContextVk *contextVk,
                                        const vk::Format &format,
                                        const bool sized,
                                        uint32_t levelCount,
                                        uint32_t layerCount)
{
    ASSERT(mImage != nullptr && mImage->valid());

    uint32_t baseLevel = getNativeImageLevel(0);
    uint32_t baseLayer = getNativeImageLayer(0);

    gl::SwizzleState formatSwizzle = GetFormatSwizzle(contextVk, format, sized);
    gl::SwizzleState readSwizzle   = ApplySwizzle(formatSwizzle, mState.getSwizzleState());

    ANGLE_TRY(mImageViews.initReadViews(contextVk, mState.getType(), *mImage, format, formatSwizzle,
                                        readSwizzle, baseLevel, levelCount, baseLayer, layerCount));

    if (mRequiresSRGBViews)
    {
        ASSERT((mImageCreateFlags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0);
        ANGLE_TRY(mImageViews.initSRGBReadViews(
            contextVk, mState.getType(), *mImage, format, formatSwizzle, readSwizzle, baseLevel,
            levelCount, baseLayer, layerCount, mImageUsageFlags & ~VK_IMAGE_USAGE_STORAGE_BIT));
    }

    return angle::Result::Continue;
}

void TextureVk::releaseImage(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    if (mImage)
    {
        if (mOwnsImage)
        {
            mImage->releaseImage(renderer);
        }
        else
        {
            mImageObserverBinding.bind(nullptr);
            mImage = nullptr;
        }
    }

    mImageViews.release(renderer);

    for (RenderTargetVector &renderTargetLevels : mRenderTargets)
    {
        // Clear the layers tracked for each level
        renderTargetLevels.clear();
    }
    // Then clear the levels
    mRenderTargets.clear();

    onStateChange(angle::SubjectMessage::SubjectChanged);
    mRedefinedLevels.reset();
}

void TextureVk::releaseStagingBuffer(ContextVk *contextVk)
{
    if (mImage)
    {
        mImage->releaseStagingBuffer(contextVk->getRenderer());
    }
}

uint32_t TextureVk::getMipLevelCount(ImageMipLevels mipLevels) const
{
    switch (mipLevels)
    {
        case ImageMipLevels::EnabledLevels:
            return mState.getEnabledLevelCount();
        case ImageMipLevels::FullMipChain:
            return getMaxLevelCount() - mState.getEffectiveBaseLevel();

        default:
            UNREACHABLE();
            return 0;
    }
}

uint32_t TextureVk::getMaxLevelCount() const
{
    // getMipmapMaxLevel will be 0 here if mipmaps are not used, so the levelCount is always +1.
    return mState.getMipmapMaxLevel() + 1;
}

angle::Result TextureVk::generateMipmapLevelsWithCPU(ContextVk *contextVk,
                                                     const angle::Format &sourceFormat,
                                                     GLuint layer,
                                                     GLuint firstMipLevel,
                                                     GLuint maxMipLevel,
                                                     const size_t sourceWidth,
                                                     const size_t sourceHeight,
                                                     const size_t sourceDepth,
                                                     const size_t sourceRowPitch,
                                                     const size_t sourceDepthPitch,
                                                     uint8_t *sourceData)
{
    size_t previousLevelWidth      = sourceWidth;
    size_t previousLevelHeight     = sourceHeight;
    size_t previousLevelDepth      = sourceDepth;
    uint8_t *previousLevelData     = sourceData;
    size_t previousLevelRowPitch   = sourceRowPitch;
    size_t previousLevelDepthPitch = sourceDepthPitch;

    for (GLuint currentMipLevel = firstMipLevel; currentMipLevel <= maxMipLevel; currentMipLevel++)
    {
        // Compute next level width and height.
        size_t mipWidth  = std::max<size_t>(1, previousLevelWidth >> 1);
        size_t mipHeight = std::max<size_t>(1, previousLevelHeight >> 1);
        size_t mipDepth  = std::max<size_t>(1, previousLevelDepth >> 1);

        // With the width and height of the next mip, we can allocate the next buffer we need.
        uint8_t *destData     = nullptr;
        size_t destRowPitch   = mipWidth * sourceFormat.pixelBytes;
        size_t destDepthPitch = destRowPitch * mipHeight;

        size_t mipAllocationSize = destDepthPitch * mipDepth;
        gl::Extents mipLevelExtents(static_cast<int>(mipWidth), static_cast<int>(mipHeight),
                                    static_cast<int>(mipDepth));

        ANGLE_TRY(mImage->stageSubresourceUpdateAndGetData(
            contextVk, mipAllocationSize,
            gl::ImageIndex::MakeFromType(mState.getType(), currentMipLevel, layer), mipLevelExtents,
            gl::Offset(), &destData));

        // Generate the mipmap into that new buffer
        sourceFormat.mipGenerationFunction(
            previousLevelWidth, previousLevelHeight, previousLevelDepth, previousLevelData,
            previousLevelRowPitch, previousLevelDepthPitch, destData, destRowPitch, destDepthPitch);

        // Swap for the next iteration
        previousLevelWidth      = mipWidth;
        previousLevelHeight     = mipHeight;
        previousLevelDepth      = mipDepth;
        previousLevelData       = destData;
        previousLevelRowPitch   = destRowPitch;
        previousLevelDepthPitch = destDepthPitch;
    }

    return angle::Result::Continue;
}

const gl::InternalFormat &TextureVk::getImplementationSizedFormat(const gl::Context *context) const
{
    GLenum sizedFormat = GL_NONE;

    if (mImage && mImage->valid())
    {
        sizedFormat = mImage->getFormat().actualImageFormat().glInternalFormat;
    }
    else
    {
        ContextVk *contextVk     = vk::GetImpl(context);
        const vk::Format &format = getBaseLevelFormat(contextVk->getRenderer());
        sizedFormat              = format.actualImageFormat().glInternalFormat;
    }

    return gl::GetSizedInternalFormatInfo(sizedFormat);
}

GLenum TextureVk::getColorReadFormat(const gl::Context *context)
{
    const gl::InternalFormat &sizedFormat = getImplementationSizedFormat(context);
    return sizedFormat.format;
}

GLenum TextureVk::getColorReadType(const gl::Context *context)
{
    const gl::InternalFormat &sizedFormat = getImplementationSizedFormat(context);
    return sizedFormat.type;
}

angle::Result TextureVk::getTexImage(const gl::Context *context,
                                     const gl::PixelPackState &packState,
                                     gl::Buffer *packBuffer,
                                     gl::TextureTarget target,
                                     GLint level,
                                     GLenum format,
                                     GLenum type,
                                     void *pixels)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Assumes Texture is consistent.
    // TODO(http://anglebug.com/4058): Handle incomplete textures.
    if (!mImage || !mImage->valid())
    {
        ANGLE_TRY(ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));
    }

    size_t layer =
        gl::IsCubeMapFaceTarget(target) ? gl::CubeMapTextureTargetToFaceIndex(target) : 0;
    return mImage->readPixelsForGetImage(contextVk, packState, packBuffer, level,
                                         static_cast<uint32_t>(layer), format, type, pixels);
}

const vk::Format &TextureVk::getBaseLevelFormat(RendererVk *renderer) const
{
    const gl::ImageDesc &baseLevelDesc = mState.getBaseLevelDesc();
    return renderer->getFormat(baseLevelDesc.format.info->sizedInternalFormat);
}

void TextureVk::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    ASSERT(index == kTextureImageSubjectIndex && message == angle::SubjectMessage::SubjectChanged);

    // Forward the notification to the parent that the staging buffer changed.
    onStateChange(angle::SubjectMessage::SubjectChanged);
}
}  // namespace rx
