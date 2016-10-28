//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libANGLE/Framebuffer.h"

#include "common/Optional.h"
#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/RenderbufferImpl.h"
#include "libANGLE/renderer/SurfaceImpl.h"

using namespace angle;

namespace gl
{

namespace
{

void BindResourceChannel(ChannelBinding *binding, FramebufferAttachmentObject *resource)
{
    binding->bind(resource ? resource->getDirtyChannel() : nullptr);
}

}  // anonymous namespace

FramebufferState::FramebufferState()
    : mLabel(),
      mColorAttachments(1),
      mDrawBufferStates(1, GL_NONE),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT)
{
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
}

FramebufferState::FramebufferState(const Caps &caps)
    : mLabel(),
      mColorAttachments(caps.maxColorAttachments),
      mDrawBufferStates(caps.maxDrawBuffers, GL_NONE),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT)
{
    ASSERT(mDrawBufferStates.size() > 0);
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
}

FramebufferState::~FramebufferState()
{
}

const std::string &FramebufferState::getLabel()
{
    return mLabel;
}

const FramebufferAttachment *FramebufferState::getAttachment(GLenum attachment) const
{
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15)
    {
        return getColorAttachment(attachment - GL_COLOR_ATTACHMENT0);
    }

    switch (attachment)
    {
        case GL_COLOR:
        case GL_BACK:
            return getColorAttachment(0);
        case GL_DEPTH:
        case GL_DEPTH_ATTACHMENT:
            return getDepthAttachment();
        case GL_STENCIL:
        case GL_STENCIL_ATTACHMENT:
            return getStencilAttachment();
        case GL_DEPTH_STENCIL:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            return getDepthStencilAttachment();
        default:
            UNREACHABLE();
            return nullptr;
    }
}

const FramebufferAttachment *FramebufferState::getReadAttachment() const
{
    ASSERT(mReadBufferState == GL_BACK || (mReadBufferState >= GL_COLOR_ATTACHMENT0 && mReadBufferState <= GL_COLOR_ATTACHMENT15));
    size_t readIndex = (mReadBufferState == GL_BACK ? 0 : static_cast<size_t>(mReadBufferState - GL_COLOR_ATTACHMENT0));
    ASSERT(readIndex < mColorAttachments.size());
    return mColorAttachments[readIndex].isAttached() ? &mColorAttachments[readIndex] : nullptr;
}

const FramebufferAttachment *FramebufferState::getFirstColorAttachment() const
{
    for (const FramebufferAttachment &colorAttachment : mColorAttachments)
    {
        if (colorAttachment.isAttached())
        {
            return &colorAttachment;
        }
    }

    return nullptr;
}

const FramebufferAttachment *FramebufferState::getDepthOrStencilAttachment() const
{
    if (mDepthAttachment.isAttached())
    {
        return &mDepthAttachment;
    }
    if (mStencilAttachment.isAttached())
    {
        return &mStencilAttachment;
    }
    return nullptr;
}

const FramebufferAttachment *FramebufferState::getColorAttachment(size_t colorAttachment) const
{
    ASSERT(colorAttachment < mColorAttachments.size());
    return mColorAttachments[colorAttachment].isAttached() ?
           &mColorAttachments[colorAttachment] :
           nullptr;
}

const FramebufferAttachment *FramebufferState::getDepthAttachment() const
{
    return mDepthAttachment.isAttached() ? &mDepthAttachment : nullptr;
}

const FramebufferAttachment *FramebufferState::getStencilAttachment() const
{
    return mStencilAttachment.isAttached() ? &mStencilAttachment : nullptr;
}

const FramebufferAttachment *FramebufferState::getDepthStencilAttachment() const
{
    // A valid depth-stencil attachment has the same resource bound to both the
    // depth and stencil attachment points.
    if (mDepthAttachment.isAttached() && mStencilAttachment.isAttached() &&
        mDepthAttachment.type() == mStencilAttachment.type() &&
        mDepthAttachment.id() == mStencilAttachment.id())
    {
        return &mDepthAttachment;
    }

    return nullptr;
}

