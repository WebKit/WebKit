//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferMtl.mm:
//    Implements the class methods for FramebufferMtl.
//

#include "libANGLE/renderer/metal/ContextMtl.h"

#include <TargetConditionals.h>

#include "common/MemoryBuffer.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/FrameBufferMtl.h"
#include "libANGLE/renderer/metal/SurfaceMtl.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "libANGLE/renderer/renderer_utils.h"

namespace rx
{
// FramebufferMtl implementation
FramebufferMtl::FramebufferMtl(const gl::FramebufferState &state,
                               bool flipY,
                               WindowSurfaceMtl *backbuffer)
    : FramebufferImpl(state), mBackbuffer(backbuffer), mFlipY(flipY)
{
    reset();
}

FramebufferMtl::~FramebufferMtl() {}

void FramebufferMtl::reset()
{
    for (auto &rt : mColorRenderTargets)
    {
        rt = nullptr;
    }
    mDepthRenderTarget = mStencilRenderTarget = nullptr;
}

void FramebufferMtl::destroy(const gl::Context *context)
{
    reset();
}

angle::Result FramebufferMtl::discard(const gl::Context *context,
                                      size_t count,
                                      const GLenum *attachments)
{
    return invalidate(context, count, attachments);
}

angle::Result FramebufferMtl::invalidate(const gl::Context *context,
                                         size_t count,
                                         const GLenum *attachments)
{
    return invalidateImpl(mtl::GetImpl(context), count, attachments);
}

angle::Result FramebufferMtl::invalidateSub(const gl::Context *context,
                                            size_t count,
                                            const GLenum *attachments,
                                            const gl::Rectangle &area)
{
    // NOTE(hqle): ES 3.0 feature.
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

angle::Result FramebufferMtl::clear(const gl::Context *context, GLbitfield mask)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    mtl::ClearRectParams clearOpts;

    bool clearColor   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));
    bool clearDepth   = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_DEPTH_BUFFER_BIT));
    bool clearStencil = IsMaskFlagSet(mask, static_cast<GLbitfield>(GL_STENCIL_BUFFER_BIT));

    gl::DrawBufferMask clearColorBuffers;
    if (clearColor)
    {
        clearColorBuffers    = mState.getEnabledDrawBuffers();
        clearOpts.clearColor = contextMtl->getClearColorValue();
    }
    if (clearDepth)
    {
        clearOpts.clearDepth = contextMtl->getClearDepthValue();
    }
    if (clearStencil)
    {
        clearOpts.clearStencil = contextMtl->getClearStencilValue();
    }

    return clearImpl(context, clearColorBuffers, &clearOpts);
}

