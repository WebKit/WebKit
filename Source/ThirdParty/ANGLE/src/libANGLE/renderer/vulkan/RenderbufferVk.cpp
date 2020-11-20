//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferVk.cpp:
//    Implements the class methods for RenderbufferVk.
//

#include "libANGLE/renderer/vulkan/RenderbufferVk.h"

#include "libANGLE/Context.h"
#include "libANGLE/Image.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ImageVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{
namespace
{
angle::SubjectIndex kRenderbufferImageSubjectIndex = 0;
}  // namespace

RenderbufferVk::RenderbufferVk(const gl::RenderbufferState &state)
    : RenderbufferImpl(state),
      mOwnsImage(false),
      mImage(nullptr),
      mImageObserverBinding(this, kRenderbufferImageSubjectIndex)
{}

RenderbufferVk::~RenderbufferVk() {}

void RenderbufferVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    releaseAndDeleteImage(contextVk);
}

angle::Result RenderbufferVk::setStorageImpl(const gl::Context *context,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height,
                                             gl::MultisamplingMode mode)
{
    ContextVk *contextVk       = vk::GetImpl(context);
    RendererVk *renderer       = contextVk->getRenderer();
    const vk::Format &vkFormat = renderer->getFormat(internalformat);

    if (!mOwnsImage)
    {
        releaseAndDeleteImage(contextVk);
    }

    if (mImage != nullptr && mImage->valid())
    {
        // Check against the state if we need to recreate the storage.
        if (internalformat != mState.getFormat().info->internalFormat ||
            width != mState.getWidth() || height != mState.getHeight() ||
            samples != mState.getSamples() || mode != mState.getMultisamplingMode())
        {
            releaseImage(contextVk);
        }
    }

    if ((mImage != nullptr && mImage->valid()) || width == 0 || height == 0)
    {
        return angle::Result::Continue;
    }

    if (mImage == nullptr)
    {
        mImage     = new vk::ImageHelper();
        mOwnsImage = true;
        mImageObserverBinding.bind(mImage);
        mImageViews.init(renderer);
    }

    // With the introduction of sRGB related GLES extensions any texture could be respecified
    // causing it to be interpreted in a different colorspace. Create the VkImage accordingly.
    VkImageCreateFlags imageCreateFlags                  = vk::kVkImageCreateFlagsNone;
    VkImageFormatListCreateInfoKHR *additionalCreateInfo = nullptr;
    VkFormat vkImageFormat                               = vkFormat.vkImageFormat;
    VkFormat vkImageListFormat                           = vkFormat.actualImageFormat().isSRGB
                                     ? vk::ConvertToLinear(vkImageFormat)
                                     : vk::ConvertToSRGB(vkImageFormat);

    VkImageFormatListCreateInfoKHR formatListInfo = {};
    if (renderer->getFeatures().supportsImageFormatList.enabled)
    {
        // Add VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT to VkImage create flag
        imageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        // There is just 1 additional format we might use to create a VkImageView for this VkImage
        formatListInfo.sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
        formatListInfo.pNext           = nullptr;
        formatListInfo.viewFormatCount = 1;
        formatListInfo.pViewFormats    = &vkImageListFormat;
        additionalCreateInfo           = &formatListInfo;
    }

    const angle::Format &textureFormat = vkFormat.actualImageFormat();
    const bool isDepthStencilFormat    = textureFormat.hasDepthOrStencilBits();
    ASSERT(textureFormat.redBits > 0 || isDepthStencilFormat);

    // TODO(syoussefi): Currently not supported for depth/stencil images if
    // VK_KHR_depth_stencil_resolve is not supported.  Chromium only uses this for depth/stencil
    // buffers and doesn't attempt to read from it.  http://anglebug.com/5065
    const bool isRenderToTexture =
        mode == gl::MultisamplingMode::MultisampledRenderToTexture &&
        (!isDepthStencilFormat || renderer->getFeatures().supportsDepthStencilResolve.enabled);

    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        (isDepthStencilFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                              : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
        (isRenderToTexture ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0);

    const uint32_t imageSamples = isRenderToTexture ? 1 : samples;

    bool robustInit = contextVk->isRobustResourceInitEnabled();

    VkExtent3D extents = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u};
    ANGLE_TRY(mImage->initExternal(contextVk, gl::TextureType::_2D, extents, vkFormat, imageSamples,
                                   usage, imageCreateFlags, vk::ImageLayout::Undefined,
                                   additionalCreateInfo, gl::LevelIndex(0), gl::LevelIndex(0), 1, 1,
                                   robustInit));

    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ANGLE_TRY(mImage->initMemory(contextVk, renderer->getMemoryProperties(), flags));

    // If multisampled render to texture, an implicit multisampled image is created which is used as
    // the color or depth/stencil attachment.  At the end of the render pass, this image is
    // automatically resolved into |mImage| and its contents are discarded.
    if (isRenderToTexture)
    {
        mMultisampledImageViews.init(renderer);

        ANGLE_TRY(mMultisampledImage.initImplicitMultisampledRenderToTexture(
            contextVk, renderer->getMemoryProperties(), gl::TextureType::_2D, samples, *mImage,
            robustInit));

        mRenderTarget.init(&mMultisampledImage, &mMultisampledImageViews, mImage, &mImageViews,
                           gl::LevelIndex(0), 0, RenderTargetTransience::MultisampledTransient);
    }
    else
    {
        mRenderTarget.init(mImage, &mImageViews, nullptr, nullptr, gl::LevelIndex(0), 0,
                           RenderTargetTransience::Default);
    }

    return angle::Result::Continue;
}

