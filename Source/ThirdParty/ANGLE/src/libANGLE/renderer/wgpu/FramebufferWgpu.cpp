//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferWgpu.cpp:
//    Implements the class methods for FramebufferWgpu.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"

#include <__config>

#include "common/Color.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/RenderTargetWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"
#include "webgpu/webgpu.h"

namespace rx
{

FramebufferWgpu::FramebufferWgpu(const gl::FramebufferState &state) : FramebufferImpl(state)
{
    mCurrentColorAttachmentFormats.fill(WGPUTextureFormat_Undefined);
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
    const bool clearColor   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    const bool clearDepth   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    const bool clearStencil = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));

    gl::ColorF clearColorValue = context->getState().getColorClearValue();
    gl::DrawBufferMask clearColorBuffers =
        clearColor ? mState.getEnabledDrawBuffers() : gl::DrawBufferMask();

    uint32_t clearStencilValue = static_cast<uint32_t>(context->getState().getStencilClearValue());

    float clearDepthValue = context->getState().getDepthClearValue();

    return clearImpl(context, clearColorBuffers, clearDepth, clearStencil, clearColorValue,
                     clearDepthValue, clearStencilValue);
}

gl::ColorF FramebufferWgpu::getClearColorWithCorrectAlpha(const gl::ColorF &clearValue,
                                                          size_t drawBufferIndex)
{
    webgpu::ImageHelper *colorImage = mRenderTargetCache.getColors()[drawBufferIndex]->getImage();
    const angle::Format &dstIntendedFormat = angle::Format::Get(colorImage->getIntendedFormatID());
    const angle::Format &dstActualFormat   = angle::Format::Get(colorImage->getActualFormatID());
    // If the intended format does not have alpha bits, but the texture is backed by an actual
    // format with alpha bits, ensure the alpha bits are cleared to 1.0.
    if (dstIntendedFormat.alphaBits == 0 && dstActualFormat.alphaBits != 0)
    {
        return gl::ColorF(clearValue.red, clearValue.green, clearValue.blue, 1.0);
    }

    return clearValue;
}

