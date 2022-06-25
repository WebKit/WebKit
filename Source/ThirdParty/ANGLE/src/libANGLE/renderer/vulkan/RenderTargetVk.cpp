//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderTargetVk:
//   Wrapper around a Vulkan renderable resource, using an ImageView.
//

#include "libANGLE/renderer/vulkan/RenderTargetVk.h"

#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

RenderTargetVk::RenderTargetVk()
{
    reset();
}

RenderTargetVk::~RenderTargetVk() {}

RenderTargetVk::RenderTargetVk(RenderTargetVk &&other)
    : mImage(other.mImage),
      mImageViews(other.mImageViews),
      mResolveImage(other.mResolveImage),
      mResolveImageViews(other.mResolveImageViews),
      mLevelIndexGL(other.mLevelIndexGL),
      mLayerIndex(other.mLayerIndex),
      mLayerCount(other.mLayerCount)
{
    other.reset();
}

void RenderTargetVk::init(vk::ImageHelper *image,
                          vk::ImageViewHelper *imageViews,
                          vk::ImageHelper *resolveImage,
                          vk::ImageViewHelper *resolveImageViews,
                          gl::LevelIndex levelIndexGL,
                          uint32_t layerIndex,
                          uint32_t layerCount,
                          RenderTargetTransience transience)
{
    mImage             = image;
    mImageViews        = imageViews;
    mResolveImage      = resolveImage;
    mResolveImageViews = resolveImageViews;
    mLevelIndexGL      = levelIndexGL;
    mLayerIndex        = layerIndex;
    mLayerCount        = layerCount;

    mTransience = transience;
}

void RenderTargetVk::reset()
{
    mImage             = nullptr;
    mImageViews        = nullptr;
    mResolveImage      = nullptr;
    mResolveImageViews = nullptr;
    mLevelIndexGL      = gl::LevelIndex(0);
    mLayerIndex        = 0;
    mLayerCount        = 0;
}

vk::ImageOrBufferViewSubresourceSerial RenderTargetVk::getSubresourceSerialImpl(
    vk::ImageViewHelper *imageViews) const
{
    ASSERT(imageViews);
    ASSERT(mLayerIndex < std::numeric_limits<uint16_t>::max());
    ASSERT(mLevelIndexGL.get() < std::numeric_limits<uint16_t>::max());

    vk::ImageOrBufferViewSubresourceSerial imageViewSerial = imageViews->getSubresourceSerial(
        mLevelIndexGL, 1, mLayerIndex, vk::GetLayerMode(*mImage, mLayerCount),
        vk::SrgbDecodeMode::SkipDecode, gl::SrgbOverride::Default);
    return imageViewSerial;
}

vk::ImageOrBufferViewSubresourceSerial RenderTargetVk::getDrawSubresourceSerial() const
{
    return getSubresourceSerialImpl(mImageViews);
}

vk::ImageOrBufferViewSubresourceSerial RenderTargetVk::getResolveSubresourceSerial() const
{
    return getSubresourceSerialImpl(mResolveImageViews);
}

void RenderTargetVk::onColorDraw(ContextVk *contextVk,
                                 uint32_t framebufferLayerCount,
                                 vk::PackedAttachmentIndex packedAttachmentIndex)
{
    ASSERT(!mImage->getActualFormat().hasDepthOrStencilBits());
    ASSERT(framebufferLayerCount <= mLayerCount);

    contextVk->onColorDraw(mLevelIndexGL, mLayerIndex, framebufferLayerCount, mImage, mResolveImage,
                           packedAttachmentIndex);

    // Multisampled render to texture framebuffers cannot be layered.
    ASSERT(mResolveImage == nullptr || framebufferLayerCount == 1);
}

void RenderTargetVk::onColorResolve(ContextVk *contextVk, uint32_t framebufferLayerCount)
{
    ASSERT(!mImage->getActualFormat().hasDepthOrStencilBits());
    ASSERT(framebufferLayerCount <= mLayerCount);
    ASSERT(mResolveImage == nullptr);

    contextVk->onImageRenderPassWrite(mLevelIndexGL, mLayerIndex, framebufferLayerCount,
                                      VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ColorAttachment,
                                      mImage);
}

void RenderTargetVk::onDepthStencilDraw(ContextVk *contextVk, uint32_t framebufferLayerCount)
{
    const angle::Format &format = mImage->getActualFormat();
    ASSERT(format.hasDepthOrStencilBits());
    ASSERT(framebufferLayerCount <= mLayerCount);

    contextVk->onDepthStencilDraw(mLevelIndexGL, mLayerIndex, framebufferLayerCount, mImage,
                                  mResolveImage);
}

vk::ImageHelper &RenderTargetVk::getImageForRenderPass()
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

const vk::ImageHelper &RenderTargetVk::getImageForRenderPass() const
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

