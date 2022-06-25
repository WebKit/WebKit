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
#include "libANGLE/renderer/metal/DisplayMtl.h"

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
ConversionBufferMtl::ConversionBufferMtl(ContextMtl *contextMtl,
                                         size_t initialSize,
                                         size_t alignment)
    : dirty(true), convertedBuffer(nullptr), convertedOffset(0)
{
    data.initialize(contextMtl, initialSize, alignment, 0);
}

ConversionBufferMtl::~ConversionBufferMtl() = default;

// IndexConversionBufferMtl implementation.
IndexConversionBufferMtl::IndexConversionBufferMtl(ContextMtl *context,
                                                   gl::DrawElementsType elemTypeIn,
                                                   bool primitiveRestartEnabledIn,
                                                   size_t offsetIn)
    : ConversionBufferMtl(context,
                          kConvertedElementArrayBufferInitialSize,
                          mtl::kIndexBufferOffsetAlignment),
      elemType(elemTypeIn),
      offset(offsetIn),
      primitiveRestartEnabled(primitiveRestartEnabledIn)
{}

IndexRange IndexConversionBufferMtl::getRangeForConvertedBuffer(size_t count)
{
    return IndexRange{0, count};
}

// UniformConversionBufferMtl implementation
UniformConversionBufferMtl::UniformConversionBufferMtl(ContextMtl *context, size_t offsetIn)
    : ConversionBufferMtl(context, 0, mtl::kUniformBufferSettingOffsetMinAlignment),
      offset(offsetIn)
{}

// VertexConversionBufferMtl implementation.
VertexConversionBufferMtl::VertexConversionBufferMtl(ContextMtl *context,
                                                     angle::FormatID formatIDIn,
                                                     GLuint strideIn,
                                                     size_t offsetIn)
    : ConversionBufferMtl(context, 0, mtl::kVertexAttribBufferStrideAlignment),
      formatID(formatIDIn),
      stride(strideIn),
      offset(offsetIn)
{}

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
    return setDataImpl(context, target, data, intendedSize, usage);
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

    ContextMtl *contextMtl = mtl::GetImpl(context);
    auto srcMtl            = GetAs<BufferMtl>(source);

    if (srcMtl->clientShadowCopyDataNeedSync(contextMtl) || mBuffer->isBeingUsedByGPU(contextMtl))
    {
        // If shadow copy requires a synchronization then use blit command instead.
        // It might break a pending render pass, but still faster than synchronization with
        // GPU.
        mtl::BlitCommandEncoder *blitEncoder = contextMtl->getBlitCommandEncoder();
        blitEncoder->copyBuffer(srcMtl->getCurrentBuffer(), sourceOffset, mBuffer, destOffset,
                                size);

        return angle::Result::Continue;
    }
    return setSubDataImpl(context, srcMtl->getClientShadowCopyData(contextMtl) + sourceOffset, size,
                          destOffset);
}

angle::Result BufferMtl::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    GLbitfield mapRangeAccess = 0;
    if ((access & GL_WRITE_ONLY_OES) != 0 || (access & GL_READ_WRITE) != 0)
    {
        mapRangeAccess |= GL_MAP_WRITE_BIT;
    }
    return mapRange(context, 0, size(), mapRangeAccess, mapPtr);
}

angle::Result BufferMtl::mapRange(const gl::Context *context,
                                  size_t offset,
                                  size_t length,
                                  GLbitfield access,
                                  void **mapPtr)
{
    if (access & GL_MAP_INVALIDATE_BUFFER_BIT)
    {
        ANGLE_TRY(setDataImpl(context, gl::BufferBinding::InvalidEnum, nullptr, size(),
                              mState.getUsage()));
    }

    if (mapPtr)
    {
        ContextMtl *contextMtl = mtl::GetImpl(context);
        if (mBufferPool.getMaxBuffers() == 1)
        {
            *mapPtr = mBuffer->mapWithOpt(contextMtl, (access & GL_MAP_WRITE_BIT) == 0,
                                          access & GL_MAP_UNSYNCHRONIZED_BIT) +
                      offset;
        }
        else
        {
            *mapPtr = syncAndObtainShadowCopy(contextMtl) + offset;
        }
    }

    return angle::Result::Continue;
}

