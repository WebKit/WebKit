//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CommandGraph:
//    Deferred work constructed by GL calls, that will later be flushed to Vulkan.
//

#include "libANGLE/renderer/vulkan/CommandGraph.h"

#include <iostream>

#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RenderTargetVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

#include "third_party/trace_event/trace_event.h"

namespace rx
{
namespace vk
{
namespace
{
ANGLE_MAYBE_UNUSED
angle::Result InitAndBeginCommandBuffer(vk::Context *context,
                                        const CommandPool &commandPool,
                                        const VkCommandBufferInheritanceInfo &inheritanceInfo,
                                        VkCommandBufferUsageFlags flags,
                                        angle::PoolAllocator *poolAllocator,
                                        priv::SecondaryCommandBuffer *commandBuffer)
{
    ASSERT(!commandBuffer->valid());
    commandBuffer->initialize(poolAllocator);
    return angle::Result::Continue;
}

ANGLE_MAYBE_UNUSED
angle::Result InitAndBeginCommandBuffer(vk::Context *context,
                                        const CommandPool &commandPool,
                                        const VkCommandBufferInheritanceInfo &inheritanceInfo,
                                        VkCommandBufferUsageFlags flags,
                                        angle::PoolAllocator *poolAllocator,
                                        PrimaryCommandBuffer *commandBuffer)
{
    ASSERT(!commandBuffer->valid());
    VkCommandBufferAllocateInfo createInfo = {};
    createInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    createInfo.commandPool                 = commandPool.getHandle();
    createInfo.level                       = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    createInfo.commandBufferCount          = 1;

    ANGLE_VK_TRY(context, commandBuffer->init(context->getDevice(), createInfo));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = flags | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = &inheritanceInfo;

    ANGLE_VK_TRY(context, commandBuffer->begin(beginInfo));
    return angle::Result::Continue;
}

const char *GetResourceTypeName(CommandGraphResourceType resourceType,
                                CommandGraphNodeFunction function)
{
    switch (resourceType)
    {
        case CommandGraphResourceType::Buffer:
            return "Buffer";
        case CommandGraphResourceType::Framebuffer:
            return "Framebuffer";
        case CommandGraphResourceType::Image:
            return "Image";
        case CommandGraphResourceType::Query:
            switch (function)
            {
                case CommandGraphNodeFunction::BeginQuery:
                    return "BeginQuery";
                case CommandGraphNodeFunction::EndQuery:
                    return "EndQuery";
                case CommandGraphNodeFunction::WriteTimestamp:
                    return "WriteTimestamp";
                default:
                    UNREACHABLE();
                    return "Query";
            }
        case CommandGraphResourceType::FenceSync:
            switch (function)
            {
                case CommandGraphNodeFunction::SetFenceSync:
                    return "SetFenceSync";
                case CommandGraphNodeFunction::WaitFenceSync:
                    return "WaitFenceSync";
                default:
                    UNREACHABLE();
                    return "FenceSync";
            }
        case CommandGraphResourceType::DebugMarker:
            switch (function)
            {
                case CommandGraphNodeFunction::InsertDebugMarker:
                    return "InsertDebugMarker";
                case CommandGraphNodeFunction::PushDebugMarker:
                    return "PushDebugMarker";
                case CommandGraphNodeFunction::PopDebugMarker:
                    return "PopDebugMarker";
                default:
                    UNREACHABLE();
                    return "DebugMarker";
            }
        default:
            UNREACHABLE();
            return "";
    }
}

void MakeDebugUtilsLabel(GLenum source, const char *marker, VkDebugUtilsLabelEXT *label)
{
    static constexpr angle::ColorF kLabelColors[6] = {
        angle::ColorF(1.0f, 0.5f, 0.5f, 1.0f),  // DEBUG_SOURCE_API
        angle::ColorF(0.5f, 1.0f, 0.5f, 1.0f),  // DEBUG_SOURCE_WINDOW_SYSTEM
        angle::ColorF(0.5f, 0.5f, 1.0f, 1.0f),  // DEBUG_SOURCE_SHADER_COMPILER
        angle::ColorF(0.7f, 0.7f, 0.7f, 1.0f),  // DEBUG_SOURCE_THIRD_PARTY
        angle::ColorF(0.5f, 0.8f, 0.9f, 1.0f),  // DEBUG_SOURCE_APPLICATION
        angle::ColorF(0.9f, 0.8f, 0.5f, 1.0f),  // DEBUG_SOURCE_OTHER
    };

    int colorIndex = source - GL_DEBUG_SOURCE_API;
    ASSERT(colorIndex >= 0 && static_cast<size_t>(colorIndex) < ArraySize(kLabelColors));

    label->sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label->pNext      = nullptr;
    label->pLabelName = marker;
    kLabelColors[colorIndex].writeData(label->color);
}

constexpr VkSubpassContents kRenderPassContents =
    CommandBuffer::ExecutesInline() ? VK_SUBPASS_CONTENTS_INLINE
                                    : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

// Helpers to unify executeCommands call based on underlying cmd buffer type
ANGLE_MAYBE_UNUSED
void ExecuteCommands(PrimaryCommandBuffer *primCmdBuffer,
                     priv::SecondaryCommandBuffer *secCmdBuffer)
{
    secCmdBuffer->executeCommands(primCmdBuffer->getHandle());
}

ANGLE_MAYBE_UNUSED
void ExecuteCommands(PrimaryCommandBuffer *primCmdBuffer, priv::CommandBuffer *secCmdBuffer)
{
    primCmdBuffer->executeCommands(1, secCmdBuffer);
}

ANGLE_MAYBE_UNUSED
std::string DumpCommands(const priv::SecondaryCommandBuffer &commandBuffer, const char *separator)
{
    return commandBuffer.dumpCommands(separator);
}

ANGLE_MAYBE_UNUSED
std::string DumpCommands(const priv::CommandBuffer &commandBuffer, const char *separator)
{
    return "--blob--";
}

}  // anonymous namespace

// CommandGraphResource implementation.
CommandGraphResource::CommandGraphResource(CommandGraphResourceType resourceType)
    : mCurrentWritingNode(nullptr), mResourceType(resourceType)
{}

CommandGraphResource::~CommandGraphResource() = default;

bool CommandGraphResource::isResourceInUse(RendererVk *renderer) const
{
    return renderer->isSerialInUse(mStoredQueueSerial);
}

angle::Result CommandGraphResource::recordCommands(Context *context,
                                                   CommandBuffer **commandBufferOut)
{
    updateQueueSerial(context->getRenderer()->getCurrentQueueSerial());

    if (!hasChildlessWritingNode() || hasStartedRenderPass())
    {
        startNewCommands(context->getRenderer());
        return mCurrentWritingNode->beginOutsideRenderPassRecording(
            context, context->getRenderer()->getCommandPool(), commandBufferOut);
    }

    CommandBuffer *outsideRenderPassCommands = mCurrentWritingNode->getOutsideRenderPassCommands();
    if (!outsideRenderPassCommands->valid())
    {
        ANGLE_TRY(mCurrentWritingNode->beginOutsideRenderPassRecording(
            context, context->getRenderer()->getCommandPool(), commandBufferOut));
    }
    else
    {
        *commandBufferOut = outsideRenderPassCommands;
    }

    return angle::Result::Continue;
}

angle::Result CommandGraphResource::beginRenderPass(
    ContextVk *contextVk,
    const Framebuffer &framebuffer,
    const gl::Rectangle &renderArea,
    const RenderPassDesc &renderPassDesc,
    const AttachmentOpsArray &renderPassAttachmentOps,
    const std::vector<VkClearValue> &clearValues,
    CommandBuffer **commandBufferOut)
{
    // If a barrier has been inserted in the meantime, stop the command buffer.
    if (!hasChildlessWritingNode())
    {
        startNewCommands(contextVk->getRenderer());
    }

    mCurrentWritingNode->storeRenderPassInfo(framebuffer, renderArea, renderPassDesc,
                                             renderPassAttachmentOps, clearValues);

    mCurrentWritingNode->setCommandBufferOwner(contextVk);

    return mCurrentWritingNode->beginInsideRenderPassRecording(contextVk, commandBufferOut);
}

void CommandGraphResource::addWriteDependency(CommandGraphResource *writingResource)
{
    CommandGraphNode *writingNode = writingResource->mCurrentWritingNode;
    ASSERT(writingNode);

    onWriteImpl(writingNode, writingResource->getStoredQueueSerial());
}

void CommandGraphResource::addReadDependency(CommandGraphResource *readingResource)
{
    updateQueueSerial(readingResource->getStoredQueueSerial());

    CommandGraphNode *readingNode = readingResource->mCurrentWritingNode;
    ASSERT(readingNode);

    if (mCurrentWritingNode)
    {
        // Ensure 'readingNode' happens after the current writing node.
        CommandGraphNode::SetHappensBeforeDependency(mCurrentWritingNode, readingNode);
    }

    // Add the read node to the list of nodes currently reading this resource.
    mCurrentReadingNodes.push_back(readingNode);
}

void CommandGraphResource::finishCurrentCommands(RendererVk *renderer)
{
    startNewCommands(renderer);
}

void CommandGraphResource::startNewCommands(RendererVk *renderer)
{
    CommandGraphNode *newCommands =
        renderer->getCommandGraph()->allocateNode(CommandGraphNodeFunction::Generic);
    newCommands->setDiagnosticInfo(mResourceType, reinterpret_cast<uintptr_t>(this));
    onWriteImpl(newCommands, renderer->getCurrentQueueSerial());
}

void CommandGraphResource::onWriteImpl(CommandGraphNode *writingNode, Serial currentSerial)
{
    updateQueueSerial(currentSerial);

    // Make sure any open reads and writes finish before we execute 'writingNode'.
    if (!mCurrentReadingNodes.empty())
    {
        CommandGraphNode::SetHappensBeforeDependencies(mCurrentReadingNodes.data(),
                                                       mCurrentReadingNodes.size(), writingNode);
        mCurrentReadingNodes.clear();
    }

    if (mCurrentWritingNode && mCurrentWritingNode != writingNode)
    {
        CommandGraphNode::SetHappensBeforeDependency(mCurrentWritingNode, writingNode);
    }

    mCurrentWritingNode = writingNode;
}

// CommandGraphNode implementation.
CommandGraphNode::CommandGraphNode(CommandGraphNodeFunction function,
                                   angle::PoolAllocator *poolAllocator)
    : mRenderPassClearValues{},
      mFunction(function),
      mPoolAllocator(poolAllocator),
      mQueryPool(VK_NULL_HANDLE),
      mQueryIndex(0),
      mFenceSyncEvent(VK_NULL_HANDLE),
      mHasChildren(false),
      mVisitedState(VisitedState::Unvisited),
      mGlobalMemoryBarrierSrcAccess(0),
      mGlobalMemoryBarrierDstAccess(0),
      mCommandBufferOwner(nullptr)
{}

CommandGraphNode::~CommandGraphNode()
{
    mRenderPassFramebuffer.setHandle(VK_NULL_HANDLE);
    // Command buffers are managed by the command pool, so don't need to be freed.
    mOutsideRenderPassCommands.releaseHandle();
    mInsideRenderPassCommands.releaseHandle();
}

angle::Result CommandGraphNode::beginOutsideRenderPassRecording(Context *context,
                                                                const CommandPool &commandPool,
                                                                CommandBuffer **commandsOut)
{
    ASSERT(!mHasChildren);

    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass  = VK_NULL_HANDLE;
    inheritanceInfo.subpass     = 0;
    inheritanceInfo.framebuffer = VK_NULL_HANDLE;
    inheritanceInfo.occlusionQueryEnable =
        CommandBuffer::SupportsQueries(context->getRenderer()->getPhysicalDeviceFeatures());
    inheritanceInfo.queryFlags         = 0;
    inheritanceInfo.pipelineStatistics = 0;

    ANGLE_TRY(InitAndBeginCommandBuffer(context, commandPool, inheritanceInfo, 0, mPoolAllocator,
                                        &mOutsideRenderPassCommands));

    *commandsOut = &mOutsideRenderPassCommands;
    return angle::Result::Continue;
}

angle::Result CommandGraphNode::beginInsideRenderPassRecording(Context *context,
                                                               CommandBuffer **commandsOut)
{
    ASSERT(!mHasChildren);

    // Get a compatible RenderPass from the cache so we can initialize the inheritance info.
    // TODO(jmadill): Support query for compatible/conformant render pass. http://anglebug.com/2361
    RenderPass *compatibleRenderPass;
    ANGLE_TRY(context->getRenderer()->getCompatibleRenderPass(context, mRenderPassDesc,
                                                              &compatibleRenderPass));

    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass  = compatibleRenderPass->getHandle();
    inheritanceInfo.subpass     = 0;
    inheritanceInfo.framebuffer = mRenderPassFramebuffer.getHandle();
    inheritanceInfo.occlusionQueryEnable =
        CommandBuffer::SupportsQueries(context->getRenderer()->getPhysicalDeviceFeatures());
    inheritanceInfo.queryFlags         = 0;
    inheritanceInfo.pipelineStatistics = 0;

    ANGLE_TRY(InitAndBeginCommandBuffer(context, context->getRenderer()->getCommandPool(),
                                        inheritanceInfo,
                                        VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
                                        mPoolAllocator, &mInsideRenderPassCommands));

    *commandsOut = &mInsideRenderPassCommands;
    return angle::Result::Continue;
}

void CommandGraphNode::storeRenderPassInfo(const Framebuffer &framebuffer,
                                           const gl::Rectangle renderArea,
                                           const vk::RenderPassDesc &renderPassDesc,
                                           const AttachmentOpsArray &renderPassAttachmentOps,
                                           const std::vector<VkClearValue> &clearValues)
{
    mRenderPassDesc          = renderPassDesc;
    mRenderPassAttachmentOps = renderPassAttachmentOps;
    mRenderPassFramebuffer.setHandle(framebuffer.getHandle());
    mRenderPassRenderArea = renderArea;
    std::copy(clearValues.begin(), clearValues.end(), mRenderPassClearValues.begin());
}

// static
void CommandGraphNode::SetHappensBeforeDependencies(CommandGraphNode **beforeNodes,
                                                    size_t beforeNodesCount,
                                                    CommandGraphNode *afterNode)
{
    afterNode->mParents.insert(afterNode->mParents.end(), beforeNodes,
                               beforeNodes + beforeNodesCount);

    // TODO(jmadill): is there a faster way to do this?
    for (size_t i = 0; i < beforeNodesCount; ++i)
    {
        beforeNodes[i]->setHasChildren();

        ASSERT(beforeNodes[i] != afterNode && !beforeNodes[i]->isChildOf(afterNode));
    }
}

void CommandGraphNode::SetHappensBeforeDependencies(CommandGraphNode *beforeNode,
                                                    CommandGraphNode **afterNodes,
                                                    size_t afterNodesCount)
{
    for (size_t i = 0; i < afterNodesCount; ++i)
    {
        SetHappensBeforeDependency(beforeNode, afterNodes[i]);
    }
}

bool CommandGraphNode::hasParents() const
{
    return !mParents.empty();
}

void CommandGraphNode::setQueryPool(const QueryPool *queryPool, uint32_t queryIndex)
{
    ASSERT(mFunction == CommandGraphNodeFunction::BeginQuery ||
           mFunction == CommandGraphNodeFunction::EndQuery ||
           mFunction == CommandGraphNodeFunction::WriteTimestamp);
    mQueryPool  = queryPool->getHandle();
    mQueryIndex = queryIndex;
}

void CommandGraphNode::setFenceSync(const vk::Event &event)
{
    ASSERT(mFunction == CommandGraphNodeFunction::SetFenceSync ||
           mFunction == CommandGraphNodeFunction::WaitFenceSync);
    mFenceSyncEvent = event.getHandle();
}

void CommandGraphNode::setDebugMarker(GLenum source, std::string &&marker)
{
    ASSERT(mFunction == CommandGraphNodeFunction::InsertDebugMarker ||
           mFunction == CommandGraphNodeFunction::PushDebugMarker);
    mDebugMarkerSource = source;
    mDebugMarker       = std::move(marker);
}

// Do not call this in anything but testing code, since it's slow.
bool CommandGraphNode::isChildOf(CommandGraphNode *parent)
{
    std::set<CommandGraphNode *> visitedList;
    std::vector<CommandGraphNode *> openList;
    openList.insert(openList.begin(), mParents.begin(), mParents.end());
    while (!openList.empty())
    {
        CommandGraphNode *current = openList.back();
        openList.pop_back();
        if (visitedList.count(current) == 0)
        {
            if (current == parent)
            {
                return true;
            }
            visitedList.insert(current);
            openList.insert(openList.end(), current->mParents.begin(), current->mParents.end());
        }
    }

    return false;
}

VisitedState CommandGraphNode::visitedState() const
{
    return mVisitedState;
}

void CommandGraphNode::visitParents(std::vector<CommandGraphNode *> *stack)
{
    ASSERT(mVisitedState == VisitedState::Unvisited);
    stack->insert(stack->end(), mParents.begin(), mParents.end());
    mVisitedState = VisitedState::Ready;
}

angle::Result CommandGraphNode::visitAndExecute(vk::Context *context,
                                                Serial serial,
                                                RenderPassCache *renderPassCache,
                                                PrimaryCommandBuffer *primaryCommandBuffer)
{
    switch (mFunction)
    {
        case CommandGraphNodeFunction::Generic:
            ASSERT(mQueryPool == VK_NULL_HANDLE && mFenceSyncEvent == VK_NULL_HANDLE);

            // Record the deferred pipeline barrier if necessary.
            ASSERT((mGlobalMemoryBarrierDstAccess == 0) == (mGlobalMemoryBarrierSrcAccess == 0));
            if (mGlobalMemoryBarrierSrcAccess)
            {
                VkMemoryBarrier memoryBarrier = {};
                memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memoryBarrier.srcAccessMask   = mGlobalMemoryBarrierSrcAccess;
                memoryBarrier.dstAccessMask   = mGlobalMemoryBarrierDstAccess;

                // Use the all pipe stage to keep the state management simple.
                primaryCommandBuffer->pipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                                                      &memoryBarrier, 0, nullptr, 0, nullptr);
            }

            if (mOutsideRenderPassCommands.valid())
            {
                ANGLE_VK_TRY(context, mOutsideRenderPassCommands.end());
                ExecuteCommands(primaryCommandBuffer, &mOutsideRenderPassCommands);
            }

            if (mInsideRenderPassCommands.valid())
            {
                // Pull a RenderPass from the cache.
                // TODO(jmadill): Insert layout transitions.
                RenderPass *renderPass = nullptr;
                ANGLE_TRY(renderPassCache->getRenderPassWithOps(
                    context, serial, mRenderPassDesc, mRenderPassAttachmentOps, &renderPass));

                ANGLE_VK_TRY(context, mInsideRenderPassCommands.end());

                VkRenderPassBeginInfo beginInfo = {};
                beginInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                beginInfo.renderPass            = renderPass->getHandle();
                beginInfo.framebuffer           = mRenderPassFramebuffer.getHandle();
                beginInfo.renderArea.offset.x   = static_cast<uint32_t>(mRenderPassRenderArea.x);
                beginInfo.renderArea.offset.y   = static_cast<uint32_t>(mRenderPassRenderArea.y);
                beginInfo.renderArea.extent.width =
                    static_cast<uint32_t>(mRenderPassRenderArea.width);
                beginInfo.renderArea.extent.height =
                    static_cast<uint32_t>(mRenderPassRenderArea.height);
                beginInfo.clearValueCount = mRenderPassDesc.attachmentCount();
                beginInfo.pClearValues    = mRenderPassClearValues.data();

                primaryCommandBuffer->beginRenderPass(beginInfo, kRenderPassContents);
                ExecuteCommands(primaryCommandBuffer, &mInsideRenderPassCommands);
                primaryCommandBuffer->endRenderPass();
            }
            break;

        case CommandGraphNodeFunction::BeginQuery:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());
            ASSERT(mQueryPool != VK_NULL_HANDLE);

