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
static const GLenum SourceBufferOperationTarget = GL_COPY_READ_BUFFER;

// Use the GL_ELEMENT_ARRAY_BUFFER binding for most operations since it's available on all
// supported GL versions and doesn't affect any current state when it changes.
static const GLenum DestBufferOperationTarget = GL_ARRAY_BUFFER;

BufferGL::BufferGL(const FunctionsGL *functions, StateManagerGL *stateManager)
    : BufferImpl(),
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

gl::Error BufferGL::setData(GLenum /*target*/, const void *data, size_t size, GLenum usage)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mFunctions->bufferData(DestBufferOperationTarget, size, data, usage);

    if (mShadowBufferData)
    {
        if (!mShadowCopy.resize(size))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to resize buffer data shadow copy.");
        }

        if (size > 0 && data != nullptr)
        {
            memcpy(mShadowCopy.data(), data, size);
        }
    }

    mBufferSize = size;

    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::setSubData(GLenum /*target*/, const void *data, size_t size, size_t offset)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mFunctions->bufferSubData(DestBufferOperationTarget, offset, size, data);

    if (mShadowBufferData && size > 0)
    {
        memcpy(mShadowCopy.data() + offset, data, size);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size)
{
    BufferGL *sourceGL = GetAs<BufferGL>(source);

    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mStateManager->bindBuffer(SourceBufferOperationTarget, sourceGL->getBufferID());

    mFunctions->copyBufferSubData(SourceBufferOperationTarget, DestBufferOperationTarget, sourceOffset, destOffset, size);

    if (mShadowBufferData && size > 0)
    {
        ASSERT(sourceGL->mShadowBufferData);
        memcpy(mShadowCopy.data() + destOffset, sourceGL->mShadowCopy.data() + sourceOffset, size);
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::map(GLenum access, GLvoid **mapPtr)
{
    if (mShadowBufferData)
    {
        *mapPtr = mShadowCopy.data();
    }
    else
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr = mFunctions->mapBuffer(DestBufferOperationTarget, access);
    }

    mIsMapped = true;
    mMapOffset = 0;
    mMapSize   = mBufferSize;

    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::mapRange(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr)
{
    if (mShadowBufferData)
    {
        *mapPtr = mShadowCopy.data() + offset;
    }
    else
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *mapPtr = mFunctions->mapBufferRange(DestBufferOperationTarget, offset, length, access);
    }

    mIsMapped = true;
    mMapOffset = offset;
    mMapSize   = length;

    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::unmap(GLboolean *result)
{
    ASSERT(result);
    ASSERT(mIsMapped);

    if (mShadowBufferData)
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        mFunctions->bufferSubData(DestBufferOperationTarget, mMapOffset, mMapSize,
                                  mShadowCopy.data() + mMapOffset);
        *result = GL_TRUE;
    }
    else
    {
        mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
        *result = mFunctions->unmapBuffer(DestBufferOperationTarget);
    }

    mIsMapped = false;
    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::getIndexRange(GLenum type,
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
        const uint8_t *bufferData = MapBufferRangeWithFallback(
            mFunctions, DestBufferOperationTarget, offset, count * typeInfo.bytes, GL_MAP_READ_BIT);
        *outRange = gl::ComputeIndexRange(type, bufferData, count, primitiveRestartEnabled);
        mFunctions->unmapBuffer(DestBufferOperationTarget);
    }

    return gl::Error(GL_NO_ERROR);
}

GLuint BufferGL::getBufferID() const
{
    return mBufferID;
}

}
