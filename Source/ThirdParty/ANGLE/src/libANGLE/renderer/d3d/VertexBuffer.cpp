//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer.cpp: Defines the abstract VertexBuffer class and VertexBufferInterface
// class with derivations, classes that perform graphics API agnostic vertex buffer operations.

#include "libANGLE/renderer/d3d/VertexBuffer.h"

#include "common/mathutil.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/VertexAttribute.h"

namespace rx
{

// VertexBuffer Implementation
unsigned int VertexBuffer::mNextSerial = 1;

VertexBuffer::VertexBuffer() : mRefCount(1)
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

void VertexBuffer::addRef()
{
    mRefCount++;
}

void VertexBuffer::release()
{
    ASSERT(mRefCount > 0);
    mRefCount--;

    if (mRefCount == 0)
    {
        delete this;
    }
}

// VertexBufferInterface Implementation
VertexBufferInterface::VertexBufferInterface(BufferFactoryD3D *factory, bool dynamic)
    : mFactory(factory), mVertexBuffer(factory->createVertexBuffer()), mDynamic(dynamic)
{
}

VertexBufferInterface::~VertexBufferInterface()
{
    if (mVertexBuffer)
    {
        mVertexBuffer->release();
    }
}

unsigned int VertexBufferInterface::getSerial() const
{
    return mVertexBuffer->getSerial();
}

unsigned int VertexBufferInterface::getBufferSize() const
{
    return mVertexBuffer->getBufferSize();
}

gl::Error VertexBufferInterface::setBufferSize(unsigned int size)
{
    if (mVertexBuffer->getBufferSize() == 0)
    {
        return mVertexBuffer->initialize(size, mDynamic);
    }

    return mVertexBuffer->setBufferSize(size);
}

gl::ErrorOrResult<unsigned int> VertexBufferInterface::getSpaceRequired(
    const gl::VertexAttribute &attrib,
    GLsizei count,
    GLsizei instances) const
{
    unsigned int spaceRequired = 0;
    ANGLE_TRY_RESULT(mFactory->getVertexSpaceRequired(attrib, count, instances), spaceRequired);

    // Align to 16-byte boundary
    unsigned int alignedSpaceRequired = roundUp(spaceRequired, 16u);

    if (alignedSpaceRequired < spaceRequired)
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Vertex buffer overflow in VertexBufferInterface::getSpaceRequired.");
    }

    return alignedSpaceRequired;
}

gl::Error VertexBufferInterface::discard()
{
    return mVertexBuffer->discard();
}

VertexBuffer *VertexBufferInterface::getVertexBuffer() const
{
    return mVertexBuffer;
}

// StreamingVertexBufferInterface Implementation
StreamingVertexBufferInterface::StreamingVertexBufferInterface(BufferFactoryD3D *factory,
                                                               std::size_t initialSize)
    : VertexBufferInterface(factory, true), mWritePosition(0), mReservedSpace(0)
{
    setBufferSize(static_cast<unsigned int>(initialSize));
}

StreamingVertexBufferInterface::~StreamingVertexBufferInterface()
{
}

gl::Error StreamingVertexBufferInterface::reserveSpace(unsigned int size)
{
    unsigned int curBufferSize = getBufferSize();
    if (size > curBufferSize)
    {
        ANGLE_TRY(setBufferSize(std::max(size, 3 * curBufferSize / 2)));
        mWritePosition = 0;
    }
    else if (mWritePosition + size > curBufferSize)
    {
        ANGLE_TRY(discard());
        mWritePosition = 0;
    }

    return gl::NoError();
}