            primaryCommandBuffer->resetQueryPool(mQueryPool, mQueryIndex, 1);
            primaryCommandBuffer->beginQuery(mQueryPool, mQueryIndex, 0);

            break;

        case CommandGraphNodeFunction::EndQuery:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());
            ASSERT(mQueryPool != VK_NULL_HANDLE);

            primaryCommandBuffer->endQuery(mQueryPool, mQueryIndex);

            break;

        case CommandGraphNodeFunction::WriteTimestamp:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());
            ASSERT(mQueryPool != VK_NULL_HANDLE);

            primaryCommandBuffer->resetQueryPool(mQueryPool, mQueryIndex, 1);
            primaryCommandBuffer->writeTimestamp(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mQueryPool,
                                                 mQueryIndex);

            break;

        case CommandGraphNodeFunction::SetFenceSync:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());
            ASSERT(mFenceSyncEvent != VK_NULL_HANDLE);

            primaryCommandBuffer->setEvent(mFenceSyncEvent, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

            break;

        case CommandGraphNodeFunction::WaitFenceSync:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());
            ASSERT(mFenceSyncEvent != VK_NULL_HANDLE);

            // Fence Syncs are purely execution barriers, so there are no memory barriers attached.
            primaryCommandBuffer->waitEvents(
                1, &mFenceSyncEvent, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, nullptr, 0, nullptr, 0, nullptr);

            break;

        case CommandGraphNodeFunction::InsertDebugMarker:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());

            if (vkCmdInsertDebugUtilsLabelEXT)
            {
                VkDebugUtilsLabelEXT label;
                MakeDebugUtilsLabel(mDebugMarkerSource, mDebugMarker.c_str(), &label);

                vkCmdInsertDebugUtilsLabelEXT(primaryCommandBuffer->getHandle(), &label);
            }
            break;

        case CommandGraphNodeFunction::PushDebugMarker:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());

            if (vkCmdBeginDebugUtilsLabelEXT)
            {
                VkDebugUtilsLabelEXT label;
                MakeDebugUtilsLabel(mDebugMarkerSource, mDebugMarker.c_str(), &label);

                vkCmdBeginDebugUtilsLabelEXT(primaryCommandBuffer->getHandle(), &label);
            }
            break;

        case CommandGraphNodeFunction::PopDebugMarker:
            ASSERT(!mOutsideRenderPassCommands.valid() && !mInsideRenderPassCommands.valid());

            if (vkCmdEndDebugUtilsLabelEXT)
            {
                vkCmdEndDebugUtilsLabelEXT(primaryCommandBuffer->getHandle());
            }
            break;

        default:
            UNREACHABLE();
    }

    mVisitedState = VisitedState::Visited;
    return angle::Result::Continue;
}