vk::ImageHelper &RenderTargetVk::getResolveImageForRenderPass()
{
    ASSERT(mResolveImage && mResolveImage->valid());
    return *mResolveImage;
}

const vk::ImageHelper &RenderTargetVk::getResolveImageForRenderPass() const
{
    ASSERT(mResolveImage && mResolveImage->valid());
    return *mResolveImage;
}

angle::Result RenderTargetVk::getImageViewImpl(vk::Context *context,
                                               const vk::ImageHelper &image,
                                               gl::SrgbWriteControlMode mode,
                                               vk::ImageViewHelper *imageViews,
                                               const vk::ImageView **imageViewOut) const
{
    ASSERT(image.valid() && imageViews);
    vk::LevelIndex levelVk = mImage->toVkLevel(mLevelIndexGL);
    if (mLayerCount == 1)
    {
        return imageViews->getLevelLayerDrawImageView(context, image, levelVk, mLayerIndex, mode,
                                                      imageViewOut);
    }

    // Layered render targets view the whole level or a handful of layers in case of multiview.
    return imageViews->getLevelDrawImageView(context, image, levelVk, mLayerIndex, mLayerCount,
                                             mode, imageViewOut);
}

angle::Result RenderTargetVk::getImageView(vk::Context *context,
                                           const vk::ImageView **imageViewOut) const
{
    ASSERT(mImage);
    return getImageViewImpl(context, *mImage, gl::SrgbWriteControlMode::Default, mImageViews,
                            imageViewOut);
}

angle::Result RenderTargetVk::getImageViewWithColorspace(vk::Context *context,
                                                         gl::SrgbWriteControlMode mode,
                                                         const vk::ImageView **imageViewOut) const
{
    ASSERT(mImage);
    return getImageViewImpl(context, *mImage, mode, mImageViews, imageViewOut);
}

angle::Result RenderTargetVk::getResolveImageView(vk::Context *context,
                                                  const vk::ImageView **imageViewOut) const
{
    ASSERT(mResolveImage);
    return getImageViewImpl(context, *mResolveImage, gl::SrgbWriteControlMode::Default,
                            mResolveImageViews, imageViewOut);
}

bool RenderTargetVk::isResolveImageOwnerOfData() const
{
    // If there's a resolve attachment and the image itself is transient, it's the resolve
    // attachment that owns the data, so all non-render-pass accesses to the render target data
    // should go through the resolve attachment.
    return isImageTransient();
}

vk::ImageHelper *RenderTargetVk::getOwnerOfData() const
{
    return isResolveImageOwnerOfData() ? mResolveImage : mImage;
}

angle::Result RenderTargetVk::getCopyImageView(vk::Context *context,
                                               const vk::ImageView **imageViewOut) const
{
    const vk::ImageViewHelper *imageViews =
        isResolveImageOwnerOfData() ? mResolveImageViews : mImageViews;

    // If the source of render target is a texture or renderbuffer, this will always be valid.  This
    // is also where 3D or 2DArray images could be the source of the render target.
    if (imageViews->hasCopyImageView())
    {
        *imageViewOut = &imageViews->getCopyImageView();
        return angle::Result::Continue;
    }

    // Otherwise, this must come from the surface, in which case the image is 2D, so the image view
    // used to draw is just as good for fetching.  If resolve attachment is present, fetching is
    // done from that.
    return isResolveImageOwnerOfData() ? getResolveImageView(context, imageViewOut)
                                       : getImageView(context, imageViewOut);
}

angle::FormatID RenderTargetVk::getImageActualFormatID() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getActualFormatID();
}

angle::FormatID RenderTargetVk::getImageIntendedFormatID() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getIntendedFormatID();
}

const angle::Format &RenderTargetVk::getImageActualFormat() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getActualFormat();
}

const angle::Format &RenderTargetVk::getImageIntendedFormat() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getIntendedFormat();
}

gl::Extents RenderTargetVk::getExtents() const
{
    ASSERT(mImage && mImage->valid());
    vk::LevelIndex levelVk = mImage->toVkLevel(mLevelIndexGL);
    return mImage->getLevelExtents2D(levelVk);
}

gl::Extents RenderTargetVk::getRotatedExtents() const
{
    ASSERT(mImage && mImage->valid());
    vk::LevelIndex levelVk = mImage->toVkLevel(mLevelIndexGL);
    return mImage->getRotatedLevelExtents2D(levelVk);
}

void RenderTargetVk::updateSwapchainImage(vk::ImageHelper *image,
                                          vk::ImageViewHelper *imageViews,
                                          vk::ImageHelper *resolveImage,
                                          vk::ImageViewHelper *resolveImageViews)
{
    ASSERT(image && image->valid() && imageViews);
    mImage             = image;
    mImageViews        = imageViews;
    mResolveImage      = resolveImage;
    mResolveImageViews = resolveImageViews;
}