angle::Result FramebufferMtl::clearBufferfv(const gl::Context *context,
                                            GLenum buffer,
                                            GLint drawbuffer,
                                            const GLfloat *values)
{
    // NOTE(hqle): ES 3.0 feature.
    UNIMPLEMENTED();
    return angle::Result::Stop;
}
angle::Result FramebufferMtl::clearBufferuiv(const gl::Context *context,
                                             GLenum buffer,
                                             GLint drawbuffer,
                                             const GLuint *values)
{
    // NOTE(hqle): ES 3.0 feature.
    UNIMPLEMENTED();
    return angle::Result::Stop;
}
angle::Result FramebufferMtl::clearBufferiv(const gl::Context *context,
                                            GLenum buffer,
                                            GLint drawbuffer,
                                            const GLint *values)
{
    // NOTE(hqle): ES 3.0 feature.
    UNIMPLEMENTED();
    return angle::Result::Stop;
}
angle::Result FramebufferMtl::clearBufferfi(const gl::Context *context,
                                            GLenum buffer,
                                            GLint drawbuffer,
                                            GLfloat depth,
                                            GLint stencil)
{
    // NOTE(hqle): ES 3.0 feature.
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

const gl::InternalFormat &FramebufferMtl::getImplementationColorReadFormat(
    const gl::Context *context) const
{
    ContextMtl *contextMtl   = mtl::GetImpl(context);
    GLenum sizedFormat       = mState.getReadAttachment()->getFormat().info->sizedInternalFormat;
    angle::FormatID formatID = angle::Format::InternalFormatToID(sizedFormat);
    const mtl::Format &mtlFormat = contextMtl->getDisplay()->getPixelFormat(formatID);
    GLenum implFormat            = mtlFormat.actualAngleFormat().fboImplementationInternalFormat;
    return gl::GetSizedInternalFormatInfo(implFormat);
}

angle::Result FramebufferMtl::readPixels(const gl::Context *context,
                                         const gl::Rectangle &area,
                                         GLenum format,
                                         GLenum type,
                                         const gl::PixelPackState &pack,
                                         gl::Buffer *packBuffer,
                                         void *pixels)
{
    // Clip read area to framebuffer.
    const gl::Extents &fbSize = getState().getReadAttachment()->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);

    gl::Rectangle clippedArea;
    if (!ClipRectangle(area, fbRect, &clippedArea))
    {
        // nothing to read
        return angle::Result::Continue;
    }
    gl::Rectangle flippedArea = getCorrectFlippedReadArea(context, clippedArea);

    ContextMtl *contextMtl = mtl::GetImpl(context);

    const gl::InternalFormat &sizedFormatInfo = gl::GetInternalFormatInfo(format, type);

    GLuint outputPitch = 0;
    ANGLE_CHECK_GL_MATH(contextMtl,
                        sizedFormatInfo.computeRowPitch(type, area.width, pack.alignment,
                                                        pack.rowLength, &outputPitch));
    GLuint outputSkipBytes = 0;
    ANGLE_CHECK_GL_MATH(contextMtl, sizedFormatInfo.computeSkipBytes(type, outputPitch, 0, pack,
                                                                     false, &outputSkipBytes));

    outputSkipBytes += (clippedArea.x - area.x) * sizedFormatInfo.pixelBytes +
                       (clippedArea.y - area.y) * outputPitch;

    const angle::Format &angleFormat = GetFormatFromFormatType(format, type);

    PackPixelsParams params(flippedArea, angleFormat, outputPitch, pack.reverseRowOrder, packBuffer,
                            0);
    if (mFlipY)
    {
        params.reverseRowOrder = !params.reverseRowOrder;
    }

    ANGLE_TRY(readPixelsImpl(context, flippedArea, params, getColorReadRenderTarget(context),
                             static_cast<uint8_t *>(pixels) + outputSkipBytes));

    return angle::Result::Continue;
}