gl::Error StreamingVertexBufferInterface::storeDynamicAttribute(const gl::VertexAttribute &attrib,
                                                                GLenum currentValueType,
                                                                GLint start,
                                                                GLsizei count,
                                                                GLsizei instances,
                                                                unsigned int *outStreamOffset,
                                                                const uint8_t *sourceData)
{
    unsigned int spaceRequired = 0;
    ANGLE_TRY_RESULT(getSpaceRequired(attrib, count, instances), spaceRequired);

    // Protect against integer overflow
    if (!IsUnsignedAdditionSafe(mWritePosition, spaceRequired))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Internal error, new vertex buffer write position would overflow.");
    }

    ANGLE_TRY(reserveSpace(mReservedSpace));
    mReservedSpace = 0;

    ANGLE_TRY(mVertexBuffer->storeVertexAttributes(attrib, currentValueType, start, count,
                                                   instances, mWritePosition, sourceData));

    if (outStreamOffset)
    {
        *outStreamOffset = mWritePosition;
    }

    mWritePosition += spaceRequired;

    return gl::NoError();
}

gl::Error StreamingVertexBufferInterface::reserveVertexSpace(const gl::VertexAttribute &attrib,
                                                             GLsizei count,
                                                             GLsizei instances)
{
    unsigned int requiredSpace = 0;
    ANGLE_TRY_RESULT(mFactory->getVertexSpaceRequired(attrib, count, instances), requiredSpace);

    // Align to 16-byte boundary
    unsigned int alignedRequiredSpace = roundUp(requiredSpace, 16u);

    // Protect against integer overflow
    if (!IsUnsignedAdditionSafe(mReservedSpace, alignedRequiredSpace) ||
        alignedRequiredSpace < requiredSpace)
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Unable to reserve %u extra bytes in internal vertex buffer, "
                         "it would result in an overflow.",
                         requiredSpace);
    }

    mReservedSpace += alignedRequiredSpace;

    return gl::NoError();
}

// StaticVertexBufferInterface Implementation
StaticVertexBufferInterface::AttributeSignature::AttributeSignature()
    : type(GL_NONE), size(0), stride(0), normalized(false), pureInteger(false), offset(0)
{
}

bool StaticVertexBufferInterface::AttributeSignature::matchesAttribute(
    const gl::VertexAttribute &attrib) const
{
    size_t attribStride = ComputeVertexAttributeStride(attrib);

    if (type != attrib.type || size != attrib.size || static_cast<GLuint>(stride) != attribStride ||
        normalized != attrib.normalized || pureInteger != attrib.pureInteger)
    {
        return false;
    }

    size_t attribOffset = (static_cast<size_t>(attrib.offset) % attribStride);
    return (offset == attribOffset);
}

void StaticVertexBufferInterface::AttributeSignature::set(const gl::VertexAttribute &attrib)
{
    type        = attrib.type;
    size        = attrib.size;
    normalized  = attrib.normalized;
    pureInteger = attrib.pureInteger;
    offset = stride = static_cast<GLuint>(ComputeVertexAttributeStride(attrib));
    offset = static_cast<size_t>(attrib.offset) % ComputeVertexAttributeStride(attrib);
}

StaticVertexBufferInterface::StaticVertexBufferInterface(BufferFactoryD3D *factory)
    : VertexBufferInterface(factory, false)
{
}

StaticVertexBufferInterface::~StaticVertexBufferInterface()
{
}

bool StaticVertexBufferInterface::matchesAttribute(const gl::VertexAttribute &attrib) const
{
    return mSignature.matchesAttribute(attrib);
}

void StaticVertexBufferInterface::setAttribute(const gl::VertexAttribute &attrib)
{
    return mSignature.set(attrib);
}

gl::Error StaticVertexBufferInterface::storeStaticAttribute(const gl::VertexAttribute &attrib,
                                                            GLint start,
                                                            GLsizei count,
                                                            GLsizei instances,
                                                            const uint8_t *sourceData)
{
    unsigned int spaceRequired = 0;
    ANGLE_TRY_RESULT(getSpaceRequired(attrib, count, instances), spaceRequired);
    setBufferSize(spaceRequired);

    ASSERT(attrib.enabled);
    ANGLE_TRY(mVertexBuffer->storeVertexAttributes(attrib, GL_NONE, start, count, instances, 0,
                                                   sourceData));

    mSignature.set(attrib);
    mVertexBuffer->hintUnmapResource();
    return gl::NoError();
}

}  // namespace rx