const std::vector<CommandGraphNode *> &CommandGraphNode::getParentsForDiagnostics() const
{
    return mParents;
}

void CommandGraphNode::setDiagnosticInfo(CommandGraphResourceType resourceType,
                                         uintptr_t resourceID)
{
    mResourceType = resourceType;
    mResourceID   = resourceID;
}

std::string CommandGraphNode::dumpCommandsForDiagnostics(const char *separator) const
{
    std::string result;
    if (mOutsideRenderPassCommands.valid())
    {
        result += separator;
        result += "Outside RP:";
        result += DumpCommands(mOutsideRenderPassCommands, separator);
    }
    if (mInsideRenderPassCommands.valid())
    {
        result += separator;
        result += "Inside RP:";
        result += DumpCommands(mInsideRenderPassCommands, separator);
    }
    return result;
}

// CommandGraph implementation.
CommandGraph::CommandGraph(bool enableGraphDiagnostics, angle::PoolAllocator *poolAllocator)
    : mEnableGraphDiagnostics(enableGraphDiagnostics),
      mPoolAllocator(poolAllocator),
      mLastBarrierIndex(kInvalidNodeIndex)
{
    // Push so that allocations made from here will be recycled in clear() below.
    mPoolAllocator->push();
}

CommandGraph::~CommandGraph()
{
    ASSERT(empty());
}