angle::Result FramebufferMtl::blit(const gl::Context *context,
                                   const gl::Rectangle &sourceArea,
                                   const gl::Rectangle &destArea,
                                   GLbitfield mask,
                                   GLenum filter)
{
    // NOTE(hqle): MSAA feature.
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

bool FramebufferMtl::checkStatus(const gl::Context *context) const
{
    if (!mState.attachmentsHaveSameDimensions())
    {
        return false;
    }

    ContextMtl *contextMtl = mtl::GetImpl(context);
    if (!contextMtl->getDisplay()->getFeatures().allowSeparatedDepthStencilBuffers.enabled &&
        mState.hasSeparateDepthAndStencilAttachments())
    {
        return false;
    }

    return true;
}

angle::Result FramebufferMtl::syncState(const gl::Context *context,
                                        GLenum binding,
                                        const gl::Framebuffer::DirtyBits &dirtyBits)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    ASSERT(dirtyBits.any());
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
                ANGLE_TRY(updateDepthRenderTarget(context));
                break;
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
                ANGLE_TRY(updateStencilRenderTarget(context));
                break;
            case gl::Framebuffer::DIRTY_BIT_DEPTH_BUFFER_CONTENTS:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_BUFFER_CONTENTS:
                // NOTE(hqle): What are we supposed to do?
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_WIDTH:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_HEIGHT:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_SAMPLES:
            case gl::Framebuffer::DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS:
                break;
            default:
            {
                static_assert(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0, "FB dirty bits");
                if (dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX)
                {
                    size_t colorIndexGL = static_cast<size_t>(
                        dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                    ANGLE_TRY(updateColorRenderTarget(context, colorIndexGL));
                }
                else
                {
                    ASSERT(dirtyBit >= gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_0 &&
                           dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_BUFFER_CONTENTS_MAX);
                    // NOTE: might need to notify context.
                }
                break;
            }
        }
    }

    auto oldRenderPassDesc = mRenderPassDesc;

    ANGLE_TRY(prepareRenderPass(context, &mRenderPassDesc));

    if (!oldRenderPassDesc.equalIgnoreLoadStoreOptions(mRenderPassDesc))
    {
        FramebufferMtl *currentDrawFramebuffer =
            mtl::GetImpl(context->getState().getDrawFramebuffer());
        if (currentDrawFramebuffer == this)
        {
            contextMtl->onDrawFrameBufferChange(context, this);
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferMtl::getSamplePosition(const gl::Context *context,
                                                size_t index,
                                                GLfloat *xy) const
{
    UNIMPLEMENTED();
    return angle::Result::Stop;
}

RenderTargetMtl *FramebufferMtl::getColorReadRenderTarget(const gl::Context *context) const
{
    if (mState.getReadIndex() >= mColorRenderTargets.size())
    {
        return nullptr;
    }

    if (mBackbuffer)
    {
        if (IsError(mBackbuffer->ensureCurrentDrawableObtained(context)))
        {
            return nullptr;
        }
    }

    return mColorRenderTargets[mState.getReadIndex()];
}

int FramebufferMtl::getSamples() const
{
    return mRenderPassDesc.sampleCount;
}

gl::Rectangle FramebufferMtl::getCompleteRenderArea() const
{
    return gl::Rectangle(0, 0, mState.getDimensions().width, mState.getDimensions().height);
}

bool FramebufferMtl::renderPassHasStarted(ContextMtl *contextMtl) const
{
    return contextMtl->hasStartedRenderPass(mRenderPassDesc);
}

mtl::RenderCommandEncoder *FramebufferMtl::ensureRenderPassStarted(const gl::Context *context)
{
    return ensureRenderPassStarted(context, mRenderPassDesc);
}

mtl::RenderCommandEncoder *FramebufferMtl::ensureRenderPassStarted(const gl::Context *context,
                                                                   const mtl::RenderPassDesc &desc)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    if (renderPassHasStarted(contextMtl))
    {
        return contextMtl->getRenderCommandEncoder();
    }

    if (mBackbuffer)
    {
        // Backbuffer might obtain new drawable, which means it might change the
        // the native texture used as the target of the render pass.
        // We need to call this before creating render encoder.
        if (IsError(mBackbuffer->ensureCurrentDrawableObtained(context)))
        {
            return nullptr;
        }
    }

    // Only support ensureRenderPassStarted() with different load & store options.
    // The texture, level, slice must be the same.
    ASSERT(desc.equalIgnoreLoadStoreOptions(mRenderPassDesc));

    mtl::RenderCommandEncoder *encoder = contextMtl->getRenderCommandEncoder(desc);

    if (mRenderPassCleanStart)
    {
        // After a clean start we should reset the loadOp to MTLLoadActionLoad in case this render
        // pass could be interrupted by a conversion compute shader pass then being resumed later.
        mRenderPassCleanStart = false;
        for (mtl::RenderPassColorAttachmentDesc &colorAttachment : mRenderPassDesc.colorAttachments)
        {
            colorAttachment.loadAction = MTLLoadActionLoad;
        }
        mRenderPassDesc.depthAttachment.loadAction   = MTLLoadActionLoad;
        mRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionLoad;
    }

    return encoder;
}

void FramebufferMtl::setLoadStoreActionOnRenderPassFirstStart(
    mtl::RenderPassAttachmentDesc *attachmentOut)
{
    ASSERT(mRenderPassCleanStart);

    mtl::RenderPassAttachmentDesc &attachment = *attachmentOut;

    if (attachment.storeAction == MTLStoreActionDontCare ||
        attachment.storeAction == MTLStoreActionMultisampleResolve)
    {
        // If we previously discarded attachment's content, then don't need to load it.
        attachment.loadAction = MTLLoadActionDontCare;
    }
    else
    {
        attachment.loadAction = MTLLoadActionLoad;
    }

    if (attachment.hasImplicitMSTexture())
    {
        if (mBackbuffer)
        {
            // Default action for default framebuffer is resolve and keep MS texture's content.
            // We only discard MS texture's content at the end of the frame. See onFrameEnd().
            attachment.storeAction = MTLStoreActionStoreAndMultisampleResolve;
        }
        else
        {
            // Default action is resolve but don't keep MS texture's content.
            attachment.storeAction = MTLStoreActionMultisampleResolve;
        }
    }
    else
    {
        attachment.storeAction = MTLStoreActionStore;  // Default action is store
    }
}

void FramebufferMtl::onStartedDrawingToFrameBuffer(const gl::Context *context)
{
    mRenderPassCleanStart = true;

    // Compute loadOp based on previous storeOp and reset storeOp flags:
    for (mtl::RenderPassColorAttachmentDesc &colorAttachment : mRenderPassDesc.colorAttachments)
    {
        setLoadStoreActionOnRenderPassFirstStart(&colorAttachment);
    }
    // Depth load/store
    setLoadStoreActionOnRenderPassFirstStart(&mRenderPassDesc.depthAttachment);

    // Stencil load/store
    setLoadStoreActionOnRenderPassFirstStart(&mRenderPassDesc.stencilAttachment);
}

void FramebufferMtl::onFrameEnd(const gl::Context *context)
{
    if (!mBackbuffer)
    {
        return;
    }

    ContextMtl *contextMtl = mtl::GetImpl(context);
    // Always discard default FBO's depth stencil & multisample buffers at the end of the frame:
    if (this->renderPassHasStarted(contextMtl))
    {
        mtl::RenderCommandEncoder *encoder = contextMtl->getRenderCommandEncoder();

        constexpr GLenum dsAttachments[] = {GL_DEPTH, GL_STENCIL};
        (void)invalidateImpl(contextMtl, 2, dsAttachments);
        if (mBackbuffer->getSamples() > 1)
        {
            encoder->setColorStoreAction(MTLStoreActionMultisampleResolve, 0);
        }

        contextMtl->endEncoding(false);

        // Reset discard flag.
        onStartedDrawingToFrameBuffer(context);
    }
}

angle::Result FramebufferMtl::updateColorRenderTarget(const gl::Context *context,
                                                      size_t colorIndexGL)
{
    ASSERT(colorIndexGL < mtl::kMaxRenderTargets);
    // Reset load store action
    mRenderPassDesc.colorAttachments[colorIndexGL].reset();
    return updateCachedRenderTarget(context, mState.getColorAttachment(colorIndexGL),
                                    &mColorRenderTargets[colorIndexGL]);
}

angle::Result FramebufferMtl::updateDepthRenderTarget(const gl::Context *context)
{
    // Reset load store action
    mRenderPassDesc.depthAttachment.reset();
    return updateCachedRenderTarget(context, mState.getDepthAttachment(), &mDepthRenderTarget);
}

angle::Result FramebufferMtl::updateStencilRenderTarget(const gl::Context *context)
{
    // Reset load store action
    mRenderPassDesc.stencilAttachment.reset();
    return updateCachedRenderTarget(context, mState.getStencilAttachment(), &mStencilRenderTarget);
}

angle::Result FramebufferMtl::updateCachedRenderTarget(const gl::Context *context,
                                                       const gl::FramebufferAttachment *attachment,
                                                       RenderTargetMtl **cachedRenderTarget)
{
    RenderTargetMtl *newRenderTarget = nullptr;
    if (attachment)
    {
        ASSERT(attachment->isAttached());
        ANGLE_TRY(attachment->getRenderTarget(context, attachment->getRenderToTextureSamples(),
                                              &newRenderTarget));
    }
    *cachedRenderTarget = newRenderTarget;
    return angle::Result::Continue;
}

angle::Result FramebufferMtl::prepareRenderPass(const gl::Context *context,
                                                mtl::RenderPassDesc *pDescOut)
{
    gl::DrawBufferMask enabledBuffer = mState.getEnabledDrawBuffers();

    mtl::RenderPassDesc &desc = *pDescOut;

    uint32_t maxColorAttachments = static_cast<uint32_t>(mState.getColorAttachments().size());
    desc.numColorAttachments     = 0;
    desc.sampleCount             = 1;
    for (uint32_t colorIndexGL = 0; colorIndexGL < maxColorAttachments; ++colorIndexGL)
    {
        ASSERT(colorIndexGL < mtl::kMaxRenderTargets);

        mtl::RenderPassColorAttachmentDesc &colorAttachment = desc.colorAttachments[colorIndexGL];
        const RenderTargetMtl *colorRenderTarget            = mColorRenderTargets[colorIndexGL];

        if (colorRenderTarget && enabledBuffer.test(colorIndexGL))
        {
            colorRenderTarget->toRenderPassAttachmentDesc(&colorAttachment);

            desc.numColorAttachments = std::max(desc.numColorAttachments, colorIndexGL + 1);
            desc.sampleCount = std::max(desc.sampleCount, colorRenderTarget->getRenderSamples());
        }
        else
        {
            colorAttachment.reset();
        }
    }

    if (mDepthRenderTarget)
    {
        mDepthRenderTarget->toRenderPassAttachmentDesc(&desc.depthAttachment);
        desc.sampleCount = std::max(desc.sampleCount, mDepthRenderTarget->getRenderSamples());
    }
    else
    {
        desc.depthAttachment.reset();
    }

    if (mStencilRenderTarget)
    {
        mStencilRenderTarget->toRenderPassAttachmentDesc(&desc.stencilAttachment);
        desc.sampleCount = std::max(desc.sampleCount, mStencilRenderTarget->getRenderSamples());
    }
    else
    {
        desc.stencilAttachment.reset();
    }

    return angle::Result::Continue;
}

// Override clear color based on texture's write mask
void FramebufferMtl::overrideClearColor(const mtl::TextureRef &texture,
                                        MTLClearColor clearColor,
                                        MTLClearColor *colorOut)
{
    *colorOut = mtl::EmulatedAlphaClearColor(clearColor, texture->getColorWritableMask());
}

angle::Result FramebufferMtl::clearWithLoadOp(const gl::Context *context,
                                              gl::DrawBufferMask clearColorBuffers,
                                              const mtl::ClearRectParams &clearOpts)
{
    ContextMtl *contextMtl             = mtl::GetImpl(context);
    bool startedRenderPass             = contextMtl->hasStartedRenderPass(mRenderPassDesc);
    mtl::RenderCommandEncoder *encoder = nullptr;
    mtl::RenderPassDesc tempDesc       = mRenderPassDesc;

    if (startedRenderPass)
    {
        encoder = contextMtl->getRenderCommandEncoder();
    }

    size_t attachmentCount = 0;
    for (size_t colorIndexGL : mState.getEnabledDrawBuffers())
    {
        ASSERT(colorIndexGL < mtl::kMaxRenderTargets);

        uint32_t attachmentIdx = static_cast<uint32_t>(attachmentCount++);
        mtl::RenderPassColorAttachmentDesc &colorAttachment =
            tempDesc.colorAttachments[attachmentIdx];
        const mtl::TextureRef &texture = colorAttachment.texture;

        if (clearColorBuffers.test(colorIndexGL))
        {
            if (startedRenderPass)
            {
                // Render pass already started, and we want to clear this buffer,
                // then discard its content before clearing.
                encoder->setColorStoreAction(MTLStoreActionDontCare, attachmentIdx);
            }
            colorAttachment.loadAction = MTLLoadActionClear;
            overrideClearColor(texture, clearOpts.clearColor.value(), &colorAttachment.clearColor);
        }
        else if (startedRenderPass)
        {
            // If render pass already started and we don't want to clear this buffer,
            // then store it with current render encoder and load it before clearing step
            encoder->setColorStoreAction(MTLStoreActionStore, attachmentIdx);
            colorAttachment.loadAction = MTLLoadActionLoad;
        }
    }

    MTLStoreAction preClearDethpStoreAction   = MTLStoreActionStore,
                   preClearStencilStoreAction = MTLStoreActionStore;
    if (clearOpts.clearDepth.valid())
    {
        preClearDethpStoreAction            = MTLStoreActionDontCare;
        tempDesc.depthAttachment.loadAction = MTLLoadActionClear;
        tempDesc.depthAttachment.clearDepth = clearOpts.clearDepth.value();
    }
    else if (startedRenderPass)
    {
        // If render pass already started and we don't want to clear this buffer,
        // then store it with current render encoder and load it before clearing step
        preClearDethpStoreAction            = MTLStoreActionStore;
        tempDesc.depthAttachment.loadAction = MTLLoadActionLoad;
    }

    if (clearOpts.clearStencil.valid())
    {
        preClearStencilStoreAction              = MTLStoreActionDontCare;
        tempDesc.stencilAttachment.loadAction   = MTLLoadActionClear;
        tempDesc.stencilAttachment.clearStencil = clearOpts.clearStencil.value();
    }
    else if (startedRenderPass)
    {
        // If render pass already started and we don't want to clear this buffer,
        // then store it with current render encoder and load it before clearing step
        preClearStencilStoreAction            = MTLStoreActionStore;
        tempDesc.stencilAttachment.loadAction = MTLLoadActionLoad;
    }

    // End current render encoder.
    if (startedRenderPass)
    {
        encoder->setDepthStencilStoreAction(preClearDethpStoreAction, preClearStencilStoreAction);
        contextMtl->endEncoding(encoder);
    }

    // Start new render encoder with loadOp=Clear
    ensureRenderPassStarted(context, tempDesc);

    return angle::Result::Continue;
}

angle::Result FramebufferMtl::clearWithDraw(const gl::Context *context,
                                            gl::DrawBufferMask clearColorBuffers,
                                            const mtl::ClearRectParams &clearOpts)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    DisplayMtl *display    = contextMtl->getDisplay();

    // Start new render encoder if not already.
    mtl::RenderCommandEncoder *encoder = ensureRenderPassStarted(context, mRenderPassDesc);

    return display->getUtils().clearWithDraw(context, encoder, clearOpts);
}

