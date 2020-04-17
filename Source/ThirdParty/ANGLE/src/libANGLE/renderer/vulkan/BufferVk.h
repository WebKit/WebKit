//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferVk.h:
//    Defines the class interface for BufferVk, implementing BufferImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_

#include "libANGLE/Buffer.h"
#include "libANGLE/Observer.h"
#include "libANGLE/renderer/BufferImpl.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class RendererVk;

// Conversion buffers hold translated index and vertex data.
struct ConversionBuffer
{
    ConversionBuffer(RendererVk *renderer,
                     VkBufferUsageFlags usageFlags,
                     size_t initialSize,
                     size_t alignment);
    ~ConversionBuffer();

    ConversionBuffer(ConversionBuffer &&other);

    // One state value determines if we need to re-stream vertex data.
    bool dirty;

    // One additional state value keeps the last allocation offset.
    VkDeviceSize lastAllocationOffset;

    // The conversion is stored in a dynamic buffer.
    vk::DynamicBuffer data;
};

class BufferVk : public BufferImpl
{
  public:
    BufferVk(const gl::BufferState &state);
    ~BufferVk() override;
    void destroy(const gl::Context *context) override;

    angle::Result setData(const gl::Context *context,
                          gl::BufferBinding target,
                          const void *data,
                          size_t size,
                          gl::BufferUsage usage) override;
    angle::Result setSubData(const gl::Context *context,
                             gl::BufferBinding target,
                             const void *data,
                             size_t size,
                             size_t offset) override;
    angle::Result copySubData(const gl::Context *context,
                              BufferImpl *source,
                              GLintptr sourceOffset,
                              GLintptr destOffset,
                              GLsizeiptr size) override;
    angle::Result map(const gl::Context *context, GLenum access, void **mapPtr) override;
    angle::Result mapRange(const gl::Context *context,
                           size_t offset,
                           size_t length,
                           GLbitfield access,
                           void **mapPtr) override;
    angle::Result unmap(const gl::Context *context, GLboolean *result) override;

    angle::Result getIndexRange(const gl::Context *context,
                                gl::DrawElementsType type,
                                size_t offset,
                                size_t count,
                                bool primitiveRestartEnabled,
                                gl::IndexRange *outRange) override;

    GLint64 getSize() const { return mState.getSize(); }

    void onDataChanged() override;

    const vk::BufferHelper &getBuffer() const
    {
        ASSERT(mBuffer.valid());
        return mBuffer;
    }

    vk::BufferHelper &getBuffer()
    {
        ASSERT(mBuffer.valid());
        return mBuffer;
    }

    angle::Result mapImpl(ContextVk *contextVk, void **mapPtr);
    angle::Result mapRangeImpl(ContextVk *contextVk,
                               VkDeviceSize offset,
                               VkDeviceSize length,
                               GLbitfield access,
                               void **mapPtr);
    void unmapImpl(ContextVk *contextVk);

    // Calls copyBuffer internally.
    angle::Result copyToBuffer(ContextVk *contextVk,
                               vk::BufferHelper *destBuffer,
                               uint32_t copyCount,
                               const VkBufferCopy *copies);

    ConversionBuffer *getVertexConversionBuffer(RendererVk *renderer,
                                                angle::FormatID formatID,
                                                GLuint stride,
                                                size_t offset);

  private:
    void initializeStagingBuffer(ContextVk *contextVk, gl::BufferBinding target, size_t size);
    angle::Result setDataImpl(ContextVk *contextVk,
                              const uint8_t *data,
                              size_t size,
                              size_t offset);
    void release(ContextVk *context);
    void markConversionBuffersDirty();

    struct VertexConversionBuffer : public ConversionBuffer
    {
        VertexConversionBuffer(RendererVk *renderer,
                               angle::FormatID formatIDIn,
                               GLuint strideIn,
                               size_t offsetIn);
        ~VertexConversionBuffer();

        VertexConversionBuffer(VertexConversionBuffer &&other);

        // The conversion is identified by the triple of {format, stride, offset}.
        angle::FormatID formatID;
        GLuint stride;
        size_t offset;
    };

    vk::BufferHelper mBuffer;

    // All staging buffer support is provided by a DynamicBuffer.
    vk::DynamicBuffer mStagingBuffer;

    // A cache of converted vertex data.
    std::vector<VertexConversionBuffer> mVertexConversionBuffers;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
