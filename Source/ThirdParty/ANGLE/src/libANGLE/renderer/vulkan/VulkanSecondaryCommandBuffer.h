//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanSecondaryCommandBuffer:
//    A class wrapping a Vulkan command buffer for use as a secondary command buffer.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VULKANSECONDARYCOMMANDBUFFERVK_H_
#define LIBANGLE_RENDERER_VULKAN_VULKANSECONDARYCOMMANDBUFFERVK_H_

#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/vulkan/vk_command_buffer_utils.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

namespace angle
{
class PoolAllocator;
}  // namespace angle

namespace rx
{
class ContextVk;

namespace vk
{
class Context;
class RenderPassDesc;

class VulkanSecondaryCommandBuffer : public priv::CommandBuffer
{
  public:
    VulkanSecondaryCommandBuffer() = default;

    static angle::Result InitializeCommandPool(Context *context,
                                               CommandPool *pool,
                                               uint32_t queueFamilyIndex,
                                               bool hasProtectedContent);
    static angle::Result InitializeRenderPassInheritanceInfo(
        ContextVk *contextVk,
        const Framebuffer &framebuffer,
        const RenderPassDesc &renderPassDesc,
        VkCommandBufferInheritanceInfo *inheritanceInfoOut);

    angle::Result initialize(vk::Context *context,
                             vk::CommandPool *pool,
                             bool isRenderPassCommandBuffer,
                             angle::PoolAllocator *allocator);

    angle::Result begin(vk::Context *context,
                        const VkCommandBufferInheritanceInfo &inheritanceInfo);
    angle::Result end(vk::Context *context);
    VkResult reset();

    void executeCommands(PrimaryCommandBuffer *primary) { primary->executeCommands(1, this); }

    void beginQuery(const QueryPool &queryPool, uint32_t query, VkQueryControlFlags flags);

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

    void endQuery(const QueryPool &queryPool, uint32_t query);

    void fillBuffer(const Buffer &dstBuffer,
                    VkDeviceSize dstOffset,
                    VkDeviceSize size,
                    uint32_t data);

    void executionBarrier(VkPipelineStageFlags stageMask);

    void bufferBarrier(VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       const VkBufferMemoryBarrier *bufferMemoryBarrier);

    void imageBarrier(VkPipelineStageFlags srcStageMask,
                      VkPipelineStageFlags dstStageMask,
                      const VkImageMemoryBarrier &imageMemoryBarrier);

    void memoryBarrier(VkPipelineStageFlags srcStageMask,
                       VkPipelineStageFlags dstStageMask,
                       const VkMemoryBarrier *memoryBarrier);

    void nextSubpass(VkSubpassContents subpassContents);

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
    void beginTransformFeedback(uint32_t firstCounterBuffer,
                                uint32_t counterBufferCount,
                                const VkBuffer *counterBuffers,
                                const VkDeviceSize *counterBufferOffsets);
    void endTransformFeedback(uint32_t firstCounterBuffer,
                              uint32_t counterBufferCount,
                              const VkBuffer *counterBuffers,
                              const VkDeviceSize *counterBufferOffsets);

    // VK_EXT_debug_utils
    void beginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &labelInfo);
    void endDebugUtilsLabelEXT();
    void insertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT &labelInfo);

    void open() const {}
    void close() const {}
    bool empty() const { return !mAnyCommand; }
    uint32_t getRenderPassWriteCommandCount() const
    {
        return mCommandTracker.getRenderPassWriteCommandCount();
    }
    std::string dumpCommands(const char *separator) const { return ""; }

  private:
    void onRecordCommand() { mAnyCommand = true; }

    CommandBufferCommandTracker mCommandTracker;
    bool mAnyCommand = false;
};

