//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.cpp:
//    Implements the class methods for BufferVk.
//

#include "libANGLE/renderer/vulkan/BufferVk.h"

#include "common/FixedVector.h"
#include "common/debug.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{
VkBufferUsageFlags GetDefaultBufferUsageFlags(RendererVk *renderer)
{
    // We could potentially use multiple backing buffers for different usages.
    // For now keep a single buffer with all relevant usage flags.
    VkBufferUsageFlags defaultBufferUsageFlags =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (renderer->getFeatures().supportsTransformFeedbackExtension.enabled)
    {
        defaultBufferUsageFlags |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT |
                                   VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
    }
    return defaultBufferUsageFlags;
}

namespace
{
constexpr VkMemoryPropertyFlags kDeviceLocalFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
constexpr VkMemoryPropertyFlags kDeviceLocalHostCoherentFlags =
    (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
constexpr VkMemoryPropertyFlags kHostCachedFlags =
    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
     VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
constexpr VkMemoryPropertyFlags kHostUncachedFlags =
    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

// Vertex attribute buffers are used as storage buffers for conversion in compute, where access to
// the buffer is made in 4-byte chunks.  Assume the size of the buffer is 4k+n where n is in [0, 3).
// On some hardware, reading 4 bytes from address 4k returns 0, making it impossible to read the
// last n bytes.  By rounding up the buffer sizes to a multiple of 4, the problem is alleviated.
constexpr size_t kBufferSizeGranularity = 4;
static_assert(gl::isPow2(kBufferSizeGranularity), "use as alignment, must be power of two");

// Start with a fairly small buffer size. We can increase this dynamically as we convert more data.
constexpr size_t kConvertedArrayBufferInitialSize = 1024 * 8;

// Buffers that have a static usage pattern will be allocated in
// device local memory to speed up access to and from the GPU.
// Dynamic usage patterns or that are frequently mapped
// will now request host cached memory to speed up access from the CPU.
VkMemoryPropertyFlags GetPreferredMemoryType(RendererVk *renderer,
                                             gl::BufferBinding target,
                                             gl::BufferUsage usage)
{
    if (target == gl::BufferBinding::PixelUnpack)
    {
        return kHostCachedFlags;
    }

    switch (usage)
    {
        case gl::BufferUsage::StaticCopy:
        case gl::BufferUsage::StaticDraw:
        case gl::BufferUsage::StaticRead:
            // For static usage, request a device local memory
            return renderer->getFeatures().preferDeviceLocalMemoryHostVisible.enabled
                       ? kDeviceLocalHostCoherentFlags
                       : kDeviceLocalFlags;
        case gl::BufferUsage::DynamicDraw:
        case gl::BufferUsage::StreamDraw:
            // For non-static usage where the CPU performs a write-only access, request
            // a host uncached memory
            return kHostUncachedFlags;
        case gl::BufferUsage::DynamicCopy:
        case gl::BufferUsage::DynamicRead:
        case gl::BufferUsage::StreamCopy:
        case gl::BufferUsage::StreamRead:
            // For all other types of usage, request a host cached memory
            return kHostCachedFlags;
        default:
            UNREACHABLE();
            return kHostCachedFlags;
    }
}

VkMemoryPropertyFlags GetStorageMemoryType(RendererVk *renderer,
                                           GLbitfield storageFlags,
                                           bool externalBuffer)
{
    const bool hasMapAccess =
        (storageFlags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT)) != 0;

    if (renderer->getFeatures().preferDeviceLocalMemoryHostVisible.enabled)
    {
        const bool canUpdate = (storageFlags & GL_DYNAMIC_STORAGE_BIT_EXT) != 0;
        if (canUpdate || hasMapAccess || externalBuffer)
        {
            // We currently allocate coherent memory for persistently mapped buffers.
            // GL_EXT_buffer_storage allows non-coherent memory, but currently the implementation of
            // |glMemoryBarrier(CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT)| relies on the mapping being
            // coherent.
            //
            // If persistently mapped buffers ever use non-coherent memory, then said
            // |glMemoryBarrier| call must result in |vkInvalidateMappedMemoryRanges| for all
            // persistently mapped buffers.
            return kDeviceLocalHostCoherentFlags;
        }
        return kDeviceLocalFlags;
    }

    return hasMapAccess ? kHostCachedFlags : kDeviceLocalFlags;
}

bool ShouldAllocateNewMemoryForUpdate(ContextVk *contextVk, size_t subDataSize, size_t bufferSize)
{
    // A sub data update with size > 50% of buffer size meets the threshold
    // to acquire a new BufferHelper from the pool.
    return contextVk->getRenderer()->getFeatures().preferCPUForBufferSubData.enabled ||
           subDataSize > (bufferSize / 2);
}

bool ShouldUseCPUToCopyData(ContextVk *contextVk, size_t copySize, size_t bufferSize)
{
    RendererVk *renderer = contextVk->getRenderer();
    // For some GPU (ARM) we always prefer using CPU to do copy instead of use GPU to avoid pipeline
    // bubbles. If GPU is currently busy and data copy size is less than certain threshold, we
    // choose to use CPU to do data copy over GPU to achieve better parallelism.
    return renderer->getFeatures().preferCPUForBufferSubData.enabled ||
           (renderer->isCommandQueueBusy() &&
            copySize < renderer->getMaxCopyBytesUsingCPUWhenPreservingBufferData());
}

bool RenderPassUsesBufferForReadOnly(ContextVk *contextVk, const vk::BufferHelper &buffer)
{
    if (!contextVk->hasStartedRenderPass())
    {
        return false;
    }

    vk::RenderPassCommandBufferHelper &renderPassCommands =
        contextVk->getStartedRenderPassCommands();
    return renderPassCommands.usesBuffer(buffer) && !renderPassCommands.usesBufferForWrite(buffer);
}

// If a render pass is open which uses the buffer in read-only mode, render pass break can be
// avoided by using acquireAndUpdate.  This can be costly however if the update is very small, and
// is limited to platforms where render pass break is itself costly (i.e. tiled-based renderers).
bool ShouldAvoidRenderPassBreakOnUpdate(ContextVk *contextVk,
                                        const vk::BufferHelper &buffer,
                                        size_t bufferSize)
{
    // Only avoid breaking the render pass if the buffer is not so big such that duplicating it
    // would outweight the cost of breaking the render pass.  A value of 1KB is temporary chosen as
    // a heuristic, and can be adjusted when such a situation is encountered.
    constexpr size_t kPreferDuplicateOverRenderPassBreakMaxBufferSize = 1024;
    if (!contextVk->getFeatures().preferCPUForBufferSubData.enabled ||
        bufferSize > kPreferDuplicateOverRenderPassBreakMaxBufferSize)
    {
        return false;
    }

    return RenderPassUsesBufferForReadOnly(contextVk, buffer);
}

bool IsUsageDynamic(gl::BufferUsage usage)
{
    return (usage == gl::BufferUsage::DynamicDraw || usage == gl::BufferUsage::DynamicCopy ||
            usage == gl::BufferUsage::DynamicRead);
}

angle::Result GetMemoryTypeIndex(ContextVk *contextVk,
                                 VkDeviceSize size,
                                 VkMemoryPropertyFlags memoryPropertyFlags,
                                 uint32_t *memoryTypeIndexOut)
{
    RendererVk *renderer           = contextVk->getRenderer();
    const vk::Allocator &allocator = renderer->getAllocator();

    bool persistentlyMapped = renderer->getFeatures().persistentlyMappedBuffers.enabled;
    VkBufferUsageFlags defaultBufferUsageFlags = GetDefaultBufferUsageFlags(renderer);

    VkBufferCreateInfo createInfo    = {};
    createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.flags                 = 0;
    createInfo.size                  = size;
    createInfo.usage                 = defaultBufferUsageFlags;
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    // Host visible is required, all other bits are preferred, (i.e., optional)
    VkMemoryPropertyFlags requiredFlags =
        (memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VkMemoryPropertyFlags preferredFlags =
        (memoryPropertyFlags & (~VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

    // Check that the allocation is not too large.
    uint32_t memoryTypeIndex = 0;
    ANGLE_VK_TRY(contextVk, allocator.findMemoryTypeIndexForBufferInfo(
                                createInfo, requiredFlags, preferredFlags, persistentlyMapped,
                                &memoryTypeIndex));
    *memoryTypeIndexOut = memoryTypeIndex;

    return angle::Result::Continue;
}
}  // namespace

// ConversionBuffer implementation.
ConversionBuffer::ConversionBuffer(RendererVk *renderer,
                                   VkBufferUsageFlags usageFlags,
                                   size_t initialSize,
                                   size_t alignment,
                                   bool hostVisible)
    : dirty(true)
{
    data = std::make_unique<vk::BufferHelper>();
}

ConversionBuffer::~ConversionBuffer()
{
    ASSERT(!data || !data->valid());
}

ConversionBuffer::ConversionBuffer(ConversionBuffer &&other) = default;

// BufferVk::VertexConversionBuffer implementation.
BufferVk::VertexConversionBuffer::VertexConversionBuffer(RendererVk *renderer,
                                                         angle::FormatID formatIDIn,
                                                         GLuint strideIn,
                                                         size_t offsetIn,
                                                         bool hostVisible)
    : ConversionBuffer(renderer,
                       vk::kVertexBufferUsageFlags,
                       kConvertedArrayBufferInitialSize,
                       vk::kVertexBufferAlignment,
                       hostVisible),
      formatID(formatIDIn),
      stride(strideIn),
      offset(offsetIn)
{}

BufferVk::VertexConversionBuffer::VertexConversionBuffer(VertexConversionBuffer &&other) = default;

BufferVk::VertexConversionBuffer::~VertexConversionBuffer() = default;

// BufferVk implementation.
BufferVk::BufferVk(const gl::BufferState &state)
    : BufferImpl(state),
      mClientBuffer(nullptr),
      mMemoryTypeIndex(0),
      mMemoryPropertyFlags(0),
      mIsStagingBufferMapped(false),
      mHasValidData(false),
      mIsMappedForWrite(false),
      mMappedOffset(0),
      mMappedLength(0)
{}

BufferVk::~BufferVk() {}

void BufferVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    release(contextVk);
}

void BufferVk::release(ContextVk *contextVk)
{
    RendererVk *renderer = contextVk->getRenderer();
    if (mBuffer.valid())
    {
        mBuffer.releaseBufferAndDescriptorSetCache(contextVk);
    }
    if (mStagingBuffer.valid())
    {
        mStagingBuffer.release(renderer);
    }

    for (ConversionBuffer &buffer : mVertexConversionBuffers)
    {
        buffer.data->release(renderer);
    }
    mVertexConversionBuffers.clear();
}

angle::Result BufferVk::setExternalBufferData(const gl::Context *context,
                                              gl::BufferBinding target,
                                              GLeglClientBufferEXT clientBuffer,
                                              size_t size,
                                              VkMemoryPropertyFlags memoryPropertyFlags)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Release and re-create the memory and buffer.
    release(contextVk);

    VkBufferCreateInfo createInfo    = {};
    createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.flags                 = 0;
    createInfo.size                  = size;
    createInfo.usage                 = GetDefaultBufferUsageFlags(contextVk->getRenderer());
    createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices   = nullptr;

    return mBuffer.initExternal(contextVk, memoryPropertyFlags, createInfo, clientBuffer);
}

angle::Result BufferVk::setDataWithUsageFlags(const gl::Context *context,
                                              gl::BufferBinding target,
                                              GLeglClientBufferEXT clientBuffer,
                                              const void *data,
                                              size_t size,
                                              gl::BufferUsage usage,
                                              GLbitfield flags)
{
    ContextVk *contextVk                      = vk::GetImpl(context);
    VkMemoryPropertyFlags memoryPropertyFlags = 0;
    bool persistentMapRequired                = false;
    const bool isExternalBuffer               = clientBuffer != nullptr;

    switch (usage)
    {
        case gl::BufferUsage::InvalidEnum:
        {
            // glBufferStorage API call
            memoryPropertyFlags =
                GetStorageMemoryType(contextVk->getRenderer(), flags, isExternalBuffer);
            persistentMapRequired = (flags & GL_MAP_PERSISTENT_BIT_EXT) != 0;
            break;
        }
        default:
        {
            // glBufferData API call
            memoryPropertyFlags = GetPreferredMemoryType(contextVk->getRenderer(), target, usage);
            break;
        }
    }

    if (isExternalBuffer)
    {
        ANGLE_TRY(setExternalBufferData(context, target, clientBuffer, size, memoryPropertyFlags));
        if (!mBuffer.isHostVisible())
        {
            // If external buffer's memory does not support host visible memory property, we cannot
            // support a persistent map request.
            ANGLE_VK_CHECK(contextVk, !persistentMapRequired, VK_ERROR_MEMORY_MAP_FAILED);
        }

        mClientBuffer = clientBuffer;

        return angle::Result::Continue;
    }
    return setDataWithMemoryType(context, target, data, size, memoryPropertyFlags,
                                 persistentMapRequired, usage);
}

angle::Result BufferVk::setData(const gl::Context *context,
                                gl::BufferBinding target,
                                const void *data,
                                size_t size,
                                gl::BufferUsage usage)
{
    ContextVk *contextVk = vk::GetImpl(context);
    // Assume host visible/coherent memory available.
    VkMemoryPropertyFlags memoryPropertyFlags =
        GetPreferredMemoryType(contextVk->getRenderer(), target, usage);
    return setDataWithMemoryType(context, target, data, size, memoryPropertyFlags, false, usage);
}

angle::Result BufferVk::setDataWithMemoryType(const gl::Context *context,
                                              gl::BufferBinding target,
                                              const void *data,
                                              size_t size,
                                              VkMemoryPropertyFlags memoryPropertyFlags,
                                              bool persistentMapRequired,
                                              gl::BufferUsage usage)
{
    ContextVk *contextVk = vk::GetImpl(context);

    // Reset the flag since the buffer contents are being reinitialized. If the caller passed in
    // data to fill the buffer, the flag will be updated when the data is copied to the buffer.
    mHasValidData = false;

    if (size == 0)
    {
        // Nothing to do.
        return angle::Result::Continue;
    }

    const bool bufferSizeChanged              = size != static_cast<size_t>(mState.getSize());
    const bool inUseAndRespecifiedWithoutData = (data == nullptr && isCurrentlyInUse(contextVk));

    // The entire buffer is being respecified, possibly with null data.
    // Release and init a new mBuffer with requested size.
    if (bufferSizeChanged || inUseAndRespecifiedWithoutData)
    {
        // Release and re-create the memory and buffer.
        release(contextVk);

        mMemoryPropertyFlags = memoryPropertyFlags;
        ANGLE_TRY(GetMemoryTypeIndex(contextVk, size, memoryPropertyFlags, &mMemoryTypeIndex));

        ANGLE_TRY(acquireBufferHelper(contextVk, size));
    }

    if (data)
    {
        // Treat full-buffer updates as SubData calls.
        BufferUpdateType updateType = bufferSizeChanged ? BufferUpdateType::StorageRedefined
                                                        : BufferUpdateType::ContentsUpdate;

        ANGLE_TRY(setDataImpl(contextVk, static_cast<const uint8_t *>(data), size, 0, updateType));
    }

    return angle::Result::Continue;
}

angle::Result BufferVk::setSubData(const gl::Context *context,
                                   gl::BufferBinding target,
                                   const void *data,
                                   size_t size,
                                   size_t offset)
{
    ASSERT(mBuffer.valid());

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(setDataImpl(contextVk, static_cast<const uint8_t *>(data), size, offset,
                          BufferUpdateType::ContentsUpdate));

    return angle::Result::Continue;
}

angle::Result BufferVk::copySubData(const gl::Context *context,
                                    BufferImpl *source,
                                    GLintptr sourceOffset,
                                    GLintptr destOffset,
                                    GLsizeiptr size)
{
    ASSERT(mBuffer.valid());

    ContextVk *contextVk           = vk::GetImpl(context);
    BufferVk *sourceVk             = GetAs<BufferVk>(source);
    vk::BufferHelper &sourceBuffer = sourceVk->getBuffer();
    ASSERT(sourceBuffer.valid());
    VkDeviceSize sourceBufferOffset = sourceBuffer.getOffset();

    // Check for self-dependency.
    vk::CommandBufferAccess access;
    if (sourceBuffer.getBufferSerial() == mBuffer.getBufferSerial())
    {
        access.onBufferSelfCopy(&mBuffer);
    }
    else
    {
        access.onBufferTransferRead(&sourceBuffer);
        access.onBufferTransferWrite(&mBuffer);
    }

    vk::OutsideRenderPassCommandBuffer *commandBuffer;
    ANGLE_TRY(contextVk->getOutsideRenderPassCommandBuffer(access, &commandBuffer));

    // Enqueue a copy command on the GPU.
    const VkBufferCopy copyRegion = {static_cast<VkDeviceSize>(sourceOffset) + sourceBufferOffset,
                                     static_cast<VkDeviceSize>(destOffset) + mBuffer.getOffset(),
                                     static_cast<VkDeviceSize>(size)};

    commandBuffer->copyBuffer(sourceBuffer.getBuffer(), mBuffer.getBuffer(), 1, &copyRegion);

    // The new destination buffer data may require a conversion for the next draw, so mark it dirty.
    onDataChanged();

    return angle::Result::Continue;
}

angle::Result BufferVk::allocStagingBuffer(ContextVk *contextVk,
                                           vk::MemoryCoherency coherency,
                                           VkDeviceSize size,
                                           uint8_t **mapPtr)
{
    ASSERT(!mIsStagingBufferMapped);

    if (mStagingBuffer.valid())
    {
        if (size <= mStagingBuffer.getSize() &&
            (coherency == vk::MemoryCoherency::Coherent) == mStagingBuffer.isCoherent() &&
            !mStagingBuffer.isCurrentlyInUse(contextVk->getLastCompletedQueueSerial()))
        {
            // If size is big enough and it is idle, then just reuse the existing staging buffer
            *mapPtr                = mStagingBuffer.getMappedMemory();
            mIsStagingBufferMapped = true;
            return angle::Result::Continue;
        }
        mStagingBuffer.release(contextVk->getRenderer());
    }

    ANGLE_TRY(
        mStagingBuffer.allocateForCopyBuffer(contextVk, static_cast<size_t>(size), coherency));
    *mapPtr                = mStagingBuffer.getMappedMemory();
    mIsStagingBufferMapped = true;

    return angle::Result::Continue;
}

angle::Result BufferVk::flushStagingBuffer(ContextVk *contextVk,
                                           VkDeviceSize offset,
                                           VkDeviceSize size)
{
    RendererVk *renderer = contextVk->getRenderer();

    ASSERT(mIsStagingBufferMapped);
    ASSERT(mStagingBuffer.valid());

    if (!mStagingBuffer.isCoherent())
    {
        ANGLE_TRY(mStagingBuffer.flush(renderer));
    }

    // Enqueue a copy command on the GPU.
    VkBufferCopy copyRegion = {mStagingBuffer.getOffset(), mBuffer.getOffset() + offset, size};
    ANGLE_TRY(mBuffer.copyFromBuffer(contextVk, &mStagingBuffer, 1, &copyRegion));

    return angle::Result::Continue;
}

angle::Result BufferVk::handleDeviceLocalBufferMap(ContextVk *contextVk,
                                                   VkDeviceSize offset,
                                                   VkDeviceSize size,
                                                   uint8_t **mapPtr)
{
    ANGLE_TRY(allocStagingBuffer(contextVk, vk::MemoryCoherency::Coherent, size, mapPtr));

    // Copy data from device local buffer to host visible staging buffer.
    VkBufferCopy copyRegion = {mBuffer.getOffset() + offset, mStagingBuffer.getOffset(), size};
    ANGLE_TRY(mStagingBuffer.copyFromBuffer(contextVk, &mBuffer, 1, &copyRegion));
    ANGLE_TRY(mStagingBuffer.waitForIdle(contextVk, "GPU stall due to mapping device local buffer",
                                         RenderPassClosureReason::DeviceLocalBufferMap));
    // Because the buffer is coherent, no need to call invalidate here.

    return angle::Result::Continue;
}

angle::Result BufferVk::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    ASSERT(mBuffer.valid());
    ASSERT(access == GL_WRITE_ONLY_OES);