angle::Result FramebufferMtl::clearImpl(const gl::Context *context,
                                        gl::DrawBufferMask clearColorBuffers,
                                        mtl::ClearRectParams *pClearOpts)
{
    auto &clearOpts = *pClearOpts;

    if (!clearOpts.clearColor.valid() && !clearOpts.clearDepth.valid() &&
        !clearOpts.clearStencil.valid())
    {
        // No Op.
        return angle::Result::Continue;
    }

    ContextMtl *contextMtl = mtl::GetImpl(context);
    const gl::Rectangle renderArea(0, 0, mState.getDimensions().width,
                                   mState.getDimensions().height);

    clearOpts.clearArea = ClipRectToScissor(contextMtl->getState(), renderArea, false);
    clearOpts.flipY     = mFlipY;

    // Discard clear altogether if scissor has 0 width or height.
    if (clearOpts.clearArea.width == 0 || clearOpts.clearArea.height == 0)
    {
        return angle::Result::Continue;
    }

    MTLColorWriteMask colorMask = contextMtl->getColorMask();
    uint32_t stencilMask        = contextMtl->getStencilMask();
    if (!contextMtl->getDepthMask())
    {
        // Disable depth clearing, since depth write is disable
        clearOpts.clearDepth.reset();
    }

    if (clearOpts.clearArea == renderArea &&
        (!clearOpts.clearColor.valid() || colorMask == MTLColorWriteMaskAll) &&
        (!clearOpts.clearStencil.valid() ||
         (stencilMask & mtl::kStencilMaskAll) == mtl::kStencilMaskAll))
    {
        return clearWithLoadOp(context, clearColorBuffers, clearOpts);
    }

    return clearWithDraw(context, clearColorBuffers, clearOpts);
}

