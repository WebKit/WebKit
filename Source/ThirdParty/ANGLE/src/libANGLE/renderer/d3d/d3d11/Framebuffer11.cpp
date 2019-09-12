//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer11.cpp: Implements the Framebuffer11 class.

#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Clear11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/TextureStorage11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

using namespace angle;

namespace rx
{

namespace
{
angle::Result MarkAttachmentsDirty(const gl::Context *context,
                                   const gl::FramebufferAttachment *attachment)
{
    if (attachment->type() == GL_TEXTURE)
    {
        gl::Texture *texture = attachment->getTexture();

        TextureD3D *textureD3D = GetImplAs<TextureD3D>(texture);

        TextureStorage *texStorage = nullptr;
        ANGLE_TRY(textureD3D->getNativeTexture(context, &texStorage));

        if (texStorage)
        {
            TextureStorage11 *texStorage11 = GetAs<TextureStorage11>(texStorage);
            ASSERT(texStorage11);

            texStorage11->markLevelDirty(attachment->mipLevel());
        }
    }

    return angle::Result::Continue;
}
}  // anonymous namespace

Framebuffer11::Framebuffer11(const gl::FramebufferState &data, Renderer11 *renderer)
    : FramebufferD3D(data, renderer), mRenderer(renderer)
{
    ASSERT(mRenderer != nullptr);
}

Framebuffer11::~Framebuffer11() {}

angle::Result Framebuffer11::markAttachmentsDirty(const gl::Context *context) const
{
    const auto &colorAttachments = mState.getColorAttachments();
    for (size_t drawBuffer : mState.getEnabledDrawBuffers())
    {
        const gl::FramebufferAttachment &colorAttachment = colorAttachments[drawBuffer];
        ASSERT(colorAttachment.isAttached());
        ANGLE_TRY(MarkAttachmentsDirty(context, &colorAttachment));
    }

    const gl::FramebufferAttachment *dsAttachment = mState.getDepthOrStencilAttachment();
    if (dsAttachment)
    {
        ANGLE_TRY(MarkAttachmentsDirty(context, dsAttachment));
    }

    return angle::Result::Continue;
}

angle::Result Framebuffer11::clearImpl(const gl::Context *context,
                                       const ClearParameters &clearParams)
{
    Clear11 *clearer = mRenderer->getClearer();

    const gl::FramebufferAttachment *colorAttachment = mState.getFirstColorAttachment();
    if (clearParams.scissorEnabled == true && colorAttachment != nullptr &&
        UsePresentPathFast(mRenderer, colorAttachment))
    {
        // If the current framebuffer is using the default colorbuffer, and present path fast is
        // active, and the scissor rect is enabled, then we should invert the scissor rect
        // vertically
        ClearParameters presentPathFastClearParams = clearParams;
        gl::Extents framebufferSize                = colorAttachment->getSize();
        presentPathFastClearParams.scissor.y       = framebufferSize.height -
                                               presentPathFastClearParams.scissor.y -
                                               presentPathFastClearParams.scissor.height;
        ANGLE_TRY(clearer->clearFramebuffer(context, presentPathFastClearParams, mState));
    }
    else
    {
        ANGLE_TRY(clearer->clearFramebuffer(context, clearParams, mState));
    }

    ANGLE_TRY(markAttachmentsDirty(context));

    return angle::Result::Continue;
}

angle::Result Framebuffer11::invalidate(const gl::Context *context,
                                        size_t count,
                                        const GLenum *attachments)
{
    return invalidateBase(context, count, attachments, false);
}

angle::Result Framebuffer11::discard(const gl::Context *context,
                                     size_t count,
                                     const GLenum *attachments)
{
    return invalidateBase(context, count, attachments, true);
}

angle::Result Framebuffer11::invalidateBase(const gl::Context *context,
                                            size_t count,
                                            const GLenum *attachments,
                                            bool useEXTBehavior) const
{
    ID3D11DeviceContext1 *deviceContext1 = mRenderer->getDeviceContext1IfSupported();

    if (!deviceContext1)
    {
        // DiscardView() is only supported on ID3D11DeviceContext1
        return angle::Result::Continue;
    }

    bool foundDepth   = false;
    bool foundStencil = false;

    for (size_t i = 0; i < count; ++i)
    {
        switch (attachments[i])
        {
            // Handle depth and stencil attachments. Defer discarding until later.
            case GL_DEPTH_STENCIL_ATTACHMENT:
                foundDepth   = true;
                foundStencil = true;
                break;
            case GL_DEPTH_EXT:
            case GL_DEPTH_ATTACHMENT:
                foundDepth = true;
                break;
            case GL_STENCIL_EXT:
            case GL_STENCIL_ATTACHMENT:
                foundStencil = true;
                break;
            default:
            {
                // Handle color attachments
                ASSERT((attachments[i] >= GL_COLOR_ATTACHMENT0 &&
                        attachments[i] <= GL_COLOR_ATTACHMENT15) ||
                       (attachments[i] == GL_COLOR));

                size_t colorIndex =
                    (attachments[i] == GL_COLOR ? 0u : (attachments[i] - GL_COLOR_ATTACHMENT0));
                const gl::FramebufferAttachment *colorAttachment =
                    mState.getColorAttachment(colorIndex);
                if (colorAttachment)
                {
                    ANGLE_TRY(invalidateAttachment(context, colorAttachment));
                }
                break;
            }
        }
    }

    bool discardDepth   = false;
    bool discardStencil = false;

    // The D3D11 renderer uses the same view for depth and stencil buffers, so we must be careful.
    if (useEXTBehavior)
    {
        // In the extension, if the app discards only one of the depth and stencil attachments, but
        // those are backed by the same packed_depth_stencil buffer, then both images become
        // undefined.
        discardDepth = foundDepth;

        // Don't bother discarding the stencil buffer if the depth buffer will already do it
        discardStencil = foundStencil && (!discardDepth || mState.getDepthAttachment() == nullptr);
    }
    else
    {
        // In ES 3.0.4, if a specified attachment has base internal format DEPTH_STENCIL but the
        // attachments list does not include DEPTH_STENCIL_ATTACHMENT or both DEPTH_ATTACHMENT and
        // STENCIL_ATTACHMENT, then only the specified portion of every pixel in the subregion of
        // pixels of the DEPTH_STENCIL buffer may be invalidated, and the other portion must be
        // preserved.
        discardDepth = (foundDepth && foundStencil) ||
                       (foundDepth && (mState.getStencilAttachment() == nullptr));
        discardStencil = (foundStencil && (mState.getDepthAttachment() == nullptr));
    }

    if (discardDepth && mState.getDepthAttachment())
    {
        ANGLE_TRY(invalidateAttachment(context, mState.getDepthAttachment()));
    }

    if (discardStencil && mState.getStencilAttachment())
    {
        ANGLE_TRY(invalidateAttachment(context, mState.getStencilAttachment()));
    }

    return angle::Result::Continue;
}

angle::Result Framebuffer11::invalidateSub(const gl::Context *context,
                                           size_t count,
                                           const GLenum *attachments,
                                           const gl::Rectangle &area)
{
    // A no-op implementation conforms to the spec, so don't call UNIMPLEMENTED()
    return angle::Result::Continue;
}

angle::Result Framebuffer11::invalidateAttachment(const gl::Context *context,
                                                  const gl::FramebufferAttachment *attachment) const
{
    ID3D11DeviceContext1 *deviceContext1 = mRenderer->getDeviceContext1IfSupported();
    ASSERT(deviceContext1);
    ASSERT(attachment && attachment->isAttached());

    RenderTarget11 *renderTarget = nullptr;
    ANGLE_TRY(attachment->getRenderTarget(context, &renderTarget));
    const auto &rtv = renderTarget->getRenderTargetView();

    if (rtv.valid())
    {
        deviceContext1->DiscardView(rtv.get());
    }

    return angle::Result::Continue;
}

angle::Result Framebuffer11::readPixelsImpl(const gl::Context *context,
                                            const gl::Rectangle &area,
                                            GLenum format,
                                            GLenum type,
                                            size_t outputPitch,
                                            const gl::PixelPackState &pack,
                                            uint8_t *pixels)
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();
    ASSERT(readAttachment);