    return mapImpl(vk::GetImpl(context), GL_MAP_WRITE_BIT, mapPtr);
}

angle::Result BufferVk::mapRange(const gl::Context *context,
                                 size_t offset,
                                 size_t length,
                                 GLbitfield access,
                                 void **mapPtr)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "BufferVk::mapRange");
    return mapRangeImpl(vk::GetImpl(context), offset, length, access, mapPtr);
}

angle::Result BufferVk::mapImpl(ContextVk *contextVk, GLbitfield access, void **mapPtr)
{
    return mapRangeImpl(contextVk, 0, static_cast<VkDeviceSize>(mState.getSize()), access, mapPtr);
}

angle::Result BufferVk::ghostMappedBuffer(ContextVk *contextVk,
                                          VkDeviceSize offset,
                                          VkDeviceSize length,
                                          GLbitfield access,
                                          void **mapPtr)
{
    // We shouldn't get here if it is external memory
    ASSERT(!isExternalBuffer());

    ++contextVk->getPerfCounters().buffersGhosted;

    // If we are creating a new buffer because the GPU is using it as read-only, then we
    // also need to copy the contents of the previous buffer into the new buffer, in
    // case the caller only updates a portion of the new buffer.
    vk::BufferHelper src = std::move(mBuffer);

    ANGLE_TRY(acquireBufferHelper(contextVk, static_cast<size_t>(mState.getSize())));

    // Before returning the new buffer, map the previous buffer and copy its entire
    // contents into the new buffer.
    uint8_t *srcMapPtr = nullptr;
    uint8_t *dstMapPtr = nullptr;
    ANGLE_TRY(src.map(contextVk, &srcMapPtr));
    ANGLE_TRY(mBuffer.map(contextVk, &dstMapPtr));

    ASSERT(src.isCoherent());
    ASSERT(mBuffer.isCoherent());

    // No need to copy over [offset, offset + length), just around it
    if ((access & GL_MAP_INVALIDATE_RANGE_BIT) != 0)
    {
        if (offset != 0)
        {
            memcpy(dstMapPtr, srcMapPtr, static_cast<size_t>(offset));
        }
        size_t totalSize      = static_cast<size_t>(mState.getSize());
        size_t remainingStart = static_cast<size_t>(offset + length);
        size_t remainingSize  = totalSize - remainingStart;
        if (remainingSize != 0)
        {
            memcpy(dstMapPtr + remainingStart, srcMapPtr + remainingStart, remainingSize);
        }
    }
    else
    {
        memcpy(dstMapPtr, srcMapPtr, static_cast<size_t>(mState.getSize()));
    }

    src.releaseBufferAndDescriptorSetCache(contextVk);

    // Return the already mapped pointer with the offset adjustment to avoid the call to unmap().
    *mapPtr = dstMapPtr + offset;

    return angle::Result::Continue;
}