bool FramebufferState::attachmentsHaveSameDimensions() const
{
    Optional<Extents> attachmentSize;

    auto hasMismatchedSize = [&attachmentSize](const FramebufferAttachment &attachment)
    {
        if (!attachment.isAttached())
        {
            return false;
        }

        if (!attachmentSize.valid())
        {
            attachmentSize = attachment.getSize();
            return false;
        }

        return (attachment.getSize() != attachmentSize.value());
    };

    for (const auto &attachment : mColorAttachments)
    {
        if (hasMismatchedSize(attachment))
        {
            return false;
        }
    }

    if (hasMismatchedSize(mDepthAttachment))
    {
        return false;
    }

    return !hasMismatchedSize(mStencilAttachment);
}

const gl::FramebufferAttachment *FramebufferState::getDrawBuffer(size_t drawBufferIdx) const
{
    ASSERT(drawBufferIdx < mDrawBufferStates.size());
    if (mDrawBufferStates[drawBufferIdx] != GL_NONE)
    {
        // ES3 spec: "If the GL is bound to a draw framebuffer object, the ith buffer listed in bufs
        // must be COLOR_ATTACHMENTi or NONE"
        ASSERT(mDrawBufferStates[drawBufferIdx] == GL_COLOR_ATTACHMENT0 + drawBufferIdx ||
               (drawBufferIdx == 0 && mDrawBufferStates[drawBufferIdx] == GL_BACK));
        return getAttachment(mDrawBufferStates[drawBufferIdx]);
    }
    else
    {
        return nullptr;
    }
}

size_t FramebufferState::getDrawBufferCount() const
{
    return mDrawBufferStates.size();
}

bool FramebufferState::colorAttachmentsAreUniqueImages() const
{
    for (size_t firstAttachmentIdx = 0; firstAttachmentIdx < mColorAttachments.size();
         firstAttachmentIdx++)
    {
        const gl::FramebufferAttachment &firstAttachment = mColorAttachments[firstAttachmentIdx];
        if (!firstAttachment.isAttached())
        {
            continue;
        }

        for (size_t secondAttachmentIdx = firstAttachmentIdx + 1;
             secondAttachmentIdx < mColorAttachments.size(); secondAttachmentIdx++)
        {
            const gl::FramebufferAttachment &secondAttachment =
                mColorAttachments[secondAttachmentIdx];
            if (!secondAttachment.isAttached())
            {
                continue;
            }

            if (firstAttachment == secondAttachment)
            {
                return false;
            }
        }
    }

    return true;
}

Framebuffer::Framebuffer(const Caps &caps, rx::GLImplFactory *factory, GLuint id)
    : mState(caps),
      mImpl(factory->createFramebuffer(mState)),
      mId(id),
      mCachedStatus(),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    ASSERT(mId != 0);
    ASSERT(mImpl != nullptr);
    ASSERT(mState.mColorAttachments.size() == static_cast<size_t>(caps.maxColorAttachments));

    for (size_t colorIndex = 0; colorIndex < mState.mColorAttachments.size(); ++colorIndex)
    {
        mDirtyColorAttachmentBindings.push_back(ChannelBinding(
            this, static_cast<SignalToken>(DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex)));
    }
}

Framebuffer::Framebuffer(rx::SurfaceImpl *surface)
    : mState(),
      mImpl(surface->createDefaultFramebuffer(mState)),
      mId(0),
      mCachedStatus(GL_FRAMEBUFFER_COMPLETE),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    ASSERT(mImpl != nullptr);
    mDirtyColorAttachmentBindings.push_back(
        ChannelBinding(this, static_cast<SignalToken>(DIRTY_BIT_COLOR_ATTACHMENT_0)));
}

Framebuffer::~Framebuffer()
{
    SafeDelete(mImpl);
}

void Framebuffer::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &Framebuffer::getLabel() const
{
    return mState.mLabel;
}

void Framebuffer::detachTexture(GLuint textureId)
{
    detachResourceById(GL_TEXTURE, textureId);
}

void Framebuffer::detachRenderbuffer(GLuint renderbufferId)
{
    detachResourceById(GL_RENDERBUFFER, renderbufferId);
}

