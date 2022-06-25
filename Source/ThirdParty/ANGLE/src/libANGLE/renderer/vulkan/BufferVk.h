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

    // Where the conversion data is stored.
    std::unique_ptr<vk::BufferHelper> data;
};

enum class BufferUpdateType
{
    StorageRedefined,
    ContentsUpdate,
};

VkBufferUsageFlags GetDefaultBufferUsageFlags(RendererVk *renderer);

class BufferVk : public BufferImpl
{
  public:
    BufferVk(const gl::BufferState &state);
    ~BufferVk() override;
    void destroy(const gl::Context *context) override;

    angle::Result setExternalBufferData(const gl::Context *context,
                                        gl::BufferBinding target,
                                        GLeglClientBufferEXT clientBuffer,
                                        size_t size,
                                        VkMemoryPropertyFlags memoryPropertyFlags);
    angle::Result setDataWithUsageFlags(const gl::Context *context,
                                        gl::BufferBinding target,
                                        GLeglClientBufferEXT clientBuffer,
                                        const void *data,
                                        size_t size,
                                        gl::BufferUsage usage,
                                        GLbitfield flags) override;
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

    vk::BufferHelper &getBuffer()
    {
        ASSERT(isBufferValid());
        return mBuffer;
    }

    bool isBufferValid() const { return mBuffer.valid(); }
    bool isCurrentlyInUse(ContextVk *contextVk) const;

    angle::Result mapImpl(ContextVk *contextVk, GLbitfield access, void **mapPtr);
    angle::Result mapRangeImpl(ContextVk *contextVk,
                               VkDeviceSize offset,
                               VkDeviceSize length,
                               GLbitfield access,
                               void **mapPtr);
    angle::Result unmapImpl(ContextVk *contextVk);
    angle::Result ghostMappedBuffer(ContextVk *contextVk,
                                    VkDeviceSize offset,
                                    VkDeviceSize length,
                                    GLbitfield access,
                                    void **mapPtr);

    ConversionBuffer *getVertexConversionBuffer(RendererVk *renderer,
                                                angle::FormatID formatID,
                                                GLuint stride,
                                                size_t offset,
                                                bool hostVisible);

  private:
    angle::Result updateBuffer(ContextVk *contextVk,
                               const uint8_t *data,
                               size_t size,
                               size_t offset);
    angle::Result directUpdate(ContextVk *contextVk,
                               const uint8_t *data,
                               size_t size,
                               size_t offset);
    angle::Result stagedUpdate(ContextVk *contextVk,
                               const uint8_t *data,
                               size_t size,
                               size_t offset);
    angle::Result allocStagingBuffer(ContextVk *contextVk,
                                     vk::MemoryCoherency coherency,
                                     VkDeviceSize size,
                                     uint8_t **mapPtr);
    angle::Result flushStagingBuffer(ContextVk *contextVk, VkDeviceSize offset, VkDeviceSize size);
    angle::Result acquireAndUpdate(ContextVk *contextVk,
                                   const uint8_t *data,
                                   size_t updateSize,
                                   size_t offset,
                                   BufferUpdateType updateType);
    angle::Result setDataWithMemoryType(const gl::Context *context,
                                        gl::BufferBinding target,
                                        const void *data,
                                        size_t size,
                                        VkMemoryPropertyFlags memoryPropertyFlags,
                                        bool persistentMapRequired,
                                        gl::BufferUsage usage);
    angle::Result handleDeviceLocalBufferMap(ContextVk *contextVk,
                                             VkDeviceSize offset,
                                             VkDeviceSize size,
                                             uint8_t **mapPtr);
    angle::Result setDataImpl(ContextVk *contextVk,
                              const uint8_t *data,
                              size_t size,
                              size_t offset,
                              BufferUpdateType updateType);
    void release(ContextVk *context);
    void dataUpdated();

    angle::Result acquireBufferHelper(ContextVk *contextVk, size_t sizeInBytes);

    bool isExternalBuffer() const { return mClientBuffer != nullptr; }

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

    vk::BufferHelper mBuffer;

    // If not null, this is the external memory pointer passed from client API.
    void *mClientBuffer;

    uint32_t mMemoryTypeIndex;
    // Memory/Usage property that will be used for memory allocation.
    VkMemoryPropertyFlags mMemoryPropertyFlags;

    // The staging buffer to aid map operations. This is used when buffers are not host visible or
    // for performance optimization when only a smaller range of buffer is mapped.
    vk::BufferHelper mStagingBuffer;

    // A cache of converted vertex data.
    std::vector<VertexConversionBuffer> mVertexConversionBuffers;

    // Tracks whether mStagingBuffer has been mapped to user or not
    bool mIsStagingBufferMapped;

    // Tracks if BufferVk object has valid data or not.
    bool mHasValidData;

    // True if the buffer is currently mapped for CPU write access. If the map call is originated
    // from OpenGLES API call, then this should be consistent with mState.getAccessFlags() bits.
    // Otherwise it is mapped from ANGLE internal and will not be consistent with mState access
    // bits, so we have to keep record of it.
    bool mIsMappedForWrite;
    // Similar as mIsMappedForWrite, this maybe different from mState's getMapOffset/getMapLength if
    // mapped from angle internal.
    VkDeviceSize mMappedOffset;
    VkDeviceSize mMappedLength;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