angle::Result BufferVk::mapRangeImpl(ContextVk *contextVk,
                                     VkDeviceSize offset,
                                     VkDeviceSize length,
                                     GLbitfield access,
                                     void **mapPtr)
{
    ASSERT(mBuffer.valid());

    // Record map call parameters in case this call is from angle internal (the access/offset/length
    // will be inconsistent from mState).
    mIsMappedForWrite = (access & GL_MAP_WRITE_BIT) != 0;
    mMappedOffset     = offset;
    mMappedLength     = length;

    uint8_t **mapPtrBytes = reinterpret_cast<uint8_t **>(mapPtr);
    bool hostVisible      = mBuffer.isHostVisible();

    // MAP_UNSYNCHRONIZED_BIT, so immediately map.
    if ((access & GL_MAP_UNSYNCHRONIZED_BIT) != 0)
    {
        if (hostVisible)
        {
            return mBuffer.mapWithOffset(contextVk, mapPtrBytes, static_cast<size_t>(offset));
        }
        return handleDeviceLocalBufferMap(contextVk, offset, length, mapPtrBytes);
    }

    // Read case
    if ((access & GL_MAP_WRITE_BIT) == 0)
    {
        // If app is not going to write, all we need is to ensure GPU write is finished.
        // Concurrent reads from CPU and GPU is allowed.
        if (mBuffer.isCurrentlyInUseForWrite(contextVk->getLastCompletedQueueSerial()))
        {
            // If there are pending commands for the resource, flush them.
            if (mBuffer.usedInRecordedCommands())
            {
                ANGLE_TRY(
                    contextVk->flushImpl(nullptr, RenderPassClosureReason::BufferWriteThenMap));
            }
            ANGLE_TRY(mBuffer.finishGPUWriteCommands(contextVk));
        }
        if (hostVisible)
        {
            return mBuffer.mapWithOffset(contextVk, mapPtrBytes, static_cast<size_t>(offset));
        }
        return handleDeviceLocalBufferMap(contextVk, offset, length, mapPtrBytes);
    }

    // Write case
    if (!hostVisible)
    {
        return handleDeviceLocalBufferMap(contextVk, offset, length, mapPtrBytes);
    }

    // Write case, buffer not in use.
    if (isExternalBuffer() || !isCurrentlyInUse(contextVk))
    {
        return mBuffer.mapWithOffset(contextVk, mapPtrBytes, static_cast<size_t>(offset));
    }

    // Write case, buffer in use.
    //
    // Here, we try to map the buffer, but it's busy. Instead of waiting for the GPU to
    // finish, we just allocate a new buffer if:
    // 1.) Caller has told us it doesn't care about previous contents, or
    // 2.) The GPU won't write to the buffer.

    bool rangeInvalidate = (access & GL_MAP_INVALIDATE_RANGE_BIT) != 0;
    bool entireBufferInvalidated =
        ((access & GL_MAP_INVALIDATE_BUFFER_BIT) != 0) ||
        (rangeInvalidate && offset == 0 && static_cast<VkDeviceSize>(mState.getSize()) == length);

    if (entireBufferInvalidated)
    {
        ANGLE_TRY(acquireBufferHelper(contextVk, static_cast<size_t>(mState.getSize())));
        return mBuffer.mapWithOffset(contextVk, mapPtrBytes, static_cast<size_t>(offset));
    }

    bool smallMapRange = (length < static_cast<VkDeviceSize>(mState.getSize()) / 2);

    if (smallMapRange && rangeInvalidate)
    {
        ANGLE_TRY(allocStagingBuffer(contextVk, vk::MemoryCoherency::NonCoherent,
                                     static_cast<size_t>(length), mapPtrBytes));
        return angle::Result::Continue;
    }

    if (!mBuffer.isCurrentlyInUseForWrite(contextVk->getLastCompletedQueueSerial()))
    {
        // This will keep the new buffer mapped and update mapPtr, so return immediately.
        return ghostMappedBuffer(contextVk, offset, length, access, mapPtr);
    }

    // Write case (worst case, buffer in use for write)
    ANGLE_TRY(mBuffer.waitForIdle(contextVk, "GPU stall due to mapping buffer in use by the GPU",
                                  RenderPassClosureReason::BufferInUseWhenSynchronizedMap));
    return mBuffer.mapWithOffset(contextVk, mapPtrBytes, static_cast<size_t>(offset));
}

