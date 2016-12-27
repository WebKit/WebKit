//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferD3D.cpp: Implements the DefaultAttachmentD3D and FramebufferD3D classes.

#include "libANGLE/renderer/d3d/FramebufferD3D.h"

#include "common/BitSetIterator.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/SurfaceD3D.h"
#include "libANGLE/renderer/d3d/SwapChainD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"

namespace rx
{

namespace
{

ClearParameters GetClearParameters(const gl::State &state, GLbitfield mask)
{
    ClearParameters clearParams;
    memset(&clearParams, 0, sizeof(ClearParameters));

    const auto &blendState = state.getBlendState();

    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = false;
    }
    clearParams.colorFClearValue = state.getColorClearValue();
    clearParams.colorClearType = GL_FLOAT;
    clearParams.colorMaskRed = blendState.colorMaskRed;
    clearParams.colorMaskGreen = blendState.colorMaskGreen;
    clearParams.colorMaskBlue = blendState.colorMaskBlue;
    clearParams.colorMaskAlpha = blendState.colorMaskAlpha;
    clearParams.clearDepth = false;
    clearParams.depthClearValue =  state.getDepthClearValue();
    clearParams.clearStencil = false;
    clearParams.stencilClearValue = state.getStencilClearValue();
    clearParams.stencilWriteMask = state.getDepthStencilState().stencilWritemask;
    clearParams.scissorEnabled = state.isScissorTestEnabled();
    clearParams.scissor = state.getScissor();

    const gl::Framebuffer *framebufferObject = state.getDrawFramebuffer();
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        if (framebufferObject->hasEnabledDrawBuffer())
        {
            for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
            {
                clearParams.clearColor[i] = true;
            }
        }
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        if (state.getDepthStencilState().depthMask && framebufferObject->getDepthbuffer() != NULL)
        {
            clearParams.clearDepth = true;
        }
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        if (framebufferObject->getStencilbuffer() != NULL &&
            framebufferObject->getStencilbuffer()->getStencilSize() > 0)
        {
            clearParams.clearStencil = true;
        }
    }

    return clearParams;
}

}

FramebufferD3D::FramebufferD3D(const gl::FramebufferState &data, RendererD3D *renderer)
    : FramebufferImpl(data), mRenderer(renderer)
{
}

FramebufferD3D::~FramebufferD3D()
{
}

gl::Error FramebufferD3D::clear(ContextImpl *context, GLbitfield mask)
{
    ClearParameters clearParams = GetClearParameters(context->getGLState(), mask);
    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferfv(ContextImpl *context,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        const GLfloat *values)
{
    // glClearBufferfv can be called to clear the color buffer or depth buffer
    ClearParameters clearParams = GetClearParameters(context->getGLState(), 0);

    if (buffer == GL_COLOR)
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
        }
        clearParams.colorFClearValue = gl::ColorF(values[0], values[1], values[2], values[3]);
        clearParams.colorClearType = GL_FLOAT;
    }

    if (buffer == GL_DEPTH)
    {
        clearParams.clearDepth = true;
        clearParams.depthClearValue = values[0];
    }

    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferuiv(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLuint *values)
{
    // glClearBufferuiv can only be called to clear a color buffer
    ClearParameters clearParams = GetClearParameters(context->getGLState(), 0);
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
    }
    clearParams.colorUIClearValue = gl::ColorUI(values[0], values[1], values[2], values[3]);
    clearParams.colorClearType = GL_UNSIGNED_INT;

    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferiv(ContextImpl *context,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        const GLint *values)
{
    // glClearBufferiv can be called to clear the color buffer or stencil buffer
    ClearParameters clearParams = GetClearParameters(context->getGLState(), 0);

    if (buffer == GL_COLOR)
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
        }
        clearParams.colorIClearValue = gl::ColorI(values[0], values[1], values[2], values[3]);
        clearParams.colorClearType = GL_INT;
    }

    if (buffer == GL_STENCIL)
    {
        clearParams.clearStencil = true;
        clearParams.stencilClearValue = values[1];
    }

    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferfi(ContextImpl *context,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        GLfloat depth,
                                        GLint stencil)
{
    // glClearBufferfi can only be called to clear a depth stencil buffer
    ClearParameters clearParams   = GetClearParameters(context->getGLState(), 0);
    clearParams.clearDepth = true;
    clearParams.depthClearValue = depth;
    clearParams.clearStencil = true;
    clearParams.stencilClearValue = stencil;

    return clearImpl(context, clearParams);
}

GLenum FramebufferD3D::getImplementationColorReadFormat() const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();

    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    RenderTargetD3D *attachmentRenderTarget = NULL;
    gl::Error error = readAttachment->getRenderTarget(&attachmentRenderTarget);
    if (error.isError())
    {
        return GL_NONE;
    }

    GLenum implementationFormat = getRenderTargetImplementationFormat(attachmentRenderTarget);
    const gl::InternalFormat &implementationFormatInfo = gl::GetInternalFormatInfo(implementationFormat);

    return implementationFormatInfo.getReadPixelsFormat();
}

GLenum FramebufferD3D::getImplementationColorReadType() const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();

    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    RenderTargetD3D *attachmentRenderTarget = NULL;
    gl::Error error = readAttachment->getRenderTarget(&attachmentRenderTarget);
    if (error.isError())
    {
        return GL_NONE;
    }

    GLenum implementationFormat = getRenderTargetImplementationFormat(attachmentRenderTarget);
    const gl::InternalFormat &implementationFormatInfo = gl::GetInternalFormatInfo(implementationFormat);

    return implementationFormatInfo.getReadPixelsType();
}

