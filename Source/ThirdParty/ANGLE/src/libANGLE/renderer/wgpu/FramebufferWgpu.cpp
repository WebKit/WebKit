//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferWgpu.cpp:
//    Implements the class methods for FramebufferWgpu.
//

#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"
#include <__config>

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

namespace
{
bool CompareColorRenderPassAttachments(const wgpu::RenderPassColorAttachment &attachment1,
                                       const wgpu::RenderPassColorAttachment &attachment2)
{

    if (attachment1.nextInChain != nullptr || attachment2.nextInChain != nullptr)
    {
        return false;
    }

    return attachment1.view.Get() == attachment2.view.Get() &&
           attachment1.depthSlice == attachment2.depthSlice &&
           attachment1.resolveTarget.Get() == attachment2.resolveTarget.Get() &&
           attachment1.loadOp == attachment2.loadOp && attachment1.storeOp == attachment2.storeOp &&
           attachment1.clearValue.r == attachment2.clearValue.r &&
           attachment1.clearValue.g == attachment2.clearValue.g &&
           attachment1.clearValue.b == attachment2.clearValue.b &&
           attachment1.clearValue.a == attachment2.clearValue.a;
}

bool CompareColorRenderPassAttachmentVectors(
    const std::vector<wgpu::RenderPassColorAttachment> &attachments1,
    const std::vector<wgpu::RenderPassColorAttachment> &attachments2)
{
    if (attachments1.size() != attachments2.size())
    {
        return false;
    }

    for (uint32_t i = 0; i < attachments1.size(); ++i)
    {
        if (!CompareColorRenderPassAttachments(attachments1[i], attachments2[i]))
        {
            return false;
        }
    }

    return true;
}

bool CompareDepthStencilRenderPassAttachments(
    const wgpu::RenderPassDepthStencilAttachment &attachment1,
    const wgpu::RenderPassDepthStencilAttachment &attachment2)
{
    return attachment1.view.Get() == attachment2.view.Get() &&
           attachment1.depthLoadOp == attachment2.depthLoadOp &&
           attachment1.depthStoreOp == attachment2.depthStoreOp &&
           attachment1.depthClearValue == attachment2.depthClearValue &&
           attachment1.stencilLoadOp == attachment2.stencilLoadOp &&
           attachment1.stencilStoreOp == attachment2.stencilStoreOp &&
           attachment1.stencilClearValue == attachment2.stencilClearValue &&
           attachment1.stencilReadOnly == attachment2.stencilReadOnly;
}
}  // namespace

FramebufferWgpu::FramebufferWgpu(const gl::FramebufferState &state) : FramebufferImpl(state)
{
    mCurrentColorAttachmentFormats.fill(wgpu::TextureFormat::Undefined);
}

FramebufferWgpu::~FramebufferWgpu() {}

