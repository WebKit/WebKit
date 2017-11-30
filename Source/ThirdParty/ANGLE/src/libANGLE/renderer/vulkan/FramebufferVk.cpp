//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferVk.cpp:
//    Implements the class methods for FramebufferVk.
//

#include "libANGLE/renderer/vulkan/FramebufferVk.h"

#include <vulkan/vulkan.h>
#include <array>

#include "common/debug.h"
#include "image_util/imageformats.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/SurfaceVk.h"
#include "libANGLE/renderer/vulkan/formatutilsvk.h"

namespace rx
{

namespace
{

gl::ErrorOrResult<const gl::InternalFormat *> GetReadAttachmentInfo(
    const gl::Context *context,
    const gl::FramebufferAttachment *readAttachment)
{
    RenderTargetVk *renderTarget = nullptr;
    ANGLE_TRY(readAttachment->getRenderTarget(context, &renderTarget));

    GLenum implFormat = renderTarget->format->textureFormat().fboImplementationInternalFormat;
    return &gl::GetSizedInternalFormatInfo(implFormat);
}

VkSampleCountFlagBits ConvertSamples(GLint sampleCount)
{
    switch (sampleCount)
    {
        case 0:
        case 1:
            return VK_SAMPLE_COUNT_1_BIT;
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
        case 16:
            return VK_SAMPLE_COUNT_16_BIT;
        case 32:
            return VK_SAMPLE_COUNT_32_BIT;
        default:
            UNREACHABLE();
            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    }
}

}  // anonymous namespace

// static
FramebufferVk *FramebufferVk::CreateUserFBO(const gl::FramebufferState &state)
{
    return new FramebufferVk(state);
}

// static
FramebufferVk *FramebufferVk::CreateDefaultFBO(const gl::FramebufferState &state,
                                               WindowSurfaceVk *backbuffer)
{
    return new FramebufferVk(state, backbuffer);
}

FramebufferVk::FramebufferVk(const gl::FramebufferState &state)
    : FramebufferImpl(state), mBackbuffer(nullptr), mRenderPass(), mFramebuffer()
{
}

FramebufferVk::FramebufferVk(const gl::FramebufferState &state, WindowSurfaceVk *backbuffer)
    : FramebufferImpl(state), mBackbuffer(backbuffer), mRenderPass(), mFramebuffer()
{
}

FramebufferVk::~FramebufferVk()
{
}

void FramebufferVk::destroy(const gl::Context *context)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();

    renderer->releaseResource(*this, &mRenderPass);
    renderer->releaseResource(*this, &mFramebuffer);
}

void FramebufferVk::destroyDefault(const egl::Display *display)
{
    VkDevice device = vk::GetImpl(display)->getRenderer()->getDevice();

    mRenderPass.destroy(device);
    mFramebuffer.destroy(device);
}

gl::Error FramebufferVk::discard(const gl::Context *context,
                                 size_t count,
                                 const GLenum *attachments)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error FramebufferVk::invalidate(const gl::Context *context,
                                    size_t count,
                                    const GLenum *attachments)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error FramebufferVk::invalidateSub(const gl::Context *context,
                                       size_t count,
                                       const GLenum *attachments,
                                       const gl::Rectangle &area)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error FramebufferVk::clear(const gl::Context *context, GLbitfield mask)
{
    ContextVk *contextVk = vk::GetImpl(context);

    if (mState.getDepthAttachment() && (mask & GL_DEPTH_BUFFER_BIT) != 0)
    {
        // TODO(jmadill): Depth clear
        UNIMPLEMENTED();
    }

    if (mState.getStencilAttachment() && (mask & GL_STENCIL_BUFFER_BIT) != 0)
    {
        // TODO(jmadill): Stencil clear
        UNIMPLEMENTED();
    }

    if ((mask & GL_COLOR_BUFFER_BIT) == 0)
    {
        return gl::NoError();
    }

    const auto &glState    = context->getGLState();
    const auto &clearColor = glState.getColorClearValue();
    VkClearColorValue clearColorValue;
    clearColorValue.float32[0] = clearColor.red;
    clearColorValue.float32[1] = clearColor.green;
    clearColorValue.float32[2] = clearColor.blue;
    clearColorValue.float32[3] = clearColor.alpha;

    // TODO(jmadill): Scissored clears.
    const auto *attachment = mState.getFirstNonNullAttachment();
    ASSERT(attachment && attachment->isAttached());
    const auto &size = attachment->getSize();
    const gl::Rectangle renderArea(0, 0, size.width, size.height);

    vk::CommandBufferAndState *commandBuffer = nullptr;
    ANGLE_TRY(contextVk->getStartedCommandBuffer(&commandBuffer));

    for (const auto &colorAttachment : mState.getColorAttachments())
    {
        if (colorAttachment.isAttached())
        {
            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget(context, &renderTarget));
            renderTarget->image->changeLayoutTop(
                VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
            commandBuffer->clearSingleColorImage(*renderTarget->image, clearColorValue);
        }
    }

    return gl::NoError();
}