CommandGraphNode *CommandGraph::allocateNode(CommandGraphNodeFunction function)
{
    // TODO(jmadill): Use a pool allocator for the CPU node allocations.
    CommandGraphNode *newCommands = new CommandGraphNode(function, mPoolAllocator);
    mNodes.emplace_back(newCommands);
    return newCommands;
}

CommandGraphNode *CommandGraph::allocateBarrierNode(CommandGraphNodeFunction function,
                                                    CommandGraphResourceType resourceType,
                                                    uintptr_t resourceID)
{
    CommandGraphNode *newNode = allocateNode(function);
    newNode->setDiagnosticInfo(resourceType, resourceID);
    setNewBarrier(newNode);

    return newNode;
}

void CommandGraph::setNewBarrier(CommandGraphNode *newBarrier)
{
    size_t previousBarrierIndex       = 0;
    CommandGraphNode *previousBarrier = getLastBarrierNode(&previousBarrierIndex);

    // Add a dependency from previousBarrier to all nodes in (previousBarrier, newBarrier).
    if (previousBarrier && previousBarrierIndex + 1 < mNodes.size())
    {
        size_t afterNodesCount = mNodes.size() - (previousBarrierIndex + 2);
        CommandGraphNode::SetHappensBeforeDependencies(
            previousBarrier, &mNodes[previousBarrierIndex + 1], afterNodesCount);
    }

    // Add a dependency from all nodes in [previousBarrier, newBarrier) to newBarrier.
    addDependenciesToNextBarrier(previousBarrierIndex, mNodes.size() - 1, newBarrier);

    mLastBarrierIndex = mNodes.size() - 1;
}