angle::Result BufferMtl::unmap(const gl::Context *context, GLboolean *result)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    size_t offset          = static_cast<size_t>(mState.getMapOffset());
    size_t len             = static_cast<size_t>(mState.getMapLength());

    markConversionBuffersDirty();

    if (mBufferPool.getMaxBuffers() == 1)
    {
        ASSERT(mBuffer);
        if (mState.getAccessFlags() & GL_MAP_WRITE_BIT)
        {
            mBuffer->unmapAndFlushSubset(contextMtl, offset, len);
        }
        else
        {
            // Buffer is already mapped with readonly flag, so just unmap it, no flushing will
            // occur.
            mBuffer->unmap(contextMtl);
        }
    }
    else
    {
        ASSERT(mShadowCopy.size());

        if (mState.getAccessFlags() & GL_MAP_UNSYNCHRONIZED_BIT)
        {
            // Copy the mapped region without synchronization with GPU
            uint8_t *ptr =
                mBuffer->mapWithOpt(contextMtl, /* readonly */ false, /* noSync */ true) + offset;
            std::copy(mShadowCopy.data() + offset, mShadowCopy.data() + offset + len, ptr);
            mBuffer->unmapAndFlushSubset(contextMtl, offset, len);
        }
        else
        {
            // commit shadow copy data to GPU synchronously
            ANGLE_TRY(commitShadowCopy(context));
        }
    }

    if (result)
    {
        *result = true;
    }

    return angle::Result::Continue;
}

angle::Result BufferMtl::getIndexRange(const gl::Context *context,
                                       gl::DrawElementsType type,
                                       size_t offset,
                                       size_t count,
                                       bool primitiveRestartEnabled,
                                       gl::IndexRange *outRange)
{
    const uint8_t *indices = getClientShadowCopyData(mtl::GetImpl(context)) + offset;

    *outRange = gl::ComputeIndexRange(type, indices, count, primitiveRestartEnabled);

    return angle::Result::Continue;
}

angle::Result BufferMtl::getFirstLastIndices(ContextMtl *contextMtl,
                                             gl::DrawElementsType type,
                                             size_t offset,
                                             size_t count,
                                             std::pair<uint32_t, uint32_t> *outIndices)
{
    const uint8_t *indices = getClientShadowCopyData(contextMtl) + offset;

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
}

void BufferMtl::onDataChanged()
{
    markConversionBuffersDirty();
}

/* public */
const uint8_t *BufferMtl::getClientShadowCopyData(ContextMtl *contextMtl)
{
    if (mBufferPool.getMaxBuffers() == 1)
    {
        // Don't need shadow copy in this case, use the buffer directly
        return mBuffer->mapReadOnly(contextMtl);
    }
    return syncAndObtainShadowCopy(contextMtl);
}

bool BufferMtl::clientShadowCopyDataNeedSync(ContextMtl *contextMtl)
{
    return mBuffer->isCPUReadMemDirty();
}

void BufferMtl::ensureShadowCopySyncedFromGPU(ContextMtl *contextMtl)
{
    if (mBuffer->isCPUReadMemDirty())
    {
        const uint8_t *ptr = mBuffer->mapReadOnly(contextMtl);
        memcpy(mShadowCopy.data(), ptr, size());
        mBuffer->unmap(contextMtl);

        mBuffer->resetCPUReadMemDirty();
    }
}
uint8_t *BufferMtl::syncAndObtainShadowCopy(ContextMtl *contextMtl)
{
    ASSERT(mShadowCopy.size());

    ensureShadowCopySyncedFromGPU(contextMtl);

    return mShadowCopy.data();
}

ConversionBufferMtl *BufferMtl::getVertexConversionBuffer(ContextMtl *context,
                                                          angle::FormatID formatID,
                                                          GLuint stride,
                                                          size_t offset)
{
    for (VertexConversionBufferMtl &buffer : mVertexConversionBuffers)
    {
        if (buffer.formatID == formatID && buffer.stride == stride && buffer.offset <= offset &&
            buffer.offset % buffer.stride == offset % stride)
        {
            return &buffer;
        }
    }

    mVertexConversionBuffers.emplace_back(context, formatID, stride, offset);
    ConversionBufferMtl *conv        = &mVertexConversionBuffers.back();
    const angle::Format &angleFormat = angle::Format::Get(formatID);
    conv->data.updateAlignment(context, angleFormat.pixelBytes);

    return conv;
}

IndexConversionBufferMtl *BufferMtl::getIndexConversionBuffer(ContextMtl *context,
                                                              gl::DrawElementsType elemType,
                                                              bool primitiveRestartEnabled,
                                                              size_t offset)
{
    for (auto &buffer : mIndexConversionBuffers)
    {
        if (buffer.elemType == elemType && buffer.offset == offset &&
            buffer.primitiveRestartEnabled == primitiveRestartEnabled)
        {
            return &buffer;
        }
    }

    mIndexConversionBuffers.emplace_back(context, elemType, primitiveRestartEnabled, offset);
    return &mIndexConversionBuffers.back();
}

