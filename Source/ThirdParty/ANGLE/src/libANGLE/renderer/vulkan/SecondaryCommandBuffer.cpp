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

namespace rx
{
namespace vk
{
namespace priv
{

ANGLE_INLINE const CommandHeader *NextCommand(const CommandHeader *command)
{
    return reinterpret_cast<const CommandHeader *>(reinterpret_cast<const uint8_t *>(command) +
                                                   command->size);
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
                case CommandID::BeginQuery:
                {
                    const BeginQueryParams *params = getParamPtr<BeginQueryParams>(currentCommand);
                    vkCmdBeginQuery(cmdBuffer, params->queryPool, params->query, params->flags);
                    break;
                }
                case CommandID::BindComputeDescriptorSets:
                {
                    const BindComputeDescriptorSetParams *params =
                        getParamPtr<BindComputeDescriptorSetParams>(currentCommand);
                    const VkDescriptorSet *descriptorSets =
                        Offset<VkDescriptorSet>(params, sizeof(BindComputeDescriptorSetParams));
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                            params->layout, 0, 1, descriptorSets, 0, nullptr);
                    break;
                }
                case CommandID::BindComputePipeline:
                {
                    const BindPipelineParams *params =
                        getParamPtr<BindPipelineParams>(currentCommand);
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, params->pipeline);
                    break;
                }
                case CommandID::BindGraphicsDescriptorSets:
                {
                    const BindGraphicsDescriptorSetParams *params =
                        getParamPtr<BindGraphicsDescriptorSetParams>(currentCommand);
                    const VkDescriptorSet *descriptorSets =
                        Offset<VkDescriptorSet>(params, sizeof(BindGraphicsDescriptorSetParams));
                    const uint32_t *dynamicOffsets = Offset<uint32_t>(
                        descriptorSets, sizeof(VkDescriptorSet) * params->descriptorSetCount);
                    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            params->layout, params->firstSet,
                                            params->descriptorSetCount, descriptorSets,
                                            params->dynamicOffsetCount, dynamicOffsets);
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
                case CommandID::DrawIndexedInstanced:
                {
                    const DrawIndexedInstancedParams *params =
                        getParamPtr<DrawIndexedInstancedParams>(currentCommand);
                    vkCmdDrawIndexed(cmdBuffer, params->indexCount, params->instanceCount, 0, 0, 0);
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
                case CommandID::EndQuery:
                {
                    const EndQueryParams *params = getParamPtr<EndQueryParams>(currentCommand);
                    vkCmdEndQuery(cmdBuffer, params->queryPool, params->query);
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

std::string SecondaryCommandBuffer::dumpCommands(const char *separator) const
{
    std::string result;
    for (const CommandHeader *command : mCommands)
    {
        for (const CommandHeader *currentCommand                      = command;
             currentCommand->id != CommandID::Invalid; currentCommand = NextCommand(currentCommand))
        {
            result += separator;
            switch (currentCommand->id)
            {
                case CommandID::BeginQuery:
                    result += "BeginQuery";
                    break;
                case CommandID::BindComputeDescriptorSets:
                    result += "BindComputeDescriptorSets";
                    break;
                case CommandID::BindComputePipeline:
                    result += "BindComputePipeline";
                    break;
                case CommandID::BindGraphicsDescriptorSets:
                    result += "BindGraphicsDescriptorSets";
                    break;
                case CommandID::BindGraphicsPipeline:
                    result += "BindGraphicsPipeline";
                    break;
                case CommandID::BindIndexBuffer:
                    result += "BindIndexBuffer";
                    break;
                case CommandID::BindVertexBuffers:
                    result += "BindVertexBuffers";
                    break;
                case CommandID::BlitImage:
                    result += "BlitImage";
                    break;
                case CommandID::ClearAttachments:
                    result += "ClearAttachments";
                    break;
                case CommandID::ClearColorImage:
                    result += "ClearColorImage";
                    break;
                case CommandID::ClearDepthStencilImage:
                    result += "ClearDepthStencilImage";
                    break;
                case CommandID::CopyBuffer:
                    result += "CopyBuffer";
                    break;
                case CommandID::CopyBufferToImage:
                    result += "CopyBufferToImage";
                    break;
                case CommandID::CopyImage:
                    result += "CopyImage";
                    break;
                case CommandID::CopyImageToBuffer:
                    result += "CopyImageToBuffer";
                    break;
                case CommandID::Dispatch:
                    result += "Dispatch";
                    break;
                case CommandID::Draw:
                    result += "Draw";
                    break;
                case CommandID::DrawIndexed:
                    result += "DrawIndexed";
                    break;
                case CommandID::DrawIndexedInstanced:
                    result += "DrawIndexedInstanced";
                    break;
                case CommandID::DrawInstanced:
                    result += "DrawInstanced";
                    break;
                case CommandID::EndQuery:
                    result += "EndQuery";
                    break;
                case CommandID::ImageBarrier:
                    result += "ImageBarrier";
                    break;
                case CommandID::MemoryBarrier:
                    result += "MemoryBarrier";
                    break;
                case CommandID::PipelineBarrier:
                    result += "PipelineBarrier";
                    break;
                case CommandID::PushConstants:
                    result += "PushConstants";
                    break;
                case CommandID::ResetEvent:
                    result += "ResetEvent";
                    break;
                case CommandID::ResetQueryPool:
                    result += "ResetQueryPool";
                    break;
                case CommandID::SetEvent:
                    result += "SetEvent";
                    break;
                case CommandID::WaitEvents:
                    result += "WaitEvents";
                    break;
                case CommandID::WriteTimestamp:
                    result += "WriteTimestamp";
                    break;
                default:
                {
                    UNREACHABLE();
                    result += "--invalid--";
                    break;
                }
            }
        }
    }
    return result;
}

}  // namespace priv
}  // namespace vk
}  // namespace rx
