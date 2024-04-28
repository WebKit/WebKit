//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferWgpu.cpp:
//    Implements the class methods for FramebufferWgpu.
//

#include "libANGLE/renderer/wgpu/FramebufferWgpu.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

FramebufferWgpu::FramebufferWgpu(const gl::FramebufferState &state) : FramebufferImpl(state) {}

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
    // TODO(anglebug.com/8582): support clearing depth and stencil buffers.
    ASSERT(!clearDepth && !clearStencil && clearColor);

    ContextWgpu *contextWgpu   = GetImplAs<ContextWgpu>(context);
    gl::ColorF colorClearValue = context->getState().getColorClearValue();

    std::vector<wgpu::RenderPassColorAttachment> colorAttachments(
        mState.getEnabledDrawBuffers().count());
    for (size_t enabledDrawBuffer : mState.getEnabledDrawBuffers())
    {
        wgpu::RenderPassColorAttachment colorAttachment;
        colorAttachment.view =
            mRenderTargetCache.getColorDraw(mState, enabledDrawBuffer)->getTexture();
        colorAttachment.depthSlice   = wgpu::kDepthSliceUndefined;
        colorAttachment.loadOp       = wgpu::LoadOp::Clear;
        colorAttachment.storeOp      = wgpu::StoreOp::Store;
        colorAttachment.clearValue.r = colorClearValue.red;
        colorAttachment.clearValue.g = colorClearValue.green;
        colorAttachment.clearValue.b = colorClearValue.blue;
        colorAttachment.clearValue.a = colorClearValue.alpha;
        colorAttachments.push_back(colorAttachment);
    }

    wgpu::RenderPassDescriptor renderPassDesc;
    renderPassDesc.colorAttachmentCount = colorAttachments.size();
    renderPassDesc.colorAttachments     = colorAttachments.data();

    // TODO(anglebug.com/8582): optimize this implementation.
    ANGLE_TRY(contextWgpu->ensureRenderPassStarted(renderPassDesc));
    ANGLE_TRY(contextWgpu->endRenderPass(webgpu::RenderPassClosureReason::NewRenderPass));
    ANGLE_TRY(contextWgpu->flush());
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
    gl::Rectangle area;
    if (!ClipRectangle(origArea, fbRect, &area))
    {
        // nothing to read
        return angle::Result::Continue;
    }

    // Compute size of unclipped rows and initial skip
    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    GLuint rowBytes = 0;
    ANGLE_CHECK_GL_MATH(contextWgpu, glFormat.computeRowPitch(type, origArea.width, pack.alignment,
                                                              pack.rowLength, &rowBytes));

    GLuint skipBytes = 0;
    ANGLE_CHECK_GL_MATH(contextWgpu,
                        glFormat.computeSkipBytes(type, rowBytes, 0, pack, false, &skipBytes));
    pixels += skipBytes;

    // Skip OOB region up to first in bounds pixel
    int leftClip = area.x - origArea.x;
    int topClip  = area.y - origArea.y;
    pixels += leftClip * glFormat.pixelBytes + topClip * rowBytes;

    // Write the in-bounds readpixels data with non-zero values
    for (GLint y = area.y; y < area.y + area.height; ++y)
    {
        memset(pixels, 42, glFormat.pixelBytes * area.width);
        pixels += rowBytes;
    }

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
    ASSERT(dirtyBits.any());
    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_DEPTH_BUFFER_CONTENTS:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_BUFFER_CONTENTS:
                ANGLE_TRY(mRenderTargetCache.updateDepthStencilRenderTarget(context, mState));
                break;
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

                break;
            }
        }
    }

    return angle::Result::Continue;
}

angle::Result FramebufferWgpu::getSamplePosition(const gl::Context *context,
                                                 size_t index,
                                                 GLfloat *xy) const
{
    return angle::Result::Continue;
}

}  // namespace rx
