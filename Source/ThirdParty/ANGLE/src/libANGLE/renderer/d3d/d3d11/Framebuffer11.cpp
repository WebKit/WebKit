//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer11.cpp: Implements the Framebuffer11 class.

#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"

#include "common/debug.h"
#include "common/BitSetIterator.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Clear11.h"
#include "libANGLE/renderer/d3d/d3d11/TextureStorage11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/RenderTarget11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Texture.h"

using namespace angle;

namespace rx
{

namespace
{
gl::Error MarkAttachmentsDirty(const gl::FramebufferAttachment *attachment)
{
    if (attachment && attachment->type() == GL_TEXTURE)
    {
        gl::Texture *texture = attachment->getTexture();

        TextureD3D *textureD3D = GetImplAs<TextureD3D>(texture);

        TextureStorage *texStorage = nullptr;
        ANGLE_TRY(textureD3D->getNativeTexture(&texStorage));

        if (texStorage)
        {
            TextureStorage11 *texStorage11 = GetAs<TextureStorage11>(texStorage);
            ASSERT(texStorage11);

            texStorage11->markLevelDirty(attachment->mipLevel());
        }
    }

    return gl::NoError();
}

void UpdateCachedRenderTarget(const gl::FramebufferAttachment *attachment,
                              RenderTarget11 *&cachedRenderTarget,
                              ChannelBinding *channelBinding)
{
    RenderTarget11 *newRenderTarget = nullptr;
    if (attachment)
    {
        attachment->getRenderTarget(&newRenderTarget);
    }
    if (newRenderTarget != cachedRenderTarget)
    {
        auto channel = (newRenderTarget ? newRenderTarget->getBroadcastChannel() : nullptr);
        channelBinding->bind(channel);
        cachedRenderTarget = newRenderTarget;
    }
}
}  // anonymous namespace

Framebuffer11::Framebuffer11(const gl::FramebufferState &data, Renderer11 *renderer)
    : FramebufferD3D(data, renderer),
      mRenderer(renderer),
      mCachedDepthStencilRenderTarget(nullptr),
      mDepthStencilRenderTargetDirty(this, gl::IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS)
{
    ASSERT(mRenderer != nullptr);
    mCachedColorRenderTargets.fill(nullptr);
    for (size_t colorIndex = 0; colorIndex < data.getColorAttachments().size(); ++colorIndex)
    {
        mColorRenderTargetsDirty.push_back(
            ChannelBinding(this, static_cast<SignalToken>(colorIndex)));
    }
}

Framebuffer11::~Framebuffer11()
{
}

gl::Error Framebuffer11::markAttachmentsDirty() const
{
    for (const auto &colorAttachment : mState.getColorAttachments())
    {
        if (colorAttachment.isAttached())
        {
            ANGLE_TRY(MarkAttachmentsDirty(&colorAttachment));
        }
    }

    ANGLE_TRY(MarkAttachmentsDirty(mState.getDepthAttachment()));
    ANGLE_TRY(MarkAttachmentsDirty(mState.getStencilAttachment()));

    return gl::NoError();
}

gl::Error Framebuffer11::clearImpl(ContextImpl *context, const ClearParameters &clearParams)
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
        ANGLE_TRY(clearer->clearFramebuffer(presentPathFastClearParams, mState));
    }
    else
    {
        ANGLE_TRY(clearer->clearFramebuffer(clearParams, mState));
    }

    ANGLE_TRY(markAttachmentsDirty());

    return gl::NoError();
}

gl::Error Framebuffer11::invalidate(size_t count, const GLenum *attachments)
{
    return invalidateBase(count, attachments, false);
}

gl::Error Framebuffer11::discard(size_t count, const GLenum *attachments)
{
    return invalidateBase(count, attachments, true);
}

