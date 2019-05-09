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
RenderbufferVk::RenderbufferVk(const gl::RenderbufferState &state)
    : RenderbufferImpl(state), mOwnsImage(false), mImage(nullptr)
{}

RenderbufferVk::~RenderbufferVk() {}

void RenderbufferVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();
    releaseAndDeleteImage(context, renderer);
}

angle::Result RenderbufferVk::setStorage(const gl::Context *context,
                                         GLenum internalformat,
                                         size_t width,
                                         size_t height)
{
    ContextVk *contextVk       = vk::GetImpl(context);
    RendererVk *renderer       = contextVk->getRenderer();
    const vk::Format &vkFormat = renderer->getFormat(internalformat);

    if (!mOwnsImage)
    {
        releaseAndDeleteImage(context, renderer);
    }

    if (mImage != nullptr && mImage->valid())
    {
        // Check against the state if we need to recreate the storage.
        if (internalformat != mState.getFormat().info->internalFormat ||
            static_cast<GLsizei>(width) != mState.getWidth() ||
            static_cast<GLsizei>(height) != mState.getHeight())
        {
            releaseImage(context, renderer);
        }
    }

    if ((mImage == nullptr || !mImage->valid()) && (width != 0 && height != 0))
    {
        if (mImage == nullptr)
        {
            mImage     = new vk::ImageHelper();
            mOwnsImage = true;
        }

        const angle::Format &textureFormat = vkFormat.imageFormat();
        bool isDepthOrStencilFormat = textureFormat.depthBits > 0 || textureFormat.stencilBits > 0;
        const VkImageUsageFlags usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            (textureFormat.redBits > 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0) |
            (isDepthOrStencilFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : 0);

        gl::Extents extents(static_cast<int>(width), static_cast<int>(height), 1);
        ANGLE_TRY(mImage->init(contextVk, gl::TextureType::_2D, extents, vkFormat, 1, usage, 1, 1));

        VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        ANGLE_TRY(mImage->initMemory(contextVk, renderer->getMemoryProperties(), flags));

        VkImageAspectFlags aspect = vk::GetFormatAspectFlags(textureFormat);

        // Note that LUMA textures are not color-renderable, so a read-view with swizzle is not
        // needed.
        ANGLE_TRY(mImage->initImageView(contextVk, gl::TextureType::_2D, aspect, gl::SwizzleState(),
                                        &mImageView, 0, 1));

        // Clear the renderbuffer if it has emulated channels.
        ANGLE_TRY(mImage->clearIfEmulatedFormat(vk::GetImpl(context), gl::ImageIndex::Make2D(0),
                                                vkFormat));

        mRenderTarget.init(mImage, &mImageView, 0, 0, nullptr);
    }

    return angle::Result::Continue;
}

angle::Result RenderbufferVk::setStorageMultisample(const gl::Context *context,
                                                    size_t samples,
                                                    GLenum internalformat,
                                                    size_t width,
                                                    size_t height)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result RenderbufferVk::setStorageEGLImageTarget(const gl::Context *context,
                                                       egl::Image *image)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    releaseAndDeleteImage(context, renderer);

    ImageVk *imageVk = vk::GetImpl(image);
    mImage           = imageVk->getImage();
    mOwnsImage       = false;

    const vk::Format &vkFormat = renderer->getFormat(image->getFormat().info->sizedInternalFormat);
    const angle::Format &textureFormat = vkFormat.imageFormat();

    VkImageAspectFlags aspect = vk::GetFormatAspectFlags(textureFormat);

    // Transfer the image to this queue if needed
    uint32_t rendererQueueFamilyIndex = contextVk->getRenderer()->getQueueFamilyIndex();
    if (mImage->isQueueChangeNeccesary(rendererQueueFamilyIndex))
    {
        vk::CommandBuffer *commandBuffer = nullptr;
        ANGLE_TRY(mImage->recordCommands(contextVk, &commandBuffer));
        mImage->changeLayoutAndQueue(aspect, vk::ImageLayout::ColorAttachment,
                                     rendererQueueFamilyIndex, commandBuffer);
    }

    ANGLE_TRY(mImage->initLayerImageView(contextVk, imageVk->getImageTextureType(), aspect,
                                         gl::SwizzleState(), &mImageView, imageVk->getImageLevel(),
                                         1, imageVk->getImageLayer(), 1));

    mRenderTarget.init(mImage, &mImageView, imageVk->getImageLevel(), imageVk->getImageLayer(),
                       nullptr);

    return angle::Result::Continue;
}

angle::Result RenderbufferVk::getAttachmentRenderTarget(const gl::Context *context,
                                                        GLenum binding,
                                                        const gl::ImageIndex &imageIndex,
                                                        FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(mImage && mImage->valid());
    *rtOut = &mRenderTarget;
    return angle::Result::Continue;
}

angle::Result RenderbufferVk::initializeContents(const gl::Context *context,
                                                 const gl::ImageIndex &imageIndex)
{
    mImage->stageSubresourceRobustClear(imageIndex, mImage->getFormat().angleFormat());
    return mImage->flushAllStagedUpdates(vk::GetImpl(context));
}

void RenderbufferVk::releaseOwnershipOfImage(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    mOwnsImage = false;
    releaseAndDeleteImage(context, renderer);
}

void RenderbufferVk::releaseAndDeleteImage(const gl::Context *context, RendererVk *renderer)
{
    releaseImage(context, renderer);
    SafeDelete(mImage);
}

void RenderbufferVk::releaseImage(const gl::Context *context, RendererVk *renderer)
{
    if (mImage && mOwnsImage)
    {
        mImage->releaseImage(renderer);
        mImage->releaseStagingBuffer(renderer);
    }
    else
    {
        mImage = nullptr;
    }

    renderer->releaseObject(renderer->getCurrentQueueSerial(), &mImageView);
}

}  // namespace rx
