//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the state class for mananging GLES 3 Vertex Array Objects.
//

#include "libANGLE/VertexArray.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/VertexArrayImpl.h"

namespace gl
{

VertexArrayState::VertexArrayState(size_t maxAttribs)
    : mLabel(), mVertexAttributes(maxAttribs), mMaxEnabledAttribute(0)
{
}

VertexArrayState::~VertexArrayState()
{
    for (size_t i = 0; i < getMaxAttribs(); i++)
    {
        mVertexAttributes[i].buffer.set(nullptr);
    }
    mElementArrayBuffer.set(nullptr);
}

VertexArray::VertexArray(rx::GLImplFactory *factory, GLuint id, size_t maxAttribs)
    : mId(id), mState(maxAttribs), mVertexArray(factory->createVertexArray(mState))
{
}

VertexArray::~VertexArray()
{
    SafeDelete(mVertexArray);
}

GLuint VertexArray::id() const
{
    return mId;
}

void VertexArray::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &VertexArray::getLabel() const
{
    return mState.mLabel;
}

void VertexArray::detachBuffer(GLuint bufferName)
{
    for (size_t attribute = 0; attribute < getMaxAttribs(); attribute++)
    {
        if (mState.mVertexAttributes[attribute].buffer.id() == bufferName)
        {
            mState.mVertexAttributes[attribute].buffer.set(nullptr);
        }
    }

    if (mState.mElementArrayBuffer.id() == bufferName)
    {
        mState.mElementArrayBuffer.set(nullptr);
    }
}

const VertexAttribute &VertexArray::getVertexAttribute(size_t attributeIndex) const
{
    ASSERT(attributeIndex < getMaxAttribs());
    return mState.mVertexAttributes[attributeIndex];
}

void VertexArray::setVertexAttribDivisor(size_t index, GLuint divisor)
{
    ASSERT(index < getMaxAttribs());
    mState.mVertexAttributes[index].divisor = divisor;
    mDirtyBits.set(DIRTY_BIT_ATTRIB_0_DIVISOR + index);
}

void VertexArray::enableAttribute(size_t attributeIndex, bool enabledState)
{
    ASSERT(attributeIndex < getMaxAttribs());
    mState.mVertexAttributes[attributeIndex].enabled = enabledState;
    mDirtyBits.set(DIRTY_BIT_ATTRIB_0_ENABLED + attributeIndex);

    // Update state cache
    if (enabledState)
    {
        mState.mMaxEnabledAttribute = std::max(attributeIndex + 1, mState.mMaxEnabledAttribute);
    }
    else if (mState.mMaxEnabledAttribute == attributeIndex + 1)
    {
        while (mState.mMaxEnabledAttribute > 0 &&
               !mState.mVertexAttributes[mState.mMaxEnabledAttribute - 1].enabled)
        {
            --mState.mMaxEnabledAttribute;
        }
    }
}

void VertexArray::setAttributeState(size_t attributeIndex, gl::Buffer *boundBuffer, GLint size, GLenum type,
                                    bool normalized, bool pureInteger, GLsizei stride, const void *pointer)
{
    ASSERT(attributeIndex < getMaxAttribs());

    VertexAttribute *attrib = &mState.mVertexAttributes[attributeIndex];

    attrib->buffer.set(boundBuffer);
    attrib->size = size;
    attrib->type = type;
    attrib->normalized = normalized;
    attrib->pureInteger = pureInteger;
    attrib->stride = stride;
    attrib->pointer = pointer;
    mDirtyBits.set(DIRTY_BIT_ATTRIB_0_POINTER + attributeIndex);
}

void VertexArray::setElementArrayBuffer(Buffer *buffer)
{
    mState.mElementArrayBuffer.set(buffer);
    mDirtyBits.set(DIRTY_BIT_ELEMENT_ARRAY_BUFFER);
}

void VertexArray::syncImplState()
{
    if (mDirtyBits.any())
    {
        mVertexArray->syncState(mDirtyBits);
        mDirtyBits.reset();
    }
}

}
