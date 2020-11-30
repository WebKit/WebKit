// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SemaphoreVk.cpp: Defines the class interface for SemaphoreVk, implementing
// SemaphoreImpl.

#include "libANGLE/renderer/vulkan/SemaphoreVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

namespace rx
{

namespace
{
vk::ImageLayout GetVulkanImageLayout(GLenum layout)
{
    switch (layout)
    {
        case GL_NONE:
            return vk::ImageLayout::Undefined;
        case GL_LAYOUT_GENERAL_EXT:
            return vk::ImageLayout::ExternalShadersWrite;
        case GL_LAYOUT_COLOR_ATTACHMENT_EXT:
            return vk::ImageLayout::ColorAttachment;
        case GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT:
        case GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT:
        case GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT:
        case GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT:
            // Note: once VK_KHR_separate_depth_stencil_layouts becomes core or ubiquitous, we
            // should optimize depth/stencil image layout transitions to only be performed on the
            // aspect that needs transition.  In that case, these four layouts can be distinguished
            // and optimized.  Note that the exact equivalent of these layouts are specified in
            // VK_KHR_maintenance2, which are also usable, granted we transition the pair of
            // depth/stencil layouts accordingly elsewhere in ANGLE.
            return vk::ImageLayout::DepthStencilAttachment;
        case GL_LAYOUT_SHADER_READ_ONLY_EXT:
            return vk::ImageLayout::ExternalShadersReadOnly;
        case GL_LAYOUT_TRANSFER_SRC_EXT:
            return vk::ImageLayout::TransferSrc;
        case GL_LAYOUT_TRANSFER_DST_EXT:
            return vk::ImageLayout::TransferDst;
        default:
            UNREACHABLE();
            return vk::ImageLayout::Undefined;
    }
}
}  // anonymous namespace

SemaphoreVk::SemaphoreVk() = default;

SemaphoreVk::~SemaphoreVk() = default;

void SemaphoreVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->addGarbage(&mSemaphore);
}

angle::Result SemaphoreVk::importFd(gl::Context *context, gl::HandleType handleType, GLint fd)
{
    ContextVk *contextVk = vk::GetImpl(context);

    switch (handleType)
    {
        case gl::HandleType::OpaqueFd:
            return importOpaqueFd(contextVk, fd);

        default:
            ANGLE_VK_UNREACHABLE(contextVk);
            return angle::Result::Stop;
    }
}

angle::Result SemaphoreVk::importZirconHandle(gl::Context *context,
                                              gl::HandleType handleType,
                                              GLuint handle)
{
    ContextVk *contextVk = vk::GetImpl(context);

    switch (handleType)
    {
        case gl::HandleType::ZirconEvent:
            return importZirconEvent(contextVk, handle);

        default:
            ANGLE_VK_UNREACHABLE(contextVk);
            return angle::Result::Stop;
    }
}

angle::Result SemaphoreVk::wait(gl::Context *context,
                                const gl::BufferBarrierVector &bufferBarriers,
                                const gl::TextureBarrierVector &textureBarriers)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (!bufferBarriers.empty() || !textureBarriers.empty())
    {
        // Create one global memory barrier to cover all barriers.
        ANGLE_TRY(contextVk->syncExternalMemory());
    }

    uint32_t rendererQueueFamilyIndex = contextVk->getRenderer()->getQueueFamilyIndex();

    if (!bufferBarriers.empty())
    {
        // Perform a queue ownership transfer for each buffer.
        for (gl::Buffer *buffer : bufferBarriers)
        {
            BufferVk *bufferVk             = vk::GetImpl(buffer);
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            vk::CommandBuffer *commandBuffer;
            ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

            // Queue ownership transfer.
            bufferHelper.acquireFromExternal(contextVk, VK_QUEUE_FAMILY_EXTERNAL,
                                             rendererQueueFamilyIndex, commandBuffer);
        }
    }

    if (!textureBarriers.empty())
    {
        // Perform a queue ownership transfer for each texture.  Additionally, we are being
        // informed that the layout of the image has been externally transitioned, so we need to
        // update our internal state tracking.
        for (const gl::TextureAndLayout &textureAndLayout : textureBarriers)
        {
            TextureVk *textureVk   = vk::GetImpl(textureAndLayout.texture);
            vk::ImageHelper &image = textureVk->getImage();
            vk::ImageLayout layout = GetVulkanImageLayout(textureAndLayout.layout);

            vk::CommandBuffer *commandBuffer;
            ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

            // Image should not be accessed while unowned.
            ASSERT(!textureVk->getImage().hasStagedUpdates());

            // Queue ownership transfer and layout transition.
            image.acquireFromExternal(contextVk, VK_QUEUE_FAMILY_EXTERNAL, rendererQueueFamilyIndex,
                                      layout, commandBuffer);
        }
    }

    contextVk->addWaitSemaphore(mSemaphore.getHandle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    return angle::Result::Continue;
}

