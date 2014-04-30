#include "precompiled.h"
//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.cpp: Implements the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#include "libGLESv2/Buffer.h"

#include "libGLESv2/renderer/VertexBuffer.h"
#include "libGLESv2/renderer/IndexBuffer.h"
#include "libGLESv2/renderer/BufferStorage.h"
#include "libGLESv2/renderer/Renderer.h"

namespace gl
{

Buffer::Buffer(rx::Renderer *renderer, GLuint id)
    : RefCountObject(id),
      mRenderer(renderer),
      mUsage(GL_DYNAMIC_DRAW),
      mAccessFlags(0),
      mMapped(GL_FALSE),
      mMapPointer(NULL),
      mMapOffset(0),
      mMapLength(0),
      mBufferStorage(NULL),
      mStaticVertexBuffer(NULL),
      mStaticIndexBuffer(NULL),
      mUnmodifiedDataUse(0)
{
    mBufferStorage = renderer->createBufferStorage();
}

Buffer::~Buffer()
{
    delete mBufferStorage;
    delete mStaticVertexBuffer;
    delete mStaticIndexBuffer;
}

void Buffer::bufferData(const void *data, GLsizeiptr size, GLenum usage)
{
    mBufferStorage->clear();
    mIndexRangeCache.clear();
    mBufferStorage->setData(data, size, 0);

    mUsage = usage;

    invalidateStaticData();

    if (usage == GL_STATIC_DRAW)
    {
        mStaticVertexBuffer = new rx::StaticVertexBufferInterface(mRenderer);
        mStaticIndexBuffer = new rx::StaticIndexBufferInterface(mRenderer);
    }
}

void Buffer::bufferSubData(const void *data, GLsizeiptr size, GLintptr offset)
{
    mBufferStorage->setData(data, size, offset);
    mIndexRangeCache.invalidateRange(offset, size);
    invalidateStaticData();
}

void Buffer::copyBufferSubData(Buffer* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size)
{
    mBufferStorage->copyData(source->mBufferStorage, size, sourceOffset, destOffset);
    invalidateStaticData();
}

GLvoid *Buffer::mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    ASSERT(!mMapped);

    void *dataPointer = mBufferStorage->map(access);

    mMapped = GL_TRUE;
    mMapPointer = static_cast<GLvoid*>(static_cast<GLubyte*>(dataPointer) + offset);
    mMapOffset = static_cast<GLint64>(offset);
    mMapLength = static_cast<GLint64>(length);
    mAccessFlags = static_cast<GLint>(access);

    return mMapPointer;
}

void Buffer::unmap()
{
    ASSERT(mMapped);

    mBufferStorage->unmap();

    mMapped = GL_FALSE;
    mMapPointer = NULL;
    mMapOffset = 0;
    mMapLength = 0;
    mAccessFlags = 0;
}

rx::BufferStorage *Buffer::getStorage() const
{
    return mBufferStorage;
}

GLint64 Buffer::size() const
{
    return static_cast<GLint64>(mBufferStorage->getSize());
}

GLenum Buffer::usage() const
{
    return mUsage;
}

GLint Buffer::accessFlags() const
{
    return mAccessFlags;
}

GLboolean Buffer::mapped() const
{
    return mMapped;
}

GLvoid *Buffer::mapPointer() const
{
    return mMapPointer;
}

GLint64 Buffer::mapOffset() const
{
    return mMapOffset;
}

GLint64 Buffer::mapLength() const
{
    return mMapLength;
}

void Buffer::markTransformFeedbackUsage()
{
    mBufferStorage->markTransformFeedbackUsage();
    invalidateStaticData();
}

rx::StaticVertexBufferInterface *Buffer::getStaticVertexBuffer()
{
    return mStaticVertexBuffer;
}

rx::StaticIndexBufferInterface *Buffer::getStaticIndexBuffer()
{
    return mStaticIndexBuffer;
}

void Buffer::invalidateStaticData()
{
    if ((mStaticVertexBuffer && mStaticVertexBuffer->getBufferSize() != 0) || (mStaticIndexBuffer && mStaticIndexBuffer->getBufferSize() != 0))
    {
        delete mStaticVertexBuffer;
        mStaticVertexBuffer = NULL;

        delete mStaticIndexBuffer;
        mStaticIndexBuffer = NULL;
    }

    mUnmodifiedDataUse = 0;
}

// Creates static buffers if sufficient used data has been left unmodified
void Buffer::promoteStaticUsage(int dataSize)
{
    if (!mStaticVertexBuffer && !mStaticIndexBuffer)
    {
        mUnmodifiedDataUse += dataSize;

        if (mUnmodifiedDataUse > 3 * mBufferStorage->getSize())
        {
            mStaticVertexBuffer = new rx::StaticVertexBufferInterface(mRenderer);
            mStaticIndexBuffer = new rx::StaticIndexBufferInterface(mRenderer);
        }
    }
}

rx::IndexRangeCache *Buffer::getIndexRangeCache()
{
    return &mIndexRangeCache;
}

}
