//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_wrapper:
//    Wrapper classes around Vulkan objects. In an ideal world we could generate this
//    from vk.xml. Or reuse the generator in the vkhpp tool. For now this is manually
//    generated and we must add missing functions and objects as we need them.

#ifndef LIBANGLE_RENDERER_VULKAN_VK_WRAPPER_H_
#define LIBANGLE_RENDERER_VULKAN_VK_WRAPPER_H_

#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/renderer_utils.h"
#include "libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.h"
#include "libANGLE/trace.h"

namespace rx
{
namespace vk
{
// Helper macros that apply to all the wrapped object types.
// Unimplemented handle types:
// Instance
// PhysicalDevice
// Device
// Queue
// DescriptorSet

#define ANGLE_HANDLE_TYPES_X(FUNC) \
    FUNC(Allocation)               \
    FUNC(Allocator)                \
    FUNC(Buffer)                   \
    FUNC(BufferView)               \
    FUNC(CommandPool)              \
    FUNC(DescriptorPool)           \
    FUNC(DescriptorSetLayout)      \
    FUNC(DeviceMemory)             \
    FUNC(Event)                    \
    FUNC(Fence)                    \
    FUNC(Framebuffer)              \
    FUNC(Image)                    \
    FUNC(ImageView)                \
    FUNC(Pipeline)                 \
    FUNC(PipelineCache)            \
    FUNC(PipelineLayout)           \
    FUNC(QueryPool)                \
    FUNC(RenderPass)               \
    FUNC(Sampler)                  \
    FUNC(SamplerYcbcrConversion)   \
    FUNC(Semaphore)                \
    FUNC(ShaderModule)

#define ANGLE_COMMA_SEP_FUNC(TYPE) TYPE,

enum class HandleType
{
    Invalid,
    CommandBuffer,
    ANGLE_HANDLE_TYPES_X(ANGLE_COMMA_SEP_FUNC) EnumCount
};

#undef ANGLE_COMMA_SEP_FUNC

#define ANGLE_PRE_DECLARE_CLASS_FUNC(TYPE) class TYPE;
ANGLE_HANDLE_TYPES_X(ANGLE_PRE_DECLARE_CLASS_FUNC)
namespace priv
{
class CommandBuffer;
}  // namespace priv
#undef ANGLE_PRE_DECLARE_CLASS_FUNC

// Returns the HandleType of a Vk Handle.
template <typename T>
struct HandleTypeHelper;

#define ANGLE_HANDLE_TYPE_HELPER_FUNC(TYPE)                         \
    template <>                                                     \
    struct HandleTypeHelper<TYPE>                                   \
    {                                                               \
        constexpr static HandleType kHandleType = HandleType::TYPE; \
    };

ANGLE_HANDLE_TYPES_X(ANGLE_HANDLE_TYPE_HELPER_FUNC)
template <>
struct HandleTypeHelper<priv::CommandBuffer>
{
    constexpr static HandleType kHandleType = HandleType::CommandBuffer;
};

#undef ANGLE_HANDLE_TYPE_HELPER_FUNC

// Base class for all wrapped vulkan objects. Implements several common helper routines.
template <typename DerivedT, typename HandleT>
class WrappedObject : angle::NonCopyable
{
  public:
    HandleT getHandle() const { return mHandle; }
    void setHandle(HandleT handle) { mHandle = handle; }
    bool valid() const { return (mHandle != VK_NULL_HANDLE); }

    const HandleT *ptr() const { return &mHandle; }

    HandleT release()
    {
        HandleT handle = mHandle;
        mHandle        = VK_NULL_HANDLE;
        return handle;
    }

  protected:
    WrappedObject() : mHandle(VK_NULL_HANDLE) {}
    ~WrappedObject() { ASSERT(!valid()); }

    WrappedObject(WrappedObject &&other) : mHandle(other.mHandle)
    {
        other.mHandle = VK_NULL_HANDLE;
    }

    // Only works to initialize empty objects, since we don't have the device handle.
    WrappedObject &operator=(WrappedObject &&other)
    {
        ASSERT(!valid());
        std::swap(mHandle, other.mHandle);
        return *this;
    }

    HandleT mHandle;
};

class CommandPool final : public WrappedObject<CommandPool, VkCommandPool>
{
  public:
    CommandPool() = default;

    void destroy(VkDevice device);
    VkResult reset(VkDevice device, VkCommandPoolResetFlags flags);
    void freeCommandBuffers(VkDevice device,
                            uint32_t commandBufferCount,
                            const VkCommandBuffer *commandBuffers);

    VkResult init(VkDevice device, const VkCommandPoolCreateInfo &createInfo);
};

class Pipeline final : public WrappedObject<Pipeline, VkPipeline>
{
  public:
    Pipeline() = default;
    void destroy(VkDevice device);

    VkResult initGraphics(VkDevice device,
                          const VkGraphicsPipelineCreateInfo &createInfo,
                          const PipelineCache &pipelineCacheVk);
    VkResult initCompute(VkDevice device,
                         const VkComputePipelineCreateInfo &createInfo,
                         const PipelineCache &pipelineCacheVk);
};

namespace priv
{

// Helper class that wraps a Vulkan command buffer.
class CommandBuffer : public WrappedObject<CommandBuffer, VkCommandBuffer>
{
  public:
    CommandBuffer() = default;

    VkCommandBuffer releaseHandle();

    // This is used for normal pool allocated command buffers. It reset the handle.
    void destroy(VkDevice device);

    // This is used in conjunction with VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT.
    void destroy(VkDevice device, const CommandPool &commandPool);

    VkResult init(VkDevice device, const VkCommandBufferAllocateInfo &createInfo);

    // There is no way to know if the command buffer contains any commands.
    static bool CanKnowIfEmpty() { return false; }
    bool empty() const { return false; }

    using WrappedObject::operator=;

    static bool SupportsQueries(const VkPhysicalDeviceFeatures &features)
    {
        return (features.inheritedQueries == VK_TRUE);
    }

    // Vulkan command buffers are executed as secondary command buffers within a primary command
    // buffer.
    static constexpr bool ExecutesInline() { return false; }

    VkResult begin(const VkCommandBufferBeginInfo &info);

    void beginQuery(const QueryPool &queryPool, uint32_t query, VkQueryControlFlags flags);

    void beginRenderPass(const VkRenderPassBeginInfo &beginInfo, VkSubpassContents subpassContents);

    void bindDescriptorSets(const PipelineLayout &layout,
                            VkPipelineBindPoint pipelineBindPoint,
                            uint32_t firstSet,
                            uint32_t descriptorSetCount,
                            const VkDescriptorSet *descriptorSets,
                            uint32_t dynamicOffsetCount,
                            const uint32_t *dynamicOffsets);
    void bindGraphicsPipeline(const Pipeline &pipeline);
    void bindComputePipeline(const Pipeline &pipeline);
    void bindPipeline(VkPipelineBindPoint pipelineBindPoint, const Pipeline &pipeline);

    void bindIndexBuffer(const Buffer &buffer, VkDeviceSize offset, VkIndexType indexType);
    void bindVertexBuffers(uint32_t firstBinding,
                           uint32_t bindingCount,
                           const VkBuffer *buffers,
                           const VkDeviceSize *offsets);

    void blitImage(const Image &srcImage,
                   VkImageLayout srcImageLayout,
                   const Image &dstImage,
                   VkImageLayout dstImageLayout,
                   uint32_t regionCount,
                   const VkImageBlit *regions,
                   VkFilter filter);

