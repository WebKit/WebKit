//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferImpl.h: Defines the abstract rx::FramebufferImpl class.

#ifndef LIBANGLE_RENDERER_FRAMEBUFFERIMPL_H_
#define LIBANGLE_RENDERER_FRAMEBUFFERIMPL_H_

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/Framebuffer.h"

namespace gl
{
class State;
class Framebuffer;
class FramebufferAttachment;
struct Rectangle;
}

namespace rx
{
class DisplayImpl;

class FramebufferImpl : angle::NonCopyable
{
  public:
    explicit FramebufferImpl(const gl::FramebufferState &state) : mState(state) {}
    virtual ~FramebufferImpl() {}
    virtual void destroy(const gl::Context *context) {}
    virtual void destroyDefault(const egl::Display *display) {}

    virtual gl::Error discard(const gl::Context *context,
                              size_t count,
                              const GLenum *attachments) = 0;
    virtual gl::Error invalidate(const gl::Context *context,
                                 size_t count,
                                 const GLenum *attachments) = 0;
    virtual gl::Error invalidateSub(const gl::Context *context,
                                    size_t count,
                                    const GLenum *attachments,
                                    const gl::Rectangle &area) = 0;

    virtual gl::Error clear(const gl::Context *context, GLbitfield mask) = 0;
    virtual gl::Error clearBufferfv(const gl::Context *context,
                                    GLenum buffer,
                                    GLint drawbuffer,
                                    const GLfloat *values) = 0;
    virtual gl::Error clearBufferuiv(const gl::Context *context,
                                     GLenum buffer,
                                     GLint drawbuffer,
                                     const GLuint *values) = 0;
    virtual gl::Error clearBufferiv(const gl::Context *context,
                                    GLenum buffer,
                                    GLint drawbuffer,
                                    const GLint *values) = 0;
    virtual gl::Error clearBufferfi(const gl::Context *context,
                                    GLenum buffer,
                                    GLint drawbuffer,
                                    GLfloat depth,
                                    GLint stencil) = 0;

    virtual GLenum getImplementationColorReadFormat(const gl::Context *context) const = 0;
    virtual GLenum getImplementationColorReadType(const gl::Context *context) const   = 0;
    virtual gl::Error readPixels(const gl::Context *context,
                                 const gl::Rectangle &area,
                                 GLenum format,
                                 GLenum type,
                                 void *pixels) = 0;

    virtual gl::Error blit(const gl::Context *context,
                           const gl::Rectangle &sourceArea,
                           const gl::Rectangle &destArea,
                           GLbitfield mask,
                           GLenum filter) = 0;

    virtual bool checkStatus(const gl::Context *context) const = 0;

    virtual void syncState(const gl::Context *context,
                           const gl::Framebuffer::DirtyBits &dirtyBits) = 0;

    virtual gl::Error getSamplePosition(size_t index, GLfloat *xy) const = 0;

    const gl::FramebufferState &getState() const { return mState; }

  protected:
    const gl::FramebufferState &mState;
};
}

#endif  // LIBANGLE_RENDERER_FRAMEBUFFERIMPL_H_