    gl::Buffer *packBuffer = context->getState().getTargetBuffer(gl::BufferBinding::PixelPack);
    if (packBuffer != nullptr)
    {
        Buffer11 *packBufferStorage      = GetImplAs<Buffer11>(packBuffer);
        const angle::Format &angleFormat = GetFormatFromFormatType(format, type);
        PackPixelsParams packParams(area, angleFormat, static_cast<GLuint>(outputPitch),
                                    pack.reverseRowOrder, packBuffer,
                                    reinterpret_cast<ptrdiff_t>(pixels));

        return packBufferStorage->packPixels(context, *readAttachment, packParams);
    }

    return mRenderer->readFromAttachment(context, *readAttachment, area, format, type,
                                         static_cast<GLuint>(outputPitch), pack, pixels);
}

angle::Result Framebuffer11::blitImpl(const gl::Context *context,
                                      const gl::Rectangle &sourceArea,
                                      const gl::Rectangle &destArea,
                                      const gl::Rectangle *scissor,
                                      bool blitRenderTarget,
                                      bool blitDepth,
                                      bool blitStencil,
                                      GLenum filter,
                                      const gl::Framebuffer *sourceFramebuffer)
{
    if (blitRenderTarget)
    {
        const gl::FramebufferAttachment *readBuffer = sourceFramebuffer->getReadColorbuffer();
        ASSERT(readBuffer);

        RenderTargetD3D *readRenderTarget = nullptr;
        ANGLE_TRY(readBuffer->getRenderTarget(context, &readRenderTarget));
        ASSERT(readRenderTarget);

        const auto &colorAttachments = mState.getColorAttachments();
        const auto &drawBufferStates = mState.getDrawBufferStates();

        for (size_t colorAttachment = 0; colorAttachment < colorAttachments.size();
             colorAttachment++)
        {
            const gl::FramebufferAttachment &drawBuffer = colorAttachments[colorAttachment];

            if (drawBuffer.isAttached() && drawBufferStates[colorAttachment] != GL_NONE)
            {
                RenderTargetD3D *drawRenderTarget = nullptr;
                ANGLE_TRY(drawBuffer.getRenderTarget(context, &drawRenderTarget));
                ASSERT(drawRenderTarget);

                const bool invertColorSource   = UsePresentPathFast(mRenderer, readBuffer);
                gl::Rectangle actualSourceArea = sourceArea;
                if (invertColorSource)
                {
                    RenderTarget11 *readRenderTarget11 = GetAs<RenderTarget11>(readRenderTarget);
                    actualSourceArea.y      = readRenderTarget11->getHeight() - sourceArea.y;
                    actualSourceArea.height = -sourceArea.height;
                }

                const bool invertColorDest   = UsePresentPathFast(mRenderer, &drawBuffer);
                gl::Rectangle actualDestArea = destArea;
                if (invertColorDest)
                {
                    RenderTarget11 *drawRenderTarget11 = GetAs<RenderTarget11>(drawRenderTarget);
                    actualDestArea.y      = drawRenderTarget11->getHeight() - destArea.y;
                    actualDestArea.height = -destArea.height;
                }

                ANGLE_TRY(mRenderer->blitRenderbufferRect(
                    context, actualSourceArea, actualDestArea, readRenderTarget, drawRenderTarget,
                    filter, scissor, blitRenderTarget, false, false));
            }
        }
    }

    if (blitDepth || blitStencil)
    {
        const gl::FramebufferAttachment *readBuffer = sourceFramebuffer->getDepthOrStencilbuffer();
        ASSERT(readBuffer);
        RenderTargetD3D *readRenderTarget = nullptr;
        ANGLE_TRY(readBuffer->getRenderTarget(context, &readRenderTarget));
        ASSERT(readRenderTarget);

        const bool invertSource        = UsePresentPathFast(mRenderer, readBuffer);
        gl::Rectangle actualSourceArea = sourceArea;
        if (invertSource)
        {
            RenderTarget11 *readRenderTarget11 = GetAs<RenderTarget11>(readRenderTarget);
            actualSourceArea.y                 = readRenderTarget11->getHeight() - sourceArea.y;
            actualSourceArea.height            = -sourceArea.height;
        }

        const gl::FramebufferAttachment *drawBuffer = mState.getDepthOrStencilAttachment();
        ASSERT(drawBuffer);
        RenderTargetD3D *drawRenderTarget = nullptr;
        ANGLE_TRY(drawBuffer->getRenderTarget(context, &drawRenderTarget));
        ASSERT(drawRenderTarget);

        bool invertDest              = UsePresentPathFast(mRenderer, drawBuffer);
        gl::Rectangle actualDestArea = destArea;
        if (invertDest)
        {
            RenderTarget11 *drawRenderTarget11 = GetAs<RenderTarget11>(drawRenderTarget);
            actualDestArea.y                   = drawRenderTarget11->getHeight() - destArea.y;
            actualDestArea.height              = -destArea.height;
        }

        ANGLE_TRY(mRenderer->blitRenderbufferRect(context, actualSourceArea, actualDestArea,
                                                  readRenderTarget, drawRenderTarget, filter,
                                                  scissor, false, blitDepth, blitStencil));
    }

    ANGLE_TRY(markAttachmentsDirty(context));
    return angle::Result::Continue;
}