angle::Result RenderbufferVk::setStorage(const gl::Context *context,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height)
{
    return setStorageImpl(context, 1, internalformat, width, height,
                          gl::MultisamplingMode::Regular);
}

angle::Result RenderbufferVk::setStorageMultisample(const gl::Context *context,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    gl::MultisamplingMode mode)
{
    return setStorageImpl(context, samples, internalformat, width, height, mode);
}

angle::Result RenderbufferVk::setStorageEGLImageTarget(const gl::Context *context,
                                                       egl::Image *image)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    releaseAndDeleteImage(contextVk);

    ImageVk *imageVk = vk::GetImpl(image);
    mImage           = imageVk->getImage();
    mOwnsImage       = false;
    mImageObserverBinding.bind(mImage);
    mImageViews.init(renderer);

    const vk::Format &vkFormat = renderer->getFormat(image->getFormat().info->sizedInternalFormat);
    const angle::Format &textureFormat = vkFormat.actualImageFormat();

    VkImageAspectFlags aspect = vk::GetFormatAspectFlags(textureFormat);

    // Transfer the image to this queue if needed
    uint32_t rendererQueueFamilyIndex = contextVk->getRenderer()->getQueueFamilyIndex();
    if (mImage->isQueueChangeNeccesary(rendererQueueFamilyIndex))
    {
        vk::CommandBuffer &commandBuffer = contextVk->getOutsideRenderPassCommandBuffer();
        mImage->changeLayoutAndQueue(aspect, vk::ImageLayout::ColorAttachment,
                                     rendererQueueFamilyIndex, &commandBuffer);
    }

    gl::TextureType viewType = imageVk->getImageTextureType();

    if (imageVk->getImageTextureType() == gl::TextureType::CubeMap)
    {
        viewType = vk::Get2DTextureType(imageVk->getImage()->getLayerCount(),
                                        imageVk->getImage()->getSamples());
    }

    mRenderTarget.init(mImage, &mImageViews, nullptr, nullptr, imageVk->getImageLevel(),
                       imageVk->getImageLayer(), RenderTargetTransience::Default);

    return angle::Result::Continue;
}

angle::Result RenderbufferVk::copyRenderbufferSubData(const gl::Context *context,
                                                      const gl::Renderbuffer *srcBuffer,
                                                      GLint srcLevel,
                                                      GLint srcX,
                                                      GLint srcY,
                                                      GLint srcZ,
                                                      GLint dstLevel,
                                                      GLint dstX,
                                                      GLint dstY,
                                                      GLint dstZ,
                                                      GLsizei srcWidth,
                                                      GLsizei srcHeight,
                                                      GLsizei srcDepth)
{
    RenderbufferVk *sourceVk = vk::GetImpl(srcBuffer);

    // Make sure the source/destination targets are initialized and all staged updates are flushed.
    ANGLE_TRY(sourceVk->ensureImageInitialized(context));
    ANGLE_TRY(ensureImageInitialized(context));

    return vk::ImageHelper::CopyImageSubData(context, sourceVk->getImage(), srcLevel, srcX, srcY,
                                             srcZ, mImage, dstLevel, dstX, dstY, dstZ, srcWidth,
                                             srcHeight, srcDepth);
}