angle::Result BufferVk::unmap(const gl::Context *context, GLboolean *result)
{
    ANGLE_TRY(unmapImpl(vk::GetImpl(context)));

    // This should be false if the contents have been corrupted through external means.  Vulkan
    // doesn't provide such information.
    *result = true;

    return angle::Result::Continue;
}

angle::Result BufferVk::unmapImpl(ContextVk *contextVk)
{
    ASSERT(mBuffer.valid());

    if (mIsStagingBufferMapped)
    {
        ASSERT(mStagingBuffer.valid());
        // The buffer is device local or optimization of small range map.
        if (mIsMappedForWrite)
        {
            ANGLE_TRY(flushStagingBuffer(contextVk, mMappedOffset, mMappedLength));
        }

        mIsStagingBufferMapped = false;
    }
    else
    {
        ASSERT(mBuffer.isHostVisible());
        mBuffer.unmap(contextVk->getRenderer());
    }

    if (mIsMappedForWrite)
    {
        dataUpdated();
    }

    // Reset the mapping parameters
    mIsMappedForWrite = false;
    mMappedOffset     = 0;
    mMappedLength     = 0;

    return angle::Result::Continue;
}

angle::Result BufferVk::getSubData(const gl::Context *context,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   void *outData)
{
    ASSERT(offset + size <= getSize());
    ASSERT(mBuffer.valid());
    ContextVk *contextVk = vk::GetImpl(context);
    void *mapPtr;
    ANGLE_TRY(mapRangeImpl(contextVk, offset, size, GL_MAP_READ_BIT, &mapPtr));
    memcpy(outData, mapPtr, size);
    return unmapImpl(contextVk);
}

