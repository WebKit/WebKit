//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RefCountedEvent:
//    Manages reference count of VkEvent and its associated functions.
//

#ifndef LIBANGLE_RENDERER_VULKAN_REFCOUNTED_EVENT_H_
#define LIBANGLE_RENDERER_VULKAN_REFCOUNTED_EVENT_H_

#include <atomic>
#include <limits>
#include <queue>

#include "common/PackedEnums.h"
#include "common/debug.h"
#include "libANGLE/renderer/serial_utils.h"
#include "libANGLE/renderer/vulkan/vk_resource.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"
#include "libANGLE/renderer/vulkan/vk_wrapper.h"

namespace rx
{
namespace vk
{
enum class ImageLayout;

// There are two ways to implement a barrier: Using VkCmdPipelineBarrier or VkCmdWaitEvents. The
// BarrierType enum will be passed around to indicate which barrier caller want to use.
enum class BarrierType
{
    Pipeline,
    Event,
};

// VkCmdWaitEvents requires srcStageMask must be the bitwise OR of the stageMask parameter used in
// previous calls to vkCmdSetEvent (See VUID-vkCmdWaitEvents-srcStageMask-01158). This mean we must
// keep the record of what stageMask each event has been used in VkCmdSetEvent call so that we can
// retrieve that information when we need to wait for the event. Instead of keeping just stageMask
// here, we keep the ImageLayout for now which gives us more information for debugging.
struct EventAndLayout
{
    bool valid() const { return event.valid(); }
    Event event;
    ImageLayout imageLayout;
};

// The VkCmdSetEvent is called after VkCmdEndRenderPass and all images that used at the given
// pipeline stage (i.e, they have the same stageMask) will be tracked by the same event. This means
// there will be multiple objects pointing to the same event. Events are thus reference counted so
// that we do not destroy it while other objects still referencing to it.
class RefCountedEvent final
{
  public:
    RefCountedEvent() { mHandle = nullptr; }
    ~RefCountedEvent() { ASSERT(mHandle == nullptr); }

    // Move constructor moves reference of the underline object from other to this.
    RefCountedEvent(RefCountedEvent &&other)
    {
        mHandle       = other.mHandle;
        other.mHandle = nullptr;
    }

    // Copy constructor adds reference to the underline object.
    RefCountedEvent(const RefCountedEvent &other)
    {
        ASSERT(other.valid());
        mHandle = other.mHandle;
        mHandle->addRef();
    }

    // Move assignment moves reference of the underline object from other to this.
    RefCountedEvent &operator=(RefCountedEvent &&other)
    {
        ASSERT(!valid());
        ASSERT(other.valid());
        std::swap(mHandle, other.mHandle);
        return *this;
    }

    // Copy assignment adds reference to the underline object.
    RefCountedEvent &operator=(const RefCountedEvent &other)
    {
        ASSERT(!valid());
        ASSERT(other.valid());
        mHandle = other.mHandle;
        mHandle->addRef();
        return *this;
    }

    // Returns true if both points to the same underline object.
    bool operator==(const RefCountedEvent &other) const { return mHandle == other.mHandle; }

    // Create VkEvent and associated it with given layout
    void init(Context *context, ImageLayout layout);

    // Release one reference count to the underline Event object and destroy if this is the
    // very last reference.
    void release(VkDevice device)
    {
        if (mHandle != nullptr)
        {
            const bool isLastReference = mHandle->getAndReleaseRef() == 1;
            if (isLastReference)
            {
                destroy(device);
            }
            else
            {
                mHandle = nullptr;
            }
        }
    }

    bool valid() const { return mHandle != nullptr; }

    // Returns the underlying Event object
    const Event &getEvent() const
    {
        ASSERT(valid());
        return mHandle->get().event;
    }

    // Returns the ImageLayout associated with the event.
    ImageLayout getImageLayout() const
    {
        ASSERT(valid());
        return mHandle->get().imageLayout;
    }

  private:
    void destroy(VkDevice device)
    {
        ASSERT(mHandle != nullptr);
        ASSERT(!mHandle->isReferenced());
        mHandle->get().event.destroy(device);
        SafeDelete(mHandle);
    }

    AtomicRefCounted<EventAndLayout> *mHandle;
};
using RefCountedEventCollector = std::vector<RefCountedEvent>;

// This class tracks a vector of RefcountedEvent garbage. For performance reason, instead of
// individually tracking each VkEvent garbage, we collect all events that are accessed in the
// CommandBufferHelper into this class. After we submit the command buffer, we treat this vector of
// events as one garbage object and add it to renderer's garbage list. The garbage clean up will
// decrement the refCount and destroy event only when last refCount goes away. Basically all GPU
// usage will use one refCount and that refCount ensures we never destroy event until GPU is
// finished.
class RefCountedEventsGarbage final
{
  public:
    RefCountedEventsGarbage()  = default;
    ~RefCountedEventsGarbage() = default;

    RefCountedEventsGarbage(const QueueSerial &queueSerial,
                            RefCountedEventCollector &&refCountedEvents)
        : mLifetime(queueSerial), mRefCountedEvents(std::move(refCountedEvents))
    {
        ASSERT(refCountedEvents.empty());
        ASSERT(!mRefCountedEvents.empty());
    }

    RefCountedEventsGarbage(RefCountedEventsGarbage &&other)
        : mLifetime(other.mLifetime), mRefCountedEvents(std::move(other.mRefCountedEvents))
    {}

    RefCountedEventsGarbage &operator=(RefCountedEventsGarbage &&other)
    {
        ASSERT(mRefCountedEvents.empty());
        mLifetime         = other.mLifetime;
        mRefCountedEvents = std::move(other.mRefCountedEvents);
        return *this;
    }