angle::Result FramebufferWgpu::clearImpl(const gl::Context *context,
                                         gl::DrawBufferMask clearColorBuffers,
                                         bool clearDepth,
                                         bool clearStencil,
                                         const gl::ColorF &clearColorValue,
                                         float clearDepthValue,
                                         uint32_t clearStencilValue)
{
    const bool clearColor = clearColorBuffers.any();

    ASSERT(clearDepth || clearStencil || clearColor);

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    // This function assumes that only enabled attachments are asked to be cleared.
    ASSERT((clearColorBuffers & mState.getEnabledDrawBuffers()) == clearColorBuffers);
    ASSERT(!clearDepth || mState.getDepthAttachment() != nullptr);
    ASSERT(!clearStencil || mState.getStencilAttachment() != nullptr);

    // The front-end should ensure we don't attempt to clear color if all channels are masked.
    ASSERT(!clearColor || context->getState().getBlendStateExt().getColorMaskBits() != 0);
    // The front-end should ensure we don't attempt to clear depth if depth write is disabled.
    ASSERT(!clearDepth || context->getState().getDepthStencilState().depthMask);
    // The front-end should ensure we don't attempt to clear stencil if all bits are masked.
    ASSERT(!clearStencil ||
           static_cast<uint8_t>(context->getState().getDepthStencilState().stencilWritemask) != 0);

    gl::Rectangle renderArea(0, 0, mState.getDimensions().width, mState.getDimensions().height);
    gl::Rectangle scissoredRenderArea = ClipRectToScissor(context->getState(), renderArea, false);
    if (scissoredRenderArea.empty())
    {
        return angle::Result::Continue;
    }
    const bool scissoredClear = scissoredRenderArea != renderArea;
    // TODO(anglebug.com/474131922): could avoid a clearWithDraw if a masked out channel is not
    // present in the `internalFormat` that's being cleared. Vulkan does this.
    const bool maskedClearColor =
        clearColor && (context->getState().getBlendStateExt().getColorMaskBits() !=
                       context->getState().getBlendStateExt().getAllColorMaskBits());
    GLuint allStencilBits =
        angle::BitMask<GLuint>(context->getState().getDrawFramebuffer()->getStencilBitCount());
    const bool maskedClearStencil =
        clearStencil && ((context->getState().getDepthStencilState().stencilWritemask &
                          allStencilBits) != allStencilBits);
    const bool clearWithDraw = scissoredClear || maskedClearColor || maskedClearStencil;

    if (clearWithDraw)
    {
        // Flush any deferred clears so that they do not overwrite this clearWithDraw.
        // TODO(anglebug.com/474131922): in the future this should just start a render pass for the
        // draw call to be added to.
        ANGLE_TRY(flushDeferredClears(contextWgpu));

        // If a scissor, need to flip the clearArea if this framebuffer is flipped.
        if (mFlipY)
        {
            scissoredRenderArea.y =
                mState.getDimensions().height - scissoredRenderArea.y - scissoredRenderArea.height;
        }

        webgpu::UtilsWgpu::ClearParams clearParams{
            .clearArea         = scissoredRenderArea,
            .colorMasks        = context->getState().getBlendStateExt().getColorMaskBits(),
            .clearColorBuffers = clearColor ? clearColorBuffers : gl::DrawBufferMask(),
            // RGB textures backed by the RGBA format will have their alpha cleared to 1.0 by the
            // draw.
            .clearColorValue =
                clearColor ? std::optional<gl::ColorF>(clearColorValue) : std::nullopt,
            .clearDepthValue = clearDepth ? std::optional<float>(clearDepthValue) : std::nullopt,
            .clearStencilValue =
                clearStencil ? std::optional<uint32_t>(clearStencilValue) : std::nullopt,
            .stencilWriteMask =
                clearStencil ? std::optional<uint32_t>(
                                   context->getState().getDepthStencilState().stencilWritemask)
                             : std::nullopt,
            .colorTargets = clearColor ? &mRenderTargetCache.getColors() : nullptr,
            .depthStencilTarget =
                clearDepth || clearStencil ? mRenderTargetCache.getDepthStencil() : nullptr,
        };

        return contextWgpu->getUtils()->clear(contextWgpu, std::move(clearParams));
    }

    webgpu::PackedRenderPassDescriptor clearRenderPassDesc;

    for (size_t enabledDrawBuffer : clearColorBuffers)
    {
        clearRenderPassDesc.colorAttachments.push_back(webgpu::CreateNewClearColorAttachment(
            getClearColorWithCorrectAlpha(clearColorValue, enabledDrawBuffer),
            WGPU_DEPTH_SLICE_UNDEFINED,
            mRenderTargetCache.getColorDraw(mState, enabledDrawBuffer)->getTextureView()));
    }

    if (clearDepth || clearStencil)
    {
        clearRenderPassDesc.depthStencilAttachment = webgpu::CreateNewClearDepthStencilAttachment(
            clearDepthValue, clearStencilValue,
            mRenderTargetCache.getDepthStencil()->getTextureView(), clearDepth, clearStencil);
    }

    // Attempt to end a render pass if one has already been started.
    bool isActiveRenderPass =
        mCurrentRenderPassDesc != clearRenderPassDesc || contextWgpu->hasActiveRenderPass();

    if (mDeferredClears.any())
    {
        // Merge the current clear command with any deferred clears. This is to keep the clear paths
        // simpler so they only need to consider the current or the deferred clears.
        mergeClearWithDeferredClears(clearColorValue, clearColorBuffers, clearDepthValue,
                                     clearStencilValue, clearColor, clearDepth, clearStencil);
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
                webgpu::ClearValues deferredClearValue;
                deferredClearValue = mDeferredClears[colorIndexGL];
                renderTarget->getImage()->stageClear(renderTarget->getGlLevel(), deferredClearValue,
                                                     false, false);
            }
            if (mDeferredClears.hasDepth() || mDeferredClears.hasStencil())
            {
                webgpu::ClearValues dsClearValue = {};
                dsClearValue.depthValue          = mDeferredClears.getDepthValue();
                dsClearValue.stencilValue        = mDeferredClears.getStencilValue();
                RenderTargetWgpu *renderTarget   = mRenderTargetCache.getDepthStencil();
                renderTarget->getImage()->stageClear(renderTarget->getGlLevel(), dsClearValue,
                                                     mDeferredClears.hasDepth(),
                                                     mDeferredClears.hasStencil());
            }
            mDeferredClears.reset();
        }
        return angle::Result::Continue;
    }

    if (isActiveRenderPass)
    {
        ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));
    }

    mCurrentRenderPassDesc = std::move(clearRenderPassDesc);
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
    gl::Rectangle flippedArea = getReadArea(context, clippedArea);

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    ANGLE_TRY(flushDeferredClears(contextWgpu));

    GLuint outputSkipBytes = 0;
    PackPixelsParams params;
    ANGLE_TRY(webgpu::ImageHelper::getReadPixelsParams(contextWgpu, pack, packBuffer, format, type,
                                                       origArea, clippedArea, &params,
                                                       &outputSkipBytes));

    if (mFlipY)
    {
        params.reverseRowOrder = !params.reverseRowOrder;
    }

    // Does not handle reading from depth/stencil buffer(s).
    ASSERT(format != GL_DEPTH_COMPONENT && format != GL_STENCIL_INDEX);

    RenderTargetWgpu *readRT = getReadPixelsRenderTarget();
    uint32_t layer           = readRT->getLayer();

    webgpu::ImageHelper *sourceImageHelper = readRT->getImage();
    ANGLE_TRY(sourceImageHelper->readPixels(contextWgpu, flippedArea, params,
                                            readRT->getLevelIndex(), layer,
                                            static_cast<uint8_t *>(pixels) + outputSkipBytes));

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::blit(const gl::Context *context,
                                    const gl::Rectangle &sourceArea,
                                    const gl::Rectangle &destArea,
                                    GLbitfield mask,
                                    GLenum filter)
{
    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);
    bool blitColor           = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    bool blitDepth           = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    bool blitStencil         = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));

    const gl::Framebuffer *readFBO = context->getState().getReadFramebuffer();
    FramebufferWgpu *readFBOWgpu   = GetImplAs<FramebufferWgpu>(readFBO);
    bool srcFlipY                  = readFBOWgpu->flipY();
    bool dstFlipY                  = flipY();

    if (blitColor)
    {
        RenderTargetWgpu *readRenderTarget = readFBOWgpu->getReadPixelsRenderTarget();
        ASSERT(readRenderTarget);

        const gl::DrawBufferMask &drawBufferMask = mState.getEnabledDrawBuffers();
        for (size_t drawBufferIdx : drawBufferMask)
        {
            RenderTargetWgpu *drawRenderTarget =
                mRenderTargetCache.getColorDraw(mState, drawBufferIdx);
            ASSERT(drawRenderTarget);

            if (formatsAndSizesMatchForDirectCopy(context, readFBOWgpu, readRenderTarget,
                                                  drawRenderTarget, sourceArea, destArea))
            {
                ANGLE_TRY(blitWithDirectCopy(contextWgpu, readRenderTarget, drawRenderTarget,
                                             sourceArea, destArea, srcFlipY, dstFlipY,
                                             WGPUTextureAspect_All));
            }
            else
            {
                UNIMPLEMENTED();
            }
        }
    }

    if (blitDepth || blitStencil)
    {
        RenderTargetWgpu *readRT = readFBOWgpu->mRenderTargetCache.getDepthStencil();
        RenderTargetWgpu *drawRT = mRenderTargetCache.getDepthStencil();

        if (readRT && drawRT)
        {
            WGPUTextureAspect aspect = WGPUTextureAspect_All;
            if (blitDepth && blitStencil)
            {
                aspect = WGPUTextureAspect_All;
            }
            else if (blitDepth)
            {
                aspect = WGPUTextureAspect_DepthOnly;
            }
            else if (blitStencil)
            {
                aspect = WGPUTextureAspect_StencilOnly;
            }

            if (formatsAndSizesMatchForDirectCopy(context, readFBOWgpu, readRT, drawRT, sourceArea,
                                                  destArea))
            {
                ANGLE_TRY(blitWithDirectCopy(contextWgpu, readRT, drawRT, sourceArea, destArea,
                                             srcFlipY, dstFlipY, aspect));
            }
            else
            {
                UNIMPLEMENTED();
            }
        }
    }

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
    ContextWgpu *contextWgpu         = webgpu::GetImpl(context);
    bool dirtyDepthStencilAttachment = false;
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
                dirtyDepthStencilAttachment = true;
                // Update the current depth stencil texture format let the context know if this
                // framebuffer is bound for draw
                RenderTargetWgpu *rt       = mRenderTargetCache.getDepthStencil();
                mCurrentDepthStencilFormat = (rt && rt->getImage())
                                                 ? rt->getImage()->toWgpuTextureFormat()
                                                 : WGPUTextureFormat_Undefined;
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
                                           : WGPUTextureFormat_Undefined;
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
    ASSERT(mDeferredClears.empty());

    const bool isBlitCommand     = command >= gl::Command::Blit && command <= gl::Command::BlitAll;
    bool deferColorClears        = binding == GL_DRAW_FRAMEBUFFER;
    bool deferDepthStencilClears = binding == GL_DRAW_FRAMEBUFFER;
    if (binding == GL_READ_FRAMEBUFFER && isBlitCommand)
    {
        uint32_t blitMask =
            static_cast<uint32_t>(command) - static_cast<uint32_t>(gl::Command::Blit);
        if ((blitMask & gl::CommandBlitBufferColor) == 0)
        {
            deferColorClears = true;
        }
        if ((blitMask & (gl::CommandBlitBufferDepth | gl::CommandBlitBufferStencil)) == 0)
        {
            deferDepthStencilClears = true;
        }
    }

    ANGLE_TRY(flushAttachmentUpdates(context, dirtyColorAttachments, dirtyDepthStencilAttachment,
                                     deferColorClears, deferDepthStencilClears));

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
    std::vector<webgpu::PackedRenderPassColorAttachment> newColorAttachments)
{
    if (!mNewRenderPassDesc.has_value())
    {
        mNewRenderPassDesc = webgpu::PackedRenderPassDescriptor();
    }

    for (webgpu::PackedRenderPassColorAttachment &colorAttachment : newColorAttachments)
    {
        mNewRenderPassDesc->colorAttachments.push_back(colorAttachment);
    }
}