gl::Error Framebuffer11::invalidateBase(size_t count, const GLenum *attachments, bool useEXTBehavior) const
{
    ID3D11DeviceContext1 *deviceContext1 = mRenderer->getDeviceContext1IfSupported();

    if (!deviceContext1)
    {
        // DiscardView() is only supported on ID3D11DeviceContext1
        return gl::NoError();
    }

    bool foundDepth = false;
    bool foundStencil = false;

    for (size_t i = 0; i < count; ++i)
    {
        switch (attachments[i])
        {
          // Handle depth and stencil attachments. Defer discarding until later.
          case GL_DEPTH_STENCIL_ATTACHMENT:
            foundDepth = true;
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
                ASSERT((attachments[i] >= GL_COLOR_ATTACHMENT0 && attachments[i] <= GL_COLOR_ATTACHMENT15) ||
                       (attachments[i] == GL_COLOR));

                size_t colorIndex =
                    (attachments[i] == GL_COLOR ? 0u : (attachments[i] - GL_COLOR_ATTACHMENT0));
                auto colorAttachment = mState.getColorAttachment(colorIndex);
                ANGLE_TRY(invalidateAttachment(colorAttachment));
                break;
            }
        }
    }

    bool discardDepth = false;
    bool discardStencil = false;

    // The D3D11 renderer uses the same view for depth and stencil buffers, so we must be careful.
    if (useEXTBehavior)
    {
        // In the extension, if the app discards only one of the depth and stencil attachments, but
        // those are backed by the same packed_depth_stencil buffer, then both images become undefined.
        discardDepth = foundDepth;

        // Don't bother discarding the stencil buffer if the depth buffer will already do it
        discardStencil = foundStencil && (!discardDepth || mState.getDepthAttachment() == nullptr);
    }
    else
    {
        // In ES 3.0.4, if a specified attachment has base internal format DEPTH_STENCIL but the
        // attachments list does not include DEPTH_STENCIL_ATTACHMENT or both DEPTH_ATTACHMENT and
        // STENCIL_ATTACHMENT, then only the specified portion of every pixel in the subregion of pixels
        // of the DEPTH_STENCIL buffer may be invalidated, and the other portion must be preserved.
        discardDepth = (foundDepth && foundStencil) ||
                       (foundDepth && (mState.getStencilAttachment() == nullptr));
        discardStencil = (foundStencil && (mState.getDepthAttachment() == nullptr));
    }

    if (discardDepth && mState.getDepthAttachment())
    {
        ANGLE_TRY(invalidateAttachment(mState.getDepthAttachment()));
    }

    if (discardStencil && mState.getStencilAttachment())
    {
        ANGLE_TRY(invalidateAttachment(mState.getStencilAttachment()));
    }

    return gl::NoError();
}

gl::Error Framebuffer11::invalidateSub(size_t, const GLenum *, const gl::Rectangle &)
{
    // A no-op implementation conforms to the spec, so don't call UNIMPLEMENTED()
    return gl::NoError();
}

gl::Error Framebuffer11::invalidateAttachment(const gl::FramebufferAttachment *attachment) const
{
    ID3D11DeviceContext1 *deviceContext1 = mRenderer->getDeviceContext1IfSupported();
    ASSERT(deviceContext1);
    ASSERT(attachment && attachment->isAttached());

    RenderTarget11 *renderTarget = nullptr;
    ANGLE_TRY(attachment->getRenderTarget(&renderTarget));
    ID3D11View *view = renderTarget->getRenderTargetView();

    if (view != nullptr)
    {
        deviceContext1->DiscardView(view);
    }

    return gl::NoError();
}

gl::Error Framebuffer11::readPixelsImpl(const gl::Rectangle &area,
                                        GLenum format,
                                        GLenum type,
                                        size_t outputPitch,
                                        const gl::PixelPackState &pack,
                                        uint8_t *pixels) const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();
    ASSERT(readAttachment);

    gl::Buffer *packBuffer = pack.pixelBuffer.get();
    if (packBuffer != nullptr)
    {
        Buffer11 *packBufferStorage = GetImplAs<Buffer11>(packBuffer);
        PackPixelsParams packParams(area, format, type, static_cast<GLuint>(outputPitch), pack,
                                    reinterpret_cast<ptrdiff_t>(pixels));

        return packBufferStorage->packPixels(*readAttachment, packParams);
    }

    return mRenderer->readFromAttachment(*readAttachment, area, format, type,
                                         static_cast<GLuint>(outputPitch), pack, pixels);
}

