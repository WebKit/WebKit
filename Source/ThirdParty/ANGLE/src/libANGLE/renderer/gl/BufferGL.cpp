//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferGL.cpp: Implements the class methods for BufferGL.

#include "libANGLE/renderer/gl/BufferGL.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace rx
{

// Use the GL_COPY_READ_BUFFER binding when two buffers need to be bound simultaneously.
// GL_ELEMENT_ARRAY_BUFFER is supported on more versions but can modify the state of the currently
// bound VAO.  Two simultaneous buffer bindings are only needed for glCopyBufferSubData which also
// adds the GL_COPY_READ_BUFFER binding.
static constexpr gl::BufferBinding SourceBufferOperationTarget = gl::BufferBinding::CopyRead;

// Use the GL_ELEMENT_ARRAY_BUFFER binding for most operations since it's available on all
// supported GL versions and doesn't affect any current state when it changes.
static constexpr gl::BufferBinding DestBufferOperationTarget = gl::BufferBinding::Array;

BufferGL::BufferGL(const gl::BufferState &state,
                   const FunctionsGL *functions,
                   StateManagerGL *stateManager)
    : BufferImpl(state),
      mIsMapped(false),
      mMapOffset(0),
      mMapSize(0),
      mShadowBufferData(!CanMapBufferForRead(functions)),
      mShadowCopy(),
      mBufferSize(0),
      mFunctions(functions),
      mStateManager(stateManager),
      mBufferID(0)
{
    ASSERT(mFunctions);
    ASSERT(mStateManager);

    mFunctions->genBuffers(1, &mBufferID);
}

BufferGL::~BufferGL()
{
    mStateManager->deleteBuffer(mBufferID);
    mBufferID = 0;
}

gl::Error BufferGL::setData(const gl::Context * /*context*/,
                            gl::BufferBinding /*target*/,
                            const void *data,
                            size_t size,
                            gl::BufferUsage usage)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mFunctions->bufferData(gl::ToGLenum(DestBufferOperationTarget), size, data, ToGLenum(usage));

    if (mShadowBufferData)
    {
        if (!mShadowCopy.resize(size))
        {
            return gl::OutOfMemory() << "Failed to resize buffer data shadow copy.";
        }

        if (size > 0 && data != nullptr)
        {
            memcpy(mShadowCopy.data(), data, size);
        }
    }

    mBufferSize = size;

    return gl::NoError();
}

gl::Error BufferGL::setSubData(const gl::Context * /*context*/,
                               gl::BufferBinding /*target*/,
                               const void *data,
                               size_t size,
                               size_t offset)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mFunctions->bufferSubData(gl::ToGLenum(DestBufferOperationTarget), offset, size, data);

    if (mShadowBufferData && size > 0)
    {
        memcpy(mShadowCopy.data() + offset, data, size);
    }

    return gl::NoError();
}

gl::Error BufferGL::copySubData(const gl::Context *context,
                                BufferImpl *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    BufferGL *sourceGL = GetAs<BufferGL>(source);

    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mStateManager->bindBuffer(SourceBufferOperationTarget, sourceGL->getBufferID());

    mFunctions->copyBufferSubData(gl::ToGLenum(SourceBufferOperationTarget),
                                  gl::ToGLenum(DestBufferOperationTarget), sourceOffset, destOffset,
                                  size);

    if (mShadowBufferData && size > 0)
    {
        ASSERT(sourceGL->mShadowBufferData);
        memcpy(mShadowCopy.data() + destOffset, sourceGL->mShadowCopy.data() + sourceOffset, size);
    }

    return gl::NoError();
}

gl::Error BufferGL::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    if (mShadowBufferData)
    {
        *mapPtr = mShadowCopy.data();
    }
    else if (mFunctions->mapBuffer)
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr = mFunctions->mapBuffer(gl::ToGLenum(DestBufferOperationTarget), access);
    }
    else
    {
        ASSERT(mFunctions->mapBufferRange && access == GL_WRITE_ONLY_OES);
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr = mFunctions->mapBufferRange(gl::ToGLenum(DestBufferOperationTarget), 0,
                                             mBufferSize, GL_MAP_WRITE_BIT);
    }

    mIsMapped  = true;
    mMapOffset = 0;
    mMapSize   = mBufferSize;

    return gl::NoError();
}

gl::Error BufferGL::mapRange(const gl::Context *context,
                             size_t offset,
                             size_t length,
                             GLbitfield access,
                             void **mapPtr)
{
    if (mShadowBufferData)
    {
        *mapPtr = mShadowCopy.data() + offset;
    }
    else
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr = mFunctions->mapBufferRange(gl::ToGLenum(DestBufferOperationTarget), offset,
                                             length, access);
    }

    mIsMapped  = true;
    mMapOffset = offset;
    mMapSize   = length;

    return gl::NoError();
}

gl::Error BufferGL::unmap(const gl::Context *context, GLboolean *result)
{
    ASSERT(result);
    ASSERT(mIsMapped);

    if (mShadowBufferData)
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        mFunctions->bufferSubData(gl::ToGLenum(DestBufferOperationTarget), mMapOffset, mMapSize,
                                  mShadowCopy.data() + mMapOffset);
        *result = GL_TRUE;
    }
    else
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *result = mFunctions->unmapBuffer(gl::ToGLenum(DestBufferOperationTarget));
    }

    mIsMapped = false;
    return gl::NoError();
}

gl::Error BufferGL::getIndexRange(const gl::Context *context,
                                  GLenum type,
                                  size_t offset,
                                  size_t count,
                                  bool primitiveRestartEnabled,
                                  gl::IndexRange *outRange)
{
    ASSERT(!mIsMapped);

    if (mShadowBufferData)
    {
        *outRange = gl::ComputeIndexRange(type, mShadowCopy.data() + offset, count,
                                          primitiveRestartEnabled);
    }
    else
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);

        const gl::Type &typeInfo  = gl::GetTypeInfo(type);
        const uint8_t *bufferData =
            MapBufferRangeWithFallback(mFunctions, gl::ToGLenum(DestBufferOperationTarget), offset,
                                       count * typeInfo.bytes, GL_MAP_READ_BIT);
        *outRange = gl::ComputeIndexRange(type, bufferData, count, primitiveRestartEnabled);
        mFunctions->unmapBuffer(gl::ToGLenum(DestBufferOperationTarget));
    }

    return gl::NoError();
}

GLuint BufferGL::getBufferID() const
{
    return mBufferID;
}
}