angle::Result BufferVk::getIndexRange(const gl::Context *context,
                                      gl::DrawElementsType type,
                                      size_t offset,
                                      size_t count,
                                      bool primitiveRestartEnabled,
                                      gl::IndexRange *outRange)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    // This is a workaround for the mock ICD not implementing buffer memory state.
    // Could be removed if https://github.com/KhronosGroup/Vulkan-Tools/issues/84 is fixed.
    if (renderer->isMockICDEnabled())
    {
        outRange->start = 0;
        outRange->end   = 0;
        return angle::Result::Continue;
    }

    ANGLE_TRACE_EVENT0("gpu.angle", "BufferVk::getIndexRange");

    void *mapPtr;
    ANGLE_TRY(mapRangeImpl(contextVk, offset, getSize(), GL_MAP_READ_BIT, &mapPtr));
    *outRange = gl::ComputeIndexRange(type, mapPtr, count, primitiveRestartEnabled);
    ANGLE_TRY(unmapImpl(contextVk));

    return angle::Result::Continue;
}

angle::Result BufferVk::updateBuffer(ContextVk *contextVk,
                                     const uint8_t *data,
                                     size_t size,
                                     size_t offset)
{
    if (mBuffer.isHostVisible())
    {
        ANGLE_TRY(directUpdate(contextVk, data, size, offset));
    }
    else
    {
        ANGLE_TRY(stagedUpdate(contextVk, data, size, offset));
    }
    return angle::Result::Continue;
}

