//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.h: Defines the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#ifndef LIBANGLE_FRAMEBUFFER_H_
#define LIBANGLE_FRAMEBUFFER_H_

#include <vector>

#include "common/Optional.h"
#include "common/angleutils.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/signal_utils.h"

namespace rx
{
class ContextImpl;
class GLImplFactory;
class FramebufferImpl;
class RenderbufferImpl;
class SurfaceImpl;
}

namespace egl
{
class Surface;
}

namespace gl
{
class Context;
class Framebuffer;
class Renderbuffer;
class State;
class Texture;
class TextureCapsMap;
struct Caps;
class ContextState;
struct Extensions;
struct ImageIndex;
struct Rectangle;

class FramebufferState final : angle::NonCopyable
{
  public:
    FramebufferState();
    explicit FramebufferState(const Caps &caps);
    ~FramebufferState();

    const std::string &getLabel();

    const FramebufferAttachment *getAttachment(GLenum attachment) const;
    const FramebufferAttachment *getReadAttachment() const;
    const FramebufferAttachment *getFirstColorAttachment() const;
    const FramebufferAttachment *getDepthOrStencilAttachment() const;
    const FramebufferAttachment *getColorAttachment(size_t colorAttachment) const;
    const FramebufferAttachment *getDepthAttachment() const;
    const FramebufferAttachment *getStencilAttachment() const;
    const FramebufferAttachment *getDepthStencilAttachment() const;

    const std::vector<GLenum> &getDrawBufferStates() const { return mDrawBufferStates; }
    GLenum getReadBufferState() const { return mReadBufferState; }
    const std::vector<FramebufferAttachment> &getColorAttachments() const
    {
        return mColorAttachments;
    }

    bool attachmentsHaveSameDimensions() const;
    bool colorAttachmentsAreUniqueImages() const;

    const FramebufferAttachment *getDrawBuffer(size_t drawBufferIdx) const;
    size_t getDrawBufferCount() const;

  private:
    friend class Framebuffer;

    std::string mLabel;

    std::vector<FramebufferAttachment> mColorAttachments;
    FramebufferAttachment mDepthAttachment;
    FramebufferAttachment mStencilAttachment;

    std::vector<GLenum> mDrawBufferStates;
    GLenum mReadBufferState;
};

class Framebuffer final : public LabeledObject, public angle::SignalReceiver
{
  public:
    Framebuffer(const Caps &caps, rx::GLImplFactory *factory, GLuint id);
    Framebuffer(rx::SurfaceImpl *surface);
    virtual ~Framebuffer();

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    rx::FramebufferImpl *getImplementation() const { return mImpl; }

    GLuint id() const { return mId; }

    void setAttachment(GLenum type,
                       GLenum binding,
                       const ImageIndex &textureIndex,
                       FramebufferAttachmentObject *resource);
    void resetAttachment(GLenum binding);

    void detachTexture(GLuint texture);
    void detachRenderbuffer(GLuint renderbuffer);

    const FramebufferAttachment *getColorbuffer(size_t colorAttachment) const;
    const FramebufferAttachment *getDepthbuffer() const;
    const FramebufferAttachment *getStencilbuffer() const;
    const FramebufferAttachment *getDepthStencilBuffer() const;
    const FramebufferAttachment *getDepthOrStencilbuffer() const;
    const FramebufferAttachment *getReadColorbuffer() const;
    GLenum getReadColorbufferType() const;
    const FramebufferAttachment *getFirstColorbuffer() const;

    const FramebufferAttachment *getAttachment(GLenum attachment) const;

    size_t getDrawbufferStateCount() const;
    GLenum getDrawBufferState(size_t drawBuffer) const;
    const std::vector<GLenum> &getDrawBufferStates() const;
    void setDrawBuffers(size_t count, const GLenum *buffers);
    const FramebufferAttachment *getDrawBuffer(size_t drawBuffer) const;
    bool hasEnabledDrawBuffer() const;

    GLenum getReadBufferState() const;
    void setReadBuffer(GLenum buffer);

    size_t getNumColorBuffers() const;
    bool hasDepth() const;
    bool hasStencil() const;

    bool usingExtendedDrawBuffers() const;

    // This method calls checkStatus.
    int getSamples(const ContextState &state);
    GLenum checkStatus(const ContextState &state);

    // Helper for checkStatus == GL_FRAMEBUFFER_COMPLETE.
    bool complete(const ContextState &state);

    bool hasValidDepthStencil() const;

    Error discard(size_t count, const GLenum *attachments);
    Error invalidate(size_t count, const GLenum *attachments);
    Error invalidateSub(size_t count, const GLenum *attachments, const gl::Rectangle &area);

    Error clear(rx::ContextImpl *context, GLbitfield mask);
    Error clearBufferfv(rx::ContextImpl *context,
                        GLenum buffer,
                        GLint drawbuffer,
                        const GLfloat *values);
    Error clearBufferuiv(rx::ContextImpl *context,
                         GLenum buffer,
                         GLint drawbuffer,
                         const GLuint *values);
    Error clearBufferiv(rx::ContextImpl *context,
                        GLenum buffer,
                        GLint drawbuffer,
                        const GLint *values);
    Error clearBufferfi(rx::ContextImpl *context,
                        GLenum buffer,
                        GLint drawbuffer,
                        GLfloat depth,
                        GLint stencil);

    GLenum getImplementationColorReadFormat() const;
    GLenum getImplementationColorReadType() const;
    Error readPixels(rx::ContextImpl *context,
                     const gl::Rectangle &area,
                     GLenum format,
                     GLenum type,
                     GLvoid *pixels) const;

    Error blit(rx::ContextImpl *context,
               const Rectangle &sourceArea,
               const Rectangle &destArea,
               GLbitfield mask,
               GLenum filter);

    enum DirtyBitType
    {
        DIRTY_BIT_COLOR_ATTACHMENT_0,
        DIRTY_BIT_COLOR_ATTACHMENT_MAX =
            DIRTY_BIT_COLOR_ATTACHMENT_0 + gl::IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS,
        DIRTY_BIT_DEPTH_ATTACHMENT = DIRTY_BIT_COLOR_ATTACHMENT_MAX,
        DIRTY_BIT_STENCIL_ATTACHMENT,
        DIRTY_BIT_DRAW_BUFFERS,
        DIRTY_BIT_READ_BUFFER,
        DIRTY_BIT_UNKNOWN,
        DIRTY_BIT_MAX = DIRTY_BIT_UNKNOWN,
    };

    typedef std::bitset<DIRTY_BIT_MAX> DirtyBits;
    bool hasAnyDirtyBit() const { return mDirtyBits.any(); }

    void syncState();

    // angle::SignalReceiver implementation
    void signal(angle::SignalToken token) override;

  private:
    void detachResourceById(GLenum resourceType, GLuint resourceId);
    void detachMatchingAttachment(FramebufferAttachment *attachment,
                                  GLenum matchType,
                                  GLuint matchId,
                                  size_t dirtyBit);
    GLenum checkStatusImpl(const ContextState &state);

    FramebufferState mState;
    rx::FramebufferImpl *mImpl;
    GLuint mId;

    Optional<GLenum> mCachedStatus;
    std::vector<angle::ChannelBinding> mDirtyColorAttachmentBindings;
    angle::ChannelBinding mDirtyDepthAttachmentBinding;
    angle::ChannelBinding mDirtyStencilAttachmentBinding;

    DirtyBits mDirtyBits;
};

}  // namespace gl

#endif   // LIBANGLE_FRAMEBUFFER_H_
