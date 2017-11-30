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
#include "common/utilities.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/null/ContextNULL.h"

namespace rx
{

BufferNULL::BufferNULL(const gl::BufferState &state, AllocationTrackerNULL *allocationTracker)
    : BufferImpl(state), mAllocationTracker(allocationTracker)
{
    ASSERT(mAllocationTracker != nullptr);
}

BufferNULL::~BufferNULL()
{
    bool memoryReleaseResult = mAllocationTracker->updateMemoryAllocation(mData.size(), 0);
    ASSERT(memoryReleaseResult);
}

gl::Error BufferNULL::setData(const gl::Context *context,
                              gl::BufferBinding target,
                              const void *data,
                              size_t size,
                              gl::BufferUsage usage)
{
    if (!mAllocationTracker->updateMemoryAllocation(mData.size(), size))
    {
        return gl::OutOfMemory() << "Unable to allocate internal buffer storage.";
    }

    mData.resize(size, 0);
    if (size > 0 && data != nullptr)
    {
        memcpy(mData.data(), data, size);
    }
    return gl::NoError();
}

gl::Error BufferNULL::setSubData(const gl::Context *context,
                                 gl::BufferBinding target,
                                 const void *data,
                                 size_t size,
                                 size_t offset)
{
    if (size > 0)
    {
        memcpy(mData.data() + offset, data, size);
    }
    return gl::NoError();
}

gl::Error BufferNULL::copySubData(const gl::Context *context,
                                  BufferImpl *source,
                                  GLintptr sourceOffset,
                                  GLintptr destOffset,
                                  GLsizeiptr size)
{
    BufferNULL *sourceNULL = GetAs<BufferNULL>(source);
    if (size > 0)
    {
        memcpy(mData.data() + destOffset, sourceNULL->mData.data() + sourceOffset, size);
    }
    return gl::NoError();
}

gl::Error BufferNULL::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    *mapPtr = mData.data();
    return gl::NoError();
}

gl::Error BufferNULL::mapRange(const gl::Context *context,
                               size_t offset,
                               size_t length,
                               GLbitfield access,
                               void **mapPtr)
{
    *mapPtr = mData.data() + offset;
    return gl::NoError();
}

gl::Error BufferNULL::unmap(const gl::Context *context, GLboolean *result)
{
    *result = GL_TRUE;
    return gl::NoError();
}

gl::Error BufferNULL::getIndexRange(const gl::Context *context,
                                    GLenum type,
                                    size_t offset,
                                    size_t count,
                                    bool primitiveRestartEnabled,
                                    gl::IndexRange *outRange)
{
    *outRange = gl::ComputeIndexRange(type, mData.data() + offset, count, primitiveRestartEnabled);
    return gl::NoError();
}

uint8_t *BufferNULL::getDataPtr()
{
    return mData.data();
}

const uint8_t *BufferNULL::getDataPtr() const
{
    return mData.data();
}

}  // namespace rx
