//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.cpp: Implements the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#include "libGLESv2/Buffer.h"

#include "libGLESv2/main.h"
#include "libGLESv2/geometry/VertexDataManager.h"
#include "libGLESv2/geometry/IndexDataManager.h"

namespace gl
{

Buffer::Buffer(GLuint id) : RefCountObject(id)
{
    mContents = NULL;
    mSize = 0;
    mUsage = GL_DYNAMIC_DRAW;

    mVertexBuffer = NULL;
    mIndexBuffer = NULL;
}

Buffer::~Buffer()
{
    delete[] mContents;
    delete mVertexBuffer;
    delete mIndexBuffer;
}

void Buffer::bufferData(const void *data, GLsizeiptr size, GLenum usage)
{
    if (size == 0)
    {
        delete[] mContents;
        mContents = NULL;
    }
    else if (size != mSize)
    {
        delete[] mContents;
        mContents = new GLubyte[size];
        memset(mContents, 0, size);
    }

    if (data != NULL && size > 0)
    {
        memcpy(mContents, data, size);
    }

    mSize = size;
    mUsage = usage;

    invalidateStaticData();

    if (usage == GL_STATIC_DRAW)
    {
        mVertexBuffer = new StaticVertexBuffer(getDevice());
        mIndexBuffer = new StaticIndexBuffer(getDevice());
    }
}

void Buffer::bufferSubData(const void *data, GLsizeiptr size, GLintptr offset)
{
    memcpy(mContents + offset, data, size);

    if ((mVertexBuffer && mVertexBuffer->size() != 0) || (mIndexBuffer && mIndexBuffer->size() != 0))
    {
        invalidateStaticData();

        if (mUsage == GL_STATIC_DRAW)
        {
            // If applications update the buffer data after it has already been used in a draw call,
            // it most likely isn't used as a static buffer so we should fall back to streaming usage
            // for best performance. So ignore the usage hint and don't create new static buffers.
        //  mVertexBuffer = new StaticVertexBuffer(getDevice());
        //  mIndexBuffer = new StaticIndexBuffer(getDevice());
        }
    }
}

StaticVertexBuffer *Buffer::getVertexBuffer()
{
    return mVertexBuffer;
}

StaticIndexBuffer *Buffer::getIndexBuffer()
{
    return mIndexBuffer;
}

void Buffer::invalidateStaticData()
{
    delete mVertexBuffer;
    mVertexBuffer = NULL;

    delete mIndexBuffer;
    mIndexBuffer = NULL;
}

}