    void clearColorImage(const Image &image,
                         VkImageLayout imageLayout,
                         const VkClearColorValue &color,
                         uint32_t rangeCount,
                         const VkImageSubresourceRange *ranges);
    void clearDepthStencilImage(const Image &image,
                                VkImageLayout imageLayout,
                                const VkClearDepthStencilValue &depthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *ranges);

    void clearAttachments(uint32_t attachmentCount,
                          const VkClearAttachment *attachments,
                          uint32_t rectCount,
                          const VkClearRect *rects);

    void copyBuffer(const Buffer &srcBuffer,
                    const Buffer &destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *regions);

    void copyBufferToImage(VkBuffer srcBuffer,
                           const Image &dstImage,
                           VkImageLayout dstImageLayout,
                           uint32_t regionCount,
                           const VkBufferImageCopy *regions);
    void copyImageToBuffer(const Image &srcImage,
                           VkImageLayout srcImageLayout,
                           VkBuffer dstBuffer,
                           uint32_t regionCount,
                           const VkBufferImageCopy *regions);
    void copyImage(const Image &srcImage,
                   VkImageLayout srcImageLayout,
                   const Image &dstImage,
                   VkImageLayout dstImageLayout,
                   uint32_t regionCount,
                   const VkImageCopy *regions);

    void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void dispatchIndirect(const Buffer &buffer, VkDeviceSize offset);

    void draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance);
    void draw(uint32_t vertexCount, uint32_t firstVertex);
    void drawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex);
    void drawInstancedBaseInstance(uint32_t vertexCount,
                                   uint32_t instanceCount,
                                   uint32_t firstVertex,
                                   uint32_t firstInstance);
    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance);
    void drawIndexed(uint32_t indexCount);
    void drawIndexedBaseVertex(uint32_t indexCount, uint32_t vertexOffset);
    void drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount);
    void drawIndexedInstancedBaseVertex(uint32_t indexCount,
                                        uint32_t instanceCount,
                                        uint32_t vertexOffset);
    void drawIndexedInstancedBaseVertexBaseInstance(uint32_t indexCount,
                                                    uint32_t instanceCount,
                                                    uint32_t firstIndex,
                                                    int32_t vertexOffset,
                                                    uint32_t firstInstance);
    void drawIndexedIndirect(const Buffer &buffer,
                             VkDeviceSize offset,
                             uint32_t drawCount,
                             uint32_t stride);
    void drawIndirect(const Buffer &buffer,
                      VkDeviceSize offset,
                      uint32_t drawCount,
                      uint32_t stride);

    VkResult end();
    void endQuery(const QueryPool &queryPool, uint32_t query);
    void endRenderPass();
    void executeCommands(uint32_t commandBufferCount, const CommandBuffer *commandBuffers);

    void getMemoryUsageStats(size_t *usedMemoryOut, size_t *allocatedMemoryOut) const;

    void executionBarrier(VkPipelineStageFlags stageMask);

    void fillBuffer(const Buffer &dstBuffer,
                    VkDeviceSize dstOffset,
                    VkDeviceSize size,
                    uint32_t data);

    void bufferBarrier(VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       const VkBufferMemoryBarrier *bufferMemoryBarrier);

    void imageBarrier(VkPipelineStageFlags srcStageMask,
                      VkPipelineStageFlags dstStageMask,
                      const VkImageMemoryBarrier &imageMemoryBarrier);

    void memoryBarrier(VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       const VkMemoryBarrier *memoryBarrier);

    void pipelineBarrier(VkPipelineStageFlags srcStageMask,
                         VkPipelineStageFlags dstStageMask,
                         VkDependencyFlags dependencyFlags,
                         uint32_t memoryBarrierCount,
                         const VkMemoryBarrier *memoryBarriers,
                         uint32_t bufferMemoryBarrierCount,
                         const VkBufferMemoryBarrier *bufferMemoryBarriers,
                         uint32_t imageMemoryBarrierCount,
                         const VkImageMemoryBarrier *imageMemoryBarriers);

    void pushConstants(const PipelineLayout &layout,
                       VkShaderStageFlags flag,
                       uint32_t offset,
                       uint32_t size,
                       const void *data);

    void setEvent(VkEvent event, VkPipelineStageFlags stageMask);
    VkResult reset();
    void resetEvent(VkEvent event, VkPipelineStageFlags stageMask);
    void resetQueryPool(const QueryPool &queryPool, uint32_t firstQuery, uint32_t queryCount);
    void resolveImage(const Image &srcImage,
                      VkImageLayout srcImageLayout,
                      const Image &dstImage,
                      VkImageLayout dstImageLayout,
                      uint32_t regionCount,
                      const VkImageResolve *regions);
    void waitEvents(uint32_t eventCount,
                    const VkEvent *events,
                    VkPipelineStageFlags srcStageMask,
                    VkPipelineStageFlags dstStageMask,
                    uint32_t memoryBarrierCount,
                    const VkMemoryBarrier *memoryBarriers,
                    uint32_t bufferMemoryBarrierCount,
                    const VkBufferMemoryBarrier *bufferMemoryBarriers,
                    uint32_t imageMemoryBarrierCount,
                    const VkImageMemoryBarrier *imageMemoryBarriers);

    void writeTimestamp(VkPipelineStageFlagBits pipelineStage,
                        const QueryPool &queryPool,
                        uint32_t query);

    // VK_EXT_transform_feedback
    void beginTransformFeedbackEXT(uint32_t firstCounterBuffer,
                                   uint32_t counterBufferCount,
                                   const VkBuffer *counterBuffers,
                                   const VkDeviceSize *counterBufferOffsets);
    void endTransformFeedbackEXT(uint32_t firstCounterBuffer,
                                 uint32_t counterBufferCount,
                                 const VkBuffer *counterBuffers,
                                 const VkDeviceSize *counterBufferOffsets);
    void bindTransformFeedbackBuffersEXT(uint32_t firstBinding,
                                         uint32_t bindingCount,
                                         const VkBuffer *buffers,
                                         const VkDeviceSize *offsets,
                                         const VkDeviceSize *sizes);

    // VK_EXT_debug_utils
    void beginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &labelInfo);
    void endDebugUtilsLabelEXT();
    void insertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &labelInfo);
};
}  // namespace priv

class Image final : public WrappedObject<Image, VkImage>
{
  public:
    Image() = default;

    // Use this method if the lifetime of the image is not controlled by ANGLE. (SwapChain)
    void setHandle(VkImage handle);

    // Called on shutdown when the helper class *doesn't* own the handle to the image resource.
    void reset();

    // Called on shutdown when the helper class *does* own the handle to the image resource.
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkImageCreateInfo &createInfo);

    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *requirementsOut) const;
    VkResult bindMemory(VkDevice device, const DeviceMemory &deviceMemory);

    void getSubresourceLayout(VkDevice device,
                              VkImageAspectFlagBits aspectMask,
                              uint32_t mipLevel,
                              uint32_t arrayLayer,
                              VkSubresourceLayout *outSubresourceLayout) const;
};

class ImageView final : public WrappedObject<ImageView, VkImageView>
{
  public:
    ImageView() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkImageViewCreateInfo &createInfo);
};