gl::Error Framebuffer11::blitImpl(const gl::Rectangle &sourceArea,
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
        ANGLE_TRY(readBuffer->getRenderTarget(&readRenderTarget));
        ASSERT(readRenderTarget);

        const auto &colorAttachments = mState.getColorAttachments();
        const auto &drawBufferStates = mState.getDrawBufferStates();

        for (size_t colorAttachment = 0; colorAttachment < colorAttachments.size(); colorAttachment++)
        {
            const gl::FramebufferAttachment &drawBuffer = colorAttachments[colorAttachment];

            if (drawBuffer.isAttached() &&
                drawBufferStates[colorAttachment] != GL_NONE)
            {
                RenderTargetD3D *drawRenderTarget = nullptr;
                ANGLE_TRY(drawBuffer.getRenderTarget(&drawRenderTarget));
                ASSERT(drawRenderTarget);

                const bool invertColorSource   = UsePresentPathFast(mRenderer, readBuffer);
                gl::Rectangle actualSourceArea = sourceArea;
                if (invertColorSource)
                {
                    RenderTarget11 *readRenderTarget11 = GetAs<RenderTarget11>(readRenderTarget);
                    actualSourceArea.y                 = readRenderTarget11->getHeight() - sourceArea.y;
                    actualSourceArea.height            = -sourceArea.height;
                }

                const bool invertColorDest   = UsePresentPathFast(mRenderer, &drawBuffer);
                gl::Rectangle actualDestArea = destArea;
                if (invertColorDest)
                {
                    RenderTarget11 *drawRenderTarget11 = GetAs<RenderTarget11>(drawRenderTarget);
                    actualDestArea.y                   = drawRenderTarget11->getHeight() - destArea.y;
                    actualDestArea.height              = -destArea.height;
                }

                ANGLE_TRY(mRenderer->blitRenderbufferRect(
                    actualSourceArea, actualDestArea, readRenderTarget, drawRenderTarget, filter,
                    scissor, blitRenderTarget, false, false));
            }
        }
    }

    if (blitDepth || blitStencil)
    {
        const gl::FramebufferAttachment *readBuffer = sourceFramebuffer->getDepthOrStencilbuffer();
        ASSERT(readBuffer);

        RenderTargetD3D *readRenderTarget = nullptr;
        ANGLE_TRY(readBuffer->getRenderTarget(&readRenderTarget));
        ASSERT(readRenderTarget);

        const gl::FramebufferAttachment *drawBuffer = mState.getDepthOrStencilAttachment();
        ASSERT(drawBuffer);

        RenderTargetD3D *drawRenderTarget = nullptr;
        ANGLE_TRY(drawBuffer->getRenderTarget(&drawRenderTarget));
        ASSERT(drawRenderTarget);

        ANGLE_TRY(mRenderer->blitRenderbufferRect(sourceArea, destArea, readRenderTarget,
                                                  drawRenderTarget, filter, scissor, false,
                                                  blitDepth, blitStencil));
    }

    ANGLE_TRY(markAttachmentsDirty());
    return gl::NoError();
}

GLenum Framebuffer11::getRenderTargetImplementationFormat(RenderTargetD3D *renderTarget) const
{
    RenderTarget11 *renderTarget11 = GetAs<RenderTarget11>(renderTarget);
    return renderTarget11->getFormatSet().format.fboImplementationInternalFormat;
}

void Framebuffer11::updateColorRenderTarget(size_t colorIndex)
{
    UpdateCachedRenderTarget(mState.getColorAttachment(colorIndex),
                             mCachedColorRenderTargets[colorIndex],
                             &mColorRenderTargetsDirty[colorIndex]);
}

void Framebuffer11::updateDepthStencilRenderTarget()
{
    UpdateCachedRenderTarget(mState.getDepthOrStencilAttachment(), mCachedDepthStencilRenderTarget,
                             &mDepthStencilRenderTargetDirty);
}

void Framebuffer11::syncState(const gl::Framebuffer::DirtyBits &dirtyBits)
{
    mRenderer->getStateManager()->invalidateRenderTarget();

    const auto &mergedDirtyBits = dirtyBits | mInternalDirtyBits;
    mInternalDirtyBits.reset();

    for (auto dirtyBit : IterateBitSet(mergedDirtyBits))
    {
        switch (dirtyBit)
        {
            case gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT:
            case gl::Framebuffer::DIRTY_BIT_STENCIL_ATTACHMENT:
                updateDepthStencilRenderTarget();
                break;
            case gl::Framebuffer::DIRTY_BIT_DRAW_BUFFERS:
            case gl::Framebuffer::DIRTY_BIT_READ_BUFFER:
                break;
            default:
            {
                ASSERT(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 == 0 &&
                       dirtyBit < gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_MAX);
                size_t colorIndex =
                    static_cast<size_t>(dirtyBit - gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0);
                updateColorRenderTarget(colorIndex);
                break;
            }
        }
    }

    // We should not have dirtied any additional state during our sync.
    ASSERT(!mInternalDirtyBits.any());

    FramebufferD3D::syncState(dirtyBits);
}

void Framebuffer11::signal(SignalToken token)
{
    if (token == gl::IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS)
    {
        // Stencil is redundant in this case.
        mInternalDirtyBits.set(gl::Framebuffer::DIRTY_BIT_DEPTH_ATTACHMENT);
        mCachedDepthStencilRenderTarget = nullptr;
    }
    else
    {
        mInternalDirtyBits.set(gl::Framebuffer::DIRTY_BIT_COLOR_ATTACHMENT_0 + token);
        mCachedColorRenderTargets[token] = nullptr;
    }
}

bool Framebuffer11::hasAnyInternalDirtyBit() const
{
    return mInternalDirtyBits.any();
}

void Framebuffer11::syncInternalState()
{
    syncState(gl::Framebuffer::DirtyBits());
}

}  // namespace rx