angle::Result RenderbufferVk::copyTextureSubData(const gl::Context *context,
                                                 const gl::Texture *srcTexture,
                                                 GLint srcLevel,
                                                 GLint srcX,
                                                 GLint srcY,
                                                 GLint srcZ,
                                                 GLint dstLevel,
                                                 GLint dstX,
                                                 GLint dstY,
                                                 GLint dstZ,
                                                 GLsizei srcWidth,
                                                 GLsizei srcHeight,
                                                 GLsizei srcDepth)
{
    ContextVk *contextVk = vk::GetImpl(context);
    TextureVk *sourceVk  = vk::GetImpl(srcTexture);

    // Make sure the source/destination targets are initialized and all staged updates are flushed.
    ANGLE_TRY(sourceVk->ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));
    ANGLE_TRY(ensureImageInitialized(context));

    return vk::ImageHelper::CopyImageSubData(context, &sourceVk->getImage(), srcLevel, srcX, srcY,
                                             srcZ, mImage, dstLevel, dstX, dstY, dstZ, srcWidth,
                                             srcHeight, srcDepth);
}

angle::Result RenderbufferVk::getAttachmentRenderTarget(const gl::Context *context,
                                                        GLenum binding,
                                                        const gl::ImageIndex &imageIndex,
                                                        GLsizei samples,
                                                        FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(mImage && mImage->valid());
    *rtOut = &mRenderTarget;
    return angle::Result::Continue;
}

angle::Result RenderbufferVk::initializeContents(const gl::Context *context,
                                                 const gl::ImageIndex &imageIndex)
{
    // Note: stageSubresourceRobustClear only uses the intended format to count channels.
    mImage->stageRobustResourceClear(imageIndex);
    return mImage->flushAllStagedUpdates(vk::GetImpl(context));
}

void RenderbufferVk::releaseOwnershipOfImage(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    mOwnsImage = false;
    releaseAndDeleteImage(contextVk);
}

void RenderbufferVk::releaseAndDeleteImage(ContextVk *contextVk)
{
    releaseImage(contextVk);
    SafeDelete(mImage);
    mImageObserverBinding.bind(nullptr);
}

void RenderbufferVk::releaseImage(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();

    if (mImage && mOwnsImage)
    {
        mImage->releaseImageFromShareContexts(renderer, contextVk);
        mImage->releaseStagingBuffer(renderer);
    }
    else
    {
        mImage = nullptr;
        mImageObserverBinding.bind(nullptr);
    }

    mImageViews.release(renderer);

    if (mMultisampledImage.valid())
    {
        mMultisampledImage.releaseImageFromShareContexts(renderer, contextVk);
    }
    mMultisampledImageViews.release(renderer);
}

const gl::InternalFormat &RenderbufferVk::getImplementationSizedFormat() const
{
    GLenum internalFormat = mImage->getFormat().actualImageFormat().glInternalFormat;
    return gl::GetSizedInternalFormatInfo(internalFormat);
}

GLenum RenderbufferVk::getColorReadFormat(const gl::Context *context)
{
    const gl::InternalFormat &sizedFormat = getImplementationSizedFormat();
    return sizedFormat.format;
}

GLenum RenderbufferVk::getColorReadType(const gl::Context *context)
{
    const gl::InternalFormat &sizedFormat = getImplementationSizedFormat();
    return sizedFormat.type;
}

angle::Result RenderbufferVk::getRenderbufferImage(const gl::Context *context,
                                                   const gl::PixelPackState &packState,
                                                   gl::Buffer *packBuffer,
                                                   GLenum format,
                                                   GLenum type,
                                                   void *pixels)
{
    // Storage not defined.
    if (!mImage || !mImage->valid())
    {
        return angle::Result::Continue;
    }

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(mImage->flushAllStagedUpdates(contextVk));

    gl::MaybeOverrideLuminance(format, type, getColorReadFormat(context),
                               getColorReadType(context));

    return mImage->readPixelsForGetImage(contextVk, packState, packBuffer, gl::LevelIndex(0), 0,
                                         format, type, pixels);
}

angle::Result RenderbufferVk::ensureImageInitialized(const gl::Context *context)
{
    ANGLE_TRY(setStorage(context, mState.getFormat().info->internalFormat, mState.getWidth(),
                         mState.getHeight()));

    return mImage->flushAllStagedUpdates(vk::GetImpl(context));
}

void RenderbufferVk::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    ASSERT(index == kRenderbufferImageSubjectIndex &&
           message == angle::SubjectMessage::SubjectChanged);

    // Forward the notification to the parent class that the staging buffer changed.
    onStateChange(angle::SubjectMessage::SubjectChanged);
}
}  // namespace rx
