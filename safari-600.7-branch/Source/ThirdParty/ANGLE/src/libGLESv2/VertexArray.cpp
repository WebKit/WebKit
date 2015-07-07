#include "precompiled.h"
//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the state class for mananging GLES 3 Vertex Array Objects.
//

#include "libGLESv2/VertexArray.h"
#include "libGLESv2/Buffer.h"

namespace gl
{

VertexArray::VertexArray(rx::Renderer *renderer, GLuint id)
    : RefCountObject(id)
{
}

VertexArray::~VertexArray()
{
    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        mVertexAttributes[i].mBoundBuffer.set(NULL);
    }
    mElementArrayBuffer.set(NULL);
}

void VertexArray::detachBuffer(GLuint bufferName)
{
    for (int attribute = 0; attribute < MAX_VERTEX_ATTRIBS; attribute++)
    {
        if (mVertexAttributes[attribute].mBoundBuffer.id() == bufferName)
        {
            mVertexAttributes[attribute].mBoundBuffer.set(NULL);
        }
    }

    if (mElementArrayBuffer.id() == bufferName)
    {
        mElementArrayBuffer.set(NULL);
    }
}

const VertexAttribute& VertexArray::getVertexAttribute(unsigned int attributeIndex) const
{
    ASSERT(attributeIndex < MAX_VERTEX_ATTRIBS);
    return mVertexAttributes[attributeIndex];
}

void VertexArray::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    ASSERT(index < gl::MAX_VERTEX_ATTRIBS);
    mVertexAttributes[index].mDivisor = divisor;
}

void VertexArray::enableAttribute(unsigned int attributeIndex, bool enabledState)
{
    ASSERT(attributeIndex < gl::MAX_VERTEX_ATTRIBS);
    mVertexAttributes[attributeIndex].mArrayEnabled = enabledState;
}

void VertexArray::setAttributeState(unsigned int attributeIndex, gl::Buffer *boundBuffer, GLint size, GLenum type,
                                    bool normalized, bool pureInteger, GLsizei stride, const void *pointer)
{
    ASSERT(attributeIndex < gl::MAX_VERTEX_ATTRIBS);
    mVertexAttributes[attributeIndex].setState(boundBuffer, size, type, normalized, pureInteger, stride, pointer);
}

}