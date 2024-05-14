//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RefCountedEvent:
//    Manages reference count of VkEvent and its associated functions.
//

#include "libANGLE/renderer/vulkan/vk_ref_counted_event.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

namespace rx
{
namespace vk
{
void RefCountedEvent::init(Context *context, ImageLayout layout)
{
    ASSERT(mHandle == nullptr);
    ASSERT(layout != ImageLayout::Undefined);

    mHandle                      = new AtomicRefCounted<EventAndLayout>;
    VkEventCreateInfo createInfo = {};
    createInfo.sType             = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    // Use device only for performance reasons.
    createInfo.flags = context->getFeatures().supportsSynchronization2.enabled
                           ? VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR
                           : 0;
    mHandle->get().event.init(context->getDevice(), createInfo);
    mHandle->addRef();
    mHandle->get().imageLayout = layout;
}

// RefCountedEventsGarbage implementation.
bool RefCountedEventsGarbage::destroyIfComplete(Renderer *renderer)
{
    if (renderer->hasResourceUseFinished(mLifetime))
    {
        for (RefCountedEvent &event : mRefCountedEvents)
        {
            ASSERT(event.valid());
            event.release(renderer->getDevice());
            ASSERT(!event.valid());
        }
        mRefCountedEvents.clear();
        return true;
    }
    return false;
}

bool RefCountedEventsGarbage::hasResourceUseSubmitted(Renderer *renderer) const
{
    return renderer->hasResourceUseSubmitted(mLifetime);
}

// EventBarrier implementation.
bool EventBarrier::hasEvent(const VkEvent &event) const
{
    for (const VkEvent &existingEvent : mEvents)
    {
        if (existingEvent == event)
        {
            return true;
        }
    }
    return false;
}

void EventBarrier::addDiagnosticsString(std::ostringstream &out) const
{
    if (mMemoryBarrierSrcAccess != 0 || mMemoryBarrierDstAccess != 0)
    {
        out << "Src: 0x" << std::hex << mMemoryBarrierSrcAccess << " &rarr; Dst: 0x" << std::hex
            << mMemoryBarrierDstAccess << std::endl;
    }
}

void EventBarrier::execute(PrimaryCommandBuffer *primary)
{
    if (isEmpty())
    {
        return;
    }

    // Issue vkCmdWaitEvents call
    VkMemoryBarrier memoryBarrier = {};
    uint32_t memoryBarrierCount   = 0;
    if (mMemoryBarrierDstAccess != 0)
    {
        memoryBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = mMemoryBarrierSrcAccess;
        memoryBarrier.dstAccessMask = mMemoryBarrierDstAccess;
        memoryBarrierCount++;
    }

    primary->waitEvents(static_cast<uint32_t>(mEvents.size()), mEvents.data(), mSrcStageMask,
                        mDstStageMask, memoryBarrierCount, &memoryBarrier, 0, nullptr,
                        static_cast<uint32_t>(mImageMemoryBarriers.size()),
                        mImageMemoryBarriers.data());

    reset();
}

// EventBarrierArray implementation.
void EventBarrierArray::addMemoryEvent(Context *context,
                                       const RefCountedEvent &waitEvent,
                                       VkPipelineStageFlags dstStageMask,
                                       VkAccessFlags dstAccess)
{
    ASSERT(waitEvent.valid());

    for (EventBarrier &barrier : mBarriers)
    {
        // If the event is already in the waiting list, just add the new stageMask to the
        // dstStageMask. Otherwise we will end up with two waitEvent calls that wait for the same
        // VkEvent but for different dstStage and confuses VVL.
        if (barrier.hasEvent(waitEvent.getEvent().getHandle()))
        {
            barrier.addAdditionalStageAccess(dstStageMask, dstAccess);
            return;
        }
    }

    VkAccessFlags accessMask;
    VkPipelineStageFlags stageFlags = GetRefCountedEventStageMask(context, waitEvent, &accessMask);
    // Since this is used with WAW without layout change, dstStageMask should be the same as event's
    // stageMask. Otherwise you should get into addImageEvent.
    ASSERT(stageFlags == dstStageMask && accessMask == dstAccess);
    mBarriers.emplace_back(stageFlags, dstStageMask, accessMask, dstAccess,
                           waitEvent.getEvent().getHandle());
}

void EventBarrierArray::addImageEvent(Context *context,
                                      const RefCountedEvent &waitEvent,
                                      VkPipelineStageFlags dstStageMask,
                                      const VkImageMemoryBarrier &imageMemoryBarrier)
{
    ASSERT(waitEvent.valid());
    VkPipelineStageFlags srcStageFlags = GetRefCountedEventStageMask(context, waitEvent);

    mBarriers.emplace_back();
    EventBarrier &barrier = mBarriers.back();
    // VkCmdWaitEvent must uses the same stageMask as VkCmdSetEvent due to
    // VUID-vkCmdWaitEvents-srcStageMask-01158 requirements.
    barrier.mSrcStageMask = srcStageFlags;
    // If there is an event, we use the waitEvent to do layout change.
    barrier.mEvents.emplace_back(waitEvent.getEvent().getHandle());
    barrier.mDstStageMask = dstStageMask;
    barrier.mImageMemoryBarriers.emplace_back(imageMemoryBarrier);
}

void EventBarrierArray::execute(Renderer *renderer, PrimaryCommandBuffer *primary)
{
    if (mBarriers.empty())
    {
        return;
    }

    for (EventBarrier &barrier : mBarriers)
    {
        barrier.execute(primary);
    }
    mBarriers.clear();
}

void EventBarrierArray::addDiagnosticsString(std::ostringstream &out) const
{
    out << "Event Barrier: ";
    for (const EventBarrier &barrier : mBarriers)
    {
        barrier.addDiagnosticsString(out);
    }
    out << "\\l";
}
}  // namespace vk
}  // namespace rx