ANGLE_INLINE void VulkanSecondaryCommandBuffer::blitImage(const Image &srcImage,
                                                          VkImageLayout srcImageLayout,
                                                          const Image &dstImage,
                                                          VkImageLayout dstImageLayout,
                                                          uint32_t regionCount,
                                                          const VkImageBlit *regions,
                                                          VkFilter filter)
{
    onRecordCommand();
    CommandBuffer::blitImage(srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
                             regions, filter);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::beginQuery(const QueryPool &queryPool,
                                                           uint32_t query,
                                                           VkQueryControlFlags flags)
{
    onRecordCommand();
    CommandBuffer::beginQuery(queryPool, query, flags);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::endQuery(const QueryPool &queryPool, uint32_t query)
{
    onRecordCommand();
    CommandBuffer::endQuery(queryPool, query);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::writeTimestamp(
    VkPipelineStageFlagBits pipelineStage,
    const QueryPool &queryPool,
    uint32_t query)
{
    onRecordCommand();
    CommandBuffer::writeTimestamp(pipelineStage, queryPool, query);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::clearColorImage(
    const Image &image,
    VkImageLayout imageLayout,
    const VkClearColorValue &color,
    uint32_t rangeCount,
    const VkImageSubresourceRange *ranges)
{
    onRecordCommand();
    CommandBuffer::clearColorImage(image, imageLayout, color, rangeCount, ranges);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::clearDepthStencilImage(
    const Image &image,
    VkImageLayout imageLayout,
    const VkClearDepthStencilValue &depthStencil,
    uint32_t rangeCount,
    const VkImageSubresourceRange *ranges)
{
    onRecordCommand();
    CommandBuffer::clearDepthStencilImage(image, imageLayout, depthStencil, rangeCount, ranges);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::clearAttachments(
    uint32_t attachmentCount,
    const VkClearAttachment *attachments,
    uint32_t rectCount,
    const VkClearRect *rects)
{
    onRecordCommand();
    mCommandTracker.onClearAttachments();
    CommandBuffer::clearAttachments(attachmentCount, attachments, rectCount, rects);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::copyBuffer(const Buffer &srcBuffer,
                                                           const Buffer &destBuffer,
                                                           uint32_t regionCount,
                                                           const VkBufferCopy *regions)
{
    onRecordCommand();
    CommandBuffer::copyBuffer(srcBuffer, destBuffer, regionCount, regions);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::copyBufferToImage(VkBuffer srcBuffer,
                                                                  const Image &dstImage,
                                                                  VkImageLayout dstImageLayout,
                                                                  uint32_t regionCount,
                                                                  const VkBufferImageCopy *regions)
{
    onRecordCommand();
    CommandBuffer::copyBufferToImage(srcBuffer, dstImage, dstImageLayout, regionCount, regions);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::copyImageToBuffer(const Image &srcImage,
                                                                  VkImageLayout srcImageLayout,
                                                                  VkBuffer dstBuffer,
                                                                  uint32_t regionCount,
                                                                  const VkBufferImageCopy *regions)
{
    onRecordCommand();
    CommandBuffer::copyImageToBuffer(srcImage, srcImageLayout, dstBuffer, regionCount, regions);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::copyImage(const Image &srcImage,
                                                          VkImageLayout srcImageLayout,
                                                          const Image &dstImage,
                                                          VkImageLayout dstImageLayout,
                                                          uint32_t regionCount,
                                                          const VkImageCopy *regions)
{
    onRecordCommand();
    CommandBuffer::copyImage(srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
                             regions);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::draw(uint32_t vertexCount,
                                                     uint32_t instanceCount,
                                                     uint32_t firstVertex,
                                                     uint32_t firstInstance)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::draw(uint32_t vertexCount, uint32_t firstVertex)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::draw(vertexCount, 1, firstVertex, 0);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawInstanced(uint32_t vertexCount,
                                                              uint32_t instanceCount,
                                                              uint32_t firstVertex)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::draw(vertexCount, instanceCount, firstVertex, 0);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawInstancedBaseInstance(uint32_t vertexCount,
                                                                          uint32_t instanceCount,
                                                                          uint32_t firstVertex,
                                                                          uint32_t firstInstance)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexed(uint32_t indexCount,
                                                            uint32_t instanceCount,
                                                            uint32_t firstIndex,
                                                            int32_t vertexOffset,
                                                            uint32_t firstInstance)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexed(uint32_t indexCount)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexed(indexCount, 1, 0, 0, 0);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexedBaseVertex(uint32_t indexCount,
                                                                      uint32_t vertexOffset)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexed(indexCount, 1, 0, vertexOffset, 0);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexedInstanced(uint32_t indexCount,
                                                                     uint32_t instanceCount)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexed(indexCount, instanceCount, 0, 0, 0);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexedInstancedBaseVertex(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t vertexOffset)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexed(indexCount, instanceCount, 0, vertexOffset, 0);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexedInstancedBaseVertexBaseInstance(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndexedIndirect(const Buffer &buffer,
                                                                    VkDeviceSize offset,
                                                                    uint32_t drawCount,
                                                                    uint32_t stride)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndexedIndirect(buffer, offset, drawCount, stride);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::drawIndirect(const Buffer &buffer,
                                                             VkDeviceSize offset,
                                                             uint32_t drawCount,
                                                             uint32_t stride)
{
    onRecordCommand();
    mCommandTracker.onDraw();
    CommandBuffer::drawIndirect(buffer, offset, drawCount, stride);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::dispatch(uint32_t groupCountX,
                                                         uint32_t groupCountY,
                                                         uint32_t groupCountZ)
{
    onRecordCommand();
    CommandBuffer::dispatch(groupCountX, groupCountY, groupCountZ);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::dispatchIndirect(const Buffer &buffer,
                                                                 VkDeviceSize offset)
{
    onRecordCommand();
    CommandBuffer::dispatchIndirect(buffer, offset);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::fillBuffer(const Buffer &dstBuffer,
                                                           VkDeviceSize dstOffset,
                                                           VkDeviceSize size,
                                                           uint32_t data)
{
    onRecordCommand();
    CommandBuffer::fillBuffer(dstBuffer, dstOffset, size, data);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::pipelineBarrier(
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier *memoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier *bufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier *imageMemoryBarriers)
{
    onRecordCommand();
    CommandBuffer::pipelineBarrier(srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount,
                                   memoryBarriers, bufferMemoryBarrierCount, bufferMemoryBarriers,
                                   imageMemoryBarrierCount, imageMemoryBarriers);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::memoryBarrier(VkPipelineStageFlags srcStageMask,
                                                              VkPipelineStageFlags dstStageMask,
                                                              const VkMemoryBarrier *memoryBarrier)
{
    onRecordCommand();
    CommandBuffer::memoryBarrier(srcStageMask, dstStageMask, memoryBarrier);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::bufferBarrier(
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    const VkBufferMemoryBarrier *bufferMemoryBarrier)
{
    onRecordCommand();
    CommandBuffer::pipelineBarrier(srcStageMask, dstStageMask, 0, 0, nullptr, 1,
                                   bufferMemoryBarrier, 0, nullptr);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::imageBarrier(
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    const VkImageMemoryBarrier &imageMemoryBarrier)
{
    onRecordCommand();
    CommandBuffer::imageBarrier(srcStageMask, dstStageMask, imageMemoryBarrier);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::nextSubpass(VkSubpassContents subpassContents)
{
    onRecordCommand();
    CommandBuffer::nextSubpass(subpassContents);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::pushConstants(const PipelineLayout &layout,
                                                              VkShaderStageFlags flag,
                                                              uint32_t offset,
                                                              uint32_t size,
                                                              const void *data)
{
    onRecordCommand();
    CommandBuffer::pushConstants(layout, flag, offset, size, data);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::setEvent(VkEvent event,
                                                         VkPipelineStageFlags stageMask)
{
    onRecordCommand();
    CommandBuffer::setEvent(event, stageMask);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::resetEvent(VkEvent event,
                                                           VkPipelineStageFlags stageMask)
{
    onRecordCommand();
    CommandBuffer::resetEvent(event, stageMask);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::waitEvents(
    uint32_t eventCount,
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
    onRecordCommand();
    CommandBuffer::waitEvents(eventCount, events, srcStageMask, dstStageMask, memoryBarrierCount,
                              memoryBarriers, bufferMemoryBarrierCount, bufferMemoryBarriers,
                              imageMemoryBarrierCount, imageMemoryBarriers);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::resetQueryPool(const QueryPool &queryPool,
                                                               uint32_t firstQuery,
                                                               uint32_t queryCount)
{
    onRecordCommand();
    CommandBuffer::resetQueryPool(queryPool, firstQuery, queryCount);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::resolveImage(const Image &srcImage,
                                                             VkImageLayout srcImageLayout,
                                                             const Image &dstImage,
                                                             VkImageLayout dstImageLayout,
                                                             uint32_t regionCount,
                                                             const VkImageResolve *regions)
{
    onRecordCommand();
    CommandBuffer::resolveImage(srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
                                regions);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::beginTransformFeedback(
    uint32_t firstCounterBuffer,
    uint32_t counterBufferCount,
    const VkBuffer *counterBuffers,
    const VkDeviceSize *counterBufferOffsets)
{
    onRecordCommand();
    CommandBuffer::beginTransformFeedback(firstCounterBuffer, counterBufferCount, counterBuffers,
                                          counterBufferOffsets);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::endTransformFeedback(
    uint32_t firstCounterBuffer,
    uint32_t counterBufferCount,
    const VkBuffer *counterBuffers,
    const VkDeviceSize *counterBufferOffsets)
{
    onRecordCommand();
    CommandBuffer::endTransformFeedback(firstCounterBuffer, counterBufferCount, counterBuffers,
                                        counterBufferOffsets);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::beginDebugUtilsLabelEXT(
    const VkDebugUtilsLabelEXT &labelInfo)
{
    onRecordCommand();
    CommandBuffer::beginDebugUtilsLabelEXT(labelInfo);
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::endDebugUtilsLabelEXT()
{
    onRecordCommand();
    CommandBuffer::endDebugUtilsLabelEXT();
}

ANGLE_INLINE void VulkanSecondaryCommandBuffer::insertDebugUtilsLabelEXT(
    const VkDebugUtilsLabelEXT &labelInfo)
{
    onRecordCommand();
    CommandBuffer::insertDebugUtilsLabelEXT(labelInfo);
}

}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VULKANSECONDARYCOMMANDBUFFERVK_H_
