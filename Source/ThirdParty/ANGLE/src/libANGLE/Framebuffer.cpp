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
#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/ImplFactory.h"
#include "libANGLE/renderer/RenderbufferImpl.h"
#include "libANGLE/renderer/SurfaceImpl.h"

namespace gl
{

namespace
{
void DetachMatchingAttachment(FramebufferAttachment *attachment, GLenum matchType, GLuint matchId)
{
    if (attachment->isAttached() &&
        attachment->type() == matchType &&
        attachment->id() == matchId)
    {
        attachment->detach();
    }
}
}

Framebuffer::Data::Data()
    : mLabel(),
      mColorAttachments(1),
      mDrawBufferStates(1, GL_NONE),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT)
{
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
}

Framebuffer::Data::Data(const Caps &caps)
    : mLabel(),
      mColorAttachments(caps.maxColorAttachments),
      mDrawBufferStates(caps.maxDrawBuffers, GL_NONE),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT)
{
    ASSERT(mDrawBufferStates.size() > 0);
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
}

Framebuffer::Data::~Data()
{
}

const std::string &Framebuffer::Data::getLabel()
{
    return mLabel;
}

const FramebufferAttachment *Framebuffer::Data::getReadAttachment() const
{
    ASSERT(mReadBufferState == GL_BACK || (mReadBufferState >= GL_COLOR_ATTACHMENT0 && mReadBufferState <= GL_COLOR_ATTACHMENT15));
    size_t readIndex = (mReadBufferState == GL_BACK ? 0 : static_cast<size_t>(mReadBufferState - GL_COLOR_ATTACHMENT0));
    ASSERT(readIndex < mColorAttachments.size());
    return mColorAttachments[readIndex].isAttached() ? &mColorAttachments[readIndex] : nullptr;
}

const FramebufferAttachment *Framebuffer::Data::getFirstColorAttachment() const
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

const FramebufferAttachment *Framebuffer::Data::getDepthOrStencilAttachment() const
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

const FramebufferAttachment *Framebuffer::Data::getColorAttachment(size_t colorAttachment) const
{
    ASSERT(colorAttachment < mColorAttachments.size());
    return mColorAttachments[colorAttachment].isAttached() ?
           &mColorAttachments[colorAttachment] :
           nullptr;
}

const FramebufferAttachment *Framebuffer::Data::getDepthAttachment() const
{
    return mDepthAttachment.isAttached() ? &mDepthAttachment : nullptr;
}

const FramebufferAttachment *Framebuffer::Data::getStencilAttachment() const
{
    return mStencilAttachment.isAttached() ? &mStencilAttachment : nullptr;
}

const FramebufferAttachment *Framebuffer::Data::getDepthStencilAttachment() const
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

bool Framebuffer::Data::attachmentsHaveSameDimensions() const
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

Framebuffer::Framebuffer(const Caps &caps, rx::ImplFactory *factory, GLuint id)
    : mData(caps), mImpl(factory->createFramebuffer(mData)), mId(id)
{
    ASSERT(mId != 0);
    ASSERT(mImpl != nullptr);
}

Framebuffer::Framebuffer(rx::SurfaceImpl *surface)
    : mData(), mImpl(surface->createDefaultFramebuffer(mData)), mId(0)
{
    ASSERT(mImpl != nullptr);
}

Framebuffer::~Framebuffer()
{
    SafeDelete(mImpl);
}

void Framebuffer::setLabel(const std::string &label)
{
    mData.mLabel = label;
}

const std::string &Framebuffer::getLabel() const
{
    return mData.mLabel;
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
    for (auto &colorAttachment : mData.mColorAttachments)
    {
        DetachMatchingAttachment(&colorAttachment, resourceType, resourceId);
    }

    DetachMatchingAttachment(&mData.mDepthAttachment, resourceType, resourceId);
    DetachMatchingAttachment(&mData.mStencilAttachment, resourceType, resourceId);
}

const FramebufferAttachment *Framebuffer::getColorbuffer(size_t colorAttachment) const
{
    return mData.getColorAttachment(colorAttachment);
}

const FramebufferAttachment *Framebuffer::getDepthbuffer() const
{
    return mData.getDepthAttachment();
}