angle::Result SemaphoreVk::signal(gl::Context *context,
                                  const gl::BufferBarrierVector &bufferBarriers,
                                  const gl::TextureBarrierVector &textureBarriers)
{
    ContextVk *contextVk = vk::GetImpl(context);

    uint32_t rendererQueueFamilyIndex = contextVk->getRenderer()->getQueueFamilyIndex();

    if (!bufferBarriers.empty())
    {
        // Perform a queue ownership transfer for each buffer.
        for (gl::Buffer *buffer : bufferBarriers)
        {
            BufferVk *bufferVk             = vk::GetImpl(buffer);
            vk::BufferHelper &bufferHelper = bufferVk->getBuffer();

            vk::CommandBuffer *commandBuffer;
            ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

            // Queue ownership transfer.
            bufferHelper.releaseToExternal(contextVk, rendererQueueFamilyIndex,
                                           VK_QUEUE_FAMILY_EXTERNAL, commandBuffer);
        }
    }

    if (!textureBarriers.empty())
    {
        // Perform a queue ownership transfer for each texture.  Additionally, transition the image
        // to the requested layout.
        for (const gl::TextureAndLayout &textureAndLayout : textureBarriers)
        {
            TextureVk *textureVk   = vk::GetImpl(textureAndLayout.texture);
            vk::ImageHelper &image = textureVk->getImage();
            vk::ImageLayout layout = GetVulkanImageLayout(textureAndLayout.layout);

            // Don't transition to Undefined layout.  If external wants to transition the image away
            // from Undefined after this operation, it's perfectly fine to keep the layout as is in
            // ANGLE.  Note that vk::ImageHelper doesn't expect transitions to Undefined.
            if (layout == vk::ImageLayout::Undefined)
            {
                layout = image.getCurrentImageLayout();
            }

            ANGLE_TRY(textureVk->ensureImageInitialized(contextVk, ImageMipLevels::EnabledLevels));

            vk::CommandBuffer *commandBuffer;
            ANGLE_TRY(contextVk->endRenderPassAndGetCommandBuffer(&commandBuffer));

            // Queue ownership transfer and layout transition.
            image.releaseToExternal(contextVk, rendererQueueFamilyIndex, VK_QUEUE_FAMILY_EXTERNAL,
                                    layout, commandBuffer);
        }
    }

    if (!bufferBarriers.empty() || !textureBarriers.empty())
    {
        // Create one global memory barrier to cover all barriers.
        ANGLE_TRY(contextVk->syncExternalMemory());
    }

    return contextVk->flushImpl(&mSemaphore);
}

angle::Result SemaphoreVk::importOpaqueFd(ContextVk *contextVk, GLint fd)
{
    RendererVk *renderer = contextVk->getRenderer();

    if (!mSemaphore.valid())
    {
        mSemaphore.init(renderer->getDevice());
    }

    ASSERT(mSemaphore.valid());

    VkImportSemaphoreFdInfoKHR importSemaphoreFdInfo = {};
    importSemaphoreFdInfo.sType      = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    importSemaphoreFdInfo.semaphore  = mSemaphore.getHandle();
    importSemaphoreFdInfo.flags      = 0;
    importSemaphoreFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    importSemaphoreFdInfo.fd         = fd;

    ANGLE_VK_TRY(contextVk, vkImportSemaphoreFdKHR(renderer->getDevice(), &importSemaphoreFdInfo));

    return angle::Result::Continue;
}

angle::Result SemaphoreVk::importZirconEvent(ContextVk *contextVk, GLuint handle)
{
    RendererVk *renderer = contextVk->getRenderer();

    if (!mSemaphore.valid())
    {
        mSemaphore.init(renderer->getDevice());
    }

    ASSERT(mSemaphore.valid());

    VkImportSemaphoreZirconHandleInfoFUCHSIA importSemaphoreZirconHandleInfo = {};
    importSemaphoreZirconHandleInfo.sType =
        VK_STRUCTURE_TYPE_TEMP_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA;
    importSemaphoreZirconHandleInfo.semaphore = mSemaphore.getHandle();
    importSemaphoreZirconHandleInfo.flags     = 0;
    importSemaphoreZirconHandleInfo.handleType =
        VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA;
    importSemaphoreZirconHandleInfo.handle = handle;

    // TODO(spang): Add vkImportSemaphoreZirconHandleFUCHSIA to volk.
    static PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA =
        reinterpret_cast<PFN_vkImportSemaphoreZirconHandleFUCHSIA>(
            vkGetInstanceProcAddr(renderer->getInstance(), "vkImportSemaphoreZirconHandleFUCHSIA"));

    ANGLE_VK_TRY(contextVk, vkImportSemaphoreZirconHandleFUCHSIA(renderer->getDevice(),
                                                                 &importSemaphoreZirconHandleInfo));

    return angle::Result::Continue;
}

}  // namespace rx
