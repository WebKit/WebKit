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
      mLevelIndexGL(other.mLevelIndexGL),
      mLayerIndex(other.mLayerIndex),
      mContentDefined(other.mContentDefined)
{
    other.reset();
}

void RenderTargetVk::init(vk::ImageHelper *image,
                          vk::ImageViewHelper *imageViews,
                          uint32_t levelIndexGL,
                          uint32_t layerIndex)
{
    mImage        = image;
    mImageViews   = imageViews;
    mLevelIndexGL = levelIndexGL;
    mLayerIndex   = layerIndex;

    // Conservatively assume the content is defined.
    mContentDefined = true;
}

void RenderTargetVk::reset()
{
    mImage          = nullptr;
    mImageViews     = nullptr;
    mLevelIndexGL   = 0;
    mLayerIndex     = 0;
    mContentDefined = false;
}

ImageViewSerial RenderTargetVk::getAssignImageViewSerial(ContextVk *contextVk) const
{
    ASSERT(mImageViews);
    ASSERT(mLayerIndex < std::numeric_limits<uint16_t>::max());
    ASSERT(mLevelIndexGL < std::numeric_limits<uint16_t>::max());

    ImageViewSerial imageViewSerial =
        mImageViews->getAssignSerial(contextVk, mLevelIndexGL, mLayerIndex);
    ASSERT(imageViewSerial.getValue() < std::numeric_limits<uint32_t>::max());
    return imageViewSerial;
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

angle::Result RenderTargetVk::getImageView(ContextVk *contextVk,
                                           const vk::ImageView **imageViewOut) const
{
    ASSERT(mImage && mImage->valid() && mImageViews);
    int32_t levelVK = mLevelIndexGL - mImage->getBaseLevel();
    return mImageViews->getLevelLayerDrawImageView(contextVk, *mImage, levelVK, mLayerIndex,
                                                   imageViewOut);
}

angle::Result RenderTargetVk::getAndRetainCopyImageView(ContextVk *contextVk,
                                                        const vk::ImageView **imageViewOut) const
{
    retainImageViews(contextVk);

    const vk::ImageViewHelper *imageViews = mImageViews;
    const vk::ImageView &copyView         = imageViews->getCopyImageView();

    // If the source of render target is a texture or renderbuffer, this will always be valid.  This
    // is also where 3D or 2DArray images could be the source of the render target.
    if (copyView.valid())
    {
        *imageViewOut = &copyView;
        return angle::Result::Continue;
    }

    // Otherwise, this must come from the surface, in which case the image is 2D, so the image view
    // used to draw is just as good for fetching.
    return getImageView(contextVk, imageViewOut);
}

const vk::Format &RenderTargetVk::getImageFormat() const
{
    ASSERT(mImage && mImage->valid());
    return mImage->getFormat();
}

gl::Extents RenderTargetVk::getExtents() const
{
    ASSERT(mImage && mImage->valid());
    uint32_t levelVK = mLevelIndexGL - mImage->getBaseLevel();
    return mImage->getLevelExtents2D(levelVK);
}

void RenderTargetVk::updateSwapchainImage(vk::ImageHelper *image, vk::ImageViewHelper *imageViews)
{
    ASSERT(image && image->valid() && imageViews);
    mImage      = image;
    mImageViews = imageViews;
}

vk::ImageHelper &RenderTargetVk::getImageForCopy() const
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

vk::ImageHelper &RenderTargetVk::getImageForWrite() const
{
    ASSERT(mImage && mImage->valid());
    return *mImage;
}

angle::Result RenderTargetVk::flushStagedUpdates(ContextVk *contextVk,
                                                 vk::ClearValuesArray *deferredClears,
                                                 uint32_t deferredClearIndex)
{
    // This function is called when the framebuffer is notified of an update to the attachment's
    // contents.  Therefore, set mContentDefined so that the next render pass will have loadOp=LOAD.
    mContentDefined = true;

    // Note that the layer index for 3D textures is always zero according to Vulkan.
    uint32_t layerIndex = mLayerIndex;
    if (mImage->getType() == VK_IMAGE_TYPE_3D)
    {
        layerIndex = 0;
    }

    ASSERT(mImage->valid());
    if (!mImage->isUpdateStaged(mLevelIndexGL, layerIndex))
        return angle::Result::Continue;

    vk::CommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));
    return mImage->flushSingleSubresourceStagedUpdates(
        contextVk, mLevelIndexGL, layerIndex, commandBuffer, deferredClears, deferredClearIndex);
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
        return gl::ImageIndex::Make3D(mLevelIndexGL, mLayerIndex);
    }

    // We don't need to distinguish 2D array and cube.
    if (mImage->getLayerCount() > 1)
    {
        return gl::ImageIndex::Make2DArray(mLevelIndexGL, mLayerIndex);
    }

    ASSERT(mLayerIndex == 0);
    return gl::ImageIndex::Make2D(mLevelIndexGL);
}
}  // namespace rx
