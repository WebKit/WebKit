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
                     size_t alignment,
                     bool hostVisible);
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
    angle::Result getSubData(const gl::Context *context,
                             GLintptr offset,
                             GLsizeiptr size,
                             void *outData) override;

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
        ASSERT(isBufferValid());
        return *mBuffer;
    }

    vk::BufferHelper &getBuffer()
    {
        ASSERT(isBufferValid());
        return *mBuffer;
    }

    bool isBufferValid() const { return mBuffer && mBuffer->valid(); }

    angle::Result mapImpl(ContextVk *contextVk, void **mapPtr);
    angle::Result mapRangeImpl(ContextVk *contextVk,
                               VkDeviceSize offset,
                               VkDeviceSize length,
                               GLbitfield access,
                               void **mapPtr);
    angle::Result unmapImpl(ContextVk *contextVk);

    // Calls copyBuffer internally.
    angle::Result copyToBufferImpl(ContextVk *contextVk,
                                   vk::BufferHelper *destBuffer,
                                   uint32_t copyCount,
                                   const VkBufferCopy *copies);

    ConversionBuffer *getVertexConversionBuffer(RendererVk *renderer,
                                                angle::FormatID formatID,
                                                GLuint stride,
                                                size_t offset,
                                                bool hostVisible);

  private:
    angle::Result initializeShadowBuffer(ContextVk *contextVk,
                                         gl::BufferBinding target,
                                         size_t size);

    ANGLE_INLINE uint8_t *getShadowBuffer(size_t offset)
    {
        return (mShadowBuffer.getCurrentBuffer() + offset);
    }

    ANGLE_INLINE const uint8_t *getShadowBuffer(size_t offset) const
    {
        return (mShadowBuffer.getCurrentBuffer() + offset);
    }

    void updateShadowBuffer(const uint8_t *data, size_t size, size_t offset);
    angle::Result directUpdate(ContextVk *contextVk,
                               const uint8_t *data,
                               size_t size,
                               size_t offset);
    angle::Result stagedUpdate(ContextVk *contextVk,
                               const uint8_t *data,
                               size_t size,
                               size_t offset);
    angle::Result acquireAndUpdate(ContextVk *contextVk,
                                   const uint8_t *data,
                                   size_t size,
                                   size_t offset);
    angle::Result setDataImpl(ContextVk *contextVk,
                              const uint8_t *data,
                              size_t size,
                              size_t offset);
    void release(ContextVk *context);
    void markConversionBuffersDirty();

    angle::Result acquireBufferHelper(ContextVk *contextVk,
                                      size_t sizeInBytes,
                                      vk::BufferHelper **bufferHelperOut);

    struct VertexConversionBuffer : public ConversionBuffer
    {
        VertexConversionBuffer(RendererVk *renderer,
                               angle::FormatID formatIDIn,
                               GLuint strideIn,
                               size_t offsetIn,
                               bool hostVisible);
        ~VertexConversionBuffer();

        VertexConversionBuffer(VertexConversionBuffer &&other);

        // The conversion is identified by the triple of {format, stride, offset}.
        angle::FormatID formatID;
        GLuint stride;
        size_t offset;
    };

    vk::BufferHelper *mBuffer;

    // Pool of BufferHelpers for mBuffer to acquire from
    vk::DynamicBuffer mBufferPool;

    // For GPU-read only buffers glMap* latency is reduced by maintaining a copy
    // of the buffer which is writeable only by the CPU. The contents are updated on all
    // glData/glSubData/glCopy calls. With this, a glMap* call becomes a non-blocking
    // operation by elimnating the need to wait on any recorded or in-flight GPU commands.
    // We use DynamicShadowBuffer class to encapsulate all the bookeeping logic.
    vk::DynamicShadowBuffer mShadowBuffer;

    // A cache of converted vertex data.
    std::vector<VertexConversionBuffer> mVertexConversionBuffers;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