gl::Error FramebufferD3D::readPixels(ContextImpl *context,
                                     const gl::Rectangle &area,
                                     GLenum format,
                                     GLenum type,
                                     GLvoid *pixels) const
{
    const gl::PixelPackState &packState = context->getGLState().getPackState();

    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(format, type);
    const gl::InternalFormat &sizedFormatInfo = gl::GetInternalFormatInfo(sizedInternalFormat);

    GLuint outputPitch = 0;
    ANGLE_TRY_RESULT(
        sizedFormatInfo.computeRowPitch(type, area.width, packState.alignment, packState.rowLength),
        outputPitch);
    GLuint outputSkipBytes = 0;
    ANGLE_TRY_RESULT(sizedFormatInfo.computeSkipBytes(outputPitch, 0, packState, false),
                     outputSkipBytes);

    return readPixelsImpl(area, format, type, outputPitch, packState,
                          reinterpret_cast<uint8_t *>(pixels) + outputSkipBytes);
}

gl::Error FramebufferD3D::blit(ContextImpl *context,
                               const gl::Rectangle &sourceArea,
                               const gl::Rectangle &destArea,
                               GLbitfield mask,
                               GLenum filter)
{
    const auto &glState                      = context->getGLState();
    const gl::Framebuffer *sourceFramebuffer = glState.getReadFramebuffer();
    bool blitRenderTarget = false;
    if ((mask & GL_COLOR_BUFFER_BIT) && sourceFramebuffer->getReadColorbuffer() != nullptr &&
        mState.getFirstColorAttachment() != nullptr)
    {
        blitRenderTarget = true;
    }

    bool blitStencil = false;
    if ((mask & GL_STENCIL_BUFFER_BIT) && sourceFramebuffer->getStencilbuffer() != nullptr &&
        mState.getStencilAttachment() != nullptr)
    {
        blitStencil = true;
    }

    bool blitDepth = false;
    if ((mask & GL_DEPTH_BUFFER_BIT) && sourceFramebuffer->getDepthbuffer() != nullptr &&
        mState.getDepthAttachment() != nullptr)
    {
        blitDepth = true;
    }

    if (blitRenderTarget || blitDepth || blitStencil)
    {
        const gl::Rectangle *scissor =
            glState.isScissorTestEnabled() ? &glState.getScissor() : nullptr;
        gl::Error error = blitImpl(sourceArea, destArea, scissor, blitRenderTarget, blitDepth,
                                   blitStencil, filter, sourceFramebuffer);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

bool FramebufferD3D::checkStatus() const
{
    // if we have both a depth and stencil buffer, they must refer to the same object
    // since we only support packed_depth_stencil and not separate depth and stencil
    if (mState.getDepthAttachment() != nullptr && mState.getStencilAttachment() != nullptr &&
        mState.getDepthStencilAttachment() == nullptr)
    {
        return false;
    }

    // D3D11 does not allow for overlapping RenderTargetViews
    if (!mState.colorAttachmentsAreUniqueImages())
    {
        return false;
    }

    // D3D requires all render targets to have the same dimensions.
    if (!mState.attachmentsHaveSameDimensions())
    {
        return false;
    }

    return true;
}

void FramebufferD3D::syncState(const gl::Framebuffer::DirtyBits &dirtyBits)
{
    bool invalidateColorAttachmentCache = false;

    if (!mColorAttachmentsForRender.valid())
    {
        invalidateColorAttachmentCache = true;
    }

    for (auto dirtyBit : angle::IterateBitSet(dirtyBits))
    {
        if ((dirtyBit >= gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 &&
             dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX) ||
            dirtyBit == gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS)
        {
            invalidateColorAttachmentCache = true;
        }
    }

    if (!invalidateColorAttachmentCache)
    {
        return;
    }

    // Does not actually free memory
    gl::AttachmentList colorAttachmentsForRender;

    const auto &colorAttachments = mState.getColorAttachments();
    const auto &drawBufferStates = mState.getDrawBufferStates();
    const auto &workarounds      = mRenderer->getWorkarounds();

    for (size_t attachmentIndex = 0; attachmentIndex < colorAttachments.size(); ++attachmentIndex)
    {
        GLenum drawBufferState = drawBufferStates[attachmentIndex];
        const gl::FramebufferAttachment &colorAttachment = colorAttachments[attachmentIndex];

        if (colorAttachment.isAttached() && drawBufferState != GL_NONE)
        {
            ASSERT(drawBufferState == GL_BACK || drawBufferState == (GL_COLOR_ATTACHMENT0_EXT + attachmentIndex));
            colorAttachmentsForRender.push_back(&colorAttachment);
        }
        else if (!workarounds.mrtPerfWorkaround)
        {
            colorAttachmentsForRender.push_back(nullptr);
        }
    }

    mColorAttachmentsForRender = std::move(colorAttachmentsForRender);
}

const gl::AttachmentList &FramebufferD3D::getColorAttachmentsForRender() const
{
    ASSERT(mColorAttachmentsForRender.valid());
    return mColorAttachmentsForRender.value();
}

}  // namespace rx
