//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libANGLE/Framebuffer.h"

#include "common/bitset_utils.h"
#include "common/Optional.h"
#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
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

void BindResourceChannel(OnAttachmentDirtyBinding *binding, FramebufferAttachmentObject *resource)
{
    binding->bind(resource ? resource->getDirtyChannel() : nullptr);
}

}  // anonymous namespace

// This constructor is only used for default framebuffers.
FramebufferState::FramebufferState()
    : mLabel(),
      mColorAttachments(1),
      mDrawBufferStates(IMPLEMENTATION_MAX_DRAW_BUFFERS, GL_NONE),
      mReadBufferState(GL_BACK),
      mDefaultWidth(0),
      mDefaultHeight(0),
      mDefaultSamples(0),
      mDefaultFixedSampleLocations(GL_FALSE),
      mWebGLDepthStencilConsistent(true)
{
    ASSERT(mDrawBufferStates.size() > 0);
    mDrawBufferStates[0] = GL_BACK;
    mEnabledDrawBuffers.set(0);
}

FramebufferState::FramebufferState(const Caps &caps)
    : mLabel(),
      mColorAttachments(caps.maxColorAttachments),
      mDrawBufferStates(caps.maxDrawBuffers, GL_NONE),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT),
      mDefaultWidth(0),
      mDefaultHeight(0),
      mDefaultSamples(0),
      mDefaultFixedSampleLocations(GL_FALSE),
      mWebGLDepthStencilConsistent(true)
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
    if (mReadBufferState == GL_NONE)
    {
        return nullptr;
    }
    ASSERT(mReadBufferState == GL_BACK || (mReadBufferState >= GL_COLOR_ATTACHMENT0 && mReadBufferState <= GL_COLOR_ATTACHMENT15));
    size_t readIndex = (mReadBufferState == GL_BACK ? 0 : static_cast<size_t>(mReadBufferState - GL_COLOR_ATTACHMENT0));
    ASSERT(readIndex < mColorAttachments.size());
    return mColorAttachments[readIndex].isAttached() ? &mColorAttachments[readIndex] : nullptr;
}

const FramebufferAttachment *FramebufferState::getFirstNonNullAttachment() const
{
    auto *colorAttachment = getFirstColorAttachment();
    if (colorAttachment)
    {
        return colorAttachment;
    }
    return getDepthOrStencilAttachment();
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

const FramebufferAttachment *FramebufferState::getStencilOrDepthStencilAttachment() const
{
    if (mStencilAttachment.isAttached())
    {
        return &mStencilAttachment;
    }
    return getDepthStencilAttachment();
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
        mDepthAttachment == mStencilAttachment)
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

    for (uint32_t colorIndex = 0;
         colorIndex < static_cast<uint32_t>(mState.mColorAttachments.size()); ++colorIndex)
    {
        mDirtyColorAttachmentBindings.emplace_back(this, DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex);
    }
}

Framebuffer::Framebuffer(egl::Surface *surface)
    : mState(),
      mImpl(surface->getImplementation()->createDefaultFramebuffer(mState)),
      mId(0),
      mCachedStatus(GL_FRAMEBUFFER_COMPLETE),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    ASSERT(mImpl != nullptr);
    mDirtyColorAttachmentBindings.emplace_back(this, DIRTY_BIT_COLOR_ATTACHMENT_0);

    setAttachmentImpl(GL_FRAMEBUFFER_DEFAULT, GL_BACK, gl::ImageIndex::MakeInvalid(), surface);

    if (surface->getConfig()->depthSize > 0)
    {
        setAttachmentImpl(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH, gl::ImageIndex::MakeInvalid(), surface);
    }

    if (surface->getConfig()->stencilSize > 0)
    {
        setAttachmentImpl(GL_FRAMEBUFFER_DEFAULT, GL_STENCIL, gl::ImageIndex::MakeInvalid(),
                          surface);
    }
}

Framebuffer::Framebuffer(rx::GLImplFactory *factory)
    : mState(),
      mImpl(factory->createFramebuffer(mState)),
      mId(0),
      mCachedStatus(GL_FRAMEBUFFER_UNDEFINED_OES),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    mDirtyColorAttachmentBindings.emplace_back(this, DIRTY_BIT_COLOR_ATTACHMENT_0);
}