angle::Result BufferVk::directUpdate(ContextVk *contextVk,
                                     const uint8_t *data,
                                     size_t size,
                                     size_t offset)
{
    uint8_t *mapPointer = nullptr;

    ANGLE_TRY(mBuffer.mapWithOffset(contextVk, &mapPointer, offset));
    ASSERT(mapPointer);

    memcpy(mapPointer, data, size);

    // If the buffer has dynamic usage then the intent is frequent client side updates to the
    // buffer. Don't CPU unmap the buffer, we will take care of unmapping when releasing the buffer
    // to either the renderer or mBufferFreeList.
    if (!IsUsageDynamic(mState.getUsage()))
    {
        mBuffer.unmap(contextVk->getRenderer());
    }
    ASSERT(mBuffer.isCoherent());

    return angle::Result::Continue;
}

angle::Result BufferVk::stagedUpdate(ContextVk *contextVk,
                                     const uint8_t *data,
                                     size_t size,
                                     size_t offset)
{
    // Acquire a "new" staging buffer
    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(allocStagingBuffer(contextVk, vk::MemoryCoherency::NonCoherent, size, &mapPointer));
    memcpy(mapPointer, data, size);
    ANGLE_TRY(flushStagingBuffer(contextVk, offset, size));
    mIsStagingBufferMapped = false;

    return angle::Result::Continue;
}

