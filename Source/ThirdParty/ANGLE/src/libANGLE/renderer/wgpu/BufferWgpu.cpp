//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferWgpu.cpp:
//    Implements the class methods for BufferWgpu.
//

#include "libANGLE/renderer/wgpu/BufferWgpu.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"

namespace rx
{

BufferWgpu::BufferWgpu(const gl::BufferState &state) : BufferImpl(state) {}

BufferWgpu::~BufferWgpu() {}

angle::Result BufferWgpu::setDataWithUsageFlags(const gl::Context *context,
                                                gl::BufferBinding target,
                                                GLeglClientBufferEXT clientBuffer,
                                                const void *data,
                                                size_t size,
                                                gl::BufferUsage usage,
                                                GLbitfield flags)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::setData(const gl::Context *context,
                                  gl::BufferBinding target,
                                  const void *data,
                                  size_t size,
                                  gl::BufferUsage usage)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::setSubData(const gl::Context *context,
                                     gl::BufferBinding target,
                                     const void *data,
                                     size_t size,
                                     size_t offset)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::copySubData(const gl::Context *context,
                                      BufferImpl *source,
                                      GLintptr sourceOffset,
                                      GLintptr destOffset,
                                      GLsizeiptr size)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::mapRange(const gl::Context *context,
                                   size_t offset,
                                   size_t length,
                                   GLbitfield access,
                                   void **mapPtr)
{
    return angle::Result::Continue;
}

angle::Result BufferWgpu::unmap(const gl::Context *context, GLboolean *result)
{
    *result = GL_TRUE;
    return angle::Result::Continue;
}

angle::Result BufferWgpu::getIndexRange(const gl::Context *context,
                                        gl::DrawElementsType type,
                                        size_t offset,
                                        size_t count,
                                        bool primitiveRestartEnabled,
                                        gl::IndexRange *outRange)
{
    return angle::Result::Continue;
}

uint8_t *BufferWgpu::getDataPtr()
{
    return nullptr;
}

const uint8_t *BufferWgpu::getDataPtr() const
{
    return nullptr;
}

}  // namespace rx