gl::Error FramebufferVk::clearBufferfv(const gl::Context *context,
                                       GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat *values)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error FramebufferVk::clearBufferuiv(const gl::Context *context,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        const GLuint *values)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error FramebufferVk::clearBufferiv(const gl::Context *context,
                                       GLenum buffer,
                                       GLint drawbuffer,
                                       const GLint *values)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error FramebufferVk::clearBufferfi(const gl::Context *context,
                                       GLenum buffer,
                                       GLint drawbuffer,
                                       GLfloat depth,
                                       GLint stencil)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

GLenum FramebufferVk::getImplementationColorReadFormat(const gl::Context *context) const
{
    auto errOrResult = GetReadAttachmentInfo(context, mState.getReadAttachment());

    // TODO(jmadill): Handle getRenderTarget error.
    if (errOrResult.isError())
    {
        ERR() << "Internal error in FramebufferVk::getImplementationColorReadFormat.";
        return GL_NONE;
    }

    return errOrResult.getResult()->format;
}

GLenum FramebufferVk::getImplementationColorReadType(const gl::Context *context) const
{
    auto errOrResult = GetReadAttachmentInfo(context, mState.getReadAttachment());

    // TODO(jmadill): Handle getRenderTarget error.
    if (errOrResult.isError())
    {
        ERR() << "Internal error in FramebufferVk::getImplementationColorReadFormat.";
        return GL_NONE;
    }

    return errOrResult.getResult()->type;
}

gl::Error FramebufferVk::readPixels(const gl::Context *context,
                                    const gl::Rectangle &area,
                                    GLenum format,
                                    GLenum type,
                                    void *pixels)
{
    const auto &glState         = context->getGLState();
    const auto *readFramebuffer = glState.getReadFramebuffer();
    const auto *readAttachment  = readFramebuffer->getReadColorbuffer();

    RenderTargetVk *renderTarget = nullptr;
    ANGLE_TRY(readAttachment->getRenderTarget(context, &renderTarget));

    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = renderer->getDevice();

    vk::Image *readImage = renderTarget->image;
    vk::StagingImage stagingImage;
    ANGLE_TRY(renderer->createStagingImage(TextureDimension::TEX_2D, *renderTarget->format,
                                           renderTarget->extents, vk::StagingUsage::Read,
                                           &stagingImage));

    vk::CommandBufferAndState *commandBuffer = nullptr;
    ANGLE_TRY(contextVk->getStartedCommandBuffer(&commandBuffer));

    // End render pass if we're in one.
    renderer->endRenderPass();

    stagingImage.getImage().changeLayoutTop(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                            commandBuffer);

    readImage->changeLayoutWithStages(
        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, commandBuffer);

    VkImageCopy region;
    region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel       = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount     = 1;
    region.srcOffset.x                   = area.x;
    region.srcOffset.y                   = area.y;
    region.srcOffset.z                   = 0;
    region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel       = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount     = 1;
    region.dstOffset.x                   = 0;
    region.dstOffset.y                   = 0;
    region.dstOffset.z                   = 0;
    region.extent.width                  = area.width;
    region.extent.height                 = area.height;
    region.extent.depth                  = 1;

    commandBuffer->copyImage(*readImage, stagingImage.getImage(), 1, &region);

    ANGLE_TRY(renderer->submitAndFinishCommandBuffer(commandBuffer));

    // TODO(jmadill): parameters
    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(
        stagingImage.getDeviceMemory().map(device, 0, stagingImage.getSize(), 0, &mapPointer));

    const auto &angleFormat = renderTarget->format->textureFormat();

    // TODO(jmadill): Use pixel bytes from the ANGLE format directly.
    const auto &glFormat = gl::GetSizedInternalFormatInfo(angleFormat.glInternalFormat);
    int inputPitch       = glFormat.pixelBytes * area.width;

    PackPixelsParams params;
    params.area        = area;
    params.format      = format;
    params.type        = type;
    params.outputPitch = inputPitch;
    params.packBuffer  = glState.getTargetBuffer(gl::BufferBinding::PixelPack);
    params.pack        = glState.getPackState();

    PackPixels(params, angleFormat, inputPitch, mapPointer, reinterpret_cast<uint8_t *>(pixels));

    stagingImage.getDeviceMemory().unmap(device);
    renderer->releaseObject(renderer->getCurrentQueueSerial(), &stagingImage);

    return vk::NoError();
}