angle::Result FramebufferWgpu::discard(const gl::Context *context,
                                       size_t count,
                                       const GLenum *attachments)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::invalidate(const gl::Context *context,
                                          size_t count,
                                          const GLenum *attachments)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::invalidateSub(const gl::Context *context,
                                             size_t count,
                                             const GLenum *attachments,
                                             const gl::Rectangle &area)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clear(const gl::Context *context, GLbitfield mask)
{
    bool clearColor   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    bool clearDepth   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    bool clearStencil = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));

    // TODO(anglebug.com/42267012): support clearing depth and stencil buffers.
    if (clearDepth || clearStencil)
    {
        UNIMPLEMENTED();
        return angle::Result::Continue;
    }

    ASSERT(clearColor);

    ContextWgpu *contextWgpu             = GetImplAs<ContextWgpu>(context);
    gl::ColorF colorClearValue           = context->getState().getColorClearValue();
    gl::DrawBufferMask clearColorBuffers = mState.getEnabledDrawBuffers();
    wgpu::Color clearValue;
    clearValue.r = colorClearValue.red;
    clearValue.g = colorClearValue.green;
    clearValue.b = colorClearValue.blue;
    clearValue.a = colorClearValue.alpha;
    std::vector<wgpu::RenderPassColorAttachment> colorAttachments;
    for (size_t enabledDrawBuffer : clearColorBuffers)
    {
        colorAttachments.push_back(webgpu::CreateNewClearColorAttachment(
            clearValue, wgpu::kDepthSliceUndefined,
            mRenderTargetCache.getColorDraw(mState, enabledDrawBuffer)->getTextureView()));
    }

    // Attempt to end a render pass if one has already been started.
    ANGLE_UNUSED_VARIABLE(CompareDepthStencilRenderPassAttachments);

    bool isActiveRenderPass =
        !CompareColorRenderPassAttachmentVectors(mCurrentColorAttachments, colorAttachments) ||
        contextWgpu->hasActiveRenderPass();

    // If there is not currently an active render pass, merge clears with the deferred clears. This
    // is to keep the clear paths simpler so they only need to consider the current or the deferred
    // clears.
    if (!isActiveRenderPass)
    {
        for (size_t enabledDrawBuffer : clearColorBuffers)
        {
            mDeferredClears.store(static_cast<uint32_t>(enabledDrawBuffer),
                                  {clearValue, wgpu::kDepthSliceUndefined});
        }
    }

    if (mDeferredClears.any())
    {
        if (isActiveRenderPass)
        {
            ANGLE_TRY(flushDeferredClears(contextWgpu));
        }
        else
        {
            for (size_t colorIndexGL : mDeferredClears.getColorMask())
            {
                RenderTargetWgpu *renderTarget =
                    mRenderTargetCache.getColorDraw(mState, colorIndexGL);
                renderTarget->getImage()->stageClear(
                    renderTarget->getImage()->toGlLevel(renderTarget->getLevelIndex()),
                    mDeferredClears[colorIndexGL]);
            }
            mDeferredClears.reset();
        }
        return angle::Result::Continue;
    }

    if (isActiveRenderPass)
    {
        ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

        mCurrentColorAttachments                    = std::move(colorAttachments);
        mCurrentRenderPassDesc.colorAttachmentCount = mCurrentColorAttachments.size();
        mCurrentRenderPassDesc.colorAttachments     = mCurrentColorAttachments.data();
    }

    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clearBufferfv(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             const GLfloat *values)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clearBufferuiv(const gl::Context *context,
                                              GLenum buffer,
                                              GLint drawbuffer,
                                              const GLuint *values)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clearBufferiv(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             const GLint *values)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::clearBufferfi(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             GLfloat depth,
                                             GLint stencil)
{
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::readPixels(const gl::Context *context,
                                          const gl::Rectangle &origArea,
                                          GLenum format,
                                          GLenum type,
                                          const gl::PixelPackState &pack,
                                          gl::Buffer *packBuffer,
                                          void *ptrOrOffset)
{
    // Get the pointer to write to from the argument or the pack buffer
    GLubyte *pixels = nullptr;
    if (packBuffer != nullptr)
    {
        UNREACHABLE();
    }
    else
    {
        pixels = reinterpret_cast<GLubyte *>(ptrOrOffset);
    }

    // Clip read area to framebuffer.
    const gl::Extents fbSize = getState().getReadPixelsAttachment(format)->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    gl::Rectangle clippedArea;
    if (!ClipRectangle(origArea, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    ANGLE_TRY(flushDeferredClears(contextWgpu));

    ANGLE_TRY(contextWgpu->flush(webgpu::RenderPassClosureReason::GLReadPixels));

    GLuint outputSkipBytes = 0;
    PackPixelsParams params;
    ANGLE_TRY(webgpu::ImageHelper::getReadPixelsParams(contextWgpu, pack, packBuffer, format, type,
                                                       origArea, clippedArea, &params,
                                                       &outputSkipBytes));

    webgpu::ImageHelper *sourceImageHelper = getReadPixelsRenderTarget()->getImage();
    ANGLE_TRY(sourceImageHelper->readPixels(contextWgpu, params.area, params,
                                            static_cast<uint8_t *>(pixels) + outputSkipBytes));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::blit(const gl::Context *context,
                                    const gl::Rectangle &sourceArea,
                                    const gl::Rectangle &destArea,
                                    GLbitfield mask,
                                    GLenum filter)
{
    return angle::Result::Continue;
}

gl::FramebufferStatus FramebufferWgpu::checkStatus(const gl::Context *context) const
{
    return gl::FramebufferStatus::Complete();
}

angle::Result FramebufferWgpu::syncState(const gl::Context *context,
                                         GLenum binding,
                                         const gl::Framebuffer::DirtyBits &dirtyBits,
                                         gl::Command command)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);

    ASSERT(dirtyBits.any());

    gl::DrawBufferMask dirtyColorAttachments;
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_DEPTH_BUFFER_CONTENTS:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_BUFFER_CONTENTS:
            {
                ANGLE_TRY(mRenderTargetCache.updateDepthStencilRenderTarget(context, mState));

                // Update the current depth stencil texture format let the context know if this
                // framebuffer is bound for draw
                RenderTargetWgpu *rt       = mRenderTargetCache.getDepthStencil();
                mCurrentDepthStencilFormat = (rt && rt->getImage())
                                                 ? rt->getImage()->toWgpuTextureFormat()
                                                 : wgpu::TextureFormat::Undefined;
                if (binding == GL_DRAW_FRAMEBUFFER)
                {
                    contextWgpu->setDepthStencilFormat(mCurrentDepthStencilFormat);
                }

                break;
            }

            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
                ANGLE_TRY(mRenderTargetCache.update(context, mState, dirtyBits));
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            case gl::Framebuffer::DIRTY_BIT_FRAMEBUFFER_SRGB_WRITE_CONTROL_MODE:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_LAYERS:
            case gl::Framebuffer::DIRTY_BIT_FOVEATION:
                break;
            default:
            {
                static_assert(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0, "FB dirty bits");
                uint32_t colorIndexGL;
                if (dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX)
                {
                    colorIndexGL = static_cast<uint32_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                }
                else
                {
                    ASSERT(dirtyBit >= gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0 &&
                           dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_MAX);
                    colorIndexGL = static_cast<uint32_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0);
                }

                ANGLE_TRY(
                    mRenderTargetCache.updateColorRenderTarget(context, mState, colorIndexGL));

                // Update the current color texture formats let the context know if this framebuffer
                // is bound for draw
                RenderTargetWgpu *rt = mRenderTargetCache.getColorDraw(mState, colorIndexGL);
                mCurrentColorAttachmentFormats[colorIndexGL] =
                    (rt && rt->getImage()) ? rt->getImage()->toWgpuTextureFormat()
                                           : wgpu::TextureFormat::Undefined;
                if (binding == GL_DRAW_FRAMEBUFFER)
                {
                    contextWgpu->setColorAttachmentFormat(
                        colorIndexGL, mCurrentColorAttachmentFormats[colorIndexGL]);
                }

                dirtyColorAttachments.set(colorIndexGL);
                break;
            }
        }
    }

    // Like in Vulkan, defer clears for draw framebuffer ops as well as clears to read framebuffer
    // attachments that are not taking part in a blit operation.
    const bool isBlitCommand = command >= gl::Command::Blit && command <= gl::Command::BlitAll;
    bool deferColorClears    = binding == GL_DRAW_FRAMEBUFFER;
    if (binding == GL_READ_FRAMEBUFFER && isBlitCommand)
    {
        uint32_t blitMask =
            static_cast<uint32_t>(command) - static_cast<uint32_t>(gl::Command::Blit);
        if ((blitMask & gl::CommandBlitBufferColor) == 0)
        {
            deferColorClears = true;
        }
    }

    ANGLE_TRY(flushColorAttachmentUpdates(context, dirtyColorAttachments, deferColorClears));

    // Notify the ContextWgpu to update the pipeline desc or restart the renderpass
    ANGLE_TRY(contextWgpu->onFramebufferChange(this, command));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::getSamplePosition(const gl::Context *context,
                                                 size_t index,
                                                 GLfloat *xy) const
{
    return angle::Result::Continue;
}