angle::Result CommandGraph::submitCommands(Context *context,
                                           Serial serial,
                                           RenderPassCache *renderPassCache,
                                           CommandPool *commandPool,
                                           PrimaryCommandBuffer *primaryCommandBufferOut)
{
    // There is no point in submitting an empty command buffer, so make sure not to call this
    // function if there's nothing to do.
    ASSERT(!mNodes.empty());

    size_t previousBarrierIndex       = 0;
    CommandGraphNode *previousBarrier = getLastBarrierNode(&previousBarrierIndex);

    // Add a dependency from previousBarrier to all nodes in (previousBarrier, end].
    if (previousBarrier && previousBarrierIndex + 1 < mNodes.size())
    {
        size_t afterNodesCount = mNodes.size() - (previousBarrierIndex + 1);
        CommandGraphNode::SetHappensBeforeDependencies(
            previousBarrier, &mNodes[previousBarrierIndex + 1], afterNodesCount);
    }

    VkCommandBufferAllocateInfo primaryInfo = {};
    primaryInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    primaryInfo.commandPool                 = commandPool->getHandle();
    primaryInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    primaryInfo.commandBufferCount          = 1;

    ANGLE_VK_TRY(context, primaryCommandBufferOut->init(context->getDevice(), primaryInfo));

    if (mEnableGraphDiagnostics)
    {
        dumpGraphDotFile(std::cout);
    }

    std::vector<CommandGraphNode *> nodeStack;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;

    ANGLE_VK_TRY(context, primaryCommandBufferOut->begin(beginInfo));

    ANGLE_TRY(context->getRenderer()->traceGpuEvent(
        context, primaryCommandBufferOut, TRACE_EVENT_PHASE_BEGIN, "Primary Command Buffer"));

    for (CommandGraphNode *topLevelNode : mNodes)
    {
        // Only process commands that don't have child commands. The others will be pulled in
        // automatically. Also skip commands that have already been visited.
        if (topLevelNode->hasChildren() || topLevelNode->visitedState() != VisitedState::Unvisited)
            continue;

        nodeStack.push_back(topLevelNode);

        while (!nodeStack.empty())
        {
            CommandGraphNode *node = nodeStack.back();

            switch (node->visitedState())
            {
                case VisitedState::Unvisited:
                    node->visitParents(&nodeStack);
                    break;
                case VisitedState::Ready:
                    ANGLE_TRY(node->visitAndExecute(context, serial, renderPassCache,
                                                    primaryCommandBufferOut));
                    nodeStack.pop_back();
                    break;
                case VisitedState::Visited:
                    nodeStack.pop_back();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }
    }

    ANGLE_TRY(context->getRenderer()->traceGpuEvent(
        context, primaryCommandBufferOut, TRACE_EVENT_PHASE_END, "Primary Command Buffer"));

    ANGLE_VK_TRY(context, primaryCommandBufferOut->end());

    clear();

    return angle::Result::Continue;
}

bool CommandGraph::empty() const
{
    return mNodes.empty();
}

void CommandGraph::clear()
{
    mLastBarrierIndex = kInvalidNodeIndex;
    // Release cmd graph pool memory now that cmds are submitted
    // NOTE: This frees all memory since last push. Right now only the CommandGraph
    //  will push the allocator (at creation and below). If other people start
    //  pushing the allocator this (and/or the allocator) will need to be updated.
    mPoolAllocator->pop();
    mPoolAllocator->push();

    // TODO(jmadill): Use pool allocator for performance. http://anglebug.com/2951
    for (CommandGraphNode *node : mNodes)
    {
        delete node;
    }
    mNodes.clear();
}

void CommandGraph::beginQuery(const QueryPool *queryPool, uint32_t queryIndex)
{
    CommandGraphNode *newNode = allocateBarrierNode(CommandGraphNodeFunction::BeginQuery,
                                                    CommandGraphResourceType::Query, 0);
    newNode->setQueryPool(queryPool, queryIndex);
}

void CommandGraph::endQuery(const QueryPool *queryPool, uint32_t queryIndex)
{
    CommandGraphNode *newNode =
        allocateBarrierNode(CommandGraphNodeFunction::EndQuery, CommandGraphResourceType::Query, 0);
    newNode->setQueryPool(queryPool, queryIndex);
}

void CommandGraph::writeTimestamp(const QueryPool *queryPool, uint32_t queryIndex)
{
    CommandGraphNode *newNode = allocateBarrierNode(CommandGraphNodeFunction::WriteTimestamp,
                                                    CommandGraphResourceType::Query, 0);
    newNode->setQueryPool(queryPool, queryIndex);
}

void CommandGraph::setFenceSync(const vk::Event &event)
{
    CommandGraphNode *newNode = allocateBarrierNode(CommandGraphNodeFunction::SetFenceSync,
                                                    CommandGraphResourceType::FenceSync,
                                                    reinterpret_cast<uintptr_t>(&event));
    newNode->setFenceSync(event);
}

void CommandGraph::waitFenceSync(const vk::Event &event)
{
    CommandGraphNode *newNode = allocateBarrierNode(CommandGraphNodeFunction::WaitFenceSync,
                                                    CommandGraphResourceType::FenceSync,
                                                    reinterpret_cast<uintptr_t>(&event));
    newNode->setFenceSync(event);
}

void CommandGraph::insertDebugMarker(GLenum source, std::string &&marker)
{
    CommandGraphNode *newNode = allocateBarrierNode(CommandGraphNodeFunction::InsertDebugMarker,
                                                    CommandGraphResourceType::DebugMarker, 0);
    newNode->setDebugMarker(source, std::move(marker));
}

void CommandGraph::pushDebugMarker(GLenum source, std::string &&marker)
{
    CommandGraphNode *newNode = allocateBarrierNode(CommandGraphNodeFunction::PushDebugMarker,
                                                    CommandGraphResourceType::DebugMarker, 0);
    newNode->setDebugMarker(source, std::move(marker));
}

void CommandGraph::popDebugMarker()
{
    allocateBarrierNode(CommandGraphNodeFunction::PopDebugMarker,
                        CommandGraphResourceType::DebugMarker, 0);
}

// Dumps the command graph into a dot file that works with graphviz.
void CommandGraph::dumpGraphDotFile(std::ostream &out) const
{
    // This ID maps a node pointer to a monotonic ID. It allows us to look up parent node IDs.
    std::map<const CommandGraphNode *, int> nodeIDMap;
    std::map<uintptr_t, int> objectIDMap;
    std::map<std::pair<VkQueryPool, uint32_t>, int> queryIDMap;

    // Map nodes to ids.
    for (size_t nodeIndex = 0; nodeIndex < mNodes.size(); ++nodeIndex)
    {
        const CommandGraphNode *node = mNodes[nodeIndex];
        nodeIDMap[node]              = static_cast<int>(nodeIndex) + 1;
    }

    int bufferIDCounter      = 1;
    int framebufferIDCounter = 1;
    int imageIDCounter       = 1;
    int queryIDCounter       = 1;
    int fenceIDCounter       = 1;

    out << "digraph {" << std::endl;

    for (const CommandGraphNode *node : mNodes)
    {
        int nodeID = nodeIDMap[node];

        std::stringstream strstr;
        strstr << GetResourceTypeName(node->getResourceTypeForDiagnostics(), node->getFunction());

        if (node->getResourceTypeForDiagnostics() == CommandGraphResourceType::DebugMarker)
        {
            // For debug markers, use the string from the debug marker itself.
            if (node->getFunction() != CommandGraphNodeFunction::PopDebugMarker)
            {
                strstr << " " << node->getDebugMarker();
            }
        }
        else if (node->getResourceTypeForDiagnostics() == CommandGraphResourceType::Query)
        {
            // Special case for queries as they cannot generate a resource ID at creation time that
            // would reliably fit in a uintptr_t.
            strstr << " ";

            ASSERT(node->getResourceIDForDiagnostics() == 0);

            auto queryID = std::make_pair(node->getQueryPool(), node->getQueryIndex());

            auto it = queryIDMap.find(queryID);
            if (it != queryIDMap.end())
            {
                strstr << it->second;
            }
            else
            {
                int id = queryIDCounter++;

                queryIDMap[queryID] = id;
                strstr << id;
            }
        }
        else
        {
            strstr << " ";

            // Otherwise assign each object an ID, so all the nodes of the same object have the same
            // label.
            ASSERT(node->getResourceIDForDiagnostics() != 0);
            auto it = objectIDMap.find(node->getResourceIDForDiagnostics());
            if (it != objectIDMap.end())
            {
                strstr << it->second;
            }
            else
            {
                int id = 0;

                switch (node->getResourceTypeForDiagnostics())
                {
                    case CommandGraphResourceType::Buffer:
                        id = bufferIDCounter++;
                        break;
                    case CommandGraphResourceType::Framebuffer:
                        id = framebufferIDCounter++;
                        break;
                    case CommandGraphResourceType::Image:
                        id = imageIDCounter++;
                        break;
                    case CommandGraphResourceType::FenceSync:
                        id = fenceIDCounter++;
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }

                objectIDMap[node->getResourceIDForDiagnostics()] = id;
                strstr << id;
            }
        }

        const std::string &label = strstr.str();
        out << "  " << nodeID << "[label =<" << label << "<BR/><FONT POINT-SIZE=\"10\">Node ID "
            << nodeID << node->dumpCommandsForDiagnostics("<BR/>") << "</FONT>>];" << std::endl;
    }

    for (const CommandGraphNode *node : mNodes)
    {
        int nodeID = nodeIDMap[node];

        for (const CommandGraphNode *parent : node->getParentsForDiagnostics())
        {
            int parentID = nodeIDMap[parent];
            out << "  " << parentID << " -> " << nodeID << ";" << std::endl;
        }
    }

    out << "}" << std::endl;
}

CommandGraphNode *CommandGraph::getLastBarrierNode(size_t *indexOut)
{
    *indexOut = mLastBarrierIndex == kInvalidNodeIndex ? 0 : mLastBarrierIndex;
    return mLastBarrierIndex == kInvalidNodeIndex ? nullptr : mNodes[mLastBarrierIndex];
}

void CommandGraph::addDependenciesToNextBarrier(size_t begin,
                                                size_t end,
                                                CommandGraphNode *nextBarrier)
{
    for (size_t i = begin; i < end; ++i)
    {
        // As a small optimization, only add edges to childless nodes.  The others have an
        // indirect dependency.
        if (!mNodes[i]->hasChildren())
        {
            CommandGraphNode::SetHappensBeforeDependency(mNodes[i], nextBarrier);
        }
    }
}

}  // namespace vk
}  // namespace rx
