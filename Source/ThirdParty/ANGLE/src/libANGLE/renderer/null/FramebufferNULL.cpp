//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferNULL.cpp:
//    Implements the class methods for FramebufferNULL.
//

#include "libANGLE/renderer/null/FramebufferNULL.h"

#include "libANGLE/formatutils.h"

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
    return gl::NoError();
}

gl::Error FramebufferNULL::invalidate(size_t count, const GLenum *attachments)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::invalidateSub(size_t count,
                                         const GLenum *attachments,
                                         const gl::Rectangle &area)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clear(ContextImpl *context, GLbitfield mask)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferfv(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLfloat *values)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferuiv(ContextImpl *context,
                                          GLenum buffer,
                                          GLint drawbuffer,
                                          const GLuint *values)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferiv(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLint *values)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferfi(ContextImpl *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         GLfloat depth,
                                         GLint stencil)
{
    return gl::NoError();
}

GLenum FramebufferNULL::getImplementationColorReadFormat() const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();
    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    const gl::Format &format = readAttachment->getFormat();
    ASSERT(format.info != nullptr);
    return format.info->getReadPixelsFormat();
}

GLenum FramebufferNULL::getImplementationColorReadType() const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();
    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    const gl::Format &format = readAttachment->getFormat();
    ASSERT(format.info != nullptr);
    return format.info->getReadPixelsType();
}

gl::Error FramebufferNULL::readPixels(ContextImpl *context,
                                      const gl::Rectangle &area,
                                      GLenum format,
                                      GLenum type,
                                      GLvoid *pixels) const
{
    return gl::NoError();
}

gl::Error FramebufferNULL::blit(ContextImpl *context,
                                const gl::Rectangle &sourceArea,
                                const gl::Rectangle &destArea,
                                GLbitfield mask,
                                GLenum filter)
{
    return gl::NoError();
}

bool FramebufferNULL::checkStatus() const
{
    return true;
}

void FramebufferNULL::syncState(ContextImpl *contextImpl,
                                const gl::Framebuffer::DirtyBits &dirtyBits)
{
}

gl::Error FramebufferNULL::getSamplePosition(size_t index, GLfloat *xy) const
{
    return gl::NoError();
}

}  // namespace rx
