//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferNULL.cpp:
//    Implements the class methods for FramebufferNULL.
//

#include "libANGLE/renderer/null/FramebufferNULL.h"

#include "common/debug.h"

namespace rx
{

FramebufferNULL::FramebufferNULL(const gl::FramebufferState &state) : FramebufferImpl(state)
{
}

FramebufferNULL::~FramebufferNULL()
{
}

gl::Error FramebufferNULL::discard(size_t count, const GLenum *attachments)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::invalidate(size_t count, const GLenum *attachments)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::invalidateSub(size_t count,
                                         const GLenum *attachments,
                                         const gl::Rectangle &area)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::clear(ContextImpl *context, GLbitfield mask)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::clearBufferfv(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLfloat *values)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::clearBufferuiv(ContextImpl *context,
                                          GLenum buffer,
                                          GLint drawbuffer,
                                          const GLuint *values)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::clearBufferiv(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLint *values)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::clearBufferfi(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         GLfloat depth,
                                         GLint stencil)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

GLenum FramebufferNULL::getImplementationColorReadFormat() const
{
    UNIMPLEMENTED();
    return GLenum();
}

GLenum FramebufferNULL::getImplementationColorReadType() const
{
    UNIMPLEMENTED();
    return GLenum();
}

gl::Error FramebufferNULL::readPixels(ContextImpl *context,
                                      const gl::Rectangle &area,
                                      GLenum format,
                                      GLenum type,
                                      GLvoid *pixels) const
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error FramebufferNULL::blit(ContextImpl *context,
                                const gl::Rectangle &sourceArea,
                                const gl::Rectangle &destArea,
                                GLbitfield mask,
                                GLenum filter)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

bool FramebufferNULL::checkStatus() const
{
    UNIMPLEMENTED();
    return bool();
}

void FramebufferNULL::syncState(const gl::Framebuffer::DirtyBits &dirtyBits)
{
    UNIMPLEMENTED();
}

}  // namespace rx