gl::Error FramebufferVk::blit(const gl::Context *context,
                              const gl::Rectangle &sourceArea,
                              const gl::Rectangle &destArea,
                              GLbitfield mask,
                              GLenum filter)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

bool FramebufferVk::checkStatus(const gl::Context *context) const
{
    return true;
}

void FramebufferVk::syncState(const gl::Context *context,
                              const gl::Framebuffer::DirtyBits &dirtyBits)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    ASSERT(dirtyBits.any());

    // TODO(jmadill): Smarter update.
    renderer->releaseResource(*this, &mRenderPass);
    renderer->releaseResource(*this, &mFramebuffer);
    renderer->onReleaseRenderPass(this);

    // TODO(jmadill): Use pipeline cache.
    contextVk->invalidateCurrentPipeline();
}

gl::ErrorOrResult<vk::RenderPass *> FramebufferVk::getRenderPass(const gl::Context *context,
                                                                 VkDevice device)
{
    if (mRenderPass.valid())
    {
        return &mRenderPass;
    }

    vk::RenderPassDesc desc;

    const auto &colorAttachments = mState.getColorAttachments();
    for (size_t attachmentIndex = 0; attachmentIndex < colorAttachments.size(); ++attachmentIndex)
    {
        const auto &colorAttachment = colorAttachments[attachmentIndex];
        if (colorAttachment.isAttached())
        {
            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget(context, &renderTarget));

            VkAttachmentDescription *colorDesc = desc.nextColorAttachment();

            // TODO(jmadill): We would only need this flag for duplicated attachments.
            colorDesc->flags   = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
            colorDesc->format  = renderTarget->format->vkTextureFormat;
            colorDesc->samples = ConvertSamples(colorAttachment.getSamples());

            // The load op controls the prior existing depth/color attachment data.
            // TODO(jmadill): Proper load ops. Should not be hard coded to clear.
            colorDesc->loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorDesc->storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            colorDesc->stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorDesc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorDesc->initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

            // We might want to transition directly to PRESENT_SRC for Surface attachments.
            colorDesc->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
    }

    const auto *depthStencilAttachment = mState.getDepthStencilAttachment();

    if (depthStencilAttachment && depthStencilAttachment->isAttached())
    {
        RenderTargetVk *renderTarget = nullptr;
        ANGLE_TRY(depthStencilAttachment->getRenderTarget(context, &renderTarget));

        VkAttachmentDescription *depthStencilDesc = desc.nextDepthStencilAttachment();

        depthStencilDesc->flags          = 0;
        depthStencilDesc->format         = renderTarget->format->vkTextureFormat;
        depthStencilDesc->samples        = ConvertSamples(depthStencilAttachment->getSamples());
        depthStencilDesc->loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthStencilDesc->storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        depthStencilDesc->stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthStencilDesc->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthStencilDesc->initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStencilDesc->finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    ANGLE_TRY(vk::InitializeRenderPassFromDesc(device, desc, &mRenderPass));

    return &mRenderPass;
}

gl::ErrorOrResult<vk::Framebuffer *> FramebufferVk::getFramebuffer(const gl::Context *context,
                                                                   VkDevice device)
{
    // If we've already created our cached Framebuffer, return it.
    if (mFramebuffer.valid())
    {
        return &mFramebuffer;
    }

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY_RESULT(getRenderPass(context, device), renderPass);

    // If we've a Framebuffer provided by a Surface (default FBO/backbuffer), query it.
    if (mBackbuffer)
    {
        return mBackbuffer->getCurrentFramebuffer(device, *renderPass);
    }

    // Gather VkImageViews over all FBO attachments, also size of attached region.
    std::vector<VkImageView> attachments;
    gl::Extents attachmentsSize;

    const auto &colorAttachments = mState.getColorAttachments();
    for (size_t attachmentIndex = 0; attachmentIndex < colorAttachments.size(); ++attachmentIndex)
    {
        const auto &colorAttachment = colorAttachments[attachmentIndex];
        if (colorAttachment.isAttached())
        {
            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget<RenderTargetVk>(context, &renderTarget));
            attachments.push_back(renderTarget->imageView->getHandle());

            ASSERT(attachmentsSize.empty() || attachmentsSize == colorAttachment.getSize());
            attachmentsSize = colorAttachment.getSize();
        }
    }

    const auto *depthStencilAttachment = mState.getDepthStencilAttachment();
    if (depthStencilAttachment && depthStencilAttachment->isAttached())
    {
        RenderTargetVk *renderTarget = nullptr;
        ANGLE_TRY(depthStencilAttachment->getRenderTarget<RenderTargetVk>(context, &renderTarget));
        attachments.push_back(renderTarget->imageView->getHandle());

        ASSERT(attachmentsSize.empty() || attachmentsSize == depthStencilAttachment->getSize());
        attachmentsSize = depthStencilAttachment->getSize();
    }

    ASSERT(!attachments.empty());

    VkFramebufferCreateInfo framebufferInfo;

    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext           = nullptr;
    framebufferInfo.flags           = 0;
    framebufferInfo.renderPass      = mRenderPass.getHandle();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = static_cast<uint32_t>(attachmentsSize.width);
    framebufferInfo.height          = static_cast<uint32_t>(attachmentsSize.height);
    framebufferInfo.layers          = 1;

    ANGLE_TRY(mFramebuffer.init(device, framebufferInfo));

    return &mFramebuffer;
}