class Semaphore final : public WrappedObject<Semaphore, VkSemaphore>
{
  public:
    Semaphore() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device);
    VkResult init(VkDevice device, const VkSemaphoreCreateInfo &createInfo);
    VkResult importFd(VkDevice device, const VkImportSemaphoreFdInfoKHR &importFdInfo) const;
};

class Framebuffer final : public WrappedObject<Framebuffer, VkFramebuffer>
{
  public:
    Framebuffer() = default;
    void destroy(VkDevice device);

    // Use this method only in necessary cases. (RenderPass)
    void setHandle(VkFramebuffer handle);

    VkResult init(VkDevice device, const VkFramebufferCreateInfo &createInfo);
};

class DeviceMemory final : public WrappedObject<DeviceMemory, VkDeviceMemory>
{
  public:
    DeviceMemory() = default;
    void destroy(VkDevice device);

    VkResult allocate(VkDevice device, const VkMemoryAllocateInfo &allocInfo);
    VkResult map(VkDevice device,
                 VkDeviceSize offset,
                 VkDeviceSize size,
                 VkMemoryMapFlags flags,
                 uint8_t **mapPointer) const;
    void unmap(VkDevice device) const;
};

class Allocator : public WrappedObject<Allocator, VmaAllocator>
{
  public:
    Allocator() = default;
    void destroy();

    VkResult init(VkPhysicalDevice physicalDevice,
                  VkDevice device,
                  VkInstance instance,
                  uint32_t apiVersion);

    // Initializes the buffer handle and memory allocation.
    VkResult createBuffer(const VkBufferCreateInfo &bufferCreateInfo,
                          VkMemoryPropertyFlags requiredFlags,
                          VkMemoryPropertyFlags preferredFlags,
                          bool persistentlyMappedBuffers,
                          uint32_t *memoryTypeIndexOut,
                          Buffer *bufferOut,
                          Allocation *allocationOut) const;

    void getMemoryTypeProperties(uint32_t memoryTypeIndex, VkMemoryPropertyFlags *flagsOut) const;
    VkResult findMemoryTypeIndexForBufferInfo(const VkBufferCreateInfo &bufferCreateInfo,
                                              VkMemoryPropertyFlags requiredFlags,
                                              VkMemoryPropertyFlags preferredFlags,
                                              bool persistentlyMappedBuffers,
                                              uint32_t *memoryTypeIndexOut) const;
};

class Allocation final : public WrappedObject<Allocation, VmaAllocation>
{
  public:
    Allocation() = default;
    void destroy(const Allocator &allocator);

    VkResult map(const Allocator &allocator, uint8_t **mapPointer) const;
    void unmap(const Allocator &allocator) const;
    void flush(const Allocator &allocator, VkDeviceSize offset, VkDeviceSize size);
    void invalidate(const Allocator &allocator, VkDeviceSize offset, VkDeviceSize size);

  private:
    friend class Allocator;
};

class RenderPass final : public WrappedObject<RenderPass, VkRenderPass>
{
  public:
    RenderPass() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkRenderPassCreateInfo &createInfo);
};

enum class StagingUsage
{
    Read,
    Write,
    Both,
};

class Buffer final : public WrappedObject<Buffer, VkBuffer>
{
  public:
    Buffer() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkBufferCreateInfo &createInfo);
    VkResult bindMemory(VkDevice device, const DeviceMemory &deviceMemory);
    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *memoryRequirementsOut);

  private:
    friend class Allocator;
};

class BufferView final : public WrappedObject<BufferView, VkBufferView>
{
  public:
    BufferView() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkBufferViewCreateInfo &createInfo);
};

class ShaderModule final : public WrappedObject<ShaderModule, VkShaderModule>
{
  public:
    ShaderModule() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkShaderModuleCreateInfo &createInfo);
};

class PipelineLayout final : public WrappedObject<PipelineLayout, VkPipelineLayout>
{
  public:
    PipelineLayout() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkPipelineLayoutCreateInfo &createInfo);
};

class PipelineCache final : public WrappedObject<PipelineCache, VkPipelineCache>
{
  public:
    PipelineCache() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkPipelineCacheCreateInfo &createInfo);
    VkResult getCacheData(VkDevice device, size_t *cacheSize, void *cacheData);
    VkResult merge(VkDevice device,
                   VkPipelineCache dstCache,
                   uint32_t srcCacheCount,
                   const VkPipelineCache *srcCaches);
};

class DescriptorSetLayout final : public WrappedObject<DescriptorSetLayout, VkDescriptorSetLayout>
{
  public:
    DescriptorSetLayout() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkDescriptorSetLayoutCreateInfo &createInfo);
};

class DescriptorPool final : public WrappedObject<DescriptorPool, VkDescriptorPool>
{
  public:
    DescriptorPool() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkDescriptorPoolCreateInfo &createInfo);

    VkResult allocateDescriptorSets(VkDevice device,
                                    const VkDescriptorSetAllocateInfo &allocInfo,
                                    VkDescriptorSet *descriptorSetsOut);
    VkResult freeDescriptorSets(VkDevice device,
                                uint32_t descriptorSetCount,
                                const VkDescriptorSet *descriptorSets);
};

class Sampler final : public WrappedObject<Sampler, VkSampler>
{
  public:
    Sampler() = default;
    void destroy(VkDevice device);
    VkResult init(VkDevice device, const VkSamplerCreateInfo &createInfo);
};

class SamplerYcbcrConversion final
    : public WrappedObject<SamplerYcbcrConversion, VkSamplerYcbcrConversion>
{
  public:
    SamplerYcbcrConversion() = default;
    void destroy(VkDevice device);
    VkResult init(VkDevice device, const VkSamplerYcbcrConversionCreateInfo &createInfo);
};

class Event final : public WrappedObject<Event, VkEvent>
{
  public:
    Event() = default;
    void destroy(VkDevice device);
    using WrappedObject::operator=;

    VkResult init(VkDevice device, const VkEventCreateInfo &createInfo);
    VkResult getStatus(VkDevice device) const;
    VkResult set(VkDevice device) const;
    VkResult reset(VkDevice device) const;
};

class Fence final : public WrappedObject<Fence, VkFence>
{
  public:
    Fence() = default;
    void destroy(VkDevice device);
    using WrappedObject::operator=;

    VkResult init(VkDevice device, const VkFenceCreateInfo &createInfo);
    VkResult reset(VkDevice device);
    VkResult getStatus(VkDevice device) const;
    VkResult wait(VkDevice device, uint64_t timeout) const;
    VkResult importFd(VkDevice device, const VkImportFenceFdInfoKHR &importFenceFdInfo) const;
    VkResult exportFd(VkDevice device, const VkFenceGetFdInfoKHR &fenceGetFdInfo, int *outFd) const;
};

class QueryPool final : public WrappedObject<QueryPool, VkQueryPool>
{
  public:
    QueryPool() = default;
    void destroy(VkDevice device);

    VkResult init(VkDevice device, const VkQueryPoolCreateInfo &createInfo);
    VkResult getResults(VkDevice device,
                        uint32_t firstQuery,
                        uint32_t queryCount,
                        size_t dataSize,
                        void *data,
                        VkDeviceSize stride,
                        VkQueryResultFlags flags) const;
};

