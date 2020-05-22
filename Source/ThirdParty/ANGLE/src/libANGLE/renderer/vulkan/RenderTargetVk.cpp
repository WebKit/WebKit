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
      mLevelIndex(other.mLevelIndex),
      mLayerIndex(other.mLayerIndex),
      mContentDefined(other.mContentDefined)
{
    other.reset();
}

void RenderTargetVk::init(vk::ImageHelper *image,
                          vk::ImageViewHelper *imageViews,
                          uint32_t levelIndex,
                          uint32_t layerIndex)
{
    mImage      = image;
    mImageViews = imageViews;
    mLevelIndex = levelIndex;
    mLayerIndex = layerIndex;
    // We are being conservative here since our targeted optimization is to skip surfaceVK's depth
    // buffer load after swap call.
    mContentDefined = true;
}

void RenderTargetVk::reset()
{
    mImage          = nullptr;
    mImageViews     = nullptr;
    mLevelIndex     = 0;
    mLayerIndex     = 0;
    mContentDefined = false;
}

vk::AttachmentSerial RenderTargetVk::getAssignSerial(ContextVk *contextVk)
{
    ASSERT(mImage && mImage->valid());
    vk::AttachmentSerial attachmentSerial;
    ASSERT(mLayerIndex < std::numeric_limits<uint16_t>::max());
    ASSERT(mLevelIndex < std::numeric_limits<uint16_t>::max());
    Serial imageSerial = mImage->getAssignSerial(contextVk);
    ASSERT(imageSerial.getValue() < std::numeric_limits<uint32_t>::max());
    SetBitField(attachmentSerial.layer, mLayerIndex);
    SetBitField(attachmentSerial.level, mLevelIndex);
    SetBitField(attachmentSerial.imageSerial, imageSerial.getValue());
    return attachmentSerial;
}

angle::Result RenderTargetVk::onColorDraw(ContextVk *contextVk)
{
    ASSERT(!mImage->getFormat().actualImageFormat().hasDepthOrStencilBits());

    contextVk->onRenderPassImageWrite(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::ColorAttachment,
                                      mImage);
    mContentDefined = true;
    retainImageViews(contextVk);

    return angle::Result::Continue;
}

angle::Result RenderTargetVk::onDepthStencilDraw(ContextVk *contextVk)
{
    ASSERT(mImage->getFormat().actualImageFormat().hasDepthOrStencilBits());

    const angle::Format &format    = mImage->getFormat().actualImageFormat();
    VkImageAspectFlags aspectFlags = vk::GetDepthStencilAspectFlags(format);

    contextVk->onRenderPassImageWrite(aspectFlags, vk::ImageLayout::DepthStencilAttachment, mImage);
    mContentDefined = true;
    retainImageViews(contextVk);

    return angle::Result::Continue;
}

vk::ImageHelper &RenderTargetVk::getImage()
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

const vk::ImageHelper &RenderTargetVk::getImage() const
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

angle::Result RenderTargetVk::getImageView(ContextVk *contextVk,
                                           const vk::ImageView **imageViewOut) const
{
    ASSERT(mImage && mImage->valid() && mImageViews);
    return mImageViews->getLevelLayerDrawImageView(contextVk, *mImage, mLevelIndex, mLayerIndex,
                                                   imageViewOut);
}

const vk::Format &RenderTargetVk::getImageFormat() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getFormat();
}

gl::Extents RenderTargetVk::getExtents() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getLevelExtents2D(static_cast<uint32_t>(mLevelIndex));
}

void RenderTargetVk::updateSwapchainImage(vk::ImageHelper *image, vk::ImageViewHelper *imageViews)
{
    ASSERT(image && image->valid() && imageViews);
    mImage      = image;
    mImageViews = imageViews;
}

vk::ImageHelper *RenderTargetVk::getImageForWrite(ContextVk *contextVk) const
{
    ASSERT(mImage && mImage->valid());
    retainImageViews(contextVk);
    return mImage;
}

angle::Result RenderTargetVk::flushStagedUpdates(ContextVk *contextVk,
                                                 vk::ClearValuesArray *deferredClears,
                                                 uint32_t deferredClearIndex) const
{
    // Note that the layer index for 3D textures is always zero according to Vulkan.
    uint32_t layerIndex = mLayerIndex;
    if (mImage->getType() == VK_IMAGE_TYPE_3D)
    {
        layerIndex = 0;
    }

    ASSERT(mImage->valid());
    if (!mImage->isUpdateStaged(mLevelIndex, layerIndex))
        return angle::Result::Continue;

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));
    return mImage->flushSingleSubresourceStagedUpdates(
        contextVk, mLevelIndex, layerIndex, commandBuffer, deferredClears, deferredClearIndex);
}

void RenderTargetVk::retainImageViews(ContextVk *contextVk) const
{
    mImageViews->retain(&contextVk->getResourceUseList());
}

gl::ImageIndex RenderTargetVk::getImageIndex() const
{
    // Determine the GL type from the Vk Image properties.
    if (mImage->getType() == VK_IMAGE_TYPE_3D)
    {
        return gl::ImageIndex::Make3D(mLevelIndex, mLayerIndex);
    }

    // We don't need to distinguish 2D array and cube.
    if (mImage->getLayerCount() > 1)
    {
        return gl::ImageIndex::Make2DArray(mLevelIndex, mLayerIndex);
    }

    ASSERT(mLayerIndex == 0);
    return gl::ImageIndex::Make2D(mLevelIndex);
}
}  // namespace rx
