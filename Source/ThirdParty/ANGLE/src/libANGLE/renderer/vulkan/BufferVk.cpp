//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.cpp:
//    Implements the class methods for BufferVk.
//

#include "libANGLE/renderer/vulkan/BufferVk.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

BufferVk::BufferVk(const gl::BufferState &state) : BufferImpl(state), mCurrentRequiredSize(0)
{
}

BufferVk::~BufferVk()
{
}

void BufferVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    release(renderer);
}

void BufferVk::release(RendererVk *renderer)
{
    renderer->releaseResource(*this, &mBuffer);
    renderer->releaseResource(*this, &mBufferMemory);
}

gl::Error BufferVk::setData(const gl::Context *context,
                            gl::BufferBinding target,
                            const void *data,
                            size_t size,
                            gl::BufferUsage usage)
{
    ContextVk *contextVk = vk::GetImpl(context);
    VkDevice device      = contextVk->getDevice();

    if (size > mCurrentRequiredSize)
    {
        // Release and re-create the memory and buffer.
        release(contextVk->getRenderer());

        // TODO(jmadill): Proper usage bit implementation. Likely will involve multiple backing
        // buffers like in D3D11.
        VkBufferCreateInfo createInfo;
        createInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.pNext                 = nullptr;
        createInfo.flags                 = 0;
        createInfo.size                  = size;
        createInfo.usage = (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;

        ANGLE_TRY(mBuffer.init(device, createInfo));
        ANGLE_TRY(vk::AllocateBufferMemory(contextVk, size, &mBuffer, &mBufferMemory,
                                           &mCurrentRequiredSize));
    }

    if (data)
    {
        ANGLE_TRY(setDataImpl(contextVk, static_cast<const uint8_t *>(data), size, 0));
    }

    return gl::NoError();
}

gl::Error BufferVk::setSubData(const gl::Context *context,
                               gl::BufferBinding target,
                               const void *data,
                               size_t size,
                               size_t offset)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    ContextVk *contextVk = vk::GetImpl(context);
    ANGLE_TRY(setDataImpl(contextVk, static_cast<const uint8_t *>(data), size, offset));

    return gl::NoError();
}

gl::Error BufferVk::copySubData(const gl::Context *context,
                                BufferImpl *source,
                                GLintptr sourceOffset,
                                GLintptr destOffset,
                                GLsizeiptr size)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error BufferVk::map(const gl::Context *context, GLenum access, void **mapPtr)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    VkDevice device = vk::GetImpl(context)->getDevice();

    ANGLE_TRY(
        mBufferMemory.map(device, 0, mState.getSize(), 0, reinterpret_cast<uint8_t **>(mapPtr)));

    return gl::NoError();
}

gl::Error BufferVk::mapRange(const gl::Context *context,
                             size_t offset,
                             size_t length,
                             GLbitfield access,
                             void **mapPtr)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    VkDevice device = vk::GetImpl(context)->getDevice();

    ANGLE_TRY(mBufferMemory.map(device, offset, length, 0, reinterpret_cast<uint8_t **>(mapPtr)));

    return gl::NoError();
}

gl::Error BufferVk::unmap(const gl::Context *context, GLboolean *result)
{
    ASSERT(mBuffer.getHandle() != VK_NULL_HANDLE);
    ASSERT(mBufferMemory.getHandle() != VK_NULL_HANDLE);

    VkDevice device = vk::GetImpl(context)->getDevice();

    mBufferMemory.unmap(device);

    return gl::NoError();
}

gl::Error BufferVk::getIndexRange(const gl::Context *context,
                                  GLenum type,
                                  size_t offset,
                                  size_t count,
                                  bool primitiveRestartEnabled,
                                  gl::IndexRange *outRange)
{
    VkDevice device = vk::GetImpl(context)->getDevice();

    // TODO(jmadill): Consider keeping a shadow system memory copy in some cases.
    ASSERT(mBuffer.valid());

    const gl::Type &typeInfo = gl::GetTypeInfo(type);

    uint8_t *mapPointer = nullptr;
    ANGLE_TRY(mBufferMemory.map(device, offset, typeInfo.bytes * count, 0, &mapPointer));

    *outRange = gl::ComputeIndexRange(type, mapPointer, count, primitiveRestartEnabled);

    return gl::NoError();
}

vk::Error BufferVk::setDataImpl(ContextVk *contextVk,
                                const uint8_t *data,
                                size_t size,
                                size_t offset)
{
    RendererVk *renderer = contextVk->getRenderer();
    VkDevice device      = contextVk->getDevice();

    // Use map when available.
    if (renderer->isSerialInUse(getQueueSerial()))
    {
        vk::StagingBuffer stagingBuffer;
        ANGLE_TRY(stagingBuffer.init(contextVk, static_cast<VkDeviceSize>(size),
                                     vk::StagingUsage::Write));

        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(stagingBuffer.getDeviceMemory().map(device, 0, size, 0, &mapPointer));
        ASSERT(mapPointer);

        memcpy(mapPointer, data, size);
        stagingBuffer.getDeviceMemory().unmap(device);

        // Enqueue a copy command on the GPU.
        // TODO(jmadill): Command re-ordering for render passes.
        renderer->endRenderPass();

        vk::CommandBufferAndState *commandBuffer = nullptr;
        ANGLE_TRY(renderer->getStartedCommandBuffer(&commandBuffer));

        // Insert a barrier to ensure reads from the buffer are complete.
        // TODO(jmadill): Insert minimal barriers.
        VkBufferMemoryBarrier bufferBarrier;
        bufferBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.pNext               = nullptr;
        bufferBarrier.srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
        bufferBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferBarrier.srcQueueFamilyIndex = 0;
        bufferBarrier.dstQueueFamilyIndex = 0;
        bufferBarrier.buffer              = mBuffer.getHandle();
        bufferBarrier.offset              = offset;
        bufferBarrier.size                = static_cast<VkDeviceSize>(size);

        commandBuffer->singleBufferBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, bufferBarrier);

        VkBufferCopy copyRegion = {offset, 0, size};
        commandBuffer->copyBuffer(stagingBuffer.getBuffer(), mBuffer, 1, &copyRegion);

        setQueueSerial(renderer->getCurrentQueueSerial());
        renderer->releaseObject(getQueueSerial(), &stagingBuffer);
    }
    else
    {
        uint8_t *mapPointer = nullptr;
        ANGLE_TRY(mBufferMemory.map(device, offset, size, 0, &mapPointer));
        ASSERT(mapPointer);

        memcpy(mapPointer, data, size);

        mBufferMemory.unmap(device);
    }

    return vk::NoError();
}

const vk::Buffer &BufferVk::getVkBuffer() const
{
    return mBuffer;
}

}  // namespace rx
