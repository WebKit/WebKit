//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OverlayVk.cpp:
//    Implements the OverlayVk class.
//

#include "libANGLE/renderer/vulkan/OverlayVk.h"

#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Overlay_font_autogen.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"

#include <numeric>

namespace rx
{
OverlayVk::OverlayVk(const gl::OverlayState &state)
    : OverlayImpl(state),
      mSupportsSubgroupBallot(false),
      mSupportsSubgroupArithmetic(false),
      mRefreshCulledWidgets(false)
{}
OverlayVk::~OverlayVk() = default;

angle::Result OverlayVk::init(const gl::Context *context)
{
    ContextVk *contextVk   = vk::GetImpl(context);
    RendererVk *rendererVk = contextVk->getRenderer();

    const VkPhysicalDeviceSubgroupProperties &subgroupProperties =
        rendererVk->getPhysicalDeviceSubgroupProperties();
    uint32_t subgroupSize = subgroupProperties.subgroupSize;

    // Currently, only subgroup sizes 32 and 64 are supported.
    if (subgroupSize != 32 && subgroupSize != 64)
    {
        return angle::Result::Continue;
    }

    mSubgroupSize[0] = 8;
    mSubgroupSize[1] = subgroupSize / 8;

    constexpr VkSubgroupFeatureFlags kSubgroupBallotOperations =
        VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT;
    constexpr VkSubgroupFeatureFlags kSubgroupArithmeticOperations =
        VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;

    // If not all operations used in the shaders are supported, disable the overlay.
    if ((subgroupProperties.supportedOperations & kSubgroupBallotOperations) ==
        kSubgroupBallotOperations)
    {
        mSupportsSubgroupBallot = true;
    }
    else if ((subgroupProperties.supportedOperations & kSubgroupArithmeticOperations) ==
             kSubgroupArithmeticOperations)
    {
        mSupportsSubgroupArithmetic = true;
    }

    ANGLE_TRY(createFont(contextVk));

    mRefreshCulledWidgets = true;

    return contextVk->flushImpl(nullptr);
}

void OverlayVk::onDestroy(const gl::Context *context)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();
    VkDevice device      = renderer->getDevice();

    mCulledWidgets.destroy(renderer);
    mCulledWidgetsView.destroy(device);

    mFontImage.destroy(renderer);
    mFontImageView.destroy(device);
}

