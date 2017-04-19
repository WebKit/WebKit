//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferVk.cpp:
//    Implements the class methods for FramebufferVk.
//

#include "libANGLE/renderer/vulkan/FramebufferVk.h"

#include <array>
#include <vulkan/vulkan.h>

#include "common/debug.h"
#include "image_util/imageformats.h"
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
    const gl::FramebufferAttachment *readAttachment)
{
    RenderTargetVk *renderTarget = nullptr;
    ANGLE_TRY(readAttachment->getRenderTarget(&renderTarget));

    GLenum implFormat = renderTarget->format->format().fboImplementationInternalFormat;
    return &gl::GetInternalFormatInfo(implFormat);
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

void FramebufferVk::destroy(ContextImpl *contextImpl)
{
    VkDevice device = GetAs<ContextVk>(contextImpl)->getDevice();

    mRenderPass.destroy(device);
    mFramebuffer.destroy(device);
}

void FramebufferVk::destroyDefault(DisplayImpl *displayImpl)
{
    VkDevice device = GetAs<DisplayVk>(displayImpl)->getRenderer()->getDevice();

    mRenderPass.destroy(device);
    mFramebuffer.destroy(device);
}

gl::Error FramebufferVk::discard(size_t count, const GLenum *attachments)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferVk::invalidate(size_t count, const GLenum *attachments)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferVk::invalidateSub(size_t count,
                                       const GLenum *attachments,
                                       const gl::Rectangle &area)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferVk::clear(ContextImpl *context, GLbitfield mask)
{
    ContextVk *contextVk = GetAs<ContextVk>(context);

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

    vk::CommandBuffer *commandBuffer = contextVk->getCommandBuffer();
    ANGLE_TRY(commandBuffer->begin(contextVk->getDevice()));

    for (const auto &colorAttachment : mState.getColorAttachments())
    {
        if (colorAttachment.isAttached())
        {
            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget(&renderTarget));
            renderTarget->image->changeLayoutTop(
                VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
            commandBuffer->clearSingleColorImage(*renderTarget->image, clearColorValue);
        }
    }

    commandBuffer->end();

    ANGLE_TRY(contextVk->submitCommands(*commandBuffer));

    return gl::NoError();
}

gl::Error FramebufferVk::clearBufferfv(ContextImpl *context,
                                       GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat *values)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferVk::clearBufferuiv(ContextImpl *context,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        const GLuint *values)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferVk::clearBufferiv(ContextImpl *context,
                                       GLenum buffer,
                                       GLint drawbuffer,
                                       const GLint *values)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferVk::clearBufferfi(ContextImpl *context,
                                       GLenum buffer,
                                       GLint drawbuffer,
                                       GLfloat depth,
                                       GLint stencil)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

GLenum FramebufferVk::getImplementationColorReadFormat() const
{
    auto errOrResult = GetReadAttachmentInfo(mState.getReadAttachment());

    // TODO(jmadill): Handle getRenderTarget error.
    if (errOrResult.isError())
    {
        ERR() << "Internal error in FramebufferVk::getImplementationColorReadFormat.";
        return GL_NONE;
    }

    return errOrResult.getResult()->format;
}

GLenum FramebufferVk::getImplementationColorReadType() const
{
    auto errOrResult = GetReadAttachmentInfo(mState.getReadAttachment());

    // TODO(jmadill): Handle getRenderTarget error.
    if (errOrResult.isError())
    {
        ERR() << "Internal error in FramebufferVk::getImplementationColorReadFormat.";
        return GL_NONE;
    }

    return errOrResult.getResult()->type;
}

gl::Error FramebufferVk::readPixels(ContextImpl *context,
                                    const gl::Rectangle &area,
                                    GLenum format,
                                    GLenum type,
                                    GLvoid *pixels) const
{
    const auto &glState         = context->getGLState();
    const auto *readFramebuffer = glState.getReadFramebuffer();
    const auto *readAttachment  = readFramebuffer->getReadColorbuffer();

    RenderTargetVk *renderTarget = nullptr;
    ANGLE_TRY(readAttachment->getRenderTarget(&renderTarget));

    ContextVk *contextVk = GetAs<ContextVk>(context);
    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = renderer->getDevice();

    vk::Image *readImage = renderTarget->image;
    vk::StagingImage stagingImage;
    ANGLE_TRY(renderer->createStagingImage(TextureDimension::TEX_2D, *renderTarget->format,
                                           renderTarget->extents, &stagingImage));

    vk::CommandBuffer *commandBuffer = contextVk->getCommandBuffer();
    commandBuffer->begin(device);
    stagingImage.getImage().changeLayoutTop(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                            commandBuffer);

    gl::Box copyRegion;
    copyRegion.x      = area.x;
    copyRegion.y      = area.y;
    copyRegion.z      = 0;
    copyRegion.width  = area.width;
    copyRegion.height = area.height;
    copyRegion.depth  = 1;

    readImage->changeLayoutTop(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               commandBuffer);
    commandBuffer->copySingleImage(*readImage, stagingImage.getImage(), copyRegion,
                                   VK_IMAGE_ASPECT_COLOR_BIT);
    commandBuffer->end();

    ANGLE_TRY(renderer->submitAndFinishCommandBuffer(*commandBuffer));

    // TODO(jmadill): parameters
    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(
        stagingImage.getDeviceMemory().map(device, 0, stagingImage.getSize(), 0, &mapPointer));

    const auto &angleFormat = renderTarget->format->format();

    // TODO(jmadill): Use pixel bytes from the ANGLE format directly.
    const auto &glFormat = gl::GetInternalFormatInfo(angleFormat.glInternalFormat);
    int inputPitch       = glFormat.pixelBytes * area.width;

    PackPixelsParams params;
    params.area        = area;
    params.format      = format;
    params.type        = type;
    params.outputPitch = inputPitch;
    params.pack        = glState.getPackState();

    PackPixels(params, angleFormat, inputPitch, mapPointer, reinterpret_cast<uint8_t *>(pixels));

    stagingImage.getDeviceMemory().unmap(device);
    renderer->enqueueGarbage(renderer->getCurrentQueueSerial(), std::move(stagingImage));

    stagingImage.getImage().destroy(renderer->getDevice());

    stagingImage.destroy(device);

    return vk::NoError();
}

gl::Error FramebufferVk::blit(ContextImpl *context,
                              const gl::Rectangle &sourceArea,
                              const gl::Rectangle &destArea,
                              GLbitfield mask,
                              GLenum filter)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

bool FramebufferVk::checkStatus() const
{
    UNIMPLEMENTED();
    return bool();
}

void FramebufferVk::syncState(ContextImpl *contextImpl, const gl::Framebuffer::DirtyBits &dirtyBits)
{
    auto contextVk = GetAs<ContextVk>(contextImpl);

    ASSERT(dirtyBits.any());

    // TODO(jmadill): Smarter update.
    mRenderPass.destroy(contextVk->getDevice());
    mFramebuffer.destroy(contextVk->getDevice());

    // TODO(jmadill): Use pipeline cache.
    contextVk->invalidateCurrentPipeline();
}

gl::ErrorOrResult<vk::RenderPass *> FramebufferVk::getRenderPass(VkDevice device)
{
    if (mRenderPass.valid())
    {
        return &mRenderPass;
    }

    // TODO(jmadill): Can we use stack-only memory?
    std::vector<VkAttachmentDescription> attachmentDescs;
    std::vector<VkAttachmentReference> colorAttachmentRefs;

    const auto &colorAttachments = mState.getColorAttachments();
    for (size_t attachmentIndex = 0; attachmentIndex < colorAttachments.size(); ++attachmentIndex)
    {
        const auto &colorAttachment = colorAttachments[attachmentIndex];
        if (colorAttachment.isAttached())
        {
            VkAttachmentDescription colorDesc;
            VkAttachmentReference colorRef;

            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget(&renderTarget));

            // TODO(jmadill): We would only need this flag for duplicated attachments.
            colorDesc.flags   = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
            colorDesc.format  = renderTarget->format->native;
            colorDesc.samples = ConvertSamples(colorAttachment.getSamples());

            // The load op controls the prior existing depth/color attachment data.
            // TODO(jmadill): Proper load ops. Should not be hard coded to clear.
            colorDesc.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorDesc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            colorDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorDesc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;

            // We might want to transition directly to PRESENT_SRC for Surface attachments.
            colorDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            colorRef.attachment = static_cast<uint32_t>(colorAttachments.size()) - 1u;
            colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescs.push_back(colorDesc);
            colorAttachmentRefs.push_back(colorRef);
        }
    }

    const auto *depthStencilAttachment = mState.getDepthStencilAttachment();
    VkAttachmentReference depthStencilAttachmentRef;
    bool useDepth = depthStencilAttachment && depthStencilAttachment->isAttached();

    if (useDepth)
    {
        VkAttachmentDescription depthStencilDesc;

        RenderTargetVk *renderTarget = nullptr;
        ANGLE_TRY(depthStencilAttachment->getRenderTarget(&renderTarget));

        depthStencilDesc.flags          = 0;
        depthStencilDesc.format         = renderTarget->format->native;
        depthStencilDesc.samples        = ConvertSamples(depthStencilAttachment->getSamples());
        depthStencilDesc.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthStencilDesc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        depthStencilDesc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthStencilDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthStencilDesc.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStencilDesc.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthStencilAttachmentRef.attachment = static_cast<uint32_t>(attachmentDescs.size());
        depthStencilAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachmentDescs.push_back(depthStencilDesc);
    }

    ASSERT(!attachmentDescs.empty());

    VkSubpassDescription subpassDesc;

    subpassDesc.flags                   = 0;
    subpassDesc.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.inputAttachmentCount    = 0;
    subpassDesc.pInputAttachments       = nullptr;
    subpassDesc.colorAttachmentCount    = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpassDesc.pColorAttachments       = colorAttachmentRefs.data();
    subpassDesc.pResolveAttachments     = nullptr;
    subpassDesc.pDepthStencilAttachment = (useDepth ? &depthStencilAttachmentRef : nullptr);
    subpassDesc.preserveAttachmentCount = 0;
    subpassDesc.pPreserveAttachments    = nullptr;

    VkRenderPassCreateInfo renderPassInfo;

    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext           = nullptr;
    renderPassInfo.flags           = 0;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassInfo.pAttachments    = attachmentDescs.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpassDesc;
    renderPassInfo.dependencyCount = 0;
    renderPassInfo.pDependencies   = nullptr;

    vk::RenderPass renderPass;
    ANGLE_TRY(renderPass.init(device, renderPassInfo));

    mRenderPass.retain(device, std::move(renderPass));

    return &mRenderPass;
}

