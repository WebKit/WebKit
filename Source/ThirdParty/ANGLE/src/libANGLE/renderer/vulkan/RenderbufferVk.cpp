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
                                             size_t samples,
                                             GLenum internalformat,
                                             size_t width,
                                             size_t height)
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
            static_cast<GLsizei>(width) != mState.getWidth() ||
            static_cast<GLsizei>(height) != mState.getHeight())
        {
            releaseImage(contextVk);
        }
    }

    if ((mImage == nullptr || !mImage->valid()) && (width != 0 && height != 0))
    {
        if (mImage == nullptr)
        {
            mImage     = new vk::ImageHelper();
            mOwnsImage = true;
            mImageObserverBinding.bind(mImage);
        }

        const angle::Format &textureFormat = vkFormat.actualImageFormat();
        bool isDepthOrStencilFormat = textureFormat.depthBits > 0 || textureFormat.stencilBits > 0;
        const VkImageUsageFlags usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            (textureFormat.redBits > 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
            (isDepthOrStencilFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);

        VkExtent3D extents = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u};
        ANGLE_TRY(mImage->init(contextVk, gl::TextureType::_2D, extents, vkFormat,
                               static_cast<uint32_t>(samples), usage, 0, 0, 1, 1));

        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        ANGLE_TRY(mImage->initMemory(contextVk, renderer->getMemoryProperties(), flags));

        mRenderTarget.init(mImage, &mImageViews, 0, 0);
    }

    return angle::Result::Continue;
}

angle::Result RenderbufferVk::setStorage(const gl::Context *context,
                                         GLenum internalformat,
                                         size_t width,
                                         size_t height)
{
    return setStorageImpl(context, 1, internalformat, width, height);
}

angle::Result RenderbufferVk::setStorageMultisample(const gl::Context *context,
                                                    size_t samples,
                                                    GLenum internalformat,
                                                    size_t width,
                                                    size_t height)
{
    return setStorageImpl(context, samples, internalformat, width, height);
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

    const vk::Format &vkFormat = renderer->getFormat(image->getFormat().info->sizedInternalFormat);
    const angle::Format &textureFormat = vkFormat.actualImageFormat();

    VkImageAspectFlags aspect = vk::GetFormatAspectFlags(textureFormat);

    // Transfer the image to this queue if needed
    uint32_t rendererQueueFamilyIndex = contextVk->getRenderer()->getQueueFamilyIndex();
    if (mImage->isQueueChangeNeccesary(rendererQueueFamilyIndex))
    {
        vk::CommandBuffer *commandBuffer = nullptr;
        ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));
        mImage->changeLayoutAndQueue(aspect, vk::ImageLayout::ColorAttachment,
                                     rendererQueueFamilyIndex, commandBuffer);
    }

    gl::TextureType viewType = imageVk->getImageTextureType();

    if (imageVk->getImageTextureType() == gl::TextureType::CubeMap)
    {
        viewType = vk::Get2DTextureType(imageVk->getImage()->getLayerCount(),
                                        imageVk->getImage()->getSamples());
    }

    mRenderTarget.init(mImage, &mImageViews, imageVk->getImageLevel(), imageVk->getImageLayer());

    return angle::Result::Continue;
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
        mImage->releaseImage(renderer);
        mImage->releaseStagingBuffer(renderer);
    }
    else
    {
        mImage = nullptr;
        mImageObserverBinding.bind(nullptr);
    }

    mImageViews.release(renderer);
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
        return angle::Result::Continue;

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(mImage->flushAllStagedUpdates(contextVk));
    return mImage->readPixelsForGetImage(contextVk, packState, packBuffer, 0, 0, format, type,
                                         pixels);
}

void RenderbufferVk::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    ASSERT(index == kRenderbufferImageSubjectIndex &&
           message == angle::SubjectMessage::SubjectChanged);

    // Forward the notification to the parent class that the staging buffer changed.
    onStateChange(angle::SubjectMessage::SubjectChanged);
}
}  // namespace rx