void FramebufferWgpu::updateDepthStencilAttachment(
    webgpu::PackedRenderPassDepthStencilAttachment newRenderPassDepthStencilAttachment)
{
    if (!mNewRenderPassDesc.has_value())
    {
        mNewRenderPassDesc = webgpu::PackedRenderPassDescriptor();
    }
    mNewRenderPassDesc->depthStencilAttachment = std::move(newRenderPassDepthStencilAttachment);
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
            ANGLE_TRY(drawRenderTarget->flushImageStagedUpdates(contextWgpu, &mDeferredClears,
                                                                colorIndexGL));
        }
        else
        {
            ANGLE_TRY(drawRenderTarget->flushImageStagedUpdates(contextWgpu, nullptr, 0));
        }
    }

    if (mState.getReadBufferState() != GL_NONE && mState.getReadIndex() == colorIndexGL)
    {
        readRenderTarget = mRenderTargetCache.getColorRead(mState);
        if (readRenderTarget && readRenderTarget != drawRenderTarget)
        {
            ANGLE_TRY(readRenderTarget->flushImageStagedUpdates(contextWgpu, nullptr, 0));
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::flushAttachmentUpdates(const gl::Context *context,
                                                      gl::DrawBufferMask dirtyColorAttachments,
                                                      bool dirtyDepthStencilAttachment,
                                                      bool deferColorClears,
                                                      bool deferDepthStencilClears)
{
    for (size_t colorIndexGL : dirtyColorAttachments)
    {
        ANGLE_TRY(flushOneColorAttachmentUpdate(context, deferColorClears,
                                                static_cast<uint32_t>(colorIndexGL)));
    }

    ContextWgpu *contextWgpu         = GetImplAs<ContextWgpu>(context);
    RenderTargetWgpu *depthStencilRt = mRenderTargetCache.getDepthStencil();

    if (depthStencilRt && dirtyDepthStencilAttachment)
    {
        if (deferDepthStencilClears)
        {
            // The underlying ImageHelper will check if a clear has a stencil value and store the
            // deferred clear in the correct index.
            ANGLE_TRY(depthStencilRt->flushImageStagedUpdates(contextWgpu, &mDeferredClears,
                                                              webgpu::kUnpackedDepthIndex));
        }
        else
        {
            ANGLE_TRY(depthStencilRt->flushImageStagedUpdates(contextWgpu, nullptr, 0));
        }
    }

    // If we added any new attachments, we start a render pass to fully flush the updates.
    if (mNewRenderPassDesc.has_value())
    {
        ANGLE_TRY(startRenderPassNewAttachments(contextWgpu));
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

    webgpu::PackedRenderPassDescriptor clearRenderPassDesc;
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        if (!mDeferredClears.test(colorIndexGL))
        {
            continue;
        }
        clearRenderPassDesc.colorAttachments.push_back(webgpu::CreateNewClearColorAttachment(
            mDeferredClears[colorIndexGL].clearColor, mDeferredClears[colorIndexGL].depthSlice,
            mRenderTargetCache.getColorDraw(mState, colorIndexGL)->getTextureView()));
    }
    if (mRenderTargetCache.getDepthStencil() &&
        (mDeferredClears.hasDepth() || mDeferredClears.hasStencil()))
    {
        clearRenderPassDesc.depthStencilAttachment = webgpu::CreateNewClearDepthStencilAttachment(
            mDeferredClears.getDepthValue(), mDeferredClears.getStencilValue(),
            mRenderTargetCache.getDepthStencil()->getTextureView(), !mDeferredClears.hasDepth(),
            !mDeferredClears.hasStencil());
    }

    mCurrentRenderPassDesc = std::move(clearRenderPassDesc);
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    mDeferredClears.reset();

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::startRenderPassNewAttachments(ContextWgpu *contextWgpu)
{
    // Flush out a render pass if there is an active one.
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    ASSERT(mNewRenderPassDesc.has_value());
    mCurrentRenderPassDesc = std::move(mNewRenderPassDesc.value());
    mNewRenderPassDesc.reset();

    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));
    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::startNewRenderPass(ContextWgpu *contextWgpu)
{
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));

    webgpu::PackedRenderPassDescriptor newRenderPass;
    for (size_t colorIndexGL : mState.getColorAttachmentsMask())
    {
        webgpu::PackedRenderPassColorAttachment colorAttachment;
        colorAttachment.view =
            mRenderTargetCache.getColorDraw(mState, colorIndexGL)->getTextureView();
        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAttachment.storeOp    = WGPUStoreOp_Store;

        if (mDeferredClears.test(colorIndexGL))
        {
            colorAttachment.loadOp     = WGPULoadOp_Clear;
            colorAttachment.clearValue = mDeferredClears[colorIndexGL].clearColor;
            mDeferredClears.reset(colorIndexGL);
        }
        else
        {
            colorAttachment.loadOp = WGPULoadOp_Load;
        }

        newRenderPass.colorAttachments.push_back(colorAttachment);
    }
    if (mRenderTargetCache.getDepthStencil())
    {
        webgpu::PackedRenderPassDepthStencilAttachment dsAttachment =
            webgpu::CreateNewDepthStencilAttachment(
                mRenderTargetCache.getDepthStencil()->getTextureView(), mState.hasDepth(),
                mState.hasStencil());

        if (mDeferredClears.hasDepth())
        {
            dsAttachment.depthLoadOp     = WGPULoadOp_Clear;
            dsAttachment.depthClearValue = mDeferredClears.getDepthValue();
            mDeferredClears.reset(webgpu::kUnpackedDepthIndex);
        }
        if (mDeferredClears.hasStencil())
        {
            dsAttachment.stencilLoadOp     = WGPULoadOp_Clear;
            dsAttachment.stencilClearValue = mDeferredClears.getStencilValue();
            mDeferredClears.reset(webgpu::kUnpackedStencilIndex);
        }

        newRenderPass.depthStencilAttachment = dsAttachment;
    }

    mCurrentRenderPassDesc = std::move(newRenderPass);
    ANGLE_TRY(contextWgpu->startRenderPass(mCurrentRenderPassDesc));

    return angle::Result::Continue;
}

