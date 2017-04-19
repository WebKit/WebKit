//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.cpp: Implements the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#include "libANGLE/Buffer.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"

namespace gl
{

BufferState::BufferState()
    : mLabel(),
      mUsage(GL_STATIC_DRAW),
      mSize(0),
      mAccessFlags(0),
      mAccess(GL_WRITE_ONLY_OES),
      mMapped(GL_FALSE),
      mMapPointer(nullptr),
      mMapOffset(0),
      mMapLength(0)
{
}

BufferState::~BufferState()
{
}

Buffer::Buffer(rx::GLImplFactory *factory, GLuint id)
    : RefCountObject(id), mImpl(factory->createBuffer(mState))
{
}

Buffer::~Buffer()
{
    SafeDelete(mImpl);
}

void Buffer::destroy(const Context *context)
{
    // In tests, mImpl might be null.
    if (mImpl)
        mImpl->destroy(rx::SafeGetImpl(context));
}

void Buffer::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &Buffer::getLabel() const
{
    return mState.mLabel;
}

Error Buffer::bufferData(const Context *context,
                         GLenum target,
                         const void *data,
                         GLsizeiptr size,
                         GLenum usage)
{
    const void *dataForImpl = data;

    // If we are using robust resource init, make sure the buffer starts cleared.
    // TODO(jmadill): Investigate lazier clearing.
    if (context && context->getGLState().isRobustResourceInitEnabled() && data == nullptr)
    {
        angle::MemoryBuffer *scratchBuffer = nullptr;
        ANGLE_TRY(context->getScratchBuffer(static_cast<size_t>(size), &scratchBuffer));
        std::fill(scratchBuffer->data(), scratchBuffer->data() + size, static_cast<uint8_t>(0));
        dataForImpl = scratchBuffer->data();
    }

    ANGLE_TRY(mImpl->setData(rx::SafeGetImpl(context), target, dataForImpl, size, usage));

    mIndexRangeCache.clear();
    mState.mUsage = usage;
    mState.mSize  = size;

    return NoError();
}

Error Buffer::bufferSubData(const Context *context,
                            GLenum target,
                            const void *data,
                            GLsizeiptr size,
                            GLintptr offset)
{
    ANGLE_TRY(mImpl->setSubData(rx::SafeGetImpl(context), target, data, size, offset));

    mIndexRangeCache.invalidateRange(static_cast<unsigned int>(offset), static_cast<unsigned int>(size));

    return NoError();
}

Error Buffer::copyBufferSubData(const Context *context,
                                Buffer *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    ANGLE_TRY(mImpl->copySubData(rx::SafeGetImpl(context), source->getImplementation(),
                                 sourceOffset, destOffset, size));

    mIndexRangeCache.invalidateRange(static_cast<unsigned int>(destOffset), static_cast<unsigned int>(size));

    return NoError();
}

Error Buffer::map(const Context *context, GLenum access)
{
    ASSERT(!mState.mMapped);

    mState.mMapPointer = nullptr;
    ANGLE_TRY(mImpl->map(rx::SafeGetImpl(context), access, &mState.mMapPointer));

    ASSERT(access == GL_WRITE_ONLY_OES);

    mState.mMapped      = GL_TRUE;
    mState.mMapOffset   = 0;
    mState.mMapLength   = mState.mSize;
    mState.mAccess      = access;
    mState.mAccessFlags = GL_MAP_WRITE_BIT;
    mIndexRangeCache.clear();

    return NoError();
}

Error Buffer::mapRange(const Context *context,
                       GLintptr offset,
                       GLsizeiptr length,
                       GLbitfield access)
{
    ASSERT(!mState.mMapped);
    ASSERT(offset + length <= mState.mSize);

    mState.mMapPointer = nullptr;
    ANGLE_TRY(
        mImpl->mapRange(rx::SafeGetImpl(context), offset, length, access, &mState.mMapPointer));

    mState.mMapped      = GL_TRUE;
    mState.mMapOffset   = static_cast<GLint64>(offset);
    mState.mMapLength   = static_cast<GLint64>(length);
    mState.mAccess      = GL_WRITE_ONLY_OES;
    mState.mAccessFlags = access;

    // The OES_mapbuffer extension states that GL_WRITE_ONLY_OES is the only valid
    // value for GL_BUFFER_ACCESS_OES because it was written against ES2.  Since there is
    // no update for ES3 and the GL_READ_ONLY and GL_READ_WRITE enums don't exist for ES,
    // we cannot properly set GL_BUFFER_ACCESS_OES when glMapBufferRange is called.

    if ((access & GL_MAP_WRITE_BIT) > 0)
    {
        mIndexRangeCache.invalidateRange(static_cast<unsigned int>(offset), static_cast<unsigned int>(length));
    }

    return NoError();
}

Error Buffer::unmap(const Context *context, GLboolean *result)
{
    ASSERT(mState.mMapped);

    *result = GL_FALSE;
    ANGLE_TRY(mImpl->unmap(rx::SafeGetImpl(context), result));

    mState.mMapped      = GL_FALSE;
    mState.mMapPointer  = nullptr;
    mState.mMapOffset   = 0;
    mState.mMapLength   = 0;
    mState.mAccess      = GL_WRITE_ONLY_OES;
    mState.mAccessFlags = 0;

    return NoError();
}

void Buffer::onTransformFeedback()
{
    mIndexRangeCache.clear();
}

void Buffer::onPixelUnpack()
{
    mIndexRangeCache.clear();
}

Error Buffer::getIndexRange(GLenum type,
                            size_t offset,
                            size_t count,
                            bool primitiveRestartEnabled,
                            IndexRange *outRange) const
{
    if (mIndexRangeCache.findRange(type, offset, count, primitiveRestartEnabled, outRange))
    {
        return NoError();
    }

    ANGLE_TRY(mImpl->getIndexRange(type, offset, count, primitiveRestartEnabled, outRange));

    mIndexRangeCache.addRange(type, offset, count, primitiveRestartEnabled, *outRange);

    return NoError();
}

}  // namespace gl
