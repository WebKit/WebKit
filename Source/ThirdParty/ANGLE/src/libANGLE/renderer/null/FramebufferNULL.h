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

    gl::Error discard(const gl::Context *context, size_t count, const GLenum *attachments) override;
    gl::Error invalidate(const gl::Context *context,
                         size_t count,
                         const GLenum *attachments) override;
    gl::Error invalidateSub(const gl::Context *context,
                            size_t count,
                            const GLenum *attachments,
                            const gl::Rectangle &area) override;

    gl::Error clear(const gl::Context *context, GLbitfield mask) override;
    gl::Error clearBufferfv(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLfloat *values) override;
    gl::Error clearBufferuiv(const gl::Context *context,
                             GLenum buffer,
                             GLint drawbuffer,
                             const GLuint *values) override;
    gl::Error clearBufferiv(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            const GLint *values) override;
    gl::Error clearBufferfi(const gl::Context *context,
                            GLenum buffer,
                            GLint drawbuffer,
                            GLfloat depth,
                            GLint stencil) override;

    GLenum getImplementationColorReadFormat(const gl::Context *context) const override;
    GLenum getImplementationColorReadType(const gl::Context *context) const override;
    gl::Error readPixels(const gl::Context *context,
                         const gl::Rectangle &area,
                         GLenum format,
                         GLenum type,
                         void *pixels) override;

    gl::Error blit(const gl::Context *context,
                   const gl::Rectangle &sourceArea,
                   const gl::Rectangle &destArea,
                   GLbitfield mask,
                   GLenum filter) override;

    bool checkStatus(const gl::Context *context) const override;

    void syncState(const gl::Context *context,
                   const gl::Framebuffer::DirtyBits &dirtyBits) override;

    gl::Error getSamplePosition(size_t index, GLfloat *xy) const override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_FRAMEBUFFERNULL_H_
