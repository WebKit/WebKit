//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SecondaryCommandBuffer:
//    CPU-side storage of commands to delay GPU-side allocation until commands are submitted.
//

#include "libANGLE/renderer/vulkan/SecondaryCommandBuffer.h"
#include "common/debug.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{
namespace vk
{
namespace priv
{
namespace
{
const char *GetCommandString(CommandID id)
{
    switch (id)
    {
        case CommandID::Invalid:
            return "--Invalid--";
        case CommandID::BeginDebugUtilsLabel:
            return "BeginDebugUtilsLabel";
        case CommandID::BeginQuery:
            return "BeginQuery";
        case CommandID::BeginTransformFeedback:
            return "BeginTransformFeedback";
        case CommandID::BindComputePipeline:
            return "BindComputePipeline";
        case CommandID::BindDescriptorSets:
            return "BindDescriptorSets";
        case CommandID::BindGraphicsPipeline:
            return "BindGraphicsPipeline";
        case CommandID::BindIndexBuffer:
            return "BindIndexBuffer";
        case CommandID::BindTransformFeedbackBuffers:
            return "BindTransformFeedbackBuffers";
        case CommandID::BindVertexBuffers:
            return "BindVertexBuffers";
        case CommandID::BlitImage:
            return "BlitImage";
        case CommandID::BufferBarrier:
            return "BufferBarrier";
        case CommandID::ClearAttachments:
            return "ClearAttachments";
        case CommandID::ClearColorImage:
            return "ClearColorImage";
        case CommandID::ClearDepthStencilImage:
            return "ClearDepthStencilImage";
        case CommandID::CopyBuffer:
            return "CopyBuffer";
        case CommandID::CopyBufferToImage:
            return "CopyBufferToImage";
        case CommandID::CopyImage:
            return "CopyImage";
        case CommandID::CopyImageToBuffer:
            return "CopyImageToBuffer";
        case CommandID::Dispatch:
            return "Dispatch";
        case CommandID::DispatchIndirect:
            return "DispatchIndirect";
        case CommandID::Draw:
            return "Draw";
        case CommandID::DrawIndexed:
            return "DrawIndexed";
        case CommandID::DrawIndexedBaseVertex:
            return "DrawIndexedBaseVertex";
        case CommandID::DrawIndexedIndirect:
            return "DrawIndexedIndirect";
        case CommandID::DrawIndexedInstanced:
            return "DrawIndexedInstanced";
        case CommandID::DrawIndexedInstancedBaseVertex:
            return "DrawIndexedInstancedBaseVertex";
        case CommandID::DrawIndexedInstancedBaseVertexBaseInstance:
            return "DrawIndexedInstancedBaseVertexBaseInstance";
        case CommandID::DrawIndirect:
            return "DrawIndirect";
        case CommandID::DrawInstanced:
            return "DrawInstanced";
        case CommandID::DrawInstancedBaseInstance:
            return "DrawInstancedBaseInstance";
        case CommandID::EndDebugUtilsLabel:
            return "EndDebugUtilsLabel";
        case CommandID::EndQuery:
            return "EndQuery";
        case CommandID::EndTransformFeedback:
            return "EndTransformFeedback";
        case CommandID::ExecutionBarrier:
            return "ExecutionBarrier";
        case CommandID::FillBuffer:
            return "FillBuffer";
        case CommandID::ImageBarrier:
            return "ImageBarrier";
        case CommandID::InsertDebugUtilsLabel:
            return "InsertDebugUtilsLabel";
        case CommandID::MemoryBarrier:
            return "MemoryBarrier";
        case CommandID::PipelineBarrier:
            return "PipelineBarrier";
        case CommandID::PushConstants:
            return "PushConstants";
        case CommandID::ResetEvent:
            return "ResetEvent";
        case CommandID::ResetQueryPool:
            return "ResetQueryPool";
        case CommandID::ResolveImage:
            return "ResolveImage";
        case CommandID::SetEvent:
            return "SetEvent";
        case CommandID::WaitEvents:
            return "WaitEvents";
        case CommandID::WriteTimestamp:
            return "WriteTimestamp";
        default:
            // Need this to work around MSVC warning 4715.
            UNREACHABLE();
            return "--unreachable--";
    }
}
}  // namespace

ANGLE_INLINE const CommandHeader *NextCommand(const CommandHeader *command)
{
    return reinterpret_cast<const CommandHeader *>(reinterpret_cast<const uint8_t *>(command) +
                                                   command->size);
}

// Add any queued resetQueryPool commands to the given cmdBuffer
void SecondaryCommandBuffer::executeQueuedResetQueryPoolCommands(VkCommandBuffer cmdBuffer)
{
    for (const ResetQueryPoolParams &queryParams : mResetQueryQueue)
    {
        vkCmdResetQueryPool(cmdBuffer, queryParams.queryPool, queryParams.firstQuery,
                            queryParams.queryCount);
    }
}

// Parse the cmds in this cmd buffer into given primary cmd buffer
void SecondaryCommandBuffer::executeCommands(VkCommandBuffer cmdBuffer)
{
    for (const CommandHeader *command : mCommands)
    {
        for (const CommandHeader *currentCommand                      = command;
             currentCommand->id != CommandID::Invalid; currentCommand = NextCommand(currentCommand))
        {
            switch (currentCommand->id)
            {
                case CommandID::BeginDebugUtilsLabel:
                {
                    const DebugUtilsLabelParams *params =
                        getParamPtr<DebugUtilsLabelParams>(currentCommand);
                    const char *pLabelName = Offset<char>(params, sizeof(DebugUtilsLabelParams));
                    const VkDebugUtilsLabelEXT label = {
                        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                        nullptr,
                        pLabelName,
                        {params->color[0], params->color[1], params->color[2], params->color[3]}};
                    ASSERT(vkCmdBeginDebugUtilsLabelEXT);
                    vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &label);
                    break;
                }
                case CommandID::BeginQuery:
                {
                    const BeginQueryParams *params = getParamPtr<BeginQueryParams>(currentCommand);
                    vkCmdBeginQuery(cmdBuffer, params->queryPool, params->query, params->flags);
                    break;
                }
                case CommandID::BeginTransformFeedback:
                {
                    const BeginTransformFeedbackParams *params =
                        getParamPtr<BeginTransformFeedbackParams>(currentCommand);
                    const VkBuffer *counterBuffers =
                        Offset<VkBuffer>(params, sizeof(BeginTransformFeedbackParams));
                    // Workaround for AMD driver bug where it expects the offsets array to be
                    // non-null
                    gl::TransformFeedbackBuffersArray<VkDeviceSize> offsets;
                    offsets.fill(0);
                    vkCmdBeginTransformFeedbackEXT(cmdBuffer, 0, params->bufferCount,
                                                   counterBuffers, offsets.data());
                    break;
                }
                case CommandID::BindComputePipeline:
                {
                    const BindPipelineParams *params =
                        getParamPtr<BindPipelineParams>(currentCommand);
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, params->pipeline);
                    break;
                }
                case CommandID::BindDescriptorSets:
                {
                    const BindDescriptorSetParams *params =
                        getParamPtr<BindDescriptorSetParams>(currentCommand);
                    const VkDescriptorSet *descriptorSets =
                        Offset<VkDescriptorSet>(params, sizeof(BindDescriptorSetParams));
                    const uint32_t *dynamicOffsets = Offset<uint32_t>(
                        descriptorSets, sizeof(VkDescriptorSet) * params->descriptorSetCount);
                    vkCmdBindDescriptorSets(cmdBuffer, params->pipelineBindPoint, params->layout,
                                            params->firstSet, params->descriptorSetCount,
                                            descriptorSets, params->dynamicOffsetCount,
                                            dynamicOffsets);
                    break;
                }
                case CommandID::BindGraphicsPipeline:
                {
                    const BindPipelineParams *params =
                        getParamPtr<BindPipelineParams>(currentCommand);
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, params->pipeline);
                    break;
                }
                case CommandID::BindIndexBuffer:
                {
                    const BindIndexBufferParams *params =
                        getParamPtr<BindIndexBufferParams>(currentCommand);
                    vkCmdBindIndexBuffer(cmdBuffer, params->buffer, params->offset,
                                         params->indexType);
                    break;
                }
                case CommandID::BindTransformFeedbackBuffers:
                {
                    const BindTransformFeedbackBuffersParams *params =
                        getParamPtr<BindTransformFeedbackBuffersParams>(currentCommand);
                    const VkBuffer *buffers =
                        Offset<VkBuffer>(params, sizeof(BindTransformFeedbackBuffersParams));
                    const VkDeviceSize *offsets =
                        Offset<VkDeviceSize>(buffers, sizeof(VkBuffer) * params->bindingCount);
                    const VkDeviceSize *sizes =
                        Offset<VkDeviceSize>(offsets, sizeof(VkDeviceSize) * params->bindingCount);
                    vkCmdBindTransformFeedbackBuffersEXT(cmdBuffer, 0, params->bindingCount,
                                                         buffers, offsets, sizes);
                    break;
                }
                case CommandID::BindVertexBuffers:
                {
                    const BindVertexBuffersParams *params =
                        getParamPtr<BindVertexBuffersParams>(currentCommand);
                    const VkBuffer *buffers =
                        Offset<VkBuffer>(params, sizeof(BindVertexBuffersParams));
                    const VkDeviceSize *offsets =
                        Offset<VkDeviceSize>(buffers, sizeof(VkBuffer) * params->bindingCount);
                    vkCmdBindVertexBuffers(cmdBuffer, 0, params->bindingCount, buffers, offsets);
                    break;
                }
                case CommandID::BlitImage:
                {
                    const BlitImageParams *params = getParamPtr<BlitImageParams>(currentCommand);
                    vkCmdBlitImage(cmdBuffer, params->srcImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, params->dstImage,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &params->region,
                                   params->filter);
                    break;
                }
                case CommandID::BufferBarrier:
                {
                    const BufferBarrierParams *params =
                        getParamPtr<BufferBarrierParams>(currentCommand);
                    vkCmdPipelineBarrier(cmdBuffer, params->srcStageMask, params->dstStageMask, 0,
                                         0, nullptr, 1, &params->bufferMemoryBarrier, 0, nullptr);
                    break;
                }
                case CommandID::ClearAttachments:
                {
                    const ClearAttachmentsParams *params =
                        getParamPtr<ClearAttachmentsParams>(currentCommand);
                    const VkClearAttachment *attachments =
                        Offset<VkClearAttachment>(params, sizeof(ClearAttachmentsParams));
                    vkCmdClearAttachments(cmdBuffer, params->attachmentCount, attachments, 1,
                                          &params->rect);
                    break;
                }
                case CommandID::ClearColorImage:
                {
                    const ClearColorImageParams *params =
                        getParamPtr<ClearColorImageParams>(currentCommand);
                    vkCmdClearColorImage(cmdBuffer, params->image, params->imageLayout,
                                         &params->color, 1, &params->range);
                    break;
                }
                case CommandID::ClearDepthStencilImage:
                {
                    const ClearDepthStencilImageParams *params =
                        getParamPtr<ClearDepthStencilImageParams>(currentCommand);
                    vkCmdClearDepthStencilImage(cmdBuffer, params->image, params->imageLayout,
                                                &params->depthStencil, 1, &params->range);
                    break;
                }
                case CommandID::CopyBuffer:
                {
                    const CopyBufferParams *params = getParamPtr<CopyBufferParams>(currentCommand);
                    const VkBufferCopy *regions =
                        Offset<VkBufferCopy>(params, sizeof(CopyBufferParams));
                    vkCmdCopyBuffer(cmdBuffer, params->srcBuffer, params->destBuffer,
                                    params->regionCount, regions);
                    break;
                }
                case CommandID::CopyBufferToImage:
                {
                    const CopyBufferToImageParams *params =
                        getParamPtr<CopyBufferToImageParams>(currentCommand);
                    vkCmdCopyBufferToImage(cmdBuffer, params->srcBuffer, params->dstImage,
                                           params->dstImageLayout, 1, &params->region);
                    break;
                }
                case CommandID::CopyImage:
                {
                    const CopyImageParams *params = getParamPtr<CopyImageParams>(currentCommand);
                    vkCmdCopyImage(cmdBuffer, params->srcImage, params->srcImageLayout,
                                   params->dstImage, params->dstImageLayout, 1, &params->region);
                    break;
                }
                case CommandID::CopyImageToBuffer:
                {
                    const CopyImageToBufferParams *params =
                        getParamPtr<CopyImageToBufferParams>(currentCommand);
                    vkCmdCopyImageToBuffer(cmdBuffer, params->srcImage, params->srcImageLayout,
                                           params->dstBuffer, 1, &params->region);
                    break;
                }
                case CommandID::Dispatch:
                {
                    const DispatchParams *params = getParamPtr<DispatchParams>(currentCommand);
                    vkCmdDispatch(cmdBuffer, params->groupCountX, params->groupCountY,
                                  params->groupCountZ);
                    break;
                }
                case CommandID::DispatchIndirect:
                {
                    const DispatchIndirectParams *params =
                        getParamPtr<DispatchIndirectParams>(currentCommand);
                    vkCmdDispatchIndirect(cmdBuffer, params->buffer, params->offset);
                    break;
                }
                case CommandID::Draw:
                {
                    const DrawParams *params = getParamPtr<DrawParams>(currentCommand);
                    vkCmdDraw(cmdBuffer, params->vertexCount, 1, params->firstVertex, 0);
                    break;
                }
                case CommandID::DrawIndexed:
                {
                    const DrawIndexedParams *params =
                        getParamPtr<DrawIndexedParams>(currentCommand);
                    vkCmdDrawIndexed(cmdBuffer, params->indexCount, 1, 0, 0, 0);
                    break;
                }
                case CommandID::DrawIndexedBaseVertex:
                {
                    const DrawIndexedBaseVertexParams *params =
                        getParamPtr<DrawIndexedBaseVertexParams>(currentCommand);
                    vkCmdDrawIndexed(cmdBuffer, params->indexCount, 1, 0, params->vertexOffset, 0);
                    break;
                }
                case CommandID::DrawIndexedIndirect:
                {
                    const DrawIndexedIndirectParams *params =
                        getParamPtr<DrawIndexedIndirectParams>(currentCommand);
                    vkCmdDrawIndexedIndirect(cmdBuffer, params->buffer, params->offset, 1, 0);
                    break;
                }
                case CommandID::DrawIndexedInstanced:
                {
                    const DrawIndexedInstancedParams *params =
                        getParamPtr<DrawIndexedInstancedParams>(currentCommand);
                    vkCmdDrawIndexed(cmdBuffer, params->indexCount, params->instanceCount, 0, 0, 0);
                    break;
                }
                case CommandID::DrawIndexedInstancedBaseVertex:
                {
                    const DrawIndexedInstancedBaseVertexParams *params =
                        getParamPtr<DrawIndexedInstancedBaseVertexParams>(currentCommand);
                    vkCmdDrawIndexed(cmdBuffer, params->indexCount, params->instanceCount, 0,
                                     params->vertexOffset, 0);
                    break;
                }
                case CommandID::DrawIndexedInstancedBaseVertexBaseInstance:
                {
                    const DrawIndexedInstancedBaseVertexBaseInstanceParams *params =
                        getParamPtr<DrawIndexedInstancedBaseVertexBaseInstanceParams>(
                            currentCommand);
                    vkCmdDrawIndexed(cmdBuffer, params->indexCount, params->instanceCount,
                                     params->firstIndex, params->vertexOffset,
                                     params->firstInstance);
                    break;
                }
                case CommandID::DrawIndirect:
                {
                    const DrawIndirectParams *params =
                        getParamPtr<DrawIndirectParams>(currentCommand);
                    vkCmdDrawIndirect(cmdBuffer, params->buffer, params->offset, 1, 0);
                    break;
                }
                case CommandID::DrawInstanced:
                {
                    const DrawInstancedParams *params =
                        getParamPtr<DrawInstancedParams>(currentCommand);
                    vkCmdDraw(cmdBuffer, params->vertexCount, params->instanceCount,
                              params->firstVertex, 0);
                    break;
                }
                case CommandID::DrawInstancedBaseInstance:
                {
                    const DrawInstancedBaseInstanceParams *params =
                        getParamPtr<DrawInstancedBaseInstanceParams>(currentCommand);
                    vkCmdDraw(cmdBuffer, params->vertexCount, params->instanceCount,
                              params->firstVertex, params->firstInstance);
                    break;
                }
                case CommandID::EndDebugUtilsLabel:
                {
                    ASSERT(vkCmdEndDebugUtilsLabelEXT);
                    vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                    break;
                }
                case CommandID::EndQuery:
                {
                    const EndQueryParams *params = getParamPtr<EndQueryParams>(currentCommand);
                    vkCmdEndQuery(cmdBuffer, params->queryPool, params->query);
                    break;
                }
                case CommandID::EndTransformFeedback:
                {
                    const EndTransformFeedbackParams *params =
                        getParamPtr<EndTransformFeedbackParams>(currentCommand);
                    const VkBuffer *counterBuffers =
                        Offset<VkBuffer>(params, sizeof(EndTransformFeedbackParams));
                    // Workaround for AMD driver bug where it expects the offsets array to be
                    // non-null
                    gl::TransformFeedbackBuffersArray<VkDeviceSize> offsets;
                    offsets.fill(0);
                    vkCmdEndTransformFeedbackEXT(cmdBuffer, 0, params->bufferCount, counterBuffers,
                                                 offsets.data());
                    break;
                }
                case CommandID::ExecutionBarrier:
                {
                    const ExecutionBarrierParams *params =
                        getParamPtr<ExecutionBarrierParams>(currentCommand);
                    vkCmdPipelineBarrier(cmdBuffer, params->stageMask, params->stageMask, 0, 0,
                                         nullptr, 0, nullptr, 0, nullptr);
                    break;
                }
                case CommandID::FillBuffer:
                {
                    const FillBufferParams *params = getParamPtr<FillBufferParams>(currentCommand);
                    vkCmdFillBuffer(cmdBuffer, params->dstBuffer, params->dstOffset, params->size,
                                    params->data);
                    break;
                }
                case CommandID::ImageBarrier:
                {
                    const ImageBarrierParams *params =
                        getParamPtr<ImageBarrierParams>(currentCommand);
                    vkCmdPipelineBarrier(cmdBuffer, params->srcStageMask, params->dstStageMask, 0,
                                         0, nullptr, 0, nullptr, 1, &params->imageMemoryBarrier);
                    break;
                }
                case CommandID::InsertDebugUtilsLabel:
                {
                    const DebugUtilsLabelParams *params =
                        getParamPtr<DebugUtilsLabelParams>(currentCommand);
                    const char *pLabelName = Offset<char>(params, sizeof(DebugUtilsLabelParams));
                    const VkDebugUtilsLabelEXT label = {
                        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
                        nullptr,
                        pLabelName,
                        {params->color[0], params->color[1], params->color[2], params->color[3]}};
                    ASSERT(vkCmdInsertDebugUtilsLabelEXT);
                    vkCmdInsertDebugUtilsLabelEXT(cmdBuffer, &label);
                    break;
                }
                case CommandID::MemoryBarrier:
                {
                    const MemoryBarrierParams *params =
                        getParamPtr<MemoryBarrierParams>(currentCommand);
                    vkCmdPipelineBarrier(cmdBuffer, params->srcStageMask, params->dstStageMask, 0,
                                         1, &params->memoryBarrier, 0, nullptr, 0, nullptr);
                    break;
                }
                case CommandID::PipelineBarrier:
                {
                    const PipelineBarrierParams *params =
                        getParamPtr<PipelineBarrierParams>(currentCommand);
                    const VkMemoryBarrier *memoryBarriers =
                        Offset<VkMemoryBarrier>(params, sizeof(PipelineBarrierParams));
                    const VkBufferMemoryBarrier *bufferMemoryBarriers =
                        Offset<VkBufferMemoryBarrier>(
                            memoryBarriers, params->memoryBarrierCount * sizeof(VkMemoryBarrier));
                    const VkImageMemoryBarrier *imageMemoryBarriers = Offset<VkImageMemoryBarrier>(
                        bufferMemoryBarriers,
                        params->bufferMemoryBarrierCount * sizeof(VkBufferMemoryBarrier));
                    vkCmdPipelineBarrier(cmdBuffer, params->srcStageMask, params->dstStageMask,
                                         params->dependencyFlags, params->memoryBarrierCount,
                                         memoryBarriers, params->bufferMemoryBarrierCount,
                                         bufferMemoryBarriers, params->imageMemoryBarrierCount,
                                         imageMemoryBarriers);
                    break;
                }
                case CommandID::PushConstants:
                {
                    const PushConstantsParams *params =
                        getParamPtr<PushConstantsParams>(currentCommand);
                    const void *data = Offset<void>(params, sizeof(PushConstantsParams));
                    vkCmdPushConstants(cmdBuffer, params->layout, params->flag, params->offset,
                                       params->size, data);
                    break;
                }
                case CommandID::ResetEvent:
                {
                    const ResetEventParams *params = getParamPtr<ResetEventParams>(currentCommand);
                    vkCmdResetEvent(cmdBuffer, params->event, params->stageMask);
                    break;
                }
                case CommandID::ResetQueryPool:
                {
                    const ResetQueryPoolParams *params =
                        getParamPtr<ResetQueryPoolParams>(currentCommand);
                    vkCmdResetQueryPool(cmdBuffer, params->queryPool, params->firstQuery,
                                        params->queryCount);
                    break;
                }
                case CommandID::ResolveImage:
                {
                    const ResolveImageParams *params =
                        getParamPtr<ResolveImageParams>(currentCommand);
                    vkCmdResolveImage(cmdBuffer, params->srcImage,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, params->dstImage,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &params->region);
                    break;
                }
                case CommandID::SetEvent:
                {
                    const SetEventParams *params = getParamPtr<SetEventParams>(currentCommand);
                    vkCmdSetEvent(cmdBuffer, params->event, params->stageMask);
                    break;
                }
                case CommandID::WaitEvents:
                {
                    const WaitEventsParams *params = getParamPtr<WaitEventsParams>(currentCommand);
                    const VkEvent *events = Offset<VkEvent>(params, sizeof(WaitEventsParams));
                    const VkMemoryBarrier *memoryBarriers =
                        Offset<VkMemoryBarrier>(events, params->eventCount * sizeof(VkEvent));
                    const VkBufferMemoryBarrier *bufferMemoryBarriers =
                        Offset<VkBufferMemoryBarrier>(
                            memoryBarriers, params->memoryBarrierCount * sizeof(VkMemoryBarrier));
                    const VkImageMemoryBarrier *imageMemoryBarriers = Offset<VkImageMemoryBarrier>(
                        bufferMemoryBarriers,
                        params->bufferMemoryBarrierCount * sizeof(VkBufferMemoryBarrier));
                    vkCmdWaitEvents(cmdBuffer, params->eventCount, events, params->srcStageMask,
                                    params->dstStageMask, params->memoryBarrierCount,
                                    memoryBarriers, params->bufferMemoryBarrierCount,
                                    bufferMemoryBarriers, params->imageMemoryBarrierCount,
                                    imageMemoryBarriers);
                    break;
                }
                case CommandID::WriteTimestamp:
                {
                    const WriteTimestampParams *params =
                        getParamPtr<WriteTimestampParams>(currentCommand);
                    vkCmdWriteTimestamp(cmdBuffer, params->pipelineStage, params->queryPool,
                                        params->query);
                    break;
                }
                default:
                {
                    UNREACHABLE();
                    break;
                }
            }
        }
    }
}

void SecondaryCommandBuffer::getMemoryUsageStats(size_t *usedMemoryOut,
                                                 size_t *allocatedMemoryOut) const
{
    *allocatedMemoryOut = kBlockSize * mCommands.size();

    *usedMemoryOut = 0;
    for (const CommandHeader *command : mCommands)
    {
        const CommandHeader *commandEnd = command;
        while (commandEnd->id != CommandID::Invalid)
        {
            commandEnd = NextCommand(commandEnd);
        }

        *usedMemoryOut += reinterpret_cast<const uint8_t *>(commandEnd) -
                          reinterpret_cast<const uint8_t *>(command) + sizeof(CommandHeader::id);
    }

    ASSERT(*usedMemoryOut <= *allocatedMemoryOut);
}

std::string SecondaryCommandBuffer::dumpCommands(const char *separator) const
{
    std::stringstream result;
    for (const CommandHeader *command : mCommands)
    {
        for (const CommandHeader *currentCommand                      = command;
             currentCommand->id != CommandID::Invalid; currentCommand = NextCommand(currentCommand))
        {
            result << GetCommandString(currentCommand->id) << separator;
        }
    }
    return result.str();
}

}  // namespace priv
}  // namespace vk
}  // namespace rx
