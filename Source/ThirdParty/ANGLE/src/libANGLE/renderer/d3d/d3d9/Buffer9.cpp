//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer9.cpp Defines the Buffer9 class.

#include "libANGLE/renderer/d3d/d3d9/Buffer9.h"
#include "libANGLE/renderer/d3d/d3d9/Renderer9.h"

namespace rx
{

Buffer9::Buffer9(const gl::BufferState &state, Renderer9 *renderer)
    : BufferD3D(state, renderer), mSize(0)
{
}

Buffer9::~Buffer9()
{
    mSize = 0;
}

size_t Buffer9::getSize() const
{
    return mSize;
}

bool Buffer9::supportsDirectBinding() const
{
    return false;
}

gl::Error Buffer9::setData(const gl::Context *context,
                           gl::BufferBinding /*target*/,
                           const void *data,
                           size_t size,
                           gl::BufferUsage usage)
{
    if (size > mMemory.size())
    {
        if (!mMemory.resize(size))
        {
            return gl::OutOfMemory() << "Failed to resize internal buffer.";
        }
    }

    mSize = size;
    if (data && size > 0)
    {
        memcpy(mMemory.data(), data, size);
    }

    updateD3DBufferUsage(context, usage);

    invalidateStaticData(context);

    return gl::NoError();
}

gl::Error Buffer9::getData(const gl::Context *context, const uint8_t **outData)
{
    *outData = mMemory.data();
    return gl::NoError();
}

gl::Error Buffer9::setSubData(const gl::Context *context,
                              gl::BufferBinding /*target*/,
                              const void *data,
                              size_t size,
                              size_t offset)
{
    if (offset + size > mMemory.size())
    {
        if (!mMemory.resize(offset + size))
        {
            return gl::OutOfMemory() << "Failed to resize internal buffer.";
        }
    }

    mSize = std::max(mSize, offset + size);
    if (data && size > 0)
    {
        memcpy(mMemory.data() + offset, data, size);
    }

    invalidateStaticData(context);

    return gl::NoError();
}

gl::Error Buffer9::copySubData(const gl::Context *context,
                               BufferImpl *source,
                               GLintptr sourceOffset,
                               GLintptr destOffset,
                               GLsizeiptr size)
{
    // Note: this method is currently unreachable
    Buffer9 *sourceBuffer = GetAs<Buffer9>(source);
    ASSERT(sourceBuffer);

    memcpy(mMemory.data() + destOffset, sourceBuffer->mMemory.data() + sourceOffset, size);

    invalidateStaticData(context);

    return gl::NoError();
}

// We do not support buffer mapping in D3D9
gl::Error Buffer9::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error Buffer9::mapRange(const gl::Context *context,
                            size_t offset,
                            size_t length,
                            GLbitfield access,
                            void **mapPtr)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error Buffer9::unmap(const gl::Context *context, GLboolean *result)
{
    UNREACHABLE();
    return gl::InternalError();
}

gl::Error Buffer9::markTransformFeedbackUsage(const gl::Context *context)
{
    UNREACHABLE();
    return gl::InternalError();
}

}  // namespace rx