// CommandPool implementation.
ANGLE_INLINE void CommandPool::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyCommandPool(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult CommandPool::reset(VkDevice device, VkCommandPoolResetFlags flags)
{
    ASSERT(valid());
    return vkResetCommandPool(device, mHandle, flags);
}

ANGLE_INLINE void CommandPool::freeCommandBuffers(VkDevice device,
                                                  uint32_t commandBufferCount,
                                                  const VkCommandBuffer *commandBuffers)
{
    ASSERT(valid());
    vkFreeCommandBuffers(device, mHandle, commandBufferCount, commandBuffers);
}

ANGLE_INLINE VkResult CommandPool::init(VkDevice device, const VkCommandPoolCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateCommandPool(device, &createInfo, nullptr, &mHandle);
}

namespace priv
{

// CommandBuffer implementation.
ANGLE_INLINE VkCommandBuffer CommandBuffer::releaseHandle()
{
    VkCommandBuffer handle = mHandle;
    mHandle                = nullptr;
    return handle;
}

ANGLE_INLINE VkResult CommandBuffer::init(VkDevice device,
                                          const VkCommandBufferAllocateInfo &createInfo)
{
    ASSERT(!valid());
    return vkAllocateCommandBuffers(device, &createInfo, &mHandle);
}

ANGLE_INLINE void CommandBuffer::blitImage(const Image &srcImage,
                                           VkImageLayout srcImageLayout,
                                           const Image &dstImage,
                                           VkImageLayout dstImageLayout,
                                           uint32_t regionCount,
                                           const VkImageBlit *regions,
                                           VkFilter filter)
{
    ASSERT(valid() && srcImage.valid() && dstImage.valid());
    ASSERT(regionCount == 1);
    vkCmdBlitImage(mHandle, srcImage.getHandle(), srcImageLayout, dstImage.getHandle(),
                   dstImageLayout, 1, regions, filter);
}

ANGLE_INLINE VkResult CommandBuffer::begin(const VkCommandBufferBeginInfo &info)
{
    ASSERT(valid());
    return vkBeginCommandBuffer(mHandle, &info);
}

ANGLE_INLINE VkResult CommandBuffer::end()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandBuffer::end");
    ASSERT(valid());
    return vkEndCommandBuffer(mHandle);
}

ANGLE_INLINE VkResult CommandBuffer::reset()
{
    ASSERT(valid());
    return vkResetCommandBuffer(mHandle, 0);
}

ANGLE_INLINE void CommandBuffer::memoryBarrier(VkPipelineStageFlags srcStageMask,
                                               VkPipelineStageFlags dstStageMask,
                                               const VkMemoryBarrier *memoryBarrier)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, 0, 1, memoryBarrier, 0, nullptr, 0,
                         nullptr);
}

ANGLE_INLINE void CommandBuffer::pipelineBarrier(VkPipelineStageFlags srcStageMask,
                                                 VkPipelineStageFlags dstStageMask,
                                                 VkDependencyFlags dependencyFlags,
                                                 uint32_t memoryBarrierCount,
                                                 const VkMemoryBarrier *memoryBarriers,
                                                 uint32_t bufferMemoryBarrierCount,
                                                 const VkBufferMemoryBarrier *bufferMemoryBarriers,
                                                 uint32_t imageMemoryBarrierCount,
                                                 const VkImageMemoryBarrier *imageMemoryBarriers)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount,
                         memoryBarriers, bufferMemoryBarrierCount, bufferMemoryBarriers,
                         imageMemoryBarrierCount, imageMemoryBarriers);
}

ANGLE_INLINE void CommandBuffer::executionBarrier(VkPipelineStageFlags stageMask)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, stageMask, stageMask, 0, 0, nullptr, 0, nullptr, 0, nullptr);
}

ANGLE_INLINE void CommandBuffer::bufferBarrier(VkPipelineStageFlags srcStageMask,
                                               VkPipelineStageFlags dstStageMask,
                                               const VkBufferMemoryBarrier *bufferMemoryBarrier)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, 0, 0, nullptr, 1, bufferMemoryBarrier,
                         0, nullptr);
}

ANGLE_INLINE void CommandBuffer::imageBarrier(VkPipelineStageFlags srcStageMask,
                                              VkPipelineStageFlags dstStageMask,
                                              const VkImageMemoryBarrier &imageMemoryBarrier)
{
    ASSERT(valid());
    vkCmdPipelineBarrier(mHandle, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);
}

ANGLE_INLINE void CommandBuffer::destroy(VkDevice device)
{
    releaseHandle();
}