const FramebufferAttachment *Framebuffer::getStencilbuffer() const
{
    return mData.getStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getDepthStencilBuffer() const
{
    return mData.getDepthStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getDepthOrStencilbuffer() const
{
    return mData.getDepthOrStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getReadColorbuffer() const
{
    return mData.getReadAttachment();
}

GLenum Framebuffer::getReadColorbufferType() const
{
    const FramebufferAttachment *readAttachment = mData.getReadAttachment();
    return (readAttachment != nullptr ? readAttachment->type() : GL_NONE);
}

const FramebufferAttachment *Framebuffer::getFirstColorbuffer() const
{
    return mData.getFirstColorAttachment();
}

const FramebufferAttachment *Framebuffer::getAttachment(GLenum attachment) const
{
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15)
    {
        return mData.getColorAttachment(attachment - GL_COLOR_ATTACHMENT0);
    }
    else
    {
        switch (attachment)
        {
          case GL_COLOR:
          case GL_BACK:
            return mData.getColorAttachment(0);
          case GL_DEPTH:
          case GL_DEPTH_ATTACHMENT:
            return mData.getDepthAttachment();
          case GL_STENCIL:
          case GL_STENCIL_ATTACHMENT:
            return mData.getStencilAttachment();
          case GL_DEPTH_STENCIL:
          case GL_DEPTH_STENCIL_ATTACHMENT:
            return getDepthStencilBuffer();
          default:
            UNREACHABLE();
            return nullptr;
        }
    }
}

size_t Framebuffer::getDrawbufferStateCount() const
{
    return mData.mDrawBufferStates.size();
}

GLenum Framebuffer::getDrawBufferState(size_t drawBuffer) const
{
    ASSERT(drawBuffer < mData.mDrawBufferStates.size());
    return mData.mDrawBufferStates[drawBuffer];
}

const std::vector<GLenum> &Framebuffer::getDrawBufferStates() const
{
    return mData.getDrawBufferStates();
}

void Framebuffer::setDrawBuffers(size_t count, const GLenum *buffers)
{
    auto &drawStates = mData.mDrawBufferStates;

    ASSERT(count <= drawStates.size());
    std::copy(buffers, buffers + count, drawStates.begin());
    std::fill(drawStates.begin() + count, drawStates.end(), GL_NONE);
    mDirtyBits.set(DIRTY_BIT_DRAW_BUFFERS);
}

const FramebufferAttachment *Framebuffer::getDrawBuffer(size_t drawBuffer) const
{
    ASSERT(drawBuffer < mData.mDrawBufferStates.size());
    if (mData.mDrawBufferStates[drawBuffer] != GL_NONE)
    {
        // ES3 spec: "If the GL is bound to a draw framebuffer object, the ith buffer listed in bufs
        // must be COLOR_ATTACHMENTi or NONE"
        ASSERT(mData.mDrawBufferStates[drawBuffer] == GL_COLOR_ATTACHMENT0 + drawBuffer ||
               (drawBuffer == 0 && mData.mDrawBufferStates[drawBuffer] == GL_BACK));
        return getAttachment(mData.mDrawBufferStates[drawBuffer]);
    }
    else
    {
        return nullptr;
    }
}

bool Framebuffer::hasEnabledDrawBuffer() const
{
    for (size_t drawbufferIdx = 0; drawbufferIdx < mData.mDrawBufferStates.size(); ++drawbufferIdx)
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
    return mData.mReadBufferState;
}

void Framebuffer::setReadBuffer(GLenum buffer)
{
    ASSERT(buffer == GL_BACK || buffer == GL_NONE ||
           (buffer >= GL_COLOR_ATTACHMENT0 &&
            (buffer - GL_COLOR_ATTACHMENT0) < mData.mColorAttachments.size()));
    mData.mReadBufferState = buffer;
    mDirtyBits.set(DIRTY_BIT_READ_BUFFER);
}

size_t Framebuffer::getNumColorBuffers() const
{
    return mData.mColorAttachments.size();
}

bool Framebuffer::hasDepth() const
{
    return (mData.mDepthAttachment.isAttached() && mData.mDepthAttachment.getDepthSize() > 0);
}

bool Framebuffer::hasStencil() const
{
    return (mData.mStencilAttachment.isAttached() && mData.mStencilAttachment.getStencilSize() > 0);
}

bool Framebuffer::usingExtendedDrawBuffers() const
{
    for (size_t drawbufferIdx = 1; drawbufferIdx < mData.mDrawBufferStates.size(); ++drawbufferIdx)
    {
        if (getDrawBuffer(drawbufferIdx) != nullptr)
        {
            return true;
        }
    }

    return false;
}

GLenum Framebuffer::checkStatus(const gl::Data &data) const
{
    // The default framebuffer *must* always be complete, though it may not be
    // subject to the same rules as application FBOs. ie, it could have 0x0 size.
    if (mId == 0)
    {
        return GL_FRAMEBUFFER_COMPLETE;
    }

    unsigned int colorbufferSize = 0;
    int samples = -1;
    bool missingAttachment = true;

    for (const FramebufferAttachment &colorAttachment : mData.mColorAttachments)
    {
        if (colorAttachment.isAttached())
        {
            const Extents &size = colorAttachment.getSize();
            if (size.width == 0 || size.height == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            GLenum internalformat = colorAttachment.getInternalFormat();
            const TextureCaps &formatCaps = data.textureCaps->get(internalformat);
            const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
            if (colorAttachment.type() == GL_TEXTURE)
            {
                if (!formatCaps.renderable)
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }

                if (formatInfo.depthBits > 0 || formatInfo.stencilBits > 0)
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
                if (texture->getTarget() == GL_TEXTURE_CUBE_MAP && !texture->isCubeComplete())
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
                }
            }
            else if (colorAttachment.type() == GL_RENDERBUFFER)
            {
                if (!formatCaps.renderable || formatInfo.depthBits > 0 || formatInfo.stencilBits > 0)
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
                if (data.clientVersion < 3)
                {
                    if (formatInfo.pixelBytes != colorbufferSize)
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }
            }
            else
            {
                samples = colorAttachment.getSamples();
                colorbufferSize = formatInfo.pixelBytes;
                missingAttachment = false;
            }
        }
    }

    const FramebufferAttachment &depthAttachment = mData.mDepthAttachment;
    if (depthAttachment.isAttached())
    {
        const Extents &size = depthAttachment.getSize();
        if (size.width == 0 || size.height == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        GLenum internalformat = depthAttachment.getInternalFormat();
        const TextureCaps &formatCaps = data.textureCaps->get(internalformat);
        const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
        if (depthAttachment.type() == GL_TEXTURE)
        {
            // depth texture attachments require OES/ANGLE_depth_texture
            if (!data.extensions->depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.renderable)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (formatInfo.depthBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (depthAttachment.type() == GL_RENDERBUFFER)
        {
            if (!formatCaps.renderable || formatInfo.depthBits == 0)
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
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    const FramebufferAttachment &stencilAttachment = mData.mStencilAttachment;
    if (stencilAttachment.isAttached())
    {
        const Extents &size = stencilAttachment.getSize();
        if (size.width == 0 || size.height == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        GLenum internalformat = stencilAttachment.getInternalFormat();
        const TextureCaps &formatCaps = data.textureCaps->get(internalformat);
        const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
        if (stencilAttachment.type() == GL_TEXTURE)
        {
            // texture stencil attachments come along as part
            // of OES_packed_depth_stencil + OES/ANGLE_depth_texture
            if (!data.extensions->depthTextures)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!formatCaps.renderable)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (formatInfo.stencilBits == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (stencilAttachment.type() == GL_RENDERBUFFER)
        {
            if (!formatCaps.renderable || formatInfo.stencilBits == 0)
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
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE;
        }
    }

    // we need to have at least one attachment to be complete
    if (missingAttachment)
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    // In ES 2.0, all color attachments must have the same width and height.
    // In ES 3.0, there is no such restriction.
    if (data.clientVersion < 3 && !mData.attachmentsHaveSameDimensions())
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

Error Framebuffer::clear(const gl::Data &data, GLbitfield mask)
{
    if (data.state->isRasterizerDiscardEnabled())
    {
        return gl::Error(GL_NO_ERROR);
    }

    return mImpl->clear(data, mask);
}

Error Framebuffer::clearBufferfv(const gl::Data &data,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat *values)
{
    if (data.state->isRasterizerDiscardEnabled())
    {
        return gl::Error(GL_NO_ERROR);
    }

    return mImpl->clearBufferfv(data, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferuiv(const gl::Data &data,
                                  GLenum buffer,
                                  GLint drawbuffer,
                                  const GLuint *values)
{
    if (data.state->isRasterizerDiscardEnabled())
    {
        return gl::Error(GL_NO_ERROR);
    }

    return mImpl->clearBufferuiv(data, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferiv(const gl::Data &data,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLint *values)
{
    if (data.state->isRasterizerDiscardEnabled())
    {
        return gl::Error(GL_NO_ERROR);
    }

    return mImpl->clearBufferiv(data, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferfi(const gl::Data &data,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 GLfloat depth,
                                 GLint stencil)
{
    if (data.state->isRasterizerDiscardEnabled())
    {
        return gl::Error(GL_NO_ERROR);
    }

    return mImpl->clearBufferfi(data, buffer, drawbuffer, depth, stencil);
}

GLenum Framebuffer::getImplementationColorReadFormat() const
{
    return mImpl->getImplementationColorReadFormat();
}

GLenum Framebuffer::getImplementationColorReadType() const
{
    return mImpl->getImplementationColorReadType();
}

Error Framebuffer::readPixels(const State &state,
                              const Rectangle &area,
                              GLenum format,
                              GLenum type,
                              GLvoid *pixels) const
{
    Error error = mImpl->readPixels(state, area, format, type, pixels);
    if (error.isError())
    {
        return error;
    }

    Buffer *unpackBuffer = state.getUnpackState().pixelBuffer.get();
    if (unpackBuffer)
    {
        unpackBuffer->onPixelUnpack();
    }

    return Error(GL_NO_ERROR);
}

Error Framebuffer::blit(const State &state,
                        const Rectangle &sourceArea,
                        const Rectangle &destArea,
                        GLbitfield mask,
                        GLenum filter,
                        const Framebuffer *sourceFramebuffer)
{
    return mImpl->blit(state, sourceArea, destArea, mask, filter, sourceFramebuffer);
}

int Framebuffer::getSamples(const gl::Data &data) const
{
    if (checkStatus(data) == GL_FRAMEBUFFER_COMPLETE)
    {
        // for a complete framebuffer, all attachments must have the same sample count
        // in this case return the first nonzero sample size
        for (const FramebufferAttachment &colorAttachment : mData.mColorAttachments)
        {
            if (colorAttachment.isAttached())
            {
                return colorAttachment.getSamples();
            }
        }
    }

    return 0;
}

bool Framebuffer::hasValidDepthStencil() const
{
    return mData.getDepthStencilAttachment() != nullptr;
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
            GLenum internalFormat            = resource->getAttachmentInternalFormat(target);
            const InternalFormat &formatInfo = GetInternalFormatInfo(internalFormat);
            if (formatInfo.depthBits == 0 || formatInfo.stencilBits == 0)
            {
                // Attaching nullptr detaches the current attachment.
                attachmentObj = nullptr;
            }
        }

        mData.mDepthAttachment.attach(type, binding, textureIndex, attachmentObj);
        mData.mStencilAttachment.attach(type, binding, textureIndex, attachmentObj);
        mDirtyBits.set(DIRTY_BIT_DEPTH_ATTACHMENT);
        mDirtyBits.set(DIRTY_BIT_STENCIL_ATTACHMENT);
    }
    else
    {
        switch (binding)
        {
            case GL_DEPTH:
            case GL_DEPTH_ATTACHMENT:
                mData.mDepthAttachment.attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_DEPTH_ATTACHMENT);
            break;
            case GL_STENCIL:
            case GL_STENCIL_ATTACHMENT:
                mData.mStencilAttachment.attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_STENCIL_ATTACHMENT);
            break;
            case GL_BACK:
                mData.mColorAttachments[0].attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_COLOR_ATTACHMENT_0);
            break;
            default:
            {
                size_t colorIndex = binding - GL_COLOR_ATTACHMENT0;
                ASSERT(colorIndex < mData.mColorAttachments.size());
                mData.mColorAttachments[colorIndex].attach(type, binding, textureIndex, resource);
                mDirtyBits.set(DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex);
            }
            break;
        }
    }
}

void Framebuffer::resetAttachment(GLenum binding)
{
    setAttachment(GL_NONE, binding, ImageIndex::MakeInvalid(), nullptr);
}

void Framebuffer::syncState() const
{
    if (mDirtyBits.any())
    {
        mImpl->syncState(mDirtyBits);
        mDirtyBits.reset();
    }
}

}  // namespace gl