void FramebufferWgpu::mergeClearWithDeferredClears(const gl::ColorF &clearValue,
                                                   gl::DrawBufferMask clearColorBuffers,
                                                   float depthValue,
                                                   uint32_t stencilValue,
                                                   bool clearColor,
                                                   bool clearDepth,
                                                   bool clearStencil)
{
    for (size_t enabledDrawBuffer : clearColorBuffers)
    {
        mDeferredClears.store(static_cast<uint32_t>(enabledDrawBuffer),
                              {getClearColorWithCorrectAlpha(clearValue, enabledDrawBuffer),
                               WGPU_DEPTH_SLICE_UNDEFINED, 0, 0});
    }
    if (clearDepth)
    {
        mDeferredClears.store(webgpu::kUnpackedDepthIndex,
                              {gl::ColorF(), WGPU_DEPTH_SLICE_UNDEFINED, depthValue, 0});
    }
    if (clearStencil)
    {
        mDeferredClears.store(webgpu::kUnpackedStencilIndex,
                              {gl::ColorF(), WGPU_DEPTH_SLICE_UNDEFINED, 0, stencilValue});
    }
}

gl::Rectangle FramebufferWgpu::getReadArea(const gl::Context *context,
                                           const gl::Rectangle &glArea) const
{
    RenderTargetWgpu *readRT = getReadPixelsRenderTarget();
    if (!readRT)
    {
        readRT = mRenderTargetCache.getDepthStencil();
    }
    ASSERT(readRT);
    gl::Rectangle flippedArea = glArea;
    if (mFlipY)
    {
        flippedArea.y = readRT->getImage()->getLevelSize(readRT->getLevelIndex()).height -
                        flippedArea.y - flippedArea.height;
    }

    return flippedArea;
}

