//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferNULL.cpp:
//    Implements the class methods for FramebufferNULL.
//

#include "libANGLE/renderer/null/FramebufferNULL.h"

#include "libANGLE/Context.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/null/BufferNULL.h"

#include "common/debug.h"

namespace rx
{

FramebufferNULL::FramebufferNULL(const gl::FramebufferState &state) : FramebufferImpl(state)
{
}

FramebufferNULL::~FramebufferNULL()
{
}

gl::Error FramebufferNULL::discard(const gl::Context *context,
                                   size_t count,
                                   const GLenum *attachments)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::invalidate(const gl::Context *context,
                                      size_t count,
                                      const GLenum *attachments)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::invalidateSub(const gl::Context *context,
                                         size_t count,
                                         const GLenum *attachments,
                                         const gl::Rectangle &area)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clear(const gl::Context *context, GLbitfield mask)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferfv(const gl::Context *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLfloat *values)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferuiv(const gl::Context *context,
                                          GLenum buffer,
                                          GLint drawbuffer,
                                          const GLuint *values)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferiv(const gl::Context *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         const GLint *values)
{
    return gl::NoError();
}

gl::Error FramebufferNULL::clearBufferfi(const gl::Context *context,
                                         GLenum buffer,
                                         GLint drawbuffer,
                                         GLfloat depth,
                                         GLint stencil)
{
    return gl::NoError();
}

GLenum FramebufferNULL::getImplementationColorReadFormat(const gl::Context *context) const
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

GLenum FramebufferNULL::getImplementationColorReadType(const gl::Context *context) const
{
    const gl::FramebufferAttachment *readAttachment = mState.getReadAttachment();
    if (readAttachment == nullptr)
    {
        return GL_NONE;
    }

    const gl::Format &format = readAttachment->getFormat();
    ASSERT(format.info != nullptr);
    return format.info->getReadPixelsType(context->getClientVersion());
}

gl::Error FramebufferNULL::readPixels(const gl::Context *context,
                                      const gl::Rectangle &origArea,
                                      GLenum format,
                                      GLenum type,
                                      void *ptrOrOffset)
{
    const gl::PixelPackState &packState = context->getGLState().getPackState();
    gl::Buffer *packBuffer = context->getGLState().getTargetBuffer(gl::BufferBinding::PixelPack);

    // Get the pointer to write to from the argument or the pack buffer
    GLubyte *pixels = nullptr;
    if (packBuffer != nullptr)
    {
        BufferNULL *packBufferGL = GetImplAs<BufferNULL>(packBuffer);
        pixels                     = reinterpret_cast<GLubyte *>(packBufferGL->getDataPtr());
        pixels += reinterpret_cast<intptr_t>(ptrOrOffset);
    }
    else
    {
        pixels = reinterpret_cast<GLubyte *>(ptrOrOffset);
    }

    // Clip read area to framebuffer.
    const gl::Extents fbSize = getState().getReadAttachment()->getSize();
    const gl::Rectangle fbRect(0, 0, fbSize.width, fbSize.height);
    gl::Rectangle area;
    if (!ClipRectangle(origArea, fbRect, &area))
    {
        // nothing to read
        return gl::NoError();
    }

    // Compute size of unclipped rows and initial skip
    const gl::InternalFormat &glFormat = gl::GetInternalFormatInfo(format, type);

    GLuint rowBytes = 0;
    ANGLE_TRY_RESULT(
        glFormat.computeRowPitch(type, origArea.width, packState.alignment, packState.rowLength),
        rowBytes);

    GLuint skipBytes = 0;
    ANGLE_TRY_RESULT(glFormat.computeSkipBytes(rowBytes, 0, packState, false), skipBytes);
    pixels += skipBytes;

    // Skip OOB region up to first in bounds pixel
    int leftClip = area.x - origArea.x;
    int topClip  = area.y - origArea.y;
    pixels += leftClip * glFormat.pixelBytes + topClip * rowBytes;

    // Write the in-bounds readpixels data with non-zero values
    for (GLint y = area.y; y < area.y + area.height; ++y)
    {
        memset(pixels, 42, glFormat.pixelBytes * area.width);
        pixels += rowBytes;
    }

    return gl::NoError();
}

gl::Error FramebufferNULL::blit(const gl::Context *context,
                                const gl::Rectangle &sourceArea,
                                const gl::Rectangle &destArea,
                                GLbitfield mask,
                                GLenum filter)
{
    return gl::NoError();
}

bool FramebufferNULL::checkStatus(const gl::Context *context) const
{
    return true;
}

void FramebufferNULL::syncState(const gl::Context *context,
                                const gl::Framebuffer::DirtyBits &dirtyBits)
{
}

gl::Error FramebufferNULL::getSamplePosition(size_t index, GLfloat *xy) const
{
    return gl::NoError();
}

}  // namespace rx