angle::Result FramebufferMtl::invalidateImpl(ContextMtl *contextMtl,
                                             size_t count,
                                             const GLenum *attachments)
{
    gl::DrawBufferMask invalidateColorBuffers;
    bool invalidateDepthBuffer   = false;
    bool invalidateStencilBuffer = false;

    for (size_t i = 0; i < count; ++i)
    {
        const GLenum attachment = attachments[i];

        switch (attachment)
        {
            case GL_DEPTH:
            case GL_DEPTH_ATTACHMENT:
                invalidateDepthBuffer = true;
                break;
            case GL_STENCIL:
            case GL_STENCIL_ATTACHMENT:
                invalidateStencilBuffer = true;
                break;
            case GL_DEPTH_STENCIL_ATTACHMENT:
                invalidateDepthBuffer   = true;
                invalidateStencilBuffer = true;
                break;
            default:
                ASSERT(
                    (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) ||
                    (attachment == GL_COLOR));

                invalidateColorBuffers.set(
                    attachment == GL_COLOR ? 0u : (attachment - GL_COLOR_ATTACHMENT0));
        }
    }

    // Set the appropriate storeOp for attachments.
    // If we already start the render pass, then need to set the store action now.
    bool renderPassStarted = contextMtl->hasStartedRenderPass(mRenderPassDesc);
    mtl::RenderCommandEncoder *encoder =
        renderPassStarted ? contextMtl->getRenderCommandEncoder() : nullptr;

    for (uint32_t i = 0; i < mRenderPassDesc.numColorAttachments; ++i)
    {
        if (invalidateColorBuffers.test(i))
        {
            mtl::RenderPassColorAttachmentDesc &colorAttachment =
                mRenderPassDesc.colorAttachments[i];
            colorAttachment.storeAction = MTLStoreActionDontCare;
            if (renderPassStarted)
            {
                encoder->setColorStoreAction(MTLStoreActionDontCare, i);
            }
        }
    }

    if (invalidateDepthBuffer && mDepthRenderTarget)
    {
        mRenderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
        if (renderPassStarted)
        {
            encoder->setDepthStoreAction(MTLStoreActionDontCare);
        }
    }

    if (invalidateStencilBuffer && mStencilRenderTarget)
    {
        mRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
        if (renderPassStarted)
        {
            encoder->setStencilStoreAction(MTLStoreActionDontCare);
        }
    }

    return angle::Result::Continue;
}