angle::Result OverlayVk::createFont(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    // Create a buffer to stage font data upload.
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size =
        gl::overlay::kFontCount * gl::overlay::kFontImageWidth * gl::overlay::kFontImageHeight;
    bufferCreateInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk::RendererScoped<vk::BufferHelper> fontDataBuffer(renderer);

    ANGLE_TRY(fontDataBuffer.get().init(contextVk, bufferCreateInfo,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    uint8_t *fontData;
    ANGLE_TRY(fontDataBuffer.get().map(contextVk, &fontData));

    mState.initFontData(fontData);

    ANGLE_TRY(fontDataBuffer.get().flush(renderer, 0, fontDataBuffer.get().getSize()));
    fontDataBuffer.get().unmap(renderer);

    fontDataBuffer.get().onExternalWrite(VK_ACCESS_HOST_WRITE_BIT);

    // Create the font image.
    ANGLE_TRY(
        mFontImage.init(contextVk, gl::TextureType::_2D,
                        VkExtent3D{gl::overlay::kFontImageWidth, gl::overlay::kFontImageHeight, 1},
                        renderer->getFormat(angle::FormatID::R8_UNORM), 1,
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, 0, 1,
                        gl::overlay::kFontCount));
    ANGLE_TRY(mFontImage.initMemory(contextVk, renderer->getMemoryProperties(),
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    ANGLE_TRY(mFontImage.initImageView(contextVk, gl::TextureType::_2DArray,
                                       VK_IMAGE_ASPECT_COLOR_BIT, gl::SwizzleState(),
                                       &mFontImageView, 0, 1));

    // Copy font data from staging buffer.
    vk::CommandBuffer *fontDataUpload;
    ANGLE_TRY(contextVk->onBufferTransferRead(&fontDataBuffer.get()));
    ANGLE_TRY(contextVk->onImageWrite(VK_IMAGE_ASPECT_COLOR_BIT, vk::ImageLayout::TransferDst,
                                      &mFontImage));
    ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&fontDataUpload));

    VkBufferImageCopy copy           = {};
    copy.bufferRowLength             = gl::overlay::kFontImageWidth;
    copy.bufferImageHeight           = gl::overlay::kFontImageHeight;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = gl::overlay::kFontCount;
    copy.imageExtent.width           = gl::overlay::kFontImageWidth;
    copy.imageExtent.height          = gl::overlay::kFontImageHeight;
    copy.imageExtent.depth           = 1;

    fontDataUpload->copyBufferToImage(fontDataBuffer.get().getBuffer().getHandle(),
                                      mFontImage.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      1, &copy);

    return angle::Result::Continue;
}

angle::Result OverlayVk::cullWidgets(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    // Release old culledWidgets image
    mCulledWidgets.releaseImage(renderer);
    contextVk->addGarbage(&mCulledWidgetsView);

    // Create a buffer to contain coordinates of enabled text and graph widgets.  This buffer will
    // be used to perform tiled culling and can be discarded immediately after.
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size               = mState.getWidgetCoordinatesBufferSize();
    bufferCreateInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferCreateInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    vk::RendererScoped<vk::BufferHelper> enabledWidgetsBuffer(renderer);

    ANGLE_TRY(enabledWidgetsBuffer.get().init(contextVk, bufferCreateInfo,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Fill the buffer with coordinate information from enabled widgets.
    uint8_t *enabledWidgets;
    ANGLE_TRY(enabledWidgetsBuffer.get().map(contextVk, &enabledWidgets));

    gl::Extents presentImageExtents(mPresentImageExtent.width, mPresentImageExtent.height, 1);
    mState.fillEnabledWidgetCoordinates(presentImageExtents, enabledWidgets);

    ANGLE_TRY(enabledWidgetsBuffer.get().flush(renderer, 0, enabledWidgetsBuffer.get().getSize()));
    enabledWidgetsBuffer.get().unmap(renderer);

    enabledWidgetsBuffer.get().onExternalWrite(VK_ACCESS_HOST_WRITE_BIT);

    // Allocate mCulledWidget and its view.
    VkExtent3D culledWidgetsExtent = {
        UnsignedCeilDivide(mPresentImageExtent.width, mSubgroupSize[0]),
        UnsignedCeilDivide(mPresentImageExtent.height, mSubgroupSize[1]), 1};

    ANGLE_TRY(mCulledWidgets.init(contextVk, gl::TextureType::_2D, culledWidgetsExtent,
                                  renderer->getFormat(angle::FormatID::R32G32_UINT), 1,
                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, 0, 1,
                                  1));
    ANGLE_TRY(mCulledWidgets.initMemory(contextVk, renderer->getMemoryProperties(),
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    ANGLE_TRY(mCulledWidgets.initImageView(contextVk, gl::TextureType::_2D,
                                           VK_IMAGE_ASPECT_COLOR_BIT, gl::SwizzleState(),
                                           &mCulledWidgetsView, 0, 1));

    UtilsVk::OverlayCullParameters params;
    params.subgroupSize[0]            = mSubgroupSize[0];
    params.subgroupSize[1]            = mSubgroupSize[1];
    params.supportsSubgroupBallot     = mSupportsSubgroupBallot;
    params.supportsSubgroupArithmetic = mSupportsSubgroupArithmetic;

    return contextVk->getUtils().cullOverlayWidgets(contextVk, &enabledWidgetsBuffer.get(),
                                                    &mCulledWidgets, &mCulledWidgetsView, params);
}

angle::Result OverlayVk::onPresent(ContextVk *contextVk,
                                   vk::ImageHelper *imageToPresent,
                                   const vk::ImageView *imageToPresentView)
{
    if (mState.getEnabledWidgetCount() == 0)
    {
        return angle::Result::Continue;
    }

    RendererVk *renderer = contextVk->getRenderer();

    // If the swapchain image doesn't support storage image, we can't output to it.
    VkFormatFeatureFlags featureBits = renderer->getImageFormatFeatureBits(
        imageToPresent->getFormat().vkImageFormat, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
    if ((featureBits & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
    {
        return angle::Result::Continue;
    }

    const VkExtent3D &imageExtent = imageToPresent->getExtents();

    mRefreshCulledWidgets = mRefreshCulledWidgets ||
                            mPresentImageExtent.width != imageExtent.width ||
                            mPresentImageExtent.height != imageExtent.height;

    if (mRefreshCulledWidgets)
    {
        mPresentImageExtent.width  = imageExtent.width;
        mPresentImageExtent.height = imageExtent.height;

        ANGLE_TRY(cullWidgets(contextVk));

        mRefreshCulledWidgets = false;
    }

    vk::RendererScoped<vk::BufferHelper> textDataBuffer(renderer);
    vk::RendererScoped<vk::BufferHelper> graphDataBuffer(renderer);

    VkBufferCreateInfo textBufferCreateInfo = {};
    textBufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    textBufferCreateInfo.size               = mState.getTextWidgetsBufferSize();
    textBufferCreateInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    textBufferCreateInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    VkBufferCreateInfo graphBufferCreateInfo = textBufferCreateInfo;
    graphBufferCreateInfo.size               = mState.getGraphWidgetsBufferSize();

    ANGLE_TRY(textDataBuffer.get().init(contextVk, textBufferCreateInfo,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    ANGLE_TRY(graphDataBuffer.get().init(contextVk, graphBufferCreateInfo,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    uint8_t *textData;
    uint8_t *graphData;
    ANGLE_TRY(textDataBuffer.get().map(contextVk, &textData));
    ANGLE_TRY(graphDataBuffer.get().map(contextVk, &graphData));

    gl::Extents presentImageExtents(mPresentImageExtent.width, mPresentImageExtent.height, 1);
    mState.fillWidgetData(presentImageExtents, textData, graphData);

    ANGLE_TRY(textDataBuffer.get().flush(renderer, 0, textDataBuffer.get().getSize()));
    ANGLE_TRY(graphDataBuffer.get().flush(renderer, 0, graphDataBuffer.get().getSize()));
    textDataBuffer.get().unmap(renderer);
    graphDataBuffer.get().unmap(renderer);

    UtilsVk::OverlayDrawParameters params;
    params.subgroupSize[0] = mSubgroupSize[0];
    params.subgroupSize[1] = mSubgroupSize[1];

    return contextVk->getUtils().drawOverlay(
        contextVk, &textDataBuffer.get(), &graphDataBuffer.get(), &mFontImage, &mFontImageView,
        &mCulledWidgets, &mCulledWidgetsView, imageToPresent, imageToPresentView, params);
}

}  // namespace rx
