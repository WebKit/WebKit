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
#include "libANGLE/PackedGLEnums.h"

#include <stdint.h>

namespace gl
{
class BufferState;
class Context;
}

namespace rx
{
class BufferImpl : angle::NonCopyable
{
  public:
    BufferImpl(const gl::BufferState &state) : mState(state) {}
    virtual ~BufferImpl() {}
    virtual void destroy(const gl::Context *context) {}

    virtual gl::Error setData(const gl::Context *context,
                              gl::BufferBinding target,
                              const void *data,
                              size_t size,
                              gl::BufferUsage usage)                                = 0;
    virtual gl::Error setSubData(const gl::Context *context,
                                 gl::BufferBinding target,
                                 const void *data,
                                 size_t size,
                                 size_t offset)                                     = 0;
    virtual gl::Error copySubData(const gl::Context *context,
                                  BufferImpl *source,
                                  GLintptr sourceOffset,
                                  GLintptr destOffset,
                                  GLsizeiptr size) = 0;
    virtual gl::Error map(const gl::Context *context, GLenum access, void **mapPtr) = 0;
    virtual gl::Error mapRange(const gl::Context *context,
                               size_t offset,
                               size_t length,
                               GLbitfield access,
                               void **mapPtr) = 0;
    virtual gl::Error unmap(const gl::Context *context, GLboolean *result) = 0;

    virtual gl::Error getIndexRange(const gl::Context *context,
                                    GLenum type,
                                    size_t offset,
                                    size_t count,
                                    bool primitiveRestartEnabled,
                                    gl::IndexRange *outRange) = 0;

  protected:
    const gl::BufferState &mState;
};
}

#endif  // LIBANGLE_RENDERER_BUFFERIMPL_H_
