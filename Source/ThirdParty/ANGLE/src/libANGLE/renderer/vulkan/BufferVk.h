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

enum class BufferUpdateType
{
    StorageRedefined,
    ContentsUpdate,
};

VkBufferUsageFlags GetDefaultBufferUsageFlags(RendererVk *renderer);
size_t GetDefaultBufferAlignment(RendererVk *renderer);

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
        // Always mark the BufferHelper as referenced by the GPU, whether or not there's a pending
        // submission, since this function is only called when trying to get the underlying
        // BufferHelper object so it can be used in a command.
        mHasBeenReferencedByGPU = true;
        return *mBuffer.get();
    }

    bool isBufferValid() const { return mBuffer.get() != nullptr; }
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
    angle::Result initializeShadowBuffer(ContextVk *contextVk,
                                         gl::BufferBinding target,
                                         size_t size);
    void initializeHostVisibleBufferPool(ContextVk *contextVk);

    ANGLE_INLINE uint8_t *getShadowBuffer(size_t offset)
    {
        return (mShadowBuffer.getCurrentBuffer() + offset);
    }

    ANGLE_INLINE const uint8_t *getShadowBuffer(size_t offset) const
    {
        return (mShadowBuffer.getCurrentBuffer() + offset);
    }

    void updateShadowBuffer(const uint8_t *data, size_t size, size_t offset);
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
    angle::Result allocMappedStagingBuffer(ContextVk *contextVk,
                                           size_t size,
                                           vk::DynamicBuffer **stagingBuffer,
                                           VkDeviceSize *stagingBufferOffset,
                                           uint8_t **mapPtr);
    angle::Result flushMappedStagingBuffer(ContextVk *contextVk,
                                           vk::DynamicBuffer *stagingBuffer,
                                           VkDeviceSize stagingBufferOffset,
                                           size_t size,
                                           size_t offset);
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
    angle::Result handleDeviceLocalBufferUnmap(ContextVk *contextVk,
                                               VkDeviceSize offset,
                                               VkDeviceSize size);
    angle::Result setDataImpl(ContextVk *contextVk,
                              const uint8_t *data,
                              size_t size,
                              size_t offset,
                              BufferUpdateType updateType);
    void release(ContextVk *context);
    void dataUpdated();

    angle::Result acquireBufferHelper(ContextVk *contextVk,
                                      size_t sizeInBytes,
                                      BufferUpdateType updateType);

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

    std::unique_ptr<vk::BufferHelper> mBuffer;

    uint32_t mMemoryTypeIndex;
    // Memory/Usage property that will be used for memory allocation.
    VkMemoryPropertyFlags mMemoryPropertyFlags;

    // DynamicBuffer to aid map operations of buffers when they are not host visible.
    vk::DynamicBuffer mHostVisibleBufferPool;
    VkDeviceSize mHostVisibleBufferOffset;

    // For GPU-read only buffers glMap* latency is reduced by maintaining a copy
    // of the buffer which is writeable only by the CPU. The contents are updated on all
    // glData/glSubData/glCopy calls. With this, a glMap* call becomes a non-blocking
    // operation by elimnating the need to wait on any recorded or in-flight GPU commands.
    // We use DynamicShadowBuffer class to encapsulate all the bookeeping logic.
    vk::DynamicShadowBuffer mShadowBuffer;

    // A buffer pool to service GL_MAP_INVALIDATE_RANGE_BIT -style uploads.
    vk::DynamicBuffer *mMapInvalidateRangeStagingBuffer;
    VkDeviceSize mMapInvalidateRangeStagingBufferOffset;
    uint8_t *mMapInvalidateRangeMappedPtr;

    // A cache of converted vertex data.
    std::vector<VertexConversionBuffer> mVertexConversionBuffers;

    // Tracks if BufferVk object has valid data or not.
    bool mHasValidData;

    // TODO: https://issuetracker.google.com/201826021 Remove this once we have a full fix.
    // Tracks if BufferVk's data is ever been referenced by GPU since new storage has been
    // allocated. Due to sub-allocation, we may get a new sub-allocated range in the same
    // BufferHelper object. Because we track GPU progress by the BufferHelper object, this flag will
    // help us to avoid detecting we are still GPU busy even though no one has used it yet since
    // we got last sub-allocation.
    bool mHasBeenReferencedByGPU;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_BUFFERVK_H_