ANGLE_INLINE void CommandBuffer::destroy(VkDevice device, const vk::CommandPool &commandPool)
{
    if (valid())
    {
        ASSERT(commandPool.valid());
        vkFreeCommandBuffers(device, commandPool.getHandle(), 1, &mHandle);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE void CommandBuffer::copyBuffer(const Buffer &srcBuffer,
                                            const Buffer &destBuffer,
                                            uint32_t regionCount,
                                            const VkBufferCopy *regions)
{
    ASSERT(valid() && srcBuffer.valid() && destBuffer.valid());
    vkCmdCopyBuffer(mHandle, srcBuffer.getHandle(), destBuffer.getHandle(), regionCount, regions);
}

ANGLE_INLINE void CommandBuffer::copyBufferToImage(VkBuffer srcBuffer,
                                                   const Image &dstImage,
                                                   VkImageLayout dstImageLayout,
                                                   uint32_t regionCount,
                                                   const VkBufferImageCopy *regions)
{
    ASSERT(valid() && dstImage.valid());
    ASSERT(srcBuffer != VK_NULL_HANDLE);
    ASSERT(regionCount == 1);
    vkCmdCopyBufferToImage(mHandle, srcBuffer, dstImage.getHandle(), dstImageLayout, 1, regions);
}

ANGLE_INLINE void CommandBuffer::copyImageToBuffer(const Image &srcImage,
                                                   VkImageLayout srcImageLayout,
                                                   VkBuffer dstBuffer,
                                                   uint32_t regionCount,
                                                   const VkBufferImageCopy *regions)
{
    ASSERT(valid() && srcImage.valid());
    ASSERT(dstBuffer != VK_NULL_HANDLE);
    ASSERT(regionCount == 1);
    vkCmdCopyImageToBuffer(mHandle, srcImage.getHandle(), srcImageLayout, dstBuffer, 1, regions);
}

ANGLE_INLINE void CommandBuffer::clearColorImage(const Image &image,
                                                 VkImageLayout imageLayout,
                                                 const VkClearColorValue &color,
                                                 uint32_t rangeCount,
                                                 const VkImageSubresourceRange *ranges)
{
    ASSERT(valid());
    ASSERT(rangeCount == 1);
    vkCmdClearColorImage(mHandle, image.getHandle(), imageLayout, &color, 1, ranges);
}

ANGLE_INLINE void CommandBuffer::clearDepthStencilImage(
    const Image &image,
    VkImageLayout imageLayout,
    const VkClearDepthStencilValue &depthStencil,
    uint32_t rangeCount,
    const VkImageSubresourceRange *ranges)
{
    ASSERT(valid());
    ASSERT(rangeCount == 1);
    vkCmdClearDepthStencilImage(mHandle, image.getHandle(), imageLayout, &depthStencil, 1, ranges);
}

ANGLE_INLINE void CommandBuffer::clearAttachments(uint32_t attachmentCount,
                                                  const VkClearAttachment *attachments,
                                                  uint32_t rectCount,
                                                  const VkClearRect *rects)
{
    ASSERT(valid());
    vkCmdClearAttachments(mHandle, attachmentCount, attachments, rectCount, rects);
}

ANGLE_INLINE void CommandBuffer::copyImage(const Image &srcImage,
                                           VkImageLayout srcImageLayout,
                                           const Image &dstImage,
                                           VkImageLayout dstImageLayout,
                                           uint32_t regionCount,
                                           const VkImageCopy *regions)
{
    ASSERT(valid() && srcImage.valid() && dstImage.valid());
    ASSERT(regionCount == 1);
    vkCmdCopyImage(mHandle, srcImage.getHandle(), srcImageLayout, dstImage.getHandle(),
                   dstImageLayout, 1, regions);
}

ANGLE_INLINE void CommandBuffer::beginRenderPass(const VkRenderPassBeginInfo &beginInfo,
                                                 VkSubpassContents subpassContents)
{
    ASSERT(valid());
    vkCmdBeginRenderPass(mHandle, &beginInfo, subpassContents);
}

ANGLE_INLINE void CommandBuffer::endRenderPass()
{
    ASSERT(mHandle != VK_NULL_HANDLE);
    vkCmdEndRenderPass(mHandle);
}

ANGLE_INLINE void CommandBuffer::bindIndexBuffer(const Buffer &buffer,
                                                 VkDeviceSize offset,
                                                 VkIndexType indexType)
{
    ASSERT(valid());
    vkCmdBindIndexBuffer(mHandle, buffer.getHandle(), offset, indexType);
}

ANGLE_INLINE void CommandBuffer::bindDescriptorSets(const PipelineLayout &layout,
                                                    VkPipelineBindPoint pipelineBindPoint,
                                                    uint32_t firstSet,
                                                    uint32_t descriptorSetCount,
                                                    const VkDescriptorSet *descriptorSets,
                                                    uint32_t dynamicOffsetCount,
                                                    const uint32_t *dynamicOffsets)
{
    ASSERT(valid() && layout.valid());
    vkCmdBindDescriptorSets(mHandle, pipelineBindPoint, layout.getHandle(), firstSet,
                            descriptorSetCount, descriptorSets, dynamicOffsetCount, dynamicOffsets);
}

ANGLE_INLINE void CommandBuffer::executeCommands(uint32_t commandBufferCount,
                                                 const CommandBuffer *commandBuffers)
{
    ASSERT(valid());
    vkCmdExecuteCommands(mHandle, commandBufferCount, commandBuffers[0].ptr());
}

ANGLE_INLINE void CommandBuffer::getMemoryUsageStats(size_t *usedMemoryOut,
                                                     size_t *allocatedMemoryOut) const
{
    // No data available.
    *usedMemoryOut      = 0;
    *allocatedMemoryOut = 1;
}

ANGLE_INLINE void CommandBuffer::fillBuffer(const Buffer &dstBuffer,
                                            VkDeviceSize dstOffset,
                                            VkDeviceSize size,
                                            uint32_t data)
{
    ASSERT(valid());
    vkCmdFillBuffer(mHandle, dstBuffer.getHandle(), dstOffset, size, data);
}

ANGLE_INLINE void CommandBuffer::pushConstants(const PipelineLayout &layout,
                                               VkShaderStageFlags flag,
                                               uint32_t offset,
                                               uint32_t size,
                                               const void *data)
{
    ASSERT(valid() && layout.valid());
    ASSERT(offset == 0);
    vkCmdPushConstants(mHandle, layout.getHandle(), flag, 0, size, data);
}

ANGLE_INLINE void CommandBuffer::setEvent(VkEvent event, VkPipelineStageFlags stageMask)
{
    ASSERT(valid() && event != VK_NULL_HANDLE);
    vkCmdSetEvent(mHandle, event, stageMask);
}

ANGLE_INLINE void CommandBuffer::resetEvent(VkEvent event, VkPipelineStageFlags stageMask)
{
    ASSERT(valid() && event != VK_NULL_HANDLE);
    vkCmdResetEvent(mHandle, event, stageMask);
}

ANGLE_INLINE void CommandBuffer::waitEvents(uint32_t eventCount,
                                            const VkEvent *events,
                                            VkPipelineStageFlags srcStageMask,
                                            VkPipelineStageFlags dstStageMask,
                                            uint32_t memoryBarrierCount,
                                            const VkMemoryBarrier *memoryBarriers,
                                            uint32_t bufferMemoryBarrierCount,
                                            const VkBufferMemoryBarrier *bufferMemoryBarriers,
                                            uint32_t imageMemoryBarrierCount,
                                            const VkImageMemoryBarrier *imageMemoryBarriers)
{
    ASSERT(valid());
    vkCmdWaitEvents(mHandle, eventCount, events, srcStageMask, dstStageMask, memoryBarrierCount,
                    memoryBarriers, bufferMemoryBarrierCount, bufferMemoryBarriers,
                    imageMemoryBarrierCount, imageMemoryBarriers);
}

ANGLE_INLINE void CommandBuffer::resetQueryPool(const QueryPool &queryPool,
                                                uint32_t firstQuery,
                                                uint32_t queryCount)
{
    ASSERT(valid() && queryPool.valid());
    vkCmdResetQueryPool(mHandle, queryPool.getHandle(), firstQuery, queryCount);
}

ANGLE_INLINE void CommandBuffer::resolveImage(const Image &srcImage,
                                              VkImageLayout srcImageLayout,
                                              const Image &dstImage,
                                              VkImageLayout dstImageLayout,
                                              uint32_t regionCount,
                                              const VkImageResolve *regions)
{
    ASSERT(valid() && srcImage.valid() && dstImage.valid());
    vkCmdResolveImage(mHandle, srcImage.getHandle(), srcImageLayout, dstImage.getHandle(),
                      dstImageLayout, regionCount, regions);
}

ANGLE_INLINE void CommandBuffer::beginQuery(const QueryPool &queryPool,
                                            uint32_t query,
                                            VkQueryControlFlags flags)
{
    ASSERT(valid() && queryPool.valid());
    vkCmdBeginQuery(mHandle, queryPool.getHandle(), query, flags);
}

ANGLE_INLINE void CommandBuffer::endQuery(const QueryPool &queryPool, uint32_t query)
{
    ASSERT(valid() && queryPool.valid());
    vkCmdEndQuery(mHandle, queryPool.getHandle(), query);
}

ANGLE_INLINE void CommandBuffer::writeTimestamp(VkPipelineStageFlagBits pipelineStage,
                                                const QueryPool &queryPool,
                                                uint32_t query)
{
    ASSERT(valid());
    vkCmdWriteTimestamp(mHandle, pipelineStage, queryPool.getHandle(), query);
}

ANGLE_INLINE void CommandBuffer::draw(uint32_t vertexCount,
                                      uint32_t instanceCount,
                                      uint32_t firstVertex,
                                      uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

ANGLE_INLINE void CommandBuffer::draw(uint32_t vertexCount, uint32_t firstVertex)
{
    ASSERT(valid());
    vkCmdDraw(mHandle, vertexCount, 1, firstVertex, 0);
}

ANGLE_INLINE void CommandBuffer::drawInstanced(uint32_t vertexCount,
                                               uint32_t instanceCount,
                                               uint32_t firstVertex)
{
    ASSERT(valid());
    vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, 0);
}

ANGLE_INLINE void CommandBuffer::drawInstancedBaseInstance(uint32_t vertexCount,
                                                           uint32_t instanceCount,
                                                           uint32_t firstVertex,
                                                           uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

ANGLE_INLINE void CommandBuffer::drawIndexed(uint32_t indexCount,
                                             uint32_t instanceCount,
                                             uint32_t firstIndex,
                                             int32_t vertexOffset,
                                             uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

ANGLE_INLINE void CommandBuffer::drawIndexed(uint32_t indexCount)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, 1, 0, 0, 0);
}

ANGLE_INLINE void CommandBuffer::drawIndexedBaseVertex(uint32_t indexCount, uint32_t vertexOffset)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, 1, 0, vertexOffset, 0);
}