void Framebuffer::detachResourceById(GLenum resourceType, GLuint resourceId)
{
    for (size_t colorIndex = 0; colorIndex < mState.mColorAttachments.size(); ++colorIndex)
    {
        detachMatchingAttachment(&mState.mColorAttachments[colorIndex], resourceType, resourceId,
                                 DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex);
    }

    detachMatchingAttachment(&mState.mDepthAttachment, resourceType, resourceId,
                             DIRTY_BIT_DEPTH_ATTACHMENT);
    detachMatchingAttachment(&mState.mStencilAttachment, resourceType, resourceId,
                             DIRTY_BIT_STENCIL_ATTACHMENT);
}

void Framebuffer::detachMatchingAttachment(FramebufferAttachment *attachment,
                                           GLenum matchType,
                                           GLuint matchId,
                                           size_t dirtyBit)
{
    if (attachment->isAttached() && attachment->type() == matchType && attachment->id() == matchId)
    {
        attachment->detach();
        mDirtyBits.set(dirtyBit);
    }
}

const FramebufferAttachment *Framebuffer::getColorbuffer(size_t colorAttachment) const
{
    return mState.getColorAttachment(colorAttachment);
}

const FramebufferAttachment *Framebuffer::getDepthbuffer() const
{
    return mState.getDepthAttachment();
}

