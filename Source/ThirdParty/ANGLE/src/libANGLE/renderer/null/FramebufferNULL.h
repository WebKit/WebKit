//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferNULL.h:
//    Defines the class interface for FramebufferNULL, implementing FramebufferImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_FRAMEBUFFERNULL_H_
#define LIBANGLE_RENDERER_NULL_FRAMEBUFFERNULL_H_

#include "libANGLE/renderer/FramebufferImpl.h"

namespace rx
{

class FramebufferNULL : public FramebufferImpl
{
  public:
    FramebufferNULL(const gl::FramebufferState &state);
    ~FramebufferNULL() override;

    gl::Error discard(size_t count, const GLenum *attachments) override;
    gl::Error invalidate(size_t count, const GLenum *attachments) override;
    gl::Error invalidateSub(size_t count,
                            const GLenum *attachments,
                            const gl::Rectangle &area) override;

    gl::Error clear(ContextImpl *context, GLbitfield mask) override;
    gl::Error clearBufferfv(ContextImpl *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLfloat *values) override;
    gl::Error clearBufferuiv(ContextImpl *context,
                             GLenum buffer,
                             GLint drawbuffer,
                             const GLuint *values) override;
    gl::Error clearBufferiv(ContextImpl *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLint *values) override;
    gl::Error clearBufferfi(ContextImpl *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            GLfloat depth,
                            GLint stencil) override;

    GLenum getImplementationColorReadFormat() const override;
    GLenum getImplementationColorReadType() const override;
    gl::Error readPixels(ContextImpl *context,
                         const gl::Rectangle &area,
                         GLenum format,
                         GLenum type,
                         GLvoid *pixels) const override;

    gl::Error blit(ContextImpl *context,
                   const gl::Rectangle &sourceArea,
                   const gl::Rectangle &destArea,
                   GLbitfield mask,
                   GLenum filter) override;

    bool checkStatus() const override;

    void syncState(const gl::Framebuffer::DirtyBits &dirtyBits) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_FRAMEBUFFERNULL_H_
