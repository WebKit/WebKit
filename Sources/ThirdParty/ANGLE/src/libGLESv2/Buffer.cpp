//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.cpp: Implements the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#include "libGLESv2/Buffer.h"

namespace gl
{

Buffer::Buffer(GLuint id) : RefCountObject(id)
{
    mContents = NULL;
    mSize = 0;
    mUsage = GL_DYNAMIC_DRAW;
}

Buffer::~Buffer()
{
    delete[] mContents;
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
}

void Buffer::bufferSubData(const void *data, GLsizeiptr size, GLintptr offset)
{
    memcpy(mContents + offset, data, size);
}

}