gl::ErrorOrResult<vk::Framebuffer *> FramebufferVk::getFramebuffer(VkDevice device)
{
    // If we've already created our cached Framebuffer, return it.
    if (mFramebuffer.valid())
    {
        return &mFramebuffer;
    }

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY_RESULT(getRenderPass(device), renderPass);

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
            ANGLE_TRY(colorAttachment.getRenderTarget<RenderTargetVk>(&renderTarget));
            attachments.push_back(renderTarget->imageView->getHandle());

            ASSERT(attachmentsSize.empty() || attachmentsSize == colorAttachment.getSize());
            attachmentsSize = colorAttachment.getSize();
        }
    }

    const auto *depthStencilAttachment = mState.getDepthStencilAttachment();
    if (depthStencilAttachment && depthStencilAttachment->isAttached())
    {
        RenderTargetVk *renderTarget = nullptr;
        ANGLE_TRY(depthStencilAttachment->getRenderTarget<RenderTargetVk>(&renderTarget));
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

    vk::Framebuffer framebuffer;
    ANGLE_TRY(static_cast<gl::Error>(framebuffer.init(device, framebufferInfo)));

    mFramebuffer.retain(device, std::move(framebuffer));

    return &mFramebuffer;
}

gl::Error FramebufferVk::getSamplePosition(size_t index, GLfloat *xy) const
{
    UNIMPLEMENTED();
    return gl::InternalError() << "getSamplePosition is unimplemented.";
}

