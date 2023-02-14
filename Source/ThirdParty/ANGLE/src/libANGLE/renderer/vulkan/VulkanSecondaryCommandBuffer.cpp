//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanSecondaryCommandBuffer:
//    Implementation of VulkanSecondaryCommandBuffer.
//

#include "libANGLE/renderer/vulkan/VulkanSecondaryCommandBuffer.h"

#include "common/debug.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{
namespace vk
{
angle::Result VulkanSecondaryCommandBuffer::InitializeCommandPool(Context *context,
                                                                  CommandPool *pool,
                                                                  uint32_t queueFamilyIndex,
                                                                  ProtectionType protectionType)
{
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex        = queueFamilyIndex;
    ASSERT(protectionType == ProtectionType::Unprotected ||
           protectionType == ProtectionType::Protected);
    if (protectionType == ProtectionType::Protected)
    {
        poolInfo.flags |= VK_COMMAND_POOL_CREATE_PROTECTED_BIT;
    }
    ANGLE_VK_TRY(context, pool->init(context->getDevice(), poolInfo));
    return angle::Result::Continue;
}

angle::Result VulkanSecondaryCommandBuffer::InitializeRenderPassInheritanceInfo(
    ContextVk *contextVk,
    const Framebuffer &framebuffer,
    const RenderPassDesc &renderPassDesc,
    VkCommandBufferInheritanceInfo *inheritanceInfoOut)
{
    const vk::RenderPass *compatibleRenderPass = nullptr;
    ANGLE_TRY(contextVk->getCompatibleRenderPass(renderPassDesc, &compatibleRenderPass));

    inheritanceInfoOut->sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfoOut->renderPass  = compatibleRenderPass->getHandle();
    inheritanceInfoOut->subpass     = 0;
    inheritanceInfoOut->framebuffer = framebuffer.getHandle();

    return angle::Result::Continue;
}

angle::Result VulkanSecondaryCommandBuffer::initialize(Context *context,
                                                       vk::CommandPool *pool,
                                                       bool isRenderPassCommandBuffer,
                                                       SecondaryCommandMemoryAllocator *allocator)
{
    VkDevice device = context->getDevice();

    mCommandTracker.reset();
    mAnyCommand = false;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount          = 1;
    allocInfo.commandPool                 = pool->getHandle();

    ANGLE_VK_TRY(context, init(device, allocInfo));

    // Outside-RP command buffers are begun automatically here.  RP command buffers are begun when
    // the render pass itself starts, as they require inheritance info.
    if (!isRenderPassCommandBuffer)
    {
        VkCommandBufferInheritanceInfo inheritanceInfo = {};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        ANGLE_TRY(begin(context, inheritanceInfo));
    }

    return angle::Result::Continue;
}

angle::Result VulkanSecondaryCommandBuffer::begin(
    Context *context,
    const VkCommandBufferInheritanceInfo &inheritanceInfo)
{
    ASSERT(!mAnyCommand);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = &inheritanceInfo;
    if (inheritanceInfo.renderPass != VK_NULL_HANDLE)
    {
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    }

    ANGLE_VK_TRY(context, CommandBuffer::begin(beginInfo));
    return angle::Result::Continue;
}

angle::Result VulkanSecondaryCommandBuffer::end(Context *context)
{
    ANGLE_VK_TRY(context, CommandBuffer::end());
    return angle::Result::Continue;
}

VkResult VulkanSecondaryCommandBuffer::reset()
{
    mCommandTracker.reset();
    mAnyCommand = false;
    return CommandBuffer::reset();
}
}  // namespace vk
}  // namespace rx
