//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferNULL.cpp:
//    Implements the class methods for BufferNULL.
//

#include "libANGLE/renderer/null/BufferNULL.h"

#include "common/debug.h"

namespace rx
{

BufferNULL::BufferNULL() : BufferImpl()
{
}

BufferNULL::~BufferNULL()
{
}

gl::Error BufferNULL::setData(GLenum target, const void *data, size_t size, GLenum usage)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferNULL::setSubData(GLenum target, const void *data, size_t size, size_t offset)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferNULL::copySubData(BufferImpl *source,
                                  GLintptr sourceOffset,
                                  GLintptr destOffset,
                                  GLsizeiptr size)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferNULL::map(GLenum access, GLvoid **mapPtr)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferNULL::mapRange(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferNULL::unmap(GLboolean *result)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

gl::Error BufferNULL::getIndexRange(GLenum type,
                                    size_t offset,
                                    size_t count,
                                    bool primitiveRestartEnabled,
                                    gl::IndexRange *outRange)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

}  // namespace rx
