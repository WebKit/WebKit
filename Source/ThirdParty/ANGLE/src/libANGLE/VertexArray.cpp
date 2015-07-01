//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the state class for mananging GLES 3 Vertex Array Objects.
//

#include "libANGLE/VertexArray.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/renderer/VertexArrayImpl.h"

namespace gl
{

VertexArray::VertexArray(rx::VertexArrayImpl *impl, GLuint id, size_t maxAttribs)
    : mId(id),
      mVertexArray(impl),
      mVertexAttributes(maxAttribs),
      mMaxEnabledAttribute(0)
{
    ASSERT(impl != NULL);
}

VertexArray::~VertexArray()
{
    SafeDelete(mVertexArray);

    for (size_t i = 0; i < getMaxAttribs(); i++)
    {
        mVertexAttributes[i].buffer.set(NULL);
    }
    mElementArrayBuffer.set(NULL);
}

GLuint VertexArray::id() const
{
    return mId;
}

void VertexArray::detachBuffer(GLuint bufferName)
{
    for (size_t attribute = 0; attribute < getMaxAttribs(); attribute++)
    {
        if (mVertexAttributes[attribute].buffer.id() == bufferName)
        {
            mVertexAttributes[attribute].buffer.set(NULL);
        }
    }

    if (mElementArrayBuffer.id() == bufferName)
    {
        mElementArrayBuffer.set(NULL);
    }
}

const VertexAttribute& VertexArray::getVertexAttribute(size_t attributeIndex) const
{
    ASSERT(attributeIndex < getMaxAttribs());
    return mVertexAttributes[attributeIndex];
}

const std::vector<VertexAttribute> &VertexArray::getVertexAttributes() const
{
    return mVertexAttributes;
}

void VertexArray::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    ASSERT(index < getMaxAttribs());
    mVertexAttributes[index].divisor = divisor;
    mVertexArray->setAttributeDivisor(index, divisor);
}

void VertexArray::enableAttribute(unsigned int attributeIndex, bool enabledState)
{
    ASSERT(attributeIndex < getMaxAttribs());
    mVertexAttributes[attributeIndex].enabled = enabledState;
    mVertexArray->enableAttribute(attributeIndex, enabledState);

    // Update state cache
    if (enabledState)
    {
        mMaxEnabledAttribute = std::max(attributeIndex, mMaxEnabledAttribute);
    }
    else if (mMaxEnabledAttribute == attributeIndex)
    {
        while (mMaxEnabledAttribute > 0 && !mVertexAttributes[mMaxEnabledAttribute].enabled)
        {
            --mMaxEnabledAttribute;
        }
    }
}

void VertexArray::setAttributeState(unsigned int attributeIndex, gl::Buffer *boundBuffer, GLint size, GLenum type,
                                    bool normalized, bool pureInteger, GLsizei stride, const void *pointer)
{
    ASSERT(attributeIndex < getMaxAttribs());
    mVertexAttributes[attributeIndex].buffer.set(boundBuffer);
    mVertexAttributes[attributeIndex].size = size;
    mVertexAttributes[attributeIndex].type = type;
    mVertexAttributes[attributeIndex].normalized = normalized;
    mVertexAttributes[attributeIndex].pureInteger = pureInteger;
    mVertexAttributes[attributeIndex].stride = stride;
    mVertexAttributes[attributeIndex].pointer = pointer;
    mVertexArray->setAttribute(attributeIndex, mVertexAttributes[attributeIndex]);
}

void VertexArray::setElementArrayBuffer(Buffer *buffer)
{
    mElementArrayBuffer.set(buffer);
    mVertexArray->setElementArrayBuffer(buffer);
}

}