ANGLE_INLINE void CommandBuffer::drawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, instanceCount, 0, 0, 0);
}

ANGLE_INLINE void CommandBuffer::drawIndexedInstancedBaseVertex(uint32_t indexCount,
                                                                uint32_t instanceCount,
                                                                uint32_t vertexOffset)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, instanceCount, 0, vertexOffset, 0);
}

ANGLE_INLINE void CommandBuffer::drawIndexedInstancedBaseVertexBaseInstance(uint32_t indexCount,
                                                                            uint32_t instanceCount,
                                                                            uint32_t firstIndex,
                                                                            int32_t vertexOffset,
                                                                            uint32_t firstInstance)
{
    ASSERT(valid());
    vkCmdDrawIndexed(mHandle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

ANGLE_INLINE void CommandBuffer::drawIndexedIndirect(const Buffer &buffer,
                                                     VkDeviceSize offset,
                                                     uint32_t drawCount,
                                                     uint32_t stride)
{
    ASSERT(valid());
    vkCmdDrawIndexedIndirect(mHandle, buffer.getHandle(), offset, drawCount, stride);
}

ANGLE_INLINE void CommandBuffer::drawIndirect(const Buffer &buffer,
                                              VkDeviceSize offset,
                                              uint32_t drawCount,
                                              uint32_t stride)
{
    ASSERT(valid());
    vkCmdDrawIndirect(mHandle, buffer.getHandle(), offset, drawCount, stride);
}

ANGLE_INLINE void CommandBuffer::dispatch(uint32_t groupCountX,
                                          uint32_t groupCountY,
                                          uint32_t groupCountZ)
{
    ASSERT(valid());
    vkCmdDispatch(mHandle, groupCountX, groupCountY, groupCountZ);
}

ANGLE_INLINE void CommandBuffer::dispatchIndirect(const Buffer &buffer, VkDeviceSize offset)
{
    ASSERT(valid());
    vkCmdDispatchIndirect(mHandle, buffer.getHandle(), offset);
}

ANGLE_INLINE void CommandBuffer::bindPipeline(VkPipelineBindPoint pipelineBindPoint,
                                              const Pipeline &pipeline)
{
    ASSERT(valid() && pipeline.valid());
    vkCmdBindPipeline(mHandle, pipelineBindPoint, pipeline.getHandle());
}

ANGLE_INLINE void CommandBuffer::bindGraphicsPipeline(const Pipeline &pipeline)
{
    ASSERT(valid() && pipeline.valid());
    vkCmdBindPipeline(mHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getHandle());
}

ANGLE_INLINE void CommandBuffer::bindComputePipeline(const Pipeline &pipeline)
{
    ASSERT(valid() && pipeline.valid());
    vkCmdBindPipeline(mHandle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getHandle());
}

ANGLE_INLINE void CommandBuffer::bindVertexBuffers(uint32_t firstBinding,
                                                   uint32_t bindingCount,
                                                   const VkBuffer *buffers,
                                                   const VkDeviceSize *offsets)
{
    ASSERT(valid());
    vkCmdBindVertexBuffers(mHandle, firstBinding, bindingCount, buffers, offsets);
}

ANGLE_INLINE void CommandBuffer::beginTransformFeedbackEXT(uint32_t firstCounterBuffer,
                                                           uint32_t counterBufferCount,
                                                           const VkBuffer *counterBuffers,
                                                           const VkDeviceSize *counterBufferOffsets)
{
    ASSERT(valid());
    ASSERT(vkCmdBeginTransformFeedbackEXT);
    vkCmdBeginTransformFeedbackEXT(mHandle, firstCounterBuffer, counterBufferCount, counterBuffers,
                                   counterBufferOffsets);
}

ANGLE_INLINE void CommandBuffer::endTransformFeedbackEXT(uint32_t firstCounterBuffer,
                                                         uint32_t counterBufferCount,
                                                         const VkBuffer *counterBuffers,
                                                         const VkDeviceSize *counterBufferOffsets)
{
    ASSERT(valid());
    ASSERT(vkCmdEndTransformFeedbackEXT);
    vkCmdEndTransformFeedbackEXT(mHandle, firstCounterBuffer, counterBufferCount, counterBuffers,
                                 counterBufferOffsets);
}

ANGLE_INLINE void CommandBuffer::bindTransformFeedbackBuffersEXT(uint32_t firstBinding,
                                                                 uint32_t bindingCount,
                                                                 const VkBuffer *buffers,
                                                                 const VkDeviceSize *offsets,
                                                                 const VkDeviceSize *sizes)
{
    ASSERT(valid());
    ASSERT(vkCmdBindTransformFeedbackBuffersEXT);
    vkCmdBindTransformFeedbackBuffersEXT(mHandle, firstBinding, bindingCount, buffers, offsets,
                                         sizes);
}

ANGLE_INLINE void CommandBuffer::beginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &labelInfo)
{
    ASSERT(valid());
    {
#if !defined(ANGLE_SHARED_LIBVULKAN)
        // When the vulkan-loader is statically linked, we need to use the extension
        // functions defined in ANGLE's rx namespace. When it's dynamically linked
        // with volk, this will default to the function definitions with no namespace
        using rx::vkCmdBeginDebugUtilsLabelEXT;
#endif  // !defined(ANGLE_SHARED_LIBVULKAN)
        ASSERT(vkCmdBeginDebugUtilsLabelEXT);
        vkCmdBeginDebugUtilsLabelEXT(mHandle, &labelInfo);
    }
}

ANGLE_INLINE void CommandBuffer::endDebugUtilsLabelEXT()
{
    ASSERT(valid());
    ASSERT(vkCmdEndDebugUtilsLabelEXT);
    vkCmdEndDebugUtilsLabelEXT(mHandle);
}

ANGLE_INLINE void CommandBuffer::insertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &labelInfo)
{
    ASSERT(valid());
    ASSERT(vkCmdInsertDebugUtilsLabelEXT);
    vkCmdInsertDebugUtilsLabelEXT(mHandle, &labelInfo);
}
}  // namespace priv

// Image implementation.
ANGLE_INLINE void Image::setHandle(VkImage handle)
{
    mHandle = handle;
}

ANGLE_INLINE void Image::reset()
{
    mHandle = VK_NULL_HANDLE;
}

