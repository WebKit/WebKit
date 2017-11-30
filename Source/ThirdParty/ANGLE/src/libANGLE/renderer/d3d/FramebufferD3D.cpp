//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferD3D.cpp: Implements the DefaultAttachmentD3D and FramebufferD3D classes.

#include "libANGLE/renderer/d3d/FramebufferD3D.h"

#include "common/bitset_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Surface.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/d3d/RenderTargetD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
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
    clearParams.colorF           = state.getColorClearValue();
    clearParams.colorType        = GL_FLOAT;
    clearParams.colorMaskRed     = blendState.colorMaskRed;
    clearParams.colorMaskGreen   = blendState.colorMaskGreen;
    clearParams.colorMaskBlue    = blendState.colorMaskBlue;
    clearParams.colorMaskAlpha   = blendState.colorMaskAlpha;
    clearParams.clearDepth       = false;
    clearParams.depthValue       = state.getDepthClearValue();
    clearParams.clearStencil     = false;
    clearParams.stencilValue     = state.getStencilClearValue();
    clearParams.stencilWriteMask = state.getDepthStencilState().stencilWritemask;
    clearParams.scissorEnabled   = state.isScissorTestEnabled();
    clearParams.scissor          = state.getScissor();

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
        if (state.getDepthStencilState().depthMask &&
            framebufferObject->getDepthbuffer() != nullptr)
        {
            clearParams.clearDepth = true;
        }
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        if (framebufferObject->getStencilbuffer() != nullptr &&
            framebufferObject->getStencilbuffer()->getStencilSize() > 0)
        {
            clearParams.clearStencil = true;
        }
    }

    return clearParams;
}
}

ClearParameters::ClearParameters() = default;

ClearParameters::ClearParameters(const ClearParameters &other) = default;

FramebufferD3D::FramebufferD3D(const gl::FramebufferState &data, RendererD3D *renderer)
    : FramebufferImpl(data), mRenderer(renderer)
{
}

FramebufferD3D::~FramebufferD3D()
{
}

gl::Error FramebufferD3D::clear(const gl::Context *context, GLbitfield mask)
{
    ClearParameters clearParams = GetClearParameters(context->getGLState(), mask);
    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferfv(const gl::Context *context,
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
        clearParams.colorF    = gl::ColorF(values[0], values[1], values[2], values[3]);
        clearParams.colorType = GL_FLOAT;
    }

    if (buffer == GL_DEPTH)
    {
        clearParams.clearDepth = true;
        clearParams.depthValue = values[0];
    }

    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferuiv(const gl::Context *context,
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
    clearParams.colorUI   = gl::ColorUI(values[0], values[1], values[2], values[3]);
    clearParams.colorType = GL_UNSIGNED_INT;

    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferiv(const gl::Context *context,
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
        clearParams.colorI    = gl::ColorI(values[0], values[1], values[2], values[3]);
        clearParams.colorType = GL_INT;
    }

    if (buffer == GL_STENCIL)
    {
        clearParams.clearStencil = true;
        clearParams.stencilValue = values[0];
    }

    return clearImpl(context, clearParams);
}

gl::Error FramebufferD3D::clearBufferfi(const gl::Context *context,
                                        GLenum buffer,
                                        GLint drawbuffer,
                                        GLfloat depth,
                                        GLint stencil)
{
    // glClearBufferfi can only be called to clear a depth stencil buffer
    ClearParameters clearParams   = GetClearParameters(context->getGLState(), 0);
    clearParams.clearDepth        = true;
    clearParams.depthValue        = depth;
    clearParams.clearStencil      = true;
    clearParams.stencilValue      = stencil;

    return clearImpl(context, clearParams);
}

GLenum FramebufferD3D::getImplementationColorReadFormat(const gl::Context *context) const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();

    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    RenderTargetD3D *attachmentRenderTarget = nullptr;
    gl::Error error = readAttachment->getRenderTarget(context, &attachmentRenderTarget);
    if (error.isError())
    {
        return GL_NONE;
    }

    GLenum implementationFormat = getRenderTargetImplementationFormat(attachmentRenderTarget);
    const gl::InternalFormat &implementationFormatInfo =
        gl::GetSizedInternalFormatInfo(implementationFormat);

    return implementationFormatInfo.getReadPixelsFormat();
}

GLenum FramebufferD3D::getImplementationColorReadType(const gl::Context *context) const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();

    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    RenderTargetD3D *attachmentRenderTarget = nullptr;
    gl::Error error = readAttachment->getRenderTarget(context, &attachmentRenderTarget);
    if (error.isError())
    {
        return GL_NONE;
    }

    GLenum implementationFormat = getRenderTargetImplementationFormat(attachmentRenderTarget);
    const gl::InternalFormat &implementationFormatInfo =
        gl::GetSizedInternalFormatInfo(implementationFormat);

    return implementationFormatInfo.getReadPixelsType(context->getClientVersion());
}

gl::Error FramebufferD3D::readPixels(const gl::Context *context,
                                     const gl::Rectangle &origArea,
                                     GLenum format,
                                     GLenum type,
                                     void *pixels)
{
    // Clip read area to framebuffer.
    const gl::Extents fbSize = getState().getReadAttachment()->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    gl::Rectangle area;
    if (!ClipRectangle(origArea, fbRect, &area))
    {
        // nothing to read
        return gl::NoError();
    }

    const gl::PixelPackState &packState = context->getGLState().getPackState();

    const gl::InternalFormat &sizedFormatInfo = gl::GetInternalFormatInfo(format, type);

    GLuint outputPitch = 0;
    ANGLE_TRY_RESULT(sizedFormatInfo.computeRowPitch(type, origArea.width, packState.alignment,
                                                     packState.rowLength),
                     outputPitch);
    GLuint outputSkipBytes = 0;
    ANGLE_TRY_RESULT(sizedFormatInfo.computeSkipBytes(outputPitch, 0, packState, false),
                     outputSkipBytes);
    outputSkipBytes +=
        (area.x - origArea.x) * sizedFormatInfo.pixelBytes + (area.y - origArea.y) * outputPitch;

    return readPixelsImpl(context, area, format, type, outputPitch, packState,
                          reinterpret_cast<uint8_t *>(pixels) + outputSkipBytes);
}