ConversionBufferMtl *BufferMtl::getUniformConversionBuffer(ContextMtl *context, size_t offset)
{
    for (UniformConversionBufferMtl &buffer : mUniformConversionBuffers)
    {
        if (buffer.offset == offset)
        {
            return static_cast<ConversionBufferMtl *>(&buffer);
        }
    }

    mUniformConversionBuffers.emplace_back(context, offset);
    return &mUniformConversionBuffers.back();
}

void BufferMtl::markConversionBuffersDirty()
{
    for (VertexConversionBufferMtl &buffer : mVertexConversionBuffers)
    {
        buffer.dirty           = true;
        buffer.convertedBuffer = nullptr;
        buffer.convertedOffset = 0;
    }

    for (IndexConversionBufferMtl &buffer : mIndexConversionBuffers)
    {
        buffer.dirty           = true;
        buffer.convertedBuffer = nullptr;
        buffer.convertedOffset = 0;
    }

    for (UniformConversionBufferMtl &buffer : mUniformConversionBuffers)
    {
        buffer.dirty           = true;
        buffer.convertedBuffer = nullptr;
        buffer.convertedOffset = 0;
    }
    mRestartRangeCache.reset();
}

void BufferMtl::clearConversionBuffers()
{
    mVertexConversionBuffers.clear();
    mIndexConversionBuffers.clear();
    mUniformConversionBuffers.clear();
    mRestartRangeCache.reset();
}

template <typename T>
static std::vector<IndexRange> calculateRestartRanges(ContextMtl *ctx, mtl::BufferRef idxBuffer)
{
    std::vector<IndexRange> result;
    const T *bufferData       = reinterpret_cast<const T *>(idxBuffer->mapReadOnly(ctx));
    const size_t numIndices   = idxBuffer->size() / sizeof(T);
    constexpr T restartMarker = std::numeric_limits<T>::max();
    for (size_t i = 0; i < numIndices; ++i)
    {
        // Find the start of the restart range, i.e. first index with value of restart marker.
        if (bufferData[i] != restartMarker)
            continue;
        size_t restartBegin = i;
        // Find the end of the restart range, i.e. last index with value of restart marker.
        do
        {
            ++i;
        } while (i < numIndices && bufferData[i] == restartMarker);
        result.emplace_back(restartBegin, i - 1);
    }
    idxBuffer->unmap(ctx);
    return result;
}

const std::vector<IndexRange> &BufferMtl::getRestartIndices(ContextMtl *ctx,
                                                            gl::DrawElementsType indexType)
{
    if (!mRestartRangeCache || mRestartRangeCache->indexType != indexType)
    {
        mRestartRangeCache.reset();
        std::vector<IndexRange> ranges;
        switch (indexType)
        {
            case gl::DrawElementsType::UnsignedByte:
                ranges = calculateRestartRanges<uint8_t>(ctx, getCurrentBuffer());
                break;
            case gl::DrawElementsType::UnsignedShort:
                ranges = calculateRestartRanges<uint16_t>(ctx, getCurrentBuffer());
                break;
            case gl::DrawElementsType::UnsignedInt:
                ranges = calculateRestartRanges<uint32_t>(ctx, getCurrentBuffer());
                break;
            default:
                ASSERT(false);
        }
        mRestartRangeCache.emplace(std::move(ranges), indexType);
    }
    return mRestartRangeCache->ranges;
}

const std::vector<IndexRange> BufferMtl::getRestartIndicesFromClientData(
    ContextMtl *ctx,
    gl::DrawElementsType indexType,
    mtl::BufferRef idxBuffer)
{
    std::vector<IndexRange> restartIndices;
    switch (indexType)
    {
        case gl::DrawElementsType::UnsignedByte:
            restartIndices = calculateRestartRanges<uint8_t>(ctx, idxBuffer);
            break;
        case gl::DrawElementsType::UnsignedShort:
            restartIndices = calculateRestartRanges<uint16_t>(ctx, idxBuffer);
            break;
        case gl::DrawElementsType::UnsignedInt:
            restartIndices = calculateRestartRanges<uint32_t>(ctx, idxBuffer);
            break;
        default:
            ASSERT(false);
    }
    return restartIndices;
}