ANGLE_INLINE void Image::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyImage(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Image::init(VkDevice device, const VkImageCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateImage(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE void Image::getMemoryRequirements(VkDevice device,
                                               VkMemoryRequirements *requirementsOut) const
{
    ASSERT(valid());
    vkGetImageMemoryRequirements(device, mHandle, requirementsOut);
}

ANGLE_INLINE VkResult Image::bindMemory(VkDevice device, const vk::DeviceMemory &deviceMemory)
{
    ASSERT(valid() && deviceMemory.valid());
    return vkBindImageMemory(device, mHandle, deviceMemory.getHandle(), 0);
}

ANGLE_INLINE void Image::getSubresourceLayout(VkDevice device,
                                              VkImageAspectFlagBits aspectMask,
                                              uint32_t mipLevel,
                                              uint32_t arrayLayer,
                                              VkSubresourceLayout *outSubresourceLayout) const
{
    VkImageSubresource subresource = {};
    subresource.aspectMask         = aspectMask;
    subresource.mipLevel           = mipLevel;
    subresource.arrayLayer         = arrayLayer;

    vkGetImageSubresourceLayout(device, getHandle(), &subresource, outSubresourceLayout);
}

// ImageView implementation.
ANGLE_INLINE void ImageView::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyImageView(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult ImageView::init(VkDevice device, const VkImageViewCreateInfo &createInfo)
{
    return vkCreateImageView(device, &createInfo, nullptr, &mHandle);
}

// Semaphore implementation.
ANGLE_INLINE void Semaphore::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroySemaphore(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Semaphore::init(VkDevice device)
{
    ASSERT(!valid());

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.flags                 = 0;

    return vkCreateSemaphore(device, &semaphoreInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult Semaphore::init(VkDevice device, const VkSemaphoreCreateInfo &createInfo)
{
    ASSERT(valid());
    return vkCreateSemaphore(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult Semaphore::importFd(VkDevice device,
                                          const VkImportSemaphoreFdInfoKHR &importFdInfo) const
{
    ASSERT(valid());
    return vkImportSemaphoreFdKHR(device, &importFdInfo);
}

// Framebuffer implementation.
ANGLE_INLINE void Framebuffer::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyFramebuffer(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Framebuffer::init(VkDevice device, const VkFramebufferCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateFramebuffer(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE void Framebuffer::setHandle(VkFramebuffer handle)
{
    mHandle = handle;
}

// DeviceMemory implementation.
ANGLE_INLINE void DeviceMemory::destroy(VkDevice device)
{
    if (valid())
    {
        vkFreeMemory(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult DeviceMemory::allocate(VkDevice device, const VkMemoryAllocateInfo &allocInfo)
{
    ASSERT(!valid());
    return vkAllocateMemory(device, &allocInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult DeviceMemory::map(VkDevice device,
                                        VkDeviceSize offset,
                                        VkDeviceSize size,
                                        VkMemoryMapFlags flags,
                                        uint8_t **mapPointer) const
{
    ANGLE_TRACE_EVENT0("gpu.angle", "DeviceMemory::map");
    ASSERT(valid());
    return vkMapMemory(device, mHandle, offset, size, flags, reinterpret_cast<void **>(mapPointer));
}

ANGLE_INLINE void DeviceMemory::unmap(VkDevice device) const
{
    ASSERT(valid());
    vkUnmapMemory(device, mHandle);
}

// Allocator implementation.
ANGLE_INLINE void Allocator::destroy()
{
    if (valid())
    {
        vma::DestroyAllocator(mHandle);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Allocator::init(VkPhysicalDevice physicalDevice,
                                      VkDevice device,
                                      VkInstance instance,
                                      uint32_t apiVersion)
{
    ASSERT(!valid());
    return vma::InitAllocator(physicalDevice, device, instance, apiVersion, &mHandle);
}

ANGLE_INLINE VkResult Allocator::createBuffer(const VkBufferCreateInfo &bufferCreateInfo,
                                              VkMemoryPropertyFlags requiredFlags,
                                              VkMemoryPropertyFlags preferredFlags,
                                              bool persistentlyMappedBuffers,
                                              uint32_t *memoryTypeIndexOut,
                                              Buffer *bufferOut,
                                              Allocation *allocationOut) const
{
    ASSERT(valid());
    ASSERT(bufferOut && !bufferOut->valid());
    ASSERT(allocationOut && !allocationOut->valid());
    return vma::CreateBuffer(mHandle, &bufferCreateInfo, requiredFlags, preferredFlags,
                             persistentlyMappedBuffers, memoryTypeIndexOut, &bufferOut->mHandle,
                             &allocationOut->mHandle);
}

ANGLE_INLINE void Allocator::getMemoryTypeProperties(uint32_t memoryTypeIndex,
                                                     VkMemoryPropertyFlags *flagsOut) const
{
    ASSERT(valid());
    vma::GetMemoryTypeProperties(mHandle, memoryTypeIndex, flagsOut);
}

ANGLE_INLINE VkResult
Allocator::findMemoryTypeIndexForBufferInfo(const VkBufferCreateInfo &bufferCreateInfo,
                                            VkMemoryPropertyFlags requiredFlags,
                                            VkMemoryPropertyFlags preferredFlags,
                                            bool persistentlyMappedBuffers,
                                            uint32_t *memoryTypeIndexOut) const
{
    ASSERT(valid());
    return vma::FindMemoryTypeIndexForBufferInfo(mHandle, &bufferCreateInfo, requiredFlags,
                                                 preferredFlags, persistentlyMappedBuffers,
                                                 memoryTypeIndexOut);
}

// Allocation implementation.
ANGLE_INLINE void Allocation::destroy(const Allocator &allocator)
{
    if (valid())
    {
        vma::FreeMemory(allocator.getHandle(), mHandle);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Allocation::map(const Allocator &allocator, uint8_t **mapPointer) const
{
    ASSERT(valid());
    return vma::MapMemory(allocator.getHandle(), mHandle, (void **)mapPointer);
}

ANGLE_INLINE void Allocation::unmap(const Allocator &allocator) const
{
    ASSERT(valid());
    vma::UnmapMemory(allocator.getHandle(), mHandle);
}

ANGLE_INLINE void Allocation::flush(const Allocator &allocator,
                                    VkDeviceSize offset,
                                    VkDeviceSize size)
{
    ASSERT(valid());
    vma::FlushAllocation(allocator.getHandle(), mHandle, offset, size);
}

ANGLE_INLINE void Allocation::invalidate(const Allocator &allocator,
                                         VkDeviceSize offset,
                                         VkDeviceSize size)
{
    ASSERT(valid());
    vma::InvalidateAllocation(allocator.getHandle(), mHandle, offset, size);
}

// RenderPass implementation.
ANGLE_INLINE void RenderPass::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyRenderPass(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult RenderPass::init(VkDevice device, const VkRenderPassCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateRenderPass(device, &createInfo, nullptr, &mHandle);
}

// Buffer implementation.
ANGLE_INLINE void Buffer::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyBuffer(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Buffer::init(VkDevice device, const VkBufferCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateBuffer(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult Buffer::bindMemory(VkDevice device, const DeviceMemory &deviceMemory)
{
    ASSERT(valid() && deviceMemory.valid());
    return vkBindBufferMemory(device, mHandle, deviceMemory.getHandle(), 0);
}

ANGLE_INLINE void Buffer::getMemoryRequirements(VkDevice device,
                                                VkMemoryRequirements *memoryRequirementsOut)
{
    ASSERT(valid());
    vkGetBufferMemoryRequirements(device, mHandle, memoryRequirementsOut);
}

// BufferView implementation.
ANGLE_INLINE void BufferView::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyBufferView(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult BufferView::init(VkDevice device, const VkBufferViewCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateBufferView(device, &createInfo, nullptr, &mHandle);
}

// ShaderModule implementation.
ANGLE_INLINE void ShaderModule::destroy(VkDevice device)
{
    if (mHandle != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult ShaderModule::init(VkDevice device,
                                         const VkShaderModuleCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateShaderModule(device, &createInfo, nullptr, &mHandle);
}

// PipelineLayout implementation.
ANGLE_INLINE void PipelineLayout::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipelineLayout(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult PipelineLayout::init(VkDevice device,
                                           const VkPipelineLayoutCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreatePipelineLayout(device, &createInfo, nullptr, &mHandle);
}

// PipelineCache implementation.
ANGLE_INLINE void PipelineCache::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipelineCache(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult PipelineCache::init(VkDevice device,
                                          const VkPipelineCacheCreateInfo &createInfo)
{
    ASSERT(!valid());
    // Note: if we are concerned with memory usage of this cache, we should give it custom
    // allocators.  Also, failure of this function is of little importance.
    return vkCreatePipelineCache(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult PipelineCache::merge(VkDevice device,
                                           VkPipelineCache dstCache,
                                           uint32_t srcCacheCount,
                                           const VkPipelineCache *srcCaches)
{
    ASSERT(valid());
    return vkMergePipelineCaches(device, dstCache, srcCacheCount, srcCaches);
}

ANGLE_INLINE VkResult PipelineCache::getCacheData(VkDevice device,
                                                  size_t *cacheSize,
                                                  void *cacheData)
{
    ASSERT(valid());

    // Note: vkGetPipelineCacheData can return VK_INCOMPLETE if cacheSize is smaller than actual
    // size. There are two usages of this function.  One is with *cacheSize == 0 to query the size
    // of the cache, and one is with an appropriate buffer to retrieve the cache contents.
    // VK_INCOMPLETE in the first case is an expected output.  In the second case, VK_INCOMPLETE is
    // also acceptable and the resulting buffer will contain valid value by spec.  Angle currently
    // ensures *cacheSize to be either 0 or of enough size, therefore VK_INCOMPLETE is not expected.
    return vkGetPipelineCacheData(device, mHandle, cacheSize, cacheData);
}

// Pipeline implementation.
ANGLE_INLINE void Pipeline::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyPipeline(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Pipeline::initGraphics(VkDevice device,
                                             const VkGraphicsPipelineCreateInfo &createInfo,
                                             const PipelineCache &pipelineCacheVk)
{
    ASSERT(!valid());
    return vkCreateGraphicsPipelines(device, pipelineCacheVk.getHandle(), 1, &createInfo, nullptr,
                                     &mHandle);
}

ANGLE_INLINE VkResult Pipeline::initCompute(VkDevice device,
                                            const VkComputePipelineCreateInfo &createInfo,
                                            const PipelineCache &pipelineCacheVk)
{
    ASSERT(!valid());
    return vkCreateComputePipelines(device, pipelineCacheVk.getHandle(), 1, &createInfo, nullptr,
                                    &mHandle);
}

// DescriptorSetLayout implementation.
ANGLE_INLINE void DescriptorSetLayout::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyDescriptorSetLayout(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult DescriptorSetLayout::init(VkDevice device,
                                                const VkDescriptorSetLayoutCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &mHandle);
}

// DescriptorPool implementation.
ANGLE_INLINE void DescriptorPool::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyDescriptorPool(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult DescriptorPool::init(VkDevice device,
                                           const VkDescriptorPoolCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateDescriptorPool(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult
DescriptorPool::allocateDescriptorSets(VkDevice device,
                                       const VkDescriptorSetAllocateInfo &allocInfo,
                                       VkDescriptorSet *descriptorSetsOut)
{
    ASSERT(valid());
    return vkAllocateDescriptorSets(device, &allocInfo, descriptorSetsOut);
}

ANGLE_INLINE VkResult DescriptorPool::freeDescriptorSets(VkDevice device,
                                                         uint32_t descriptorSetCount,
                                                         const VkDescriptorSet *descriptorSets)
{
    ASSERT(valid());
    ASSERT(descriptorSetCount > 0);
    return vkFreeDescriptorSets(device, mHandle, descriptorSetCount, descriptorSets);
}

// Sampler implementation.
ANGLE_INLINE void Sampler::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroySampler(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Sampler::init(VkDevice device, const VkSamplerCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateSampler(device, &createInfo, nullptr, &mHandle);
}

// SamplerYuvConversion implementation.
ANGLE_INLINE void SamplerYcbcrConversion::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroySamplerYcbcrConversionKHR(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult
SamplerYcbcrConversion::init(VkDevice device, const VkSamplerYcbcrConversionCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateSamplerYcbcrConversionKHR(device, &createInfo, nullptr, &mHandle);
}

// Event implementation.
ANGLE_INLINE void Event::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyEvent(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Event::init(VkDevice device, const VkEventCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateEvent(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult Event::getStatus(VkDevice device) const
{
    ASSERT(valid());
    return vkGetEventStatus(device, mHandle);
}

ANGLE_INLINE VkResult Event::set(VkDevice device) const
{
    ASSERT(valid());
    return vkSetEvent(device, mHandle);
}

ANGLE_INLINE VkResult Event::reset(VkDevice device) const
{
    ASSERT(valid());
    return vkResetEvent(device, mHandle);
}

// Fence implementation.
ANGLE_INLINE void Fence::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyFence(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult Fence::init(VkDevice device, const VkFenceCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateFence(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult Fence::reset(VkDevice device)
{
    ASSERT(valid());
    return vkResetFences(device, 1, &mHandle);
}

ANGLE_INLINE VkResult Fence::getStatus(VkDevice device) const
{
    ASSERT(valid());
    return vkGetFenceStatus(device, mHandle);
}

ANGLE_INLINE VkResult Fence::wait(VkDevice device, uint64_t timeout) const
{
    ASSERT(valid());
    return vkWaitForFences(device, 1, &mHandle, true, timeout);
}

ANGLE_INLINE VkResult Fence::importFd(VkDevice device,
                                      const VkImportFenceFdInfoKHR &importFenceFdInfo) const
{
    ASSERT(valid());
    return vkImportFenceFdKHR(device, &importFenceFdInfo);
}

ANGLE_INLINE VkResult Fence::exportFd(VkDevice device,
                                      const VkFenceGetFdInfoKHR &fenceGetFdInfo,
                                      int *fdOut) const
{
    ASSERT(valid());
    return vkGetFenceFdKHR(device, &fenceGetFdInfo, fdOut);
}

// QueryPool implementation.
ANGLE_INLINE void QueryPool::destroy(VkDevice device)
{
    if (valid())
    {
        vkDestroyQueryPool(device, mHandle, nullptr);
        mHandle = VK_NULL_HANDLE;
    }
}

ANGLE_INLINE VkResult QueryPool::init(VkDevice device, const VkQueryPoolCreateInfo &createInfo)
{
    ASSERT(!valid());
    return vkCreateQueryPool(device, &createInfo, nullptr, &mHandle);
}

ANGLE_INLINE VkResult QueryPool::getResults(VkDevice device,
                                            uint32_t firstQuery,
                                            uint32_t queryCount,
                                            size_t dataSize,
                                            void *data,
                                            VkDeviceSize stride,
                                            VkQueryResultFlags flags) const
{
    ASSERT(valid());
    return vkGetQueryPoolResults(device, mHandle, firstQuery, queryCount, dataSize, data, stride,
                                 flags);
}
}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_WRAPPER_H_