gl::Error FramebufferD3D::blit(const gl::Context *context,
                               const gl::Rectangle &sourceArea,
                               const gl::Rectangle &destArea,
                               GLbitfield mask,
                               GLenum filter)
{
    const auto &glState                      = context->getGLState();
    const gl::Framebuffer *sourceFramebuffer = glState.getReadFramebuffer();
    const gl::Rectangle *scissor = glState.isScissorTestEnabled() ? &glState.getScissor() : nullptr;
    ANGLE_TRY(blitImpl(context, sourceArea, destArea, scissor, (mask & GL_COLOR_BUFFER_BIT) != 0,
                       (mask & GL_DEPTH_BUFFER_BIT) != 0, (mask & GL_STENCIL_BUFFER_BIT) != 0,
                       filter, sourceFramebuffer));

    return gl::NoError();
}

bool FramebufferD3D::checkStatus(const gl::Context *context) const
{
    // if we have both a depth and stencil buffer, they must refer to the same object
    // since we only support packed_depth_stencil and not separate depth and stencil
    if (mState.getDepthAttachment() != nullptr && mState.getStencilAttachment() != nullptr &&
        mState.getDepthStencilAttachment() == nullptr)
    {
        return false;
    }

    // D3D11 does not allow for overlapping RenderTargetViews.
    // If WebGL compatibility is enabled, this has already been checked at a higher level.
    ASSERT(!context->getExtensions().webglCompatibility || mState.colorAttachmentsAreUniqueImages());
    if (!context->getExtensions().webglCompatibility)
    {
        if (!mState.colorAttachmentsAreUniqueImages())
        {
            return false;
        }
    }

    // D3D requires all render targets to have the same dimensions.
    if (!mState.attachmentsHaveSameDimensions())
    {
        return false;
    }

    return true;
}

void FramebufferD3D::syncState(const gl::Context *context,
                               const gl::Framebuffer::DirtyBits &dirtyBits)
{
    if (!mColorAttachmentsForRender.valid())
    {
        return;
    }

    for (auto dirtyBit : dirtyBits)
    {
        if ((dirtyBit >= gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 &&
             dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX) ||
            dirtyBit == gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS)
        {
            mColorAttachmentsForRender.reset();
        }
    }
}

const gl::AttachmentList &FramebufferD3D::getColorAttachmentsForRender(const gl::Context *context)
{
    gl::DrawBufferMask activeProgramOutputs =
        context->getContextState().getState().getProgram()->getActiveOutputVariables();

    if (mColorAttachmentsForRender.valid() && mCurrentActiveProgramOutputs == activeProgramOutputs)
    {
        return mColorAttachmentsForRender.value();
    }

    // Does not actually free memory
    gl::AttachmentList colorAttachmentsForRender;

    const auto &colorAttachments = mState.getColorAttachments();
    const auto &drawBufferStates = mState.getDrawBufferStates();
    const auto &workarounds      = mRenderer->getWorkarounds();

    for (size_t attachmentIndex = 0; attachmentIndex < colorAttachments.size(); ++attachmentIndex)
    {
        GLenum drawBufferState                           = drawBufferStates[attachmentIndex];
        const gl::FramebufferAttachment &colorAttachment = colorAttachments[attachmentIndex];

        if (colorAttachment.isAttached() && drawBufferState != GL_NONE &&
            activeProgramOutputs[attachmentIndex])
        {
            ASSERT(drawBufferState == GL_BACK ||
                   drawBufferState == (GL_COLOR_ATTACHMENT0_EXT + attachmentIndex));
            colorAttachmentsForRender.push_back(&colorAttachment);
        }
        else if (!workarounds.mrtPerfWorkaround)
        {
            colorAttachmentsForRender.push_back(nullptr);
        }
    }

    // When rendering with no render target on D3D, two bugs lead to incorrect behavior on Intel
    // drivers < 4815. The rendering samples always pass neglecting discard statements in pixel
    // shader. We add a dummy texture as render target in such case.
    if (mRenderer->getWorkarounds().addDummyTextureNoRenderTarget &&
        colorAttachmentsForRender.empty())
    {
        static_assert(static_cast<size_t>(activeProgramOutputs.size()) <= 32,
                      "Size of active program outputs should less or equal than 32.");
        GLenum i = static_cast<GLenum>(
            gl::ScanForward(static_cast<uint32_t>(activeProgramOutputs.bits())));

        gl::Texture *dummyTex = nullptr;
        // TODO(Jamie): Handle error if dummy texture can't be created.
        ANGLE_SWALLOW_ERR(mRenderer->getIncompleteTexture(context, GL_TEXTURE_2D, &dummyTex));
        if (dummyTex)
        {
            gl::ImageIndex index                   = gl::ImageIndex::Make2D(0);
            gl::FramebufferAttachment *dummyAttach = new gl::FramebufferAttachment(
                context, GL_TEXTURE, GL_COLOR_ATTACHMENT0_EXT + i, index, dummyTex);
            colorAttachmentsForRender.push_back(dummyAttach);
        }
    }

    mColorAttachmentsForRender = std::move(colorAttachmentsForRender);
    mCurrentActiveProgramOutputs = activeProgramOutputs;

    return mColorAttachmentsForRender.value();
}

}  // namespace rx