vk::ImageHelper &RenderTargetVk::getImageForCopy() const
{
    ASSERT(mImage && mImage->valid() && (mResolveImage == nullptr || mResolveImage->valid()));
    return *getOwnerOfData();
}

vk::ImageHelper &RenderTargetVk::getImageForWrite() const
{
    ASSERT(mImage && mImage->valid() && (mResolveImage == nullptr || mResolveImage->valid()));
    return *getOwnerOfData();
}

angle::Result RenderTargetVk::flushStagedUpdates(ContextVk *contextVk,
                                                 vk::ClearValuesArray *deferredClears,
                                                 uint32_t deferredClearIndex,
                                                 uint32_t framebufferLayerCount)
{
    ASSERT(mImage->valid() && (!isResolveImageOwnerOfData() || mResolveImage->valid()));
    ASSERT(framebufferLayerCount != 0);

    // It's impossible to defer clears to slices of a 3D images, as the clear applies to all the
    // slices, while deferred clears only clear a single slice (where the framebuffer is attached).
    // Additionally, the layer index for 3D textures is always zero according to Vulkan.
    uint32_t layerIndex = mLayerIndex;
    if (mImage->getType() == VK_IMAGE_TYPE_3D)
    {
        layerIndex         = 0;
        deferredClears     = nullptr;
        deferredClearIndex = 0;
    }

    vk::ImageHelper *image = getOwnerOfData();

    // All updates should be staged on the image that owns the data as the source of truth.  With
    // multisampled-render-to-texture framebuffers, that is the resolve image.  In that case, even
    // though deferred clears set the loadOp of the transient multisampled image, the clears
    // themselves are staged on the resolve image.  The |flushSingleSubresourceStagedUpdates| call
    // below will either flush all staged updates to the resolve image, or if the only staged update
    // is a clear, it will accumulate it in the |deferredClears| array.  Later, when the render pass
    // is started, the deferred clears are applied to the transient multisampled image.
    ASSERT(!isResolveImageOwnerOfData() ||
           !mImage->hasStagedUpdatesForSubresource(mLevelIndexGL, layerIndex, mLayerCount));
    ASSERT(isResolveImageOwnerOfData() || mResolveImage == nullptr ||
           !mResolveImage->hasStagedUpdatesForSubresource(mLevelIndexGL, layerIndex, mLayerCount));

    if (!image->hasStagedUpdatesForSubresource(mLevelIndexGL, layerIndex, framebufferLayerCount))
    {
        return angle::Result::Continue;
    }

    return image->flushSingleSubresourceStagedUpdates(contextVk, mLevelIndexGL, layerIndex,
                                                      framebufferLayerCount, deferredClears,
                                                      deferredClearIndex);
}

bool RenderTargetVk::hasDefinedContent() const
{
    vk::ImageHelper *image = getOwnerOfData();
    return image->hasSubresourceDefinedContent(mLevelIndexGL, mLayerIndex, mLayerCount);
}

bool RenderTargetVk::hasDefinedStencilContent() const
{
    vk::ImageHelper *image = getOwnerOfData();
    return image->hasSubresourceDefinedStencilContent(mLevelIndexGL, mLayerIndex, mLayerCount);
}

void RenderTargetVk::invalidateEntireContent(ContextVk *contextVk,
                                             bool *preferToKeepContentsDefinedOut)
{
    vk::ImageHelper *image = getOwnerOfData();
    image->invalidateSubresourceContent(contextVk, mLevelIndexGL, mLayerIndex, mLayerCount,
                                        preferToKeepContentsDefinedOut);
}

void RenderTargetVk::invalidateEntireStencilContent(ContextVk *contextVk,
                                                    bool *preferToKeepContentsDefinedOut)
{
    vk::ImageHelper *image = getOwnerOfData();
    image->invalidateSubresourceStencilContent(contextVk, mLevelIndexGL, mLayerIndex, mLayerCount,
                                               preferToKeepContentsDefinedOut);
}

gl::ImageIndex RenderTargetVk::getImageIndexForClear(uint32_t layerCount) const
{
    // Determine the GL type from the Vk Image properties.
    if (mImage->getType() == VK_IMAGE_TYPE_3D || mImage->getLayerCount() > 1)
    {
        // This is used for the sake of staging clears.  The depth slices of the 3D image are
        // threated as layers for this purpose.
        //
        // We also don't need to distinguish 2D array and cube.
        return gl::ImageIndex::Make2DArrayRange(mLevelIndexGL.get(), mLayerIndex, layerCount);
    }

    ASSERT(mLayerIndex == 0);
    ASSERT(mLayerCount == 1);
    ASSERT(layerCount == 1);
    return gl::ImageIndex::Make2D(mLevelIndexGL.get());
}
}  // namespace rx
