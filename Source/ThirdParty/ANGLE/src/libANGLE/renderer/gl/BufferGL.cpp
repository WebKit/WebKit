//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferGL.cpp: Implements the class methods for BufferGL.

#include "libANGLE/renderer/gl/BufferGL.h"

#include "common/debug.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"

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
    if (mBufferID)
    {
        mFunctions->deleteBuffers(1, &mBufferID);
        mBufferID = 0;
    }
}

gl::Error BufferGL::setData(const void* data, size_t size, GLenum usage)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mFunctions->bufferData(DestBufferOperationTarget, size, data, usage);
    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::setSubData(const void* data, size_t size, size_t offset)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mFunctions->bufferSubData(DestBufferOperationTarget, offset, size, data);
    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size)
{
    BufferGL *sourceGL = GetAs<BufferGL>(source);

    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    mStateManager->bindBuffer(SourceBufferOperationTarget, sourceGL->getBufferID());

    mFunctions->copyBufferSubData(SourceBufferOperationTarget, DestBufferOperationTarget, sourceOffset, destOffset, size);

    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::map(GLenum access, GLvoid **mapPtr)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    *mapPtr = mFunctions->mapBuffer(DestBufferOperationTarget, access);
    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::mapRange(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr)
{
    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    *mapPtr = mFunctions->mapBufferRange(DestBufferOperationTarget, offset, length, access);
    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::unmap(GLboolean *result)
{
    ASSERT(*result);

    mStateManager->bindBuffer(DestBufferOperationTarget, mBufferID);
    *result = mFunctions->unmapBuffer(DestBufferOperationTarget);
    return gl::Error(GL_NO_ERROR);
}

gl::Error BufferGL::getData(const uint8_t **outData)
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

GLuint BufferGL::getBufferID() const
{
    return mBufferID;
}

}