bool FramebufferWgpu::formatsAndSizesMatchForDirectCopy(const gl::Context *context,
                                                        const FramebufferWgpu *readFramebuffer,
                                                        RenderTargetWgpu *readRenderTarget,
                                                        RenderTargetWgpu *drawRenderTarget,
                                                        const gl::Rectangle &sourceArea,
                                                        const gl::Rectangle &destArea) const
{
    bool isScissorEnabled = context->getState().isScissorTestEnabled();
    bool scissorMatches = !isScissorEnabled || context->getState().getScissor().encloses(destArea);
    bool geometryMatches =
        sourceArea.width == destArea.width && sourceArea.height == destArea.height;
    bool flipsMatch = readFramebuffer->flipY() == flipY();

    webgpu::ImageHelper *srcImage = readRenderTarget->getImage();
    webgpu::ImageHelper *dstImage = drawRenderTarget->getImage();

    bool formatsMatch      = srcImage->getActualFormatID() == dstImage->getActualFormatID();
    bool srcIsMultisampled = srcImage->getSamples() > 1;

    WGPUExtent3D srcLevelSize =
        srcImage->getLevelSize(srcImage->toWgpuLevel(readRenderTarget->getGlLevel()));
    WGPUExtent3D dstLevelSize =
        dstImage->getLevelSize(dstImage->toWgpuLevel(drawRenderTarget->getGlLevel()));

    auto isWithinBounds = [](const gl::Rectangle &rect, const WGPUExtent3D &size) {
        return rect.x >= 0 && rect.y >= 0 && rect.width >= 0 && rect.height >= 0 &&
               static_cast<uint32_t>(rect.x + rect.width) <= size.width &&
               static_cast<uint32_t>(rect.y + rect.height) <= size.height;
    };

    bool boundsMatch =
        isWithinBounds(sourceArea, srcLevelSize) && isWithinBounds(destArea, dstLevelSize);

    return scissorMatches && geometryMatches && flipsMatch && formatsMatch && !srcIsMultisampled &&
           boundsMatch;
}