    bool destroyIfComplete(Renderer *renderer);
    bool hasResourceUseSubmitted(Renderer *renderer) const;
    VkDeviceSize getSize() const { return mRefCountedEvents.size(); }

    // Move event to the garbage list
    void add(RefCountedEvent &&event) { mRefCountedEvents.emplace_back(std::move(event)); }

    // Move the vector of events to the garbage list
    void add(RefCountedEventCollector &&events)
    {
        mRefCountedEvents.reserve(mRefCountedEvents.size() + events.size());
        for (RefCountedEvent &event : events)
        {
            mRefCountedEvents.emplace_back(std::move(event));
        }
        events.clear();
    }

    // Make a copy of event (which adds another refcount to the VkEvent) and add the copied event to
    // the garbages
    void add(const RefCountedEvent &event)
    {
        RefCountedEvent localEventCopy = event;
        mRefCountedEvents.emplace_back(std::move(localEventCopy));
        ASSERT(!localEventCopy.valid());
        ASSERT(event.valid());
    }

    bool empty() const { return mRefCountedEvents.empty(); }

  private:
    ResourceUse mLifetime;
    RefCountedEventCollector mRefCountedEvents;
};

// This wraps data and API for vkCmdWaitEvent call
class EventBarrier : angle::NonCopyable
{
  public:
    EventBarrier()
        : mSrcStageMask(0), mDstStageMask(0), mMemoryBarrierSrcAccess(0), mMemoryBarrierDstAccess(0)
    {}

    EventBarrier(VkPipelineStageFlags srcStageMask,
                 VkPipelineStageFlags dstStageMask,
                 VkAccessFlags srcAccess,
                 VkAccessFlags dstAccess,
                 const VkEvent &event)
        : mSrcStageMask(srcStageMask),
          mDstStageMask(dstStageMask),
          mMemoryBarrierSrcAccess(srcAccess),
          mMemoryBarrierDstAccess(dstAccess)
    {
        mEvents.push_back(event);
    }

    EventBarrier(VkPipelineStageFlags srcStageMask,
                 VkPipelineStageFlags dstStageMask,
                 const VkEvent &event,
                 const VkImageMemoryBarrier &imageMemoryBarrier)
        : mSrcStageMask(srcStageMask),
          mDstStageMask(dstStageMask),
          mMemoryBarrierSrcAccess(0),
          mMemoryBarrierDstAccess(0)
    {
        ASSERT(event != VK_NULL_HANDLE);
        ASSERT(imageMemoryBarrier.pNext == nullptr);
        mEvents.push_back(event);
        mImageMemoryBarriers.push_back(imageMemoryBarrier);
    }

    EventBarrier(EventBarrier &&other)
    {
        mSrcStageMask           = other.mSrcStageMask;
        mDstStageMask           = other.mDstStageMask;
        mMemoryBarrierSrcAccess = other.mMemoryBarrierSrcAccess;
        mMemoryBarrierDstAccess = other.mMemoryBarrierDstAccess;
        std::swap(mEvents, other.mEvents);
        std::swap(mImageMemoryBarriers, other.mImageMemoryBarriers);
        other.mSrcStageMask           = 0;
        other.mDstStageMask           = 0;
        other.mMemoryBarrierSrcAccess = 0;
        other.mMemoryBarrierDstAccess = 0;
    }

    ~EventBarrier()
    {
        ASSERT(mImageMemoryBarriers.empty());
        ASSERT(mEvents.empty());
    }

    bool isEmpty() const
    {
        return mEvents.empty() && mImageMemoryBarriers.empty() && mMemoryBarrierDstAccess == 0;
    }

    bool hasEvent(const VkEvent &event) const;

    void addAdditionalStageAccess(VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccess)
    {
        mDstStageMask |= dstStageMask;
        mMemoryBarrierDstAccess |= dstAccess;
    }

    void execute(PrimaryCommandBuffer *primary);

    void reset()
    {
        mEvents.clear();
        mImageMemoryBarriers.clear();
    }

    void addDiagnosticsString(std::ostringstream &out) const;

  private:
    friend class EventBarrierArray;
    VkPipelineStageFlags mSrcStageMask;
    VkPipelineStageFlags mDstStageMask;
    VkAccessFlags mMemoryBarrierSrcAccess;
    VkAccessFlags mMemoryBarrierDstAccess;
    std::vector<VkEvent> mEvents;
    std::vector<VkImageMemoryBarrier> mImageMemoryBarriers;
};

class EventBarrierArray final
{
  public:
    bool isEmpty() const { return mBarriers.empty(); }

    void execute(Renderer *renderer, PrimaryCommandBuffer *primary);

    void addMemoryEvent(Context *context,
                        const RefCountedEvent &waitEvent,
                        VkPipelineStageFlags dstStageMask,
                        VkAccessFlags dstAccess);

    void addImageEvent(Context *context,
                       const RefCountedEvent &waitEvent,
                       VkPipelineStageFlags dstStageMask,
                       const VkImageMemoryBarrier &imageMemoryBarrier);

    void reset() { mBarriers.clear(); }

    void addDiagnosticsString(std::ostringstream &out) const;

  private:
    std::vector<EventBarrier> mBarriers;
};

VkPipelineStageFlags GetRefCountedEventStageMask(Context *context, const RefCountedEvent &event);
VkPipelineStageFlags GetRefCountedEventStageMask(Context *context,
                                                 const RefCountedEvent &event,
                                                 VkAccessFlags *accessMask);
}  // namespace vk
}  // namespace rx
#endif  // LIBANGLE_RENDERER_VULKAN_REFCOUNTED_EVENT_H_
