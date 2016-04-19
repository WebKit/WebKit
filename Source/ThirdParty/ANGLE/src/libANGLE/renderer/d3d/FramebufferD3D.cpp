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

FramebufferD3D::FramebufferD3D(const gl::Framebuffer::Data &data, RendererD3D *renderer)
    : FramebufferImpl(data), mRenderer(renderer)
{
}

FramebufferD3D::~FramebufferD3D()
{
}

gl::Error FramebufferD3D::clear(const gl::Data &data, GLbitfield mask)
{
    const gl::State &state = *data.state;
    ClearParameters clearParams = GetClearParameters(state, mask);
    return clear(data, clearParams);
}

gl::Error FramebufferD3D::clearBufferfv(const gl::Data &data,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        const GLfloat *values)
{
    // glClearBufferfv can be called to clear the color buffer or depth buffer
    ClearParameters clearParams = GetClearParameters(*data.state, 0);

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

    return clear(data, clearParams);
}

gl::Error FramebufferD3D::clearBufferuiv(const gl::Data &data,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLuint *values)
{
    // glClearBufferuiv can only be called to clear a color buffer
    ClearParameters clearParams = GetClearParameters(*data.state, 0);
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
    }
    clearParams.colorUIClearValue = gl::ColorUI(values[0], values[1], values[2], values[3]);
    clearParams.colorClearType = GL_UNSIGNED_INT;

    return clear(data, clearParams);
}

gl::Error FramebufferD3D::clearBufferiv(const gl::Data &data,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        const GLint *values)
{
    // glClearBufferiv can be called to clear the color buffer or stencil buffer
    ClearParameters clearParams = GetClearParameters(*data.state, 0);

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

    return clear(data, clearParams);
}

gl::Error FramebufferD3D::clearBufferfi(const gl::Data &data,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        GLfloat depth,
                                        GLint stencil)
{
    // glClearBufferfi can only be called to clear a depth stencil buffer
    ClearParameters clearParams   = GetClearParameters(*data.state, 0);
    clearParams.clearDepth = true;
    clearParams.depthClearValue = depth;
    clearParams.clearStencil = true;
    clearParams.stencilClearValue = stencil;

    return clear(data, clearParams);
}

GLenum FramebufferD3D::getImplementationColorReadFormat() const
{
    const gl::FramebufferAttachment *readAttachment = mData.getReadAttachment();

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

    return implementationFormatInfo.format;
}

GLenum FramebufferD3D::getImplementationColorReadType() const
{
    const gl::FramebufferAttachment *readAttachment = mData.getReadAttachment();

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

    return implementationFormatInfo.type;
}

gl::Error FramebufferD3D::readPixels(const gl::State &state, const gl::Rectangle &area, GLenum format, GLenum type, GLvoid *pixels) const
{
    const gl::PixelPackState &packState = state.getPackState();

    GLenum sizedInternalFormat = gl::GetSizedInternalFormat(format, type);
    const gl::InternalFormat &sizedFormatInfo = gl::GetInternalFormatInfo(sizedInternalFormat);
    GLuint outputPitch =
        sizedFormatInfo.computeRowPitch(type, area.width, packState.alignment, packState.rowLength);
    GLsizei outputSkipBytes = sizedFormatInfo.computeSkipPixels(
        outputPitch, 0, 0, packState.skipRows, packState.skipPixels);

    return readPixelsImpl(area, format, type, outputPitch, packState,
                          reinterpret_cast<uint8_t *>(pixels) + outputSkipBytes);
}

gl::Error FramebufferD3D::blit(const gl::State &state, const gl::Rectangle &sourceArea, const gl::Rectangle &destArea,
                               GLbitfield mask, GLenum filter, const gl::Framebuffer *sourceFramebuffer)
{
    bool blitRenderTarget = false;
    if ((mask & GL_COLOR_BUFFER_BIT) &&
        sourceFramebuffer->getReadColorbuffer() != nullptr &&
        mData.getFirstColorAttachment() != nullptr)
    {
        blitRenderTarget = true;
    }

    bool blitStencil = false;
    if ((mask & GL_STENCIL_BUFFER_BIT) &&
        sourceFramebuffer->getStencilbuffer() != nullptr &&
        mData.getStencilAttachment() != nullptr)
    {
        blitStencil = true;
    }

    bool blitDepth = false;
    if ((mask & GL_DEPTH_BUFFER_BIT) &&
        sourceFramebuffer->getDepthbuffer() != nullptr &&
        mData.getDepthAttachment() != nullptr)
    {
        blitDepth = true;
    }

    if (blitRenderTarget || blitDepth || blitStencil)
    {
        const gl::Rectangle *scissor = state.isScissorTestEnabled() ? &state.getScissor() : NULL;
        gl::Error error = blit(sourceArea, destArea, scissor, blitRenderTarget, blitDepth, blitStencil,
                               filter, sourceFramebuffer);
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
    if (mData.getDepthAttachment() != nullptr && mData.getStencilAttachment() != nullptr &&
        mData.getDepthStencilAttachment() == nullptr)
    {
        return false;
    }

    // D3D11 does not allow for overlapping RenderTargetViews, so ensure uniqueness
    const auto &colorAttachments = mData.getColorAttachments();
    for (size_t colorAttachment = 0; colorAttachment < colorAttachments.size(); colorAttachment++)
    {
        const gl::FramebufferAttachment &attachment = colorAttachments[colorAttachment];
        if (attachment.isAttached())
        {
            for (size_t prevColorAttachment = 0; prevColorAttachment < colorAttachment; prevColorAttachment++)
            {
                const gl::FramebufferAttachment &prevAttachment = colorAttachments[prevColorAttachment];
                if (prevAttachment.isAttached() &&
                    (attachment.id() == prevAttachment.id() &&
                     attachment.type() == prevAttachment.type()))
                {
                    return false;
                }
            }
        }
    }

    // D3D requires all render targets to have the same dimensions.
    if (!mData.attachmentsHaveSameDimensions())
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

    const auto &colorAttachments = mData.getColorAttachments();
    const auto &drawBufferStates = mData.getDrawBufferStates();
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