angle::Result BufferVk::acquireAndUpdate(ContextVk *contextVk,
                                         const uint8_t *data,
                                         size_t updateSize,
                                         size_t offset,
                                         BufferUpdateType updateType)
{
    // We shouldn't get here if this is external memory
    ASSERT(!isExternalBuffer());
    // If StorageRedefined, we can not use mState.getSize() to allocate a new buffer.
    ASSERT(updateType != BufferUpdateType::StorageRedefined);

    // Here we acquire a new BufferHelper and directUpdate() the new buffer.
    // If the subData size was less than the buffer's size we additionally enqueue
    // a GPU copy of the remaining regions from the old mBuffer to the new one.
    vk::BufferHelper src;
    size_t bufferSize              = static_cast<size_t>(mState.getSize());
    size_t offsetAfterSubdata      = (offset + updateSize);
    bool updateRegionBeforeSubData = mHasValidData && (offset > 0);
    bool updateRegionAfterSubData  = mHasValidData && (offsetAfterSubdata < bufferSize);

    uint8_t *srcMapPtrBeforeSubData = nullptr;
    uint8_t *srcMapPtrAfterSubData  = nullptr;
    if (updateRegionBeforeSubData || updateRegionAfterSubData)
    {
        src = std::move(mBuffer);

        // The total bytes that we need to copy from old buffer to new buffer
        size_t copySize = bufferSize - updateSize;

        // If the buffer is host visible and the GPU is not writing to it, we use the CPU to do the
        // copy. We need to save the source buffer pointer before we acquire a new buffer.
        if (src.isHostVisible() &&
            !src.isCurrentlyInUseForWrite(contextVk->getLastCompletedQueueSerial()) &&
            ShouldUseCPUToCopyData(contextVk, copySize, bufferSize))
        {
            uint8_t *mapPointer = nullptr;
            // src buffer will be recycled (or released and unmapped) by acquireBufferHelper
            ANGLE_TRY(src.map(contextVk, &mapPointer));
            ASSERT(mapPointer);
            srcMapPtrBeforeSubData = mapPointer;
            srcMapPtrAfterSubData  = mapPointer + offsetAfterSubdata;
        }
    }

    ANGLE_TRY(acquireBufferHelper(contextVk, bufferSize));
    ANGLE_TRY(updateBuffer(contextVk, data, updateSize, offset));

    constexpr int kMaxCopyRegions = 2;
    angle::FixedVector<VkBufferCopy, kMaxCopyRegions> copyRegions;

    if (updateRegionBeforeSubData)
    {
        if (srcMapPtrBeforeSubData)
        {
            ASSERT(mBuffer.isHostVisible());
            ANGLE_TRY(directUpdate(contextVk, srcMapPtrBeforeSubData, offset, 0));
        }
        else
        {
            copyRegions.push_back({src.getOffset(), mBuffer.getOffset(), offset});
        }
    }

    if (updateRegionAfterSubData)
    {
        size_t copySize = bufferSize - offsetAfterSubdata;
        if (srcMapPtrAfterSubData)
        {
            ASSERT(mBuffer.isHostVisible());
            ANGLE_TRY(directUpdate(contextVk, srcMapPtrAfterSubData, copySize, offsetAfterSubdata));
        }
        else
        {
            copyRegions.push_back({src.getOffset() + offsetAfterSubdata,
                                   mBuffer.getOffset() + offsetAfterSubdata, copySize});
        }
    }

    if (!copyRegions.empty())
    {
        ANGLE_TRY(mBuffer.copyFromBuffer(contextVk, &src, static_cast<uint32_t>(copyRegions.size()),
                                         copyRegions.data()));
    }

    if (src.valid())
    {
        src.releaseBufferAndDescriptorSetCache(contextVk);
    }

    return angle::Result::Continue;
}

