//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferImpl.h: Defines the abstract rx::BufferImpl class.

#ifndef LIBANGLE_RENDERER_BUFFERIMPL_H_
#define LIBANGLE_RENDERER_BUFFERIMPL_H_

#include "common/angleutils.h"
#include "common/mathutil.h"
#include "libANGLE/Error.h"

#include <stdint.h>

namespace gl
{
class BufferState;
}

namespace rx
{
class ContextImpl;

class BufferImpl : angle::NonCopyable
{
  public:
    BufferImpl(const gl::BufferState &state) : mState(state) {}
    virtual ~BufferImpl() { }
    virtual void destroy(ContextImpl *contextImpl) {}

    virtual gl::Error setData(ContextImpl *context,
                              GLenum target,
                              const void *data,
                              size_t size,
                              GLenum usage) = 0;
    virtual gl::Error setSubData(ContextImpl *context,
                                 GLenum target,
                                 const void *data,
                                 size_t size,
                                 size_t offset) = 0;
    virtual gl::Error copySubData(ContextImpl *contextImpl,
                                  BufferImpl *source,
                                  GLintptr sourceOffset,
                                  GLintptr destOffset,
                                  GLsizeiptr size) = 0;
    virtual gl::Error map(ContextImpl *contextImpl, GLenum access, GLvoid **mapPtr) = 0;
    virtual gl::Error mapRange(ContextImpl *contextImpl,
                               size_t offset,
                               size_t length,
                               GLbitfield access,
                               GLvoid **mapPtr) = 0;
    virtual gl::Error unmap(ContextImpl *contextImpl, GLboolean *result) = 0;

    virtual gl::Error getIndexRange(GLenum type,
                                    size_t offset,
                                    size_t count,
                                    bool primitiveRestartEnabled,
                                    gl::IndexRange *outRange) = 0;

  protected:
    const gl::BufferState &mState;
};

}

#endif // LIBANGLE_RENDERER_BUFFERIMPL_H_