const FramebufferAttachment *Framebuffer::getStencilbuffer() const
{
    return mState.getStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getDepthStencilBuffer() const
{
    return mState.getDepthStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getDepthOrStencilbuffer() const
{
    return mState.getDepthOrStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getReadColorbuffer() const
{
    return mState.getReadAttachment();
}

GLenum Framebuffer::getReadColorbufferType() const
{
    const FramebufferAttachment *readAttachment = mState.getReadAttachment();
    return (readAttachment != nullptr ? readAttachment->type() : GL_NONE);
}

const FramebufferAttachment *Framebuffer::getFirstColorbuffer() const
{
    return mState.getFirstColorAttachment();
}

const FramebufferAttachment *Framebuffer::getAttachment(GLenum attachment) const
{
    return mState.getAttachment(attachment);
}

size_t Framebuffer::getDrawbufferStateCount() const
{
    return mState.mDrawBufferStates.size();
}

GLenum Framebuffer::getDrawBufferState(size_t drawBuffer) const
{
    ASSERT(drawBuffer < mState.mDrawBufferStates.size());
    return mState.mDrawBufferStates[drawBuffer];
}

const std::vector<GLenum> &Framebuffer::getDrawBufferStates() const
{
    return mState.getDrawBufferStates();
}

void Framebuffer::setDrawBuffers(size_t count, const GLenum *buffers)
{
    auto &drawStates = mState.mDrawBufferStates;

    ASSERT(count <= drawStates.size());
    std::copy(buffers, buffers + count, drawStates.begin());
    std::fill(drawStates.begin() + count, drawStates.end(), GL_NONE);
    mDirtyBits.set(DIRTY_BIT_DRAW_BUFFERS);
}

const FramebufferAttachment *Framebuffer::getDrawBuffer(size_t drawBuffer) const
{
    return mState.getDrawBuffer(drawBuffer);
}

bool Framebuffer::hasEnabledDrawBuffer() const
{
    for (size_t drawbufferIdx = 0; drawbufferIdx < mState.mDrawBufferStates.size(); ++drawbufferIdx)
    {
        if (getDrawBuffer(drawbufferIdx) != nullptr)
        {
            return true;
        }
    }

    return false;
}

GLenum Framebuffer::getReadBufferState() const
{
    return mState.mReadBufferState;
}

void Framebuffer::setReadBuffer(GLenum buffer)
{
    ASSERT(buffer == GL_BACK || buffer == GL_NONE ||
           (buffer >= GL_COLOR_ATTACHMENT0 &&
            (buffer - GL_COLOR_ATTACHMENT0) < mState.mColorAttachments.size()));
    mState.mReadBufferState = buffer;
    mDirtyBits.set(DIRTY_BIT_READ_BUFFER);
}

size_t Framebuffer::getNumColorBuffers() const
{
    return mState.mColorAttachments.size();
}

bool Framebuffer::hasDepth() const
{
    return (mState.mDepthAttachment.isAttached() && mState.mDepthAttachment.getDepthSize() > 0);
}

bool Framebuffer::hasStencil() const
{
    return (mState.mStencilAttachment.isAttached() &&
            mState.mStencilAttachment.getStencilSize() > 0);
}

bool Framebuffer::usingExtendedDrawBuffers() const
{
    for (size_t drawbufferIdx = 1; drawbufferIdx < mState.mDrawBufferStates.size(); ++drawbufferIdx)
    {
        if (getDrawBuffer(drawbufferIdx) != nullptr)
        {
            return true;
        }
    }

    return false;
}

GLenum Framebuffer::checkStatus(const ContextState &state)
{
    // The default framebuffer *must* always be complete, though it may not be
    // subject to the same rules as application FBOs. ie, it could have 0x0 size.
    if (mId == 0)
    {
        return GL_FRAMEBUFFER_COMPLETE;
    }

    if (hasAnyDirtyBit() || !mCachedStatus.valid())
    {
        mCachedStatus = checkStatusImpl(state);
    }

    return mCachedStatus.value();
}

GLenum Framebuffer::checkStatusImpl(const ContextState &state)
{
    ASSERT(mId != 0);

    unsigned int colorbufferSize = 0;
    int samples = -1;
    bool missingAttachment = true;

    for (const FramebufferAttachment &colorAttachment : mState.mColorAttachments)
    {
        if (colorAttachment.isAttached())
        {
            const Extents &size = colorAttachment.getSize();
            if (size.width == 0 || size.height == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            const Format &format          = colorAttachment.getFormat();
            const TextureCaps &formatCaps = state.getTextureCap(format.asSized());
            if (colorAttachment.type() == GL_TEXTURE)
            {
                if (!formatCaps.renderable)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }

                if (format.info->depthBits > 0 || format.info->stencilBits > 0)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }

                if (colorAttachment.layer() >= size.depth)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }

                // ES3 specifies that cube map texture attachments must be cube complete.
                // This language is missing from the ES2 spec, but we enforce it here because some
                // desktop OpenGL drivers also enforce this validation.
                // TODO(jmadill): Check if OpenGL ES2 drivers enforce cube completeness.
                const Texture *texture = colorAttachment.getTexture();
                ASSERT(texture);
                if (texture->getTarget() == GL_TEXTURE_CUBE_MAP &&
                    !texture->getTextureState().isCubeComplete())
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }
            else if (colorAttachment.type() == GL_RENDERBUFFER)
            {
                if (!formatCaps.renderable || format.info->depthBits > 0 ||
                    format.info->stencilBits > 0)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }

            if (!missingAttachment)
            {
                // APPLE_framebuffer_multisample, which EXT_draw_buffers refers to, requires that
                // all color attachments have the same number of samples for the FBO to be complete.
                if (colorAttachment.getSamples() != samples)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT;
                }

                // in GLES 2.0, all color attachments attachments must have the same number of bitplanes
                // in GLES 3.0, there is no such restriction
                if (state.getClientMajorVersion() < 3)
                {
                    if (format.info->pixelBytes != colorbufferSize)
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }
            }
            else
            {
                samples = colorAttachment.getSamples();
                colorbufferSize   = format.info->pixelBytes;
                missingAttachment = false;
            }
        }
    }

    const FramebufferAttachment &depthAttachment = mState.mDepthAttachment;
    if (depthAttachment.isAttached())
    {
        const Extents &size = depthAttachment.getSize();
        if (size.width == 0 || size.height == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        const Format &format          = depthAttachment.getFormat();
        const TextureCaps &formatCaps = state.getTextureCap(format.asSized());
        if (depthAttachment.type() == GL_TEXTURE)
        {
            // depth texture attachments require OES/ANGLE_depth_texture
            if (!state.getExtensions().depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.renderable)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (format.info->depthBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (depthAttachment.type() == GL_RENDERBUFFER)
        {
            if (!formatCaps.renderable || format.info->depthBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }

        if (missingAttachment)
        {
            samples = depthAttachment.getSamples();
            missingAttachment = false;
        }
        else if (samples != depthAttachment.getSamples())
        {
            // CHROMIUM_framebuffer_mixed_samples allows a framebuffer to be
            // considered complete when its depth or stencil samples are a
            // multiple of the number of color samples.
            const bool mixedSamples = state.getExtensions().framebufferMixedSamples;
            if (!mixedSamples)
                return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;

            const int colorSamples = samples ? samples : 1;
            const int depthSamples = depthAttachment.getSamples();
            if ((depthSamples % colorSamples) != 0)
                return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    const FramebufferAttachment &stencilAttachment = mState.mStencilAttachment;
    if (stencilAttachment.isAttached())
    {
        const Extents &size = stencilAttachment.getSize();
        if (size.width == 0 || size.height == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        const Format &format          = stencilAttachment.getFormat();
        const TextureCaps &formatCaps = state.getTextureCap(format.asSized());
        if (stencilAttachment.type() == GL_TEXTURE)
        {
            // texture stencil attachments come along as part
            // of OES_packed_depth_stencil + OES/ANGLE_depth_texture
            if (!state.getExtensions().depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.renderable)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (format.info->stencilBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (stencilAttachment.type() == GL_RENDERBUFFER)
        {
            if (!formatCaps.renderable || format.info->stencilBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }

        if (missingAttachment)
        {
            samples = stencilAttachment.getSamples();
            missingAttachment = false;
        }
        else if (samples != stencilAttachment.getSamples())
        {
            // see the comments in depth attachment check.
            const bool mixedSamples = state.getExtensions().framebufferMixedSamples;
            if (!mixedSamples)
                return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;

            const int colorSamples   = samples ? samples : 1;
            const int stencilSamples = stencilAttachment.getSamples();
            if ((stencilSamples % colorSamples) != 0)
                return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }

        // Starting from ES 3.0 stencil and depth, if present, should be the same image
        if (state.getClientMajorVersion() >= 3 && depthAttachment.isAttached() &&
            stencilAttachment != depthAttachment)
        {
            return GL_FRAMEBUFFER_UNSUPPORTED;
        }
    }

    // we need to have at least one attachment to be complete
    if (missingAttachment)
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    // In ES 2.0, all color attachments must have the same width and height.
    // In ES 3.0, there is no such restriction.
    if (state.getClientMajorVersion() < 3 && !mState.attachmentsHaveSameDimensions())
    {
        return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    }

    syncState();
    if (!mImpl->checkStatus())
    {
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

Error Framebuffer::discard(size_t count, const GLenum *attachments)
{
    return mImpl->discard(count, attachments);
}

Error Framebuffer::invalidate(size_t count, const GLenum *attachments)
{
    return mImpl->invalidate(count, attachments);
}

Error Framebuffer::invalidateSub(size_t count, const GLenum *attachments, const gl::Rectangle &area)
{
    return mImpl->invalidateSub(count, attachments, area);
}

Error Framebuffer::clear(rx::ContextImpl *context, GLbitfield mask)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return gl::NoError();
    }

    return mImpl->clear(context, mask);
}

Error Framebuffer::clearBufferfv(rx::ContextImpl *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat *values)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return gl::NoError();
    }

    return mImpl->clearBufferfv(context, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferuiv(rx::ContextImpl *context,
                                  GLenum buffer,
                                  GLint drawbuffer,
                                  const GLuint *values)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return gl::NoError();
    }

    return mImpl->clearBufferuiv(context, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferiv(rx::ContextImpl *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLint *values)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return gl::NoError();
    }

    return mImpl->clearBufferiv(context, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferfi(rx::ContextImpl *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 GLfloat depth,
                                 GLint stencil)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return gl::NoError();
    }

    return mImpl->clearBufferfi(context, buffer, drawbuffer, depth, stencil);
}

GLenum Framebuffer::getImplementationColorReadFormat() const
{
    return mImpl->getImplementationColorReadFormat();
}

GLenum Framebuffer::getImplementationColorReadType() const
{
    return mImpl->getImplementationColorReadType();
}

Error Framebuffer::readPixels(rx::ContextImpl *context,
                              const Rectangle &area,
                              GLenum format,
                              GLenum type,
                              GLvoid *pixels) const
{
    ANGLE_TRY(mImpl->readPixels(context, area, format, type, pixels));

    Buffer *unpackBuffer = context->getGLState().getUnpackState().pixelBuffer.get();
    if (unpackBuffer)
    {
        unpackBuffer->onPixelUnpack();
    }

    return NoError();
}

Error Framebuffer::blit(rx::ContextImpl *context,
                        const Rectangle &sourceArea,
                        const Rectangle &destArea,
                        GLbitfield mask,
                        GLenum filter)
{
    return mImpl->blit(context, sourceArea, destArea, mask, filter);
}

int Framebuffer::getSamples(const ContextState &state)
{
    if (complete(state))
    {
        // For a complete framebuffer, all attachments must have the same sample count.
        // In this case return the first nonzero sample size.
        const auto *firstColorAttachment = mState.getFirstColorAttachment();
        if (firstColorAttachment)
        {
            ASSERT(firstColorAttachment->isAttached());
            return firstColorAttachment->getSamples();
        }
    }

    return 0;
}

bool Framebuffer::hasValidDepthStencil() const
{
    return mState.getDepthStencilAttachment() != nullptr;
}

void Framebuffer::setAttachment(GLenum type,
                                GLenum binding,
                                const ImageIndex &textureIndex,
                                FramebufferAttachmentObject *resource)
{
    if (binding == GL_DEPTH_STENCIL || binding == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        // ensure this is a legitimate depth+stencil format
        FramebufferAttachmentObject *attachmentObj = resource;
        if (resource)
        {
            FramebufferAttachment::Target target(binding, textureIndex);
            const Format &format = resource->getAttachmentFormat(target);
            if (format.info->depthBits == 0 || format.info->stencilBits == 0)
            {
                // Attaching nullptr detaches the current attachment.
                attachmentObj = nullptr;
            }
        }

        mState.mDepthAttachment.attach(type, binding, textureIndex, attachmentObj);
        mState.mStencilAttachment.attach(type, binding, textureIndex, attachmentObj);
        mDirtyBits.set(DIRTY_BIT_DEPTH_ATTACHMENT);
        mDirtyBits.set(DIRTY_BIT_STENCIL_ATTACHMENT);
        BindResourceChannel(&mDirtyDepthAttachmentBinding, resource);
        BindResourceChannel(&mDirtyStencilAttachmentBinding, resource);
    }
    else
    {
        switch (binding)
        {
            case GL_DEPTH:
            case GL_DEPTH_ATTACHMENT:
                mState.mDepthAttachment.attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_DEPTH_ATTACHMENT);
                BindResourceChannel(&mDirtyDepthAttachmentBinding, resource);
                break;
            case GL_STENCIL:
            case GL_STENCIL_ATTACHMENT:
                mState.mStencilAttachment.attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_STENCIL_ATTACHMENT);
                BindResourceChannel(&mDirtyStencilAttachmentBinding, resource);
                break;
            case GL_BACK:
                mState.mColorAttachments[0].attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_COLOR_ATTACHMENT_0);
                // No need for a resource binding for the default FBO, it's always complete.
                break;
            default:
            {
                size_t colorIndex = binding - GL_COLOR_ATTACHMENT0;
                ASSERT(colorIndex < mState.mColorAttachments.size());
                mState.mColorAttachments[colorIndex].attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex);
                BindResourceChannel(&mDirtyColorAttachmentBindings[colorIndex], resource);
            }
            break;
        }
    }
}

void Framebuffer::resetAttachment(GLenum binding)
{
    setAttachment(GL_NONE, binding, ImageIndex::MakeInvalid(), nullptr);
}

void Framebuffer::syncState()
{
    if (mDirtyBits.any())
    {
        mImpl->syncState(mDirtyBits);
        mDirtyBits.reset();
        mCachedStatus.reset();
    }
}

void Framebuffer::signal(SignalToken token)
{
    // TOOD(jmadill): Make this only update individual attachments to do less work.
    mCachedStatus.reset();
}

bool Framebuffer::complete(const ContextState &state)
{
    return (checkStatus(state) == GL_FRAMEBUFFER_COMPLETE);
}

}  // namespace gl