gl::Rectangle FramebufferMtl::getCorrectFlippedReadArea(const gl::Context *context,
                                                        const gl::Rectangle &glArea) const
{
    RenderTargetMtl *colorReadRT = getColorReadRenderTarget(context);
    ASSERT(colorReadRT);
    gl::Rectangle flippedArea = glArea;
    if (mFlipY)
    {
        flippedArea.y =
            colorReadRT->getTexture()->height(static_cast<uint32_t>(colorReadRT->getLevelIndex())) -
            flippedArea.y - flippedArea.height;
    }

    return flippedArea;
}

angle::Result FramebufferMtl::readPixelsImpl(const gl::Context *context,
                                             const gl::Rectangle &area,
                                             const PackPixelsParams &packPixelsParams,
                                             RenderTargetMtl *renderTarget,
                                             uint8_t *pixels)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    if (packPixelsParams.packBuffer)
    {
        // NOTE(hqle): PBO is not supported atm
        ANGLE_MTL_CHECK(contextMtl, false, GL_INVALID_OPERATION);
    }
    if (!renderTarget)
    {
        return angle::Result::Continue;
    }

    mtl::Texture *texture;
    if (mBackbuffer)
    {
        // Backbuffer might have MSAA texture as render target, needs to obtain the
        // resolved texture to be able to read pixels.
        ANGLE_TRY(mBackbuffer->ensureColorTextureReadyForReadPixels(context));
        texture = mBackbuffer->getColorTexture().get();
    }
    else
    {
        texture = renderTarget->getTexture().get();
        // For non-default framebuffer, MSAA read pixels is disallowed.
        ANGLE_MTL_CHECK(contextMtl, texture->samples() == 1, GL_INVALID_OPERATION);
    }

    const mtl::Format &readFormat        = *renderTarget->getFormat();
    const angle::Format &readAngleFormat = readFormat.actualAngleFormat();

    // NOTE(hqle): resolve MSAA texture before readback
    int srcRowPitch = area.width * readAngleFormat.pixelBytes;
    angle::MemoryBuffer readPixelRowBuffer;
    ANGLE_CHECK_GL_ALLOC(contextMtl, readPixelRowBuffer.resize(srcRowPitch));

    auto packPixelsRowParams  = packPixelsParams;
    MTLRegion mtlSrcRowRegion = MTLRegionMake2D(area.x, area.y, area.width, 1);

    int rowOffset = packPixelsParams.reverseRowOrder ? -1 : 1;
    int startRow  = packPixelsParams.reverseRowOrder ? (area.y1() - 1) : area.y;

    // Copy pixels row by row
    packPixelsRowParams.area.height     = 1;
    packPixelsRowParams.reverseRowOrder = false;
    for (int r = startRow, i = 0; i < area.height;
         ++i, r += rowOffset, pixels += packPixelsRowParams.outputPitch)
    {
        mtlSrcRowRegion.origin.y   = r;
        packPixelsRowParams.area.y = packPixelsParams.area.y + i;

        // Read the pixels data to the row buffer
        texture->getBytes(contextMtl, srcRowPitch, mtlSrcRowRegion,
                          static_cast<uint32_t>(renderTarget->getLevelIndex()),
                          readPixelRowBuffer.data());

        // Convert to destination format
        PackPixels(packPixelsRowParams, readAngleFormat, srcRowPitch, readPixelRowBuffer.data(),
                   pixels);
    }

    return angle::Result::Continue;
}
}
