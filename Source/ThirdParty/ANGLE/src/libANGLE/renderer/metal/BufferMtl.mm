//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferMtl.mm:
//    Implements the class methods for BufferMtl.
//

#include "libANGLE/renderer/metal/BufferMtl.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/renderer/metal/ContextMtl.h"

namespace rx
{

namespace
{

// Start with a fairly small buffer size. We can increase this dynamically as we convert more data.
constexpr size_t kConvertedElementArrayBufferInitialSize = 1024 * 8;

template <typename IndexType>
angle::Result GetFirstLastIndices(const IndexType *indices,
                                  size_t count,
                                  std::pair<uint32_t, uint32_t> *outIndices)
{
    IndexType first, last;
    // Use memcpy to avoid unaligned memory access crash:
    memcpy(&first, &indices[0], sizeof(first));
    memcpy(&last, &indices[count - 1], sizeof(last));

    outIndices->first  = first;
    outIndices->second = last;

    return angle::Result::Continue;
}

}  // namespace

// ConversionBufferMtl implementation.
ConversionBufferMtl::ConversionBufferMtl(const gl::Context *context,
                                         size_t initialSize,
                                         size_t alignment)
    : dirty(true), convertedBuffer(nullptr), convertedOffset(0)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    data.initialize(contextMtl, initialSize, alignment);
}

ConversionBufferMtl::~ConversionBufferMtl() = default;

// IndexConversionBufferMtl implementation.
IndexConversionBufferMtl::IndexConversionBufferMtl(const gl::Context *context,
                                                   gl::DrawElementsType typeIn,
                                                   size_t offsetIn)
    : ConversionBufferMtl(context,
                          kConvertedElementArrayBufferInitialSize,
                          mtl::kIndexBufferOffsetAlignment),
      type(typeIn),
      offset(offsetIn)
{}

// BufferMtl::VertexConversionBuffer implementation.
BufferMtl::VertexConversionBuffer::VertexConversionBuffer(const gl::Context *context,
                                                          angle::FormatID formatIDIn,
                                                          GLuint strideIn,
                                                          size_t offsetIn)
    : ConversionBufferMtl(context, 0, mtl::kVertexAttribBufferStrideAlignment),
      formatID(formatIDIn),
      stride(strideIn),
      offset(offsetIn)
{
    // Due to Metal's strict requirement for offset and stride, we need to always allocate new
    // buffer for every conversion.
    data.setAlwaysAllocateNewBuffer(true);
}

// BufferMtl implementation
BufferMtl::BufferMtl(const gl::BufferState &state)
    : BufferImpl(state), mBufferPool(/** alwaysAllocNewBuffer */ true)
{}

BufferMtl::~BufferMtl() {}

void BufferMtl::destroy(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    mShadowCopy.clear();
    mBufferPool.destroy(contextMtl);
    mBuffer = nullptr;

    clearConversionBuffers();
}

angle::Result BufferMtl::setData(const gl::Context *context,
                                 gl::BufferBinding target,
                                 const void *data,
                                 size_t intendedSize,
                                 gl::BufferUsage usage)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    // Invalidate conversion buffers
    if (mState.getSize() != static_cast<GLint64>(intendedSize))
    {
        clearConversionBuffers();
    }
    else
    {
        markConversionBuffersDirty();
    }

    size_t adjustedSize = std::max<size_t>(1, intendedSize);

    if (!mShadowCopy.size() || intendedSize > mShadowCopy.size() || usage != mState.getUsage())
    {
        // Re-create the buffer
        ANGLE_MTL_CHECK(contextMtl, mShadowCopy.resize(adjustedSize), GL_OUT_OF_MEMORY);

        size_t maxBuffers;
        switch (usage)
        {
            case gl::BufferUsage::StaticCopy:
            case gl::BufferUsage::StaticDraw:
            case gl::BufferUsage::StaticRead:
                maxBuffers = 1;  // static buffer doesn't need high speed data update
                break;
            default:
                // dynamic buffer, allow up to 2 update per frame/encoding without
                // waiting for GPU.
                maxBuffers = 2;
                break;
        }

        mBufferPool.initialize(contextMtl, adjustedSize, 1, maxBuffers);
    }

    // Transfer data to shadow copy buffer
    if (data)
    {
        auto ptr = static_cast<const uint8_t *>(data);
        std::copy(ptr, ptr + intendedSize, mShadowCopy.data());
    }

    // Transfer data from shadow copy buffer to GPU buffer.
    return commitShadowCopy(context, adjustedSize);
}

angle::Result BufferMtl::setSubData(const gl::Context *context,
                                    gl::BufferBinding target,
                                    const void *data,
                                    size_t size,
                                    size_t offset)
{
    return setSubDataImpl(context, data, size, offset);
}

angle::Result BufferMtl::copySubData(const gl::Context *context,
                                     BufferImpl *source,
                                     GLintptr sourceOffset,
                                     GLintptr destOffset,
                                     GLsizeiptr size)
{
    if (!source)
    {
        return angle::Result::Continue;
    }

    ASSERT(mShadowCopy.size());

    auto srcMtl = GetAs<BufferMtl>(source);

    // NOTE(hqle): use blit command.
    return setSubDataImpl(context, srcMtl->getClientShadowCopyData(context) + sourceOffset, size,
                          destOffset);
}

angle::Result BufferMtl::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    ASSERT(mShadowCopy.size());
    return mapRange(context, 0, size(), 0, mapPtr);
}