angle::Result FramebufferWgpu::blitWithDirectCopy(ContextWgpu *contextWgpu,
                                                  RenderTargetWgpu *readRenderTarget,
                                                  RenderTargetWgpu *drawRenderTarget,
                                                  const gl::Rectangle &sourceArea,
                                                  const gl::Rectangle &destArea,
                                                  bool srcFlipY,
                                                  bool dstFlipY,
                                                  WGPUTextureAspect aspect)
{
    webgpu::ImageHelper *srcImage = readRenderTarget->getImage();
    webgpu::ImageHelper *dstImage = drawRenderTarget->getImage();

    ANGLE_TRY(srcImage->flushStagedUpdates(contextWgpu));
    ANGLE_TRY(dstImage->flushStagedUpdates(contextWgpu));

    WGPUExtent3D srcLevelSize =
        srcImage->getLevelSize(srcImage->toWgpuLevel(readRenderTarget->getGlLevel()));
    WGPUExtent3D dstLevelSize =
        dstImage->getLevelSize(dstImage->toWgpuLevel(drawRenderTarget->getGlLevel()));

    gl::Box sourceBox(sourceArea.x, sourceArea.y, 0, sourceArea.width, sourceArea.height, 1);
    if (srcFlipY)
    {
        sourceBox.y = srcLevelSize.height - sourceArea.y - sourceArea.height;
    }

    gl::Offset dstOffset(destArea.x, destArea.y, 0);
    if (dstFlipY)
    {
        dstOffset.y = dstLevelSize.height - destArea.y - destArea.height;
    }
    dstOffset.z = drawRenderTarget->getLayer();

    gl::ImageIndex dstIndex = gl::ImageIndex::Make2D(drawRenderTarget->getGlLevel().get());

    ANGLE_TRY(dstImage->CopyImage(contextWgpu, srcImage, dstIndex, dstOffset,
                                  readRenderTarget->getGlLevel(), readRenderTarget->getLayer(),
                                  sourceBox, aspect));

    return angle::Result::Continue;
}

}  // namespace rx