gl::Error FramebufferVk::beginRenderPass(VkDevice device,
                                         vk::CommandBuffer *commandBuffer,
                                         Serial queueSerial,
                                         const gl::State &glState)
{
    // TODO(jmadill): Cache render targets.
    for (const auto &colorAttachment : mState.getColorAttachments())
    {
        if (colorAttachment.isAttached())
        {
            RenderTargetVk *renderTarget = nullptr;
            ANGLE_TRY(colorAttachment.getRenderTarget<RenderTargetVk>(&renderTarget));
            renderTarget->resource->setQueueSerial(queueSerial);
        }
    }

    const auto *depthStencilAttachment = mState.getDepthStencilAttachment();
    if (depthStencilAttachment && depthStencilAttachment->isAttached())
    {
        RenderTargetVk *renderTarget = nullptr;
        ANGLE_TRY(depthStencilAttachment->getRenderTarget<RenderTargetVk>(&renderTarget));
        renderTarget->resource->setQueueSerial(queueSerial);
    }

    vk::Framebuffer *framebuffer = nullptr;
    ANGLE_TRY_RESULT(getFramebuffer(device), framebuffer);
    ASSERT(framebuffer && framebuffer->valid());

    vk::RenderPass *renderPass = nullptr;
    ANGLE_TRY_RESULT(getRenderPass(device), renderPass);
    ASSERT(renderPass && renderPass->valid());

    // TODO(jmadill): Proper clear value implementation.
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
    ASSERT(mBackbuffer);
    RenderTargetVk *renderTarget = nullptr;
    ANGLE_TRY(mState.getFirstColorAttachment()->getRenderTarget(&renderTarget));
    renderTarget->image->updateLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    ANGLE_TRY(commandBuffer->begin(device));
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