angle::Result BufferMtl::mapRange(const gl::Context *context,
                                  size_t offset,
                                  size_t length,
                                  GLbitfield access,
                                  void **mapPtr)
{
    ASSERT(mShadowCopy.size());

    // NOTE(hqle): use access flags
    if (mapPtr)
    {
        *mapPtr = mShadowCopy.data() + offset;
    }

    return angle::Result::Continue;
}

angle::Result BufferMtl::unmap(const gl::Context *context, GLboolean *result)
{
    ASSERT(mShadowCopy.size());

    markConversionBuffersDirty();

    ANGLE_TRY(commitShadowCopy(context));

    return angle::Result::Continue;
}

angle::Result BufferMtl::getIndexRange(const gl::Context *context,
                                       gl::DrawElementsType type,
                                       size_t offset,
                                       size_t count,
                                       bool primitiveRestartEnabled,
                                       gl::IndexRange *outRange)
{
    ASSERT(mShadowCopy.size());

    const uint8_t *indices = mShadowCopy.data() + offset;

    *outRange = gl::ComputeIndexRange(type, indices, count, primitiveRestartEnabled);

    return angle::Result::Continue;
}

angle::Result BufferMtl::getFirstLastIndices(const gl::Context *context,
                                             gl::DrawElementsType type,
                                             size_t offset,
                                             size_t count,
                                             std::pair<uint32_t, uint32_t> *outIndices) const
{
    ASSERT(mShadowCopy.size());

    const uint8_t *indices = mShadowCopy.data() + offset;

    switch (type)
    {
        case gl::DrawElementsType::UnsignedByte:
            return GetFirstLastIndices(static_cast<const GLubyte *>(indices), count, outIndices);
        case gl::DrawElementsType::UnsignedShort:
            return GetFirstLastIndices(reinterpret_cast<const GLushort *>(indices), count,
                                       outIndices);
        case gl::DrawElementsType::UnsignedInt:
            return GetFirstLastIndices(reinterpret_cast<const GLuint *>(indices), count,
                                       outIndices);
        default:
            UNREACHABLE();
            return angle::Result::Stop;
    }

    return angle::Result::Continue;
}

const uint8_t *BufferMtl::getClientShadowCopyData(const gl::Context *context)
{
    // NOTE(hqle): Support buffer update from GPU.
    // Which mean we have to stall the GPU by calling finish and copy
    // data back to shadow copy.
    return mShadowCopy.data();
}

ConversionBufferMtl *BufferMtl::getVertexConversionBuffer(const gl::Context *context,
                                                          angle::FormatID formatID,
                                                          GLuint stride,
                                                          size_t offset)
{
    for (VertexConversionBuffer &buffer : mVertexConversionBuffers)
    {
        if (buffer.formatID == formatID && buffer.stride == stride && buffer.offset == offset)
        {
            return &buffer;
        }
    }

    mVertexConversionBuffers.emplace_back(context, formatID, stride, offset);
    return &mVertexConversionBuffers.back();
}

IndexConversionBufferMtl *BufferMtl::getIndexConversionBuffer(const gl::Context *context,
                                                              gl::DrawElementsType type,
                                                              size_t offset)
{
    for (auto &buffer : mIndexConversionBuffers)
    {
        if (buffer.type == type && buffer.offset == offset)
        {
            return &buffer;
        }
    }

    mIndexConversionBuffers.emplace_back(context, type, offset);
    return &mIndexConversionBuffers.back();
}

void BufferMtl::markConversionBuffersDirty()
{
    for (VertexConversionBuffer &buffer : mVertexConversionBuffers)
    {
        buffer.dirty = true;
    }

    for (auto &buffer : mIndexConversionBuffers)
    {
        buffer.dirty           = true;
        buffer.convertedBuffer = nullptr;
        buffer.convertedOffset = 0;
    }
}

void BufferMtl::clearConversionBuffers()
{
    mVertexConversionBuffers.clear();
    mIndexConversionBuffers.clear();
}

angle::Result BufferMtl::setSubDataImpl(const gl::Context *context,
                                        const void *data,
                                        size_t size,
                                        size_t offset)
{
    if (!data)
    {
        return angle::Result::Continue;
    }
    ContextMtl *contextMtl = mtl::GetImpl(context);

    ASSERT(mShadowCopy.size());

    ANGLE_MTL_TRY(contextMtl, offset <= this->size());

    auto srcPtr     = static_cast<const uint8_t *>(data);
    auto sizeToCopy = std::min<size_t>(size, this->size() - offset);
    std::copy(srcPtr, srcPtr + sizeToCopy, mShadowCopy.data() + offset);

    markConversionBuffersDirty();

    ANGLE_TRY(commitShadowCopy(context));

    return angle::Result::Continue;
}

angle::Result BufferMtl::commitShadowCopy(const gl::Context *context)
{
    return commitShadowCopy(context, size());
}

angle::Result BufferMtl::commitShadowCopy(const gl::Context *context, size_t size)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    uint8_t *ptr = nullptr;
    ANGLE_TRY(mBufferPool.allocate(contextMtl, size, &ptr, &mBuffer, nullptr, nullptr));

    std::copy(mShadowCopy.data(), mShadowCopy.data() + size, ptr);

    ANGLE_TRY(mBufferPool.commit(contextMtl));

#ifndef NDEBUG
    ANGLE_MTL_OBJC_SCOPE { mBuffer->get().label = [NSString stringWithFormat:@"%p", this]; }
#endif

    return angle::Result::Continue;
}

// SimpleWeakBufferHolderMtl implementation
SimpleWeakBufferHolderMtl::SimpleWeakBufferHolderMtl()
{
    mIsWeak = true;
}

}  // namespace rx
