#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer.cpp: Defines the abstract VertexBuffer class and VertexBufferInterface
// class with derivations, classes that perform graphics API agnostic vertex buffer operations.

#include "libGLESv2/renderer/VertexBuffer.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/VertexAttribute.h"
#include "libGLESv2/renderer/BufferStorage.h"
#include "common/mathutil.h"

namespace rx
{

unsigned int VertexBuffer::mNextSerial = 1;

VertexBuffer::VertexBuffer()
{
    updateSerial();
}

VertexBuffer::~VertexBuffer()
{
}

void VertexBuffer::updateSerial()
{
    mSerial = mNextSerial++;
}

unsigned int VertexBuffer::getSerial() const
{
    return mSerial;
}

VertexBufferInterface::VertexBufferInterface(rx::Renderer *renderer, bool dynamic) : mRenderer(renderer)
{
    mDynamic = dynamic;
    mWritePosition = 0;
    mReservedSpace = 0;

    mVertexBuffer = renderer->createVertexBuffer();
}

VertexBufferInterface::~VertexBufferInterface()
{
    delete mVertexBuffer;
}

unsigned int VertexBufferInterface::getSerial() const
{
    return mVertexBuffer->getSerial();
}

unsigned int VertexBufferInterface::getBufferSize() const
{
    return mVertexBuffer->getBufferSize();
}

bool VertexBufferInterface::setBufferSize(unsigned int size)
{
    if (mVertexBuffer->getBufferSize() == 0)
    {
        return mVertexBuffer->initialize(size, mDynamic);
    }
    else
    {
        return mVertexBuffer->setBufferSize(size);
    }
}

unsigned int VertexBufferInterface::getWritePosition() const
{
    return mWritePosition;
}

void VertexBufferInterface::setWritePosition(unsigned int writePosition)
{
    mWritePosition = writePosition;
}

bool VertexBufferInterface::discard()
{
    return mVertexBuffer->discard();
}

bool VertexBufferInterface::storeVertexAttributes(const gl::VertexAttribute &attrib, const gl::VertexAttribCurrentValueData &currentValue,
                                                  GLint start, GLsizei count, GLsizei instances, unsigned int *outStreamOffset)
{
    unsigned int spaceRequired;
    if (!mVertexBuffer->getSpaceRequired(attrib, count, instances, &spaceRequired))
    {
        return false;
    }

    if (mWritePosition + spaceRequired < mWritePosition)
    {
        return false;
    }

    if (!reserveSpace(mReservedSpace))
    {
        return false;
    }
    mReservedSpace = 0;

    if (!mVertexBuffer->storeVertexAttributes(attrib, currentValue, start, count, instances, mWritePosition))
    {
        return false;
    }

    if (outStreamOffset)
    {
        *outStreamOffset = mWritePosition;
    }

    mWritePosition += spaceRequired;

    // Align to 16-byte boundary
    mWritePosition = rx::roundUp(mWritePosition, 16u);

    return true;
}

bool VertexBufferInterface::reserveVertexSpace(const gl::VertexAttribute &attribute, GLsizei count, GLsizei instances)
{
    unsigned int requiredSpace;
    if (!mVertexBuffer->getSpaceRequired(attribute, count, instances, &requiredSpace))
    {
        return false;
    }

    // Protect against integer overflow
    if (mReservedSpace + requiredSpace < mReservedSpace)
    {
         return false;
    }

    mReservedSpace += requiredSpace;

    // Align to 16-byte boundary
    mReservedSpace = rx::roundUp(mReservedSpace, 16u);

    return true;
}

VertexBuffer* VertexBufferInterface::getVertexBuffer() const
{
    return mVertexBuffer;
}

bool VertexBufferInterface::directStoragePossible(const gl::VertexAttribute &attrib,
                                                  const gl::VertexAttribCurrentValueData &currentValue) const
{
    gl::Buffer *buffer = attrib.mBoundBuffer.get();
    BufferStorage *storage = buffer ? buffer->getStorage() : NULL;
    gl::VertexFormat vertexFormat(attrib, currentValue.Type);

    // Alignment restrictions: In D3D, vertex data must be aligned to
    //  the format stride, or to a 4-byte boundary, whichever is smaller.
    //  (Undocumented, and experimentally confirmed)
    unsigned int outputElementSize;
    getVertexBuffer()->getSpaceRequired(attrib, 1, 0, &outputElementSize);
    size_t alignment = std::min<size_t>(static_cast<size_t>(outputElementSize), 4);

    bool isAligned = (static_cast<size_t>(attrib.stride()) % alignment == 0) &&
                     (static_cast<size_t>(attrib.mOffset) % alignment == 0);
    bool requiresConversion = (mRenderer->getVertexConversionType(vertexFormat) & VERTEX_CONVERT_CPU) > 0;

    return storage && storage->supportsDirectBinding() && !requiresConversion && isAligned;
}

StreamingVertexBufferInterface::StreamingVertexBufferInterface(rx::Renderer *renderer, std::size_t initialSize) : VertexBufferInterface(renderer, true)
{
    setBufferSize(initialSize);
}

StreamingVertexBufferInterface::~StreamingVertexBufferInterface()
{
}

bool StreamingVertexBufferInterface::reserveSpace(unsigned int size)
{
    bool result = true;
    unsigned int curBufferSize = getBufferSize();
    if (size > curBufferSize)
    {
        result = setBufferSize(std::max(size, 3 * curBufferSize / 2));
        setWritePosition(0);
    }
    else if (getWritePosition() + size > curBufferSize)
    {
        if (!discard())
        {
            return false;
        }
        setWritePosition(0);
    }

    return result;
}

StaticVertexBufferInterface::StaticVertexBufferInterface(rx::Renderer *renderer) : VertexBufferInterface(renderer, false)
{
}

StaticVertexBufferInterface::~StaticVertexBufferInterface()
{
}

bool StaticVertexBufferInterface::lookupAttribute(const gl::VertexAttribute &attribute, unsigned int *outStreamOffset)
{
    for (unsigned int element = 0; element < mCache.size(); element++)
    {
        if (mCache[element].type == attribute.mType &&
            mCache[element].size == attribute.mSize &&
            mCache[element].stride == attribute.stride() &&
            mCache[element].normalized == attribute.mNormalized &&
            mCache[element].pureInteger == attribute.mPureInteger)
        {
            if (mCache[element].attributeOffset == attribute.mOffset % attribute.stride())
            {
                if (outStreamOffset)
                {
                    *outStreamOffset = mCache[element].streamOffset;
                }
                return true;
            }
        }
    }

    return false;
}

bool StaticVertexBufferInterface::reserveSpace(unsigned int size)
{
    unsigned int curSize = getBufferSize();
    if (curSize == 0)
    {
        setBufferSize(size);
        return true;
    }
    else if (curSize >= size)
    {
        return true;
    }
    else
    {
        UNREACHABLE();   // Static vertex buffers can't be resized
        return false;
    }
}

bool StaticVertexBufferInterface::storeVertexAttributes(const gl::VertexAttribute &attrib, const gl::VertexAttribCurrentValueData &currentValue,
                                                       GLint start, GLsizei count, GLsizei instances, unsigned int *outStreamOffset)
{
    unsigned int streamOffset;
    if (VertexBufferInterface::storeVertexAttributes(attrib, currentValue, start, count, instances, &streamOffset))
    {
        int attributeOffset = attrib.mOffset % attrib.stride();
        VertexElement element = { attrib.mType, attrib.mSize, attrib.stride(), attrib.mNormalized, attrib.mPureInteger, attributeOffset, streamOffset };
        mCache.push_back(element);

        if (outStreamOffset)
        {
            *outStreamOffset = streamOffset;
        }

        return true;
    }
    else
    {
        return false;
    }
}

}