RenderTargetWgpu *FramebufferWgpu::getReadPixelsRenderTarget() const
{
    return mRenderTargetCache.getColorRead(mState);
}

void FramebufferWgpu::addNewColorAttachments(
    std::vector<wgpu::RenderPassColorAttachment> newColorAttachments)
{
    mNewColorAttachments.insert(mCurrentColorAttachments.end(), newColorAttachments.begin(),
                                newColorAttachments.end());
}

angle::Result FramebufferWgpu::flushOneColorAttachmentUpdate(const gl::Context *context,
                                                             bool deferClears,
                                                             uint32_t colorIndexGL)
{
    ContextWgpu *contextWgpu           = GetImplAs<ContextWgpu>(context);
    RenderTargetWgpu *drawRenderTarget = nullptr;
    RenderTargetWgpu *readRenderTarget = nullptr;

    drawRenderTarget = mRenderTargetCache.getColorDraw(mState, colorIndexGL);
    if (drawRenderTarget)
    {
        if (deferClears)
        {
            ANGLE_TRY(
                drawRenderTarget->flushStagedUpdates(contextWgpu, &mDeferredClears, colorIndexGL));
        }
        else
        {
            ANGLE_TRY(drawRenderTarget->flushStagedUpdates(contextWgpu));
        }
    }

    if (mState.getReadBufferState() != GL_NONE && mState.getReadIndex() == colorIndexGL)
    {
        readRenderTarget = mRenderTargetCache.getColorRead(mState);
        if (readRenderTarget && readRenderTarget != drawRenderTarget)
        {
            ANGLE_TRY(readRenderTarget->flushStagedUpdates(contextWgpu));
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::flushColorAttachmentUpdates(const gl::Context *context,
                                                           gl::DrawBufferMask dirtyColorAttachments,
                                                           bool deferClears)
{
    for (size_t colorIndexGL : dirtyColorAttachments)
    {
        ANGLE_TRY(flushOneColorAttachmentUpdate(context, deferClears,
                                                static_cast<uint32_t>(colorIndexGL)));
    }

    // If we added any new color attachments, we start a render pass to fully flush the updates.
    if (!mNewColorAttachments.empty() != mCurrentColorAttachments.size())
    {
        ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);
        // Flush out a render pass if there is an active one.
        ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

        mCurrentColorAttachments = mNewColorAttachments;
        mNewColorAttachments.clear();
        mCurrentRenderPassDesc.colorAttachmentCount = mCurrentColorAttachments.size();
        mCurrentRenderPassDesc.colorAttachments     = mCurrentColorAttachments.data();
        ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    }
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::flushDeferredClears(ContextWgpu *contextWgpu)
{
    if (mDeferredClears.empty())
    {
        return angle::Result::Continue;
    }
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));
    mCurrentColorAttachments.clear();
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        if (!mDeferredClears.test(colorIndexGL))
        {
            continue;
        }
        mCurrentColorAttachments.push_back(webgpu::CreateNewClearColorAttachment(
            mDeferredClears[colorIndexGL].clearColor, mDeferredClears[colorIndexGL].depthSlice,
            mRenderTargetCache.getColorDraw(mState, colorIndexGL)->getTextureView()));
    }
    mCurrentRenderPassDesc.colorAttachmentCount = mCurrentColorAttachments.size();
    mCurrentRenderPassDesc.colorAttachments     = mCurrentColorAttachments.data();
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::startNewRenderPass(ContextWgpu *contextWgpu)
{
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    mCurrentColorAttachments.clear();
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        wgpu::RenderPassColorAttachment colorAttachment;
        colorAttachment.view =
            mRenderTargetCache.getColorDraw(mState, colorIndexGL)->getTextureView();
        colorAttachment.depthSlice = wgpu::kDepthSliceUndefined;
        colorAttachment.loadOp     = wgpu::LoadOp::Load;
        colorAttachment.storeOp    = wgpu::StoreOp::Store;

        mCurrentColorAttachments.push_back(colorAttachment);
    }
    mCurrentRenderPassDesc.colorAttachmentCount = mCurrentColorAttachments.size();
    mCurrentRenderPassDesc.colorAttachments     = mCurrentColorAttachments.data();
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));

    return angle::Result::Continue;
}

}  // namespace rx