GLenum Framebuffer11::getRenderTargetImplementationFormat(RenderTargetD3D *renderTarget) const
{
    RenderTarget11 *renderTarget11 = GetAs<RenderTarget11>(renderTarget);
    return renderTarget11->getFormatSet().format().fboImplementationInternalFormat;
}

angle::Result Framebuffer11::syncState(const gl::Context *context,
                                       const gl::Framebuffer::DirtyBits &dirtyBits)
{
    ANGLE_TRY(mRenderTargetCache.update(context, mState, dirtyBits));
    ANGLE_TRY(FramebufferD3D::syncState(context, dirtyBits));

    // Call this last to allow the state manager to take advantage of the cached render targets.
    mRenderer->getStateManager()->invalidateRenderTarget();

    // Call this to syncViewport for framebuffer default parameters.
    if (mState.getDefaultWidth() != 0 || mState.getDefaultHeight() != 0)
    {
        mRenderer->getStateManager()->invalidateViewport(context);
    }

    return angle::Result::Continue;
}

angle::Result Framebuffer11::getSamplePosition(const gl::Context *context,
                                               size_t index,
                                               GLfloat *xy) const
{
    const gl::FramebufferAttachment *attachment = mState.getFirstNonNullAttachment();
    ASSERT(attachment);
    GLsizei sampleCount = attachment->getSamples();

    d3d11_gl::GetSamplePosition(sampleCount, index, xy);
    return angle::Result::Continue;
}

RenderTarget11 *Framebuffer11::getFirstRenderTarget() const
{
    for (auto *renderTarget : mRenderTargetCache.getColors())
    {
        if (renderTarget)
        {
            return renderTarget;
        }
    }

    return mRenderTargetCache.getDepthStencil();
}

}  // namespace rx
