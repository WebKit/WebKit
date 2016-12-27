//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.cpp:
//    Implements the class methods for BufferVk.
//

#include "libANGLE/renderer/vulkan/BufferVk.h"

#include "common/debug.h"

namespace rx
{

BufferVk::BufferVk() : BufferImpl()
{
}

BufferVk::~BufferVk()
{
}

gl::Error BufferVk::setData(GLenum target, const void *data, size_t size, GLenum usage)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferVk::setSubData(GLenum target, const void *data, size_t size, size_t offset)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferVk::copySubData(BufferImpl *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferVk::map(GLenum access, GLvoid **mapPtr)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferVk::mapRange(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferVk::unmap(GLboolean *result)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferVk::getIndexRange(GLenum type,
                                  size_t offset,
                                  size_t count,
                                  bool primitiveRestartEnabled,
                                  gl::IndexRange *outRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