gl::Error FramebufferVk::getSamplePosition(size_t index, GLfloat *xy) const
{
    UNIMPLEMENTED();
    return gl::InternalError() << "getSamplePosition is unimplemented.";
}

gl::Error FramebufferVk::beginRenderPass(const gl::Context *context,
                                         VkDevice device,
                                         vk::CommandBuffer *commandBuffer,
                                         Serial queueSerial)
{
    // TODO(jmadill): Cache render targets.
    for (const auto &colorAttachment : mState.getColorAttachments())
    {
        if (colorAttachment.isAttached())
        {
            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget<RenderTargetVk>(context, &renderTarget));
            renderTarget->resource->setQueueSerial(queueSerial);
        }
    }

    const auto *depthStencilAttachment = mState.getDepthStencilAttachment();
    if (depthStencilAttachment && depthStencilAttachment->isAttached())
    {
        RenderTargetVk *renderTarget = nullptr;
        ANGLE_TRY(depthStencilAttachment->getRenderTarget<RenderTargetVk>(context, &renderTarget));
        renderTarget->resource->setQueueSerial(queueSerial);
    }

    vk::Framebuffer *framebuffer = nullptr;
    ANGLE_TRY_RESULT(getFramebuffer(context, device), framebuffer);
    ASSERT(framebuffer && framebuffer->valid());

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY_RESULT(getRenderPass(context, device), renderPass);
    ASSERT(renderPass && renderPass->valid());

    // TODO(jmadill): Proper clear value implementation.
    const gl::State &glState = context->getGLState();
    VkClearColorValue colorClear;
    memset(&colorClear, 0, sizeof(VkClearColorValue));
    colorClear.float32[0] = glState.getColorClearValue().red;
    colorClear.float32[1] = glState.getColorClearValue().green;
    colorClear.float32[2] = glState.getColorClearValue().blue;
    colorClear.float32[3] = glState.getColorClearValue().alpha;

    std::vector<VkClearValue> attachmentClearValues;
    attachmentClearValues.push_back({colorClear});

    // Updated the cached image layout of the attachments in this FBO.
    // For a default FBO, we need to call through to the WindowSurfaceVk
    // TODO(jmadill): Iterate over all attachments.
    RenderTargetVk *renderTarget = nullptr;
    ANGLE_TRY(mState.getFirstColorAttachment()->getRenderTarget(context, &renderTarget));
    renderTarget->image->updateLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    commandBuffer->beginRenderPass(*renderPass, *framebuffer, glState.getViewport(),
                                   attachmentClearValues);

    setQueueSerial(queueSerial);
    if (mBackbuffer)
    {
        mBackbuffer->setQueueSerial(queueSerial);
    }

    return gl::NoError();
}

}  // namespace rx