Framebuffer::~Framebuffer()
{
    SafeDelete(mImpl);
}

void Framebuffer::destroy(const Context *context)
{
    mImpl->destroy(rx::SafeGetImpl(context));
}

void Framebuffer::destroyDefault(const egl::Display *display)
{
    mImpl->destroyDefault(rx::SafeGetImpl(display));
}

void Framebuffer::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &Framebuffer::getLabel() const
{
    return mState.mLabel;
}

void Framebuffer::detachTexture(const Context *context, GLuint textureId)
{
    detachResourceById(context, GL_TEXTURE, textureId);
}

void Framebuffer::detachRenderbuffer(const Context *context, GLuint renderbufferId)
{
    detachResourceById(context, GL_RENDERBUFFER, renderbufferId);
}

void Framebuffer::detachResourceById(const Context *context, GLenum resourceType, GLuint resourceId)
{
    for (size_t colorIndex = 0; colorIndex < mState.mColorAttachments.size(); ++colorIndex)
    {
        detachMatchingAttachment(&mState.mColorAttachments[colorIndex], resourceType, resourceId,
                                 DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex);
    }

    if (context->isWebGL1())
    {
        const std::array<FramebufferAttachment *, 3> attachments = {
            {&mState.mWebGLDepthStencilAttachment, &mState.mWebGLDepthAttachment,
             &mState.mWebGLStencilAttachment}};
        for (FramebufferAttachment *attachment : attachments)
        {
            if (attachment->isAttached() && attachment->type() == resourceType &&
                attachment->id() == resourceId)
            {
                resetAttachment(context, attachment->getBinding());
            }
        }
    }
    else
    {
        detachMatchingAttachment(&mState.mDepthAttachment, resourceType, resourceId,
                                 DIRTY_BIT_DEPTH_ATTACHMENT);
        detachMatchingAttachment(&mState.mStencilAttachment, resourceType, resourceId,
                                 DIRTY_BIT_STENCIL_ATTACHMENT);
    }
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

const FramebufferAttachment *Framebuffer::getStencilOrDepthStencilAttachment() const
{
    return mState.getStencilOrDepthStencilAttachment();
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

    mState.mEnabledDrawBuffers.reset();
    for (size_t index = 0; index < count; ++index)
    {
        if (drawStates[index] != GL_NONE && mState.mColorAttachments[index].isAttached())
        {
            mState.mEnabledDrawBuffers.set(index);
        }
    }
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

void Framebuffer::invalidateCompletenessCache()
{
    if (mId != 0)
    {
        mCachedStatus.reset();
    }
}

GLenum Framebuffer::checkStatus(const Context *context)
{
    // The default framebuffer is always complete except when it is surfaceless in which
    // case it is always unsupported. We return early because the default framebuffer may
    // not be subject to the same rules as application FBOs. ie, it could have 0x0 size.
    if (mId == 0)
    {
        ASSERT(mCachedStatus.valid());
        ASSERT(mCachedStatus.value() == GL_FRAMEBUFFER_COMPLETE ||
               mCachedStatus.value() == GL_FRAMEBUFFER_UNDEFINED_OES);
        return mCachedStatus.value();
    }

    if (hasAnyDirtyBit() || !mCachedStatus.valid())
    {
        mCachedStatus = checkStatusImpl(context);
    }

    return mCachedStatus.value();
}

GLenum Framebuffer::checkStatusImpl(const Context *context)
{
    const ContextState &state = context->getContextState();

    ASSERT(mId != 0);

    unsigned int colorbufferSize = 0;
    int samples = -1;
    bool missingAttachment = true;
    Optional<GLboolean> fixedSampleLocations;
    bool hasRenderbuffer = false;

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

                // ES3.1 (section 9.4) requires that the value of TEXTURE_FIXED_SAMPLE_LOCATIONS
                // should be the same for all attached textures.
                GLboolean fixedSampleloc = colorAttachment.getTexture()->getFixedSampleLocations(
                    colorAttachment.getTextureImageIndex().type, 0);
                if (fixedSampleLocations.valid() && fixedSampleloc != fixedSampleLocations.value())
                {
                    return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
                }
                else
                {
                    fixedSampleLocations = fixedSampleloc;
                }
            }
            else if (colorAttachment.type() == GL_RENDERBUFFER)
            {
                hasRenderbuffer = true;
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
                samples           = colorAttachment.getSamples();
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

    // Special additional validation for WebGL 1 DEPTH/STENCIL/DEPTH_STENCIL.
    if (state.isWebGL1())
    {
        if (!mState.mWebGLDepthStencilConsistent)
        {
            return GL_FRAMEBUFFER_UNSUPPORTED;
        }

        if (mState.mWebGLDepthStencilAttachment.isAttached())
        {
            if (mState.mWebGLDepthStencilAttachment.getDepthSize() == 0 ||
                mState.mWebGLDepthStencilAttachment.getStencilSize() == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }
        }
        else if (mState.mStencilAttachment.isAttached() &&
                 mState.mStencilAttachment.getDepthSize() > 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
        else if (mState.mDepthAttachment.isAttached() &&
                 mState.mDepthAttachment.getStencilSize() > 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
    }

    // ES3.1(section 9.4) requires that if no image is attached to the
    // framebuffer, and either the value of the framebuffer's FRAMEBUFFER_DEFAULT_WIDTH
    // or FRAMEBUFFER_DEFAULT_HEIGHT parameters is zero, the framebuffer is
    // considered incomplete.
    GLint defaultWidth  = mState.getDefaultWidth();
    GLint defaultHeight = mState.getDefaultHeight();

    if (missingAttachment && (defaultWidth == 0 || defaultHeight == 0))
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    // In ES 2.0, all color attachments must have the same width and height.
    // In ES 3.0, there is no such restriction.
    if (state.getClientMajorVersion() < 3 && !mState.attachmentsHaveSameDimensions())
    {
        return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    }

    // ES3.1(section 9.4) requires that if the attached images are a mix of renderbuffers
    // and textures, the value of TEXTURE_FIXED_SAMPLE_LOCATIONS must be TRUE for all
    // attached textures.
    if (fixedSampleLocations.valid() && hasRenderbuffer && !fixedSampleLocations.value())
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
    }

    syncState(context);
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
    GLbitfield blitMask = mask;

    // Note that blitting is called against draw framebuffer.
    // See the code in gl::Context::blitFramebuffer.
    if ((mask & GL_COLOR_BUFFER_BIT) && !hasEnabledDrawBuffer())
    {
        blitMask &= ~GL_COLOR_BUFFER_BIT;
    }

    if ((mask & GL_STENCIL_BUFFER_BIT) && mState.getStencilAttachment() == nullptr)
    {
        blitMask &= ~GL_STENCIL_BUFFER_BIT;
    }

    if ((mask & GL_DEPTH_BUFFER_BIT) && mState.getDepthAttachment() == nullptr)
    {
        blitMask &= ~GL_DEPTH_BUFFER_BIT;
    }

    if (!blitMask)
    {
        return NoError();
    }

    return mImpl->blit(context, sourceArea, destArea, blitMask, filter);
}

int Framebuffer::getSamples(const Context *context)
{
    if (complete(context))
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

Error Framebuffer::getSamplePosition(size_t index, GLfloat *xy) const
{
    ANGLE_TRY(mImpl->getSamplePosition(index, xy));
    return gl::NoError();
}

bool Framebuffer::hasValidDepthStencil() const
{
    return mState.getDepthStencilAttachment() != nullptr;
}

void Framebuffer::setAttachment(const Context *context,
                                GLenum type,
                                GLenum binding,
                                const ImageIndex &textureIndex,
                                FramebufferAttachmentObject *resource)
{
    // Context may be null in unit tests.
    if (!context || !context->isWebGL1())
    {
        setAttachmentImpl(type, binding, textureIndex, resource);
        return;
    }

    switch (binding)
    {
        case GL_DEPTH_STENCIL:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            mState.mWebGLDepthStencilAttachment.attach(type, binding, textureIndex, resource);
            break;
        case GL_DEPTH:
        case GL_DEPTH_ATTACHMENT:
            mState.mWebGLDepthAttachment.attach(type, binding, textureIndex, resource);
            break;
        case GL_STENCIL:
        case GL_STENCIL_ATTACHMENT:
            mState.mWebGLStencilAttachment.attach(type, binding, textureIndex, resource);
            break;
        default:
            setAttachmentImpl(type, binding, textureIndex, resource);
            return;
    }

    commitWebGL1DepthStencilIfConsistent();
}

void Framebuffer::commitWebGL1DepthStencilIfConsistent()
{
    int count = 0;

    std::array<FramebufferAttachment *, 3> attachments = {{&mState.mWebGLDepthStencilAttachment,
                                                           &mState.mWebGLDepthAttachment,
                                                           &mState.mWebGLStencilAttachment}};
    for (FramebufferAttachment *attachment : attachments)
    {
        if (attachment->isAttached())
        {
            count++;
        }
    }

    mState.mWebGLDepthStencilConsistent = (count <= 1);
    if (!mState.mWebGLDepthStencilConsistent)
    {
        // Inconsistent.
        return;
    }

    auto getImageIndexIfTextureAttachment = [](const FramebufferAttachment &attachment) {
        if (attachment.type() == GL_TEXTURE)
        {
            return attachment.getTextureImageIndex();
        }
        else
        {
            return ImageIndex::MakeInvalid();
        }
    };

    if (mState.mWebGLDepthAttachment.isAttached())
    {
        const auto &depth = mState.mWebGLDepthAttachment;
        setAttachmentImpl(depth.type(), GL_DEPTH_ATTACHMENT,
                          getImageIndexIfTextureAttachment(depth), depth.getResource());
        setAttachmentImpl(GL_NONE, GL_STENCIL_ATTACHMENT, ImageIndex::MakeInvalid(), nullptr);
    }
    else if (mState.mWebGLStencilAttachment.isAttached())
    {
        const auto &stencil = mState.mWebGLStencilAttachment;
        setAttachmentImpl(GL_NONE, GL_DEPTH_ATTACHMENT, ImageIndex::MakeInvalid(), nullptr);
        setAttachmentImpl(stencil.type(), GL_STENCIL_ATTACHMENT,
                          getImageIndexIfTextureAttachment(stencil), stencil.getResource());
    }
    else if (mState.mWebGLDepthStencilAttachment.isAttached())
    {
        const auto &depthStencil = mState.mWebGLDepthStencilAttachment;
        setAttachmentImpl(depthStencil.type(), GL_DEPTH_ATTACHMENT,
                          getImageIndexIfTextureAttachment(depthStencil),
                          depthStencil.getResource());
        setAttachmentImpl(depthStencil.type(), GL_STENCIL_ATTACHMENT,
                          getImageIndexIfTextureAttachment(depthStencil),
                          depthStencil.getResource());
    }
    else
    {
        setAttachmentImpl(GL_NONE, GL_DEPTH_ATTACHMENT, ImageIndex::MakeInvalid(), nullptr);
        setAttachmentImpl(GL_NONE, GL_STENCIL_ATTACHMENT, ImageIndex::MakeInvalid(), nullptr);
    }
}

void Framebuffer::setAttachmentImpl(GLenum type,
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
        return;
    }

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

            bool enabled = (type != GL_NONE && getDrawBufferState(colorIndex) != GL_NONE);
            mState.mEnabledDrawBuffers.set(colorIndex, enabled);
        }
        break;
    }
}

void Framebuffer::resetAttachment(const Context *context, GLenum binding)
{
    setAttachment(context, GL_NONE, binding, ImageIndex::MakeInvalid(), nullptr);
}

void Framebuffer::syncState(const Context *context)
{
    if (mDirtyBits.any())
    {
        mImpl->syncState(rx::SafeGetImpl(context), mDirtyBits);
        mDirtyBits.reset();
        if (mId != 0)
        {
            mCachedStatus.reset();
        }
    }
}

void Framebuffer::signal(uint32_t token)
{
    // TOOD(jmadill): Make this only update individual attachments to do less work.
    mCachedStatus.reset();
}

bool Framebuffer::complete(const Context *context)
{
    return (checkStatus(context) == GL_FRAMEBUFFER_COMPLETE);
}

bool Framebuffer::cachedComplete() const
{
    return (mCachedStatus.valid() && mCachedStatus == GL_FRAMEBUFFER_COMPLETE);
}

bool Framebuffer::formsRenderingFeedbackLoopWith(const State &state) const
{
    const Program *program = state.getProgram();

    // TODO(jmadill): Default framebuffer feedback loops.
    if (mId == 0)
    {
        return false;
    }

    // The bitset will skip inactive draw buffers.
    for (size_t drawIndex : angle::IterateBitSet(mState.mEnabledDrawBuffers))
    {
        const FramebufferAttachment *attachment = getDrawBuffer(drawIndex);
        if (attachment && attachment->type() == GL_TEXTURE)
        {
            // Validate the feedback loop.
            if (program->samplesFromTexture(state, attachment->id()))
            {
                return true;
            }
        }
    }

    // Validate depth-stencil feedback loop.
    const auto &dsState = state.getDepthStencilState();

    // We can skip the feedback loop checks if depth/stencil is masked out or disabled.
    const FramebufferAttachment *depth = getDepthbuffer();
    if (depth && depth->type() == GL_TEXTURE && dsState.depthTest && dsState.depthMask)
    {
        if (program->samplesFromTexture(state, depth->id()))
        {
            return true;
        }
    }

    // Note: we assume the front and back masks are the same for WebGL.
    const FramebufferAttachment *stencil = getStencilbuffer();
    ASSERT(dsState.stencilBackWritemask == dsState.stencilWritemask);
    if (stencil && stencil->type() == GL_TEXTURE && dsState.stencilTest &&
        dsState.stencilWritemask != 0)
    {
        // Skip the feedback loop check if depth/stencil point to the same resource.
        if (!depth || *stencil != *depth)
        {
            if (program->samplesFromTexture(state, stencil->id()))
            {
                return true;
            }
        }
    }

    return false;
}

bool Framebuffer::formsCopyingFeedbackLoopWith(GLuint copyTextureID,
                                               GLint copyTextureLevel,
                                               GLint copyTextureLayer) const
{
    if (mId == 0)
    {
        // It seems impossible to form a texture copying feedback loop with the default FBO.
        return false;
    }

    const FramebufferAttachment *readAttachment = getReadColorbuffer();
    ASSERT(readAttachment);

    if (readAttachment->isTextureWithId(copyTextureID))
    {
        const auto &imageIndex = readAttachment->getTextureImageIndex();
        if (imageIndex.mipIndex == copyTextureLevel)
        {
            // Check 3D/Array texture layers.
            return imageIndex.layerIndex == ImageIndex::ENTIRE_LEVEL ||
                   copyTextureLayer == ImageIndex::ENTIRE_LEVEL ||
                   imageIndex.layerIndex == copyTextureLayer;
        }
    }
    return false;
}

GLint Framebuffer::getDefaultWidth() const
{
    return mState.getDefaultWidth();
}

GLint Framebuffer::getDefaultHeight() const
{
    return mState.getDefaultHeight();
}

GLint Framebuffer::getDefaultSamples() const
{
    return mState.getDefaultSamples();
}

GLboolean Framebuffer::getDefaultFixedSampleLocations() const
{
    return mState.getDefaultFixedSampleLocations();
}

void Framebuffer::setDefaultWidth(GLint defaultWidth)
{
    mState.mDefaultWidth = defaultWidth;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_WIDTH);
}

void Framebuffer::setDefaultHeight(GLint defaultHeight)
{
    mState.mDefaultHeight = defaultHeight;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_HEIGHT);
}

void Framebuffer::setDefaultSamples(GLint defaultSamples)
{
    mState.mDefaultSamples = defaultSamples;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_SAMPLES);
}

void Framebuffer::setDefaultFixedSampleLocations(GLboolean defaultFixedSampleLocations)
{
    mState.mDefaultFixedSampleLocations = defaultFixedSampleLocations;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS);
}

// TODO(jmadill): Remove this kludge.
GLenum Framebuffer::checkStatus(const ValidationContext *context)
{
    return checkStatus(static_cast<const Context *>(context));
}

int Framebuffer::getSamples(const ValidationContext *context)
{
    return getSamples(static_cast<const Context *>(context));
}

}  // namespace gl