angle::Result BufferVk::setDataImpl(ContextVk *contextVk,
                                    const uint8_t *data,
                                    size_t size,
                                    size_t offset,
                                    BufferUpdateType updateType)
{
    // if the buffer is currently in use
    //     if it isn't an external buffer and sub data size meets threshold
    //          acquire a new BufferHelper from the pool
    //     else stage the update
    // else update the buffer directly
    if (isCurrentlyInUse(contextVk))
    {
        // If storage has just been redefined, don't go down acquireAndUpdate code path. There is no
        // reason you acquire another new buffer right after redefined. And if we do go into
        // acquireAndUpdate, you will also run int correctness bug that mState.getSize() has not
        // been updated with new size and you will acquire a new buffer with wrong size. This could
        // happen if the buffer memory is DEVICE_LOCAL and
        // renderer->getFeatures().allocateNonZeroMemory.enabled is true. In this case we will issue
        // a copyToBuffer immediately after allocation and isCurrentlyInUse will be true.
        // If BufferVk does not have any valid data, which means there is no data needs to be copied
        // from old buffer to new buffer when we acquire a new buffer, we also favor
        // acquireAndUpdate over stagedUpdate. This could happen when app calls glBufferData with
        // same size and we will try to reuse the existing buffer storage.
        // To avoid breaking the render pass unnecessary, acquireAndUpdate is also favored over
        // stagedUpdate if the buffer is used read-only in the current render pass.
        const bool canAcquireAndUpdate =
            !isExternalBuffer() && updateType != BufferUpdateType::StorageRedefined;
        if (canAcquireAndUpdate &&
            (!mHasValidData ||
             ShouldAvoidRenderPassBreakOnUpdate(contextVk, mBuffer,
                                                static_cast<size_t>(mState.getSize())) ||
             ShouldAllocateNewMemoryForUpdate(contextVk, size,
                                              static_cast<size_t>(mState.getSize()))))
        {
            ANGLE_TRY(acquireAndUpdate(contextVk, data, size, offset, updateType));
        }
        else
        {
            if (canAcquireAndUpdate && RenderPassUsesBufferForReadOnly(contextVk, mBuffer))
            {
                ANGLE_VK_PERF_WARNING(contextVk, GL_DEBUG_SEVERITY_LOW,
                                      "Breaking the render pass on small upload to large buffer");
            }

            ANGLE_TRY(stagedUpdate(contextVk, data, size, offset));
        }
    }
    else
    {
        ANGLE_TRY(updateBuffer(contextVk, data, size, offset));
    }

    // Update conversions
    dataUpdated();

    return angle::Result::Continue;
}

ConversionBuffer *BufferVk::getVertexConversionBuffer(RendererVk *renderer,
                                                      angle::FormatID formatID,
                                                      GLuint stride,
                                                      size_t offset,
                                                      bool hostVisible)
{
    for (VertexConversionBuffer &buffer : mVertexConversionBuffers)
    {
        if (buffer.formatID == formatID && buffer.stride == stride && buffer.offset == offset)
        {
            ASSERT(buffer.data && buffer.data->valid());
            return &buffer;
        }
    }

    mVertexConversionBuffers.emplace_back(renderer, formatID, stride, offset, hostVisible);
    return &mVertexConversionBuffers.back();
}

void BufferVk::dataUpdated()
{
    for (VertexConversionBuffer &buffer : mVertexConversionBuffers)
    {
        buffer.dirty = true;
    }
    // Now we have valid data
    mHasValidData = true;
}

void BufferVk::onDataChanged()
{
    dataUpdated();
}

angle::Result BufferVk::acquireBufferHelper(ContextVk *contextVk, size_t sizeInBytes)
{
    RendererVk *renderer = contextVk->getRenderer();
    size_t size          = roundUpPow2(sizeInBytes, kBufferSizeGranularity);
    size_t alignment     = renderer->getDefaultBufferAlignment();

    if (mBuffer.valid())
    {
        mBuffer.releaseBufferAndDescriptorSetCache(contextVk);
    }

    // Allocate the buffer directly
    ANGLE_TRY(mBuffer.initSuballocation(contextVk, mMemoryTypeIndex, size, alignment));

    // Tell the observers (front end) that a new buffer was created, so the necessary
    // dirty bits can be set. This allows the buffer views pointing to the old buffer to
    // be recreated and point to the new buffer, along with updating the descriptor sets
    // to use the new buffer.
    onStateChange(angle::SubjectMessage::InternalMemoryAllocationChanged);

    return angle::Result::Continue;
}

bool BufferVk::isCurrentlyInUse(ContextVk *contextVk) const
{
    return mBuffer.isCurrentlyInUse(contextVk->getLastCompletedQueueSerial());
}

}  // namespace rx
