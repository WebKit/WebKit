//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferNULL.h:
//    Defines the class interface for BufferNULL, implementing BufferImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_BUFFERNULL_H_
#define LIBANGLE_RENDERER_NULL_BUFFERNULL_H_

#include "libANGLE/renderer/BufferImpl.h"

namespace rx
{

class AllocationTrackerNULL;

class BufferNULL : public BufferImpl
{
  public:
    BufferNULL(const gl::BufferState &state, AllocationTrackerNULL *allocationTracker);
    ~BufferNULL() override;

    gl::Error setData(const gl::Context *context,
                      gl::BufferBinding target,
                      const void *data,
                      size_t size,
                      gl::BufferUsage usage) override;
    gl::Error setSubData(const gl::Context *context,
                         gl::BufferBinding target,
                         const void *data,
                         size_t size,
                         size_t offset) override;
    gl::Error copySubData(const gl::Context *context,
                          BufferImpl *source,
                          GLintptr sourceOffset,
                          GLintptr destOffset,
                          GLsizeiptr size) override;
    gl::Error map(const gl::Context *context, GLenum access, void **mapPtr) override;
    gl::Error mapRange(const gl::Context *context,
                       size_t offset,
                       size_t length,
                       GLbitfield access,
                       void **mapPtr) override;
    gl::Error unmap(const gl::Context *context, GLboolean *result) override;

    gl::Error getIndexRange(const gl::Context *context,
                            GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            gl::IndexRange *outRange) override;

    uint8_t *getDataPtr();
    const uint8_t *getDataPtr() const;

  private:
    std::vector<uint8_t> mData;

    AllocationTrackerNULL *mAllocationTracker;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_BUFFERNULL_H_