angle::Result BufferMtl::setDataImpl(const gl::Context *context,
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

    // Ensures no validation layer issues in std140 with data types like vec3 being 12 bytes vs 16
    // in MSL.
    if (target == gl::BufferBinding::Uniform)
    {
        adjustedSize = roundUpPow2(adjustedSize, (size_t)16);
    }

    size_t maxBuffers;
    switch (usage)
    {
        case gl::BufferUsage::StaticCopy:
        case gl::BufferUsage::StaticDraw:
        case gl::BufferUsage::StaticRead:
        case gl::BufferUsage::DynamicRead:
        case gl::BufferUsage::StreamRead:
            maxBuffers = 1;  // static/read buffer doesn't need high speed data update
            mBufferPool.setAlwaysUseGPUMem();
            break;
        default:
            // dynamic buffer, allow up to 10 update per frame/encoding without
            // waiting for GPU.
            if (adjustedSize <= kConvertedElementArrayBufferInitialSize)
            {
                maxBuffers = 10;
                mBufferPool.setAlwaysUseSharedMem();
            }
            else
            {
                maxBuffers = 1;
                mBufferPool.setAlwaysUseGPUMem();
            }
            break;
    }

    // Re-create the buffer
    mBuffer = nullptr;
    ANGLE_TRY(mBufferPool.reset(contextMtl, adjustedSize, 1, maxBuffers));

    if (maxBuffers > 1)
    {
        // We use shadow copy to maintain consistent data between buffers in pool
        ANGLE_MTL_CHECK(contextMtl, mShadowCopy.resize(adjustedSize), GL_OUT_OF_MEMORY);

        if (data)
        {
            // Transfer data to shadow copy buffer
            auto ptr = static_cast<const uint8_t *>(data);
            std::copy(ptr, ptr + intendedSize, mShadowCopy.data());

            // Transfer data from shadow copy buffer to GPU buffer.
            ANGLE_TRY(commitShadowCopy(context, adjustedSize));
        }
        else
        {
            // This is needed so that first buffer pointer could be available
            ANGLE_TRY(commitShadowCopy(context, 0));
        }
    }
    else
    {
        // We don't need shadow copy if there will be only one buffer in the pool.
        ANGLE_MTL_CHECK(contextMtl, mShadowCopy.resize(0), GL_OUT_OF_MEMORY);

        // Allocate one buffer to use
        ANGLE_TRY(
            mBufferPool.allocate(contextMtl, adjustedSize, nullptr, &mBuffer, nullptr, nullptr));

        if (data)
        {
            ANGLE_TRY(setSubDataImpl(context, data, intendedSize, 0));
        }
    }

#ifndef NDEBUG
    ANGLE_MTL_OBJC_SCOPE
    {
        mBuffer->get().label = [NSString stringWithFormat:@"BufferMtl=%p", this];
    }
#endif

    return angle::Result::Continue;
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

    ASSERT(mBuffer);

    ContextMtl *contextMtl = mtl::GetImpl(context);

    ANGLE_MTL_TRY(contextMtl, offset <= mBuffer->size());

    auto srcPtr     = static_cast<const uint8_t *>(data);
    auto sizeToCopy = std::min<size_t>(size, mBuffer->size() - offset);

    markConversionBuffersDirty();

    if (mBufferPool.getMaxBuffers() == 1)
    {
        ASSERT(mBuffer);
        uint8_t *ptr = mBuffer->map(contextMtl);
        std::copy(srcPtr, srcPtr + sizeToCopy, ptr + offset);
        mBuffer->unmapAndFlushSubset(contextMtl, offset, sizeToCopy);
    }
    else
    {
        ASSERT(mShadowCopy.size());

        // 1. Before copying data from client, we need to synchronize modified data from GPU to
        // shadow copy first.
        ensureShadowCopySyncedFromGPU(contextMtl);

        // 2. Copy data from client to shadow copy.
        std::copy(srcPtr, srcPtr + sizeToCopy, mShadowCopy.data() + offset);

        // 3. Copy data from shadow copy to GPU.
        ANGLE_TRY(commitShadowCopy(context));
    }

    return angle::Result::Continue;
}

angle::Result BufferMtl::commitShadowCopy(const gl::Context *context)
{
    return commitShadowCopy(context, size());
}

angle::Result BufferMtl::commitShadowCopy(const gl::Context *context, size_t size)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    if (!size)
    {
        // Skip mapping if size to commit is zero.
        // zero size is passed to allocate buffer only.
        ANGLE_TRY(mBufferPool.allocate(contextMtl, mShadowCopy.size(), nullptr, &mBuffer, nullptr,
                                       nullptr));
    }
    else
    {
        uint8_t *ptr = nullptr;
        mBufferPool.releaseInFlightBuffers(contextMtl);
        ANGLE_TRY(
            mBufferPool.allocate(contextMtl, mShadowCopy.size(), &ptr, &mBuffer, nullptr, nullptr));

        std::copy(mShadowCopy.data(), mShadowCopy.data() + size, ptr);
    }

    ANGLE_TRY(mBufferPool.commit(contextMtl));

    return angle::Result::Continue;
}

// SimpleWeakBufferHolderMtl implementation
SimpleWeakBufferHolderMtl::SimpleWeakBufferHolderMtl()
{
    mIsWeak = true;
}

}  // namespace rx
