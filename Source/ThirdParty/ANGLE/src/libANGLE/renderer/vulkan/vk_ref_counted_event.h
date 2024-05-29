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
#include "common/SimpleMutex.h"
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
    bool needsReset;
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

    // Create VkEvent and associated it with given layout. Returns true if success and false if
    // failed.
    bool init(Context *context, ImageLayout layout);

    // Release one reference count to the underline Event object and destroy or recycle the handle
    // to renderer's recycler if this is the very last reference.
    void release(Renderer *renderer);

    // Release one reference count to the underline Event object and destroy or recycle the handle
    // to the context share group's recycler if this is the very last reference.
    void release(Context *context);

    // Destroy the event and mHandle. Caller must ensure there is no outstanding reference to the
    // mHandle.
    void destroy(VkDevice device);

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

    bool needsReset() const
    {
        ASSERT(valid());
        return mHandle->get().needsReset;
    }

  private:
    // Release one reference count to the underline Event object and destroy or recycle the handle
    // to the provided recycler if this is the very last reference.
    friend class RefCountedEventRecycler;
    friend class RefCountedEventsGarbage;
    friend class RefCountedEventsGarbageRecycler;
    template <typename RecyclerT>
    void releaseImpl(Renderer *renderer, RecyclerT *recycler);

    RefCounted<EventAndLayout> *mHandle;
};
using RefCountedEventCollector = std::deque<RefCountedEvent>;

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
        : mQueueSerial(queueSerial), mRefCountedEvents(std::move(refCountedEvents))
    {
        ASSERT(refCountedEvents.empty());
        ASSERT(!mRefCountedEvents.empty());
    }

    RefCountedEventsGarbage(RefCountedEventsGarbage &&other)
        : mQueueSerial(other.mQueueSerial), mRefCountedEvents(std::move(other.mRefCountedEvents))
    {}

    RefCountedEventsGarbage &operator=(RefCountedEventsGarbage &&other)
    {
        ASSERT(mRefCountedEvents.empty());
        mQueueSerial      = other.mQueueSerial;
        mRefCountedEvents = std::move(other.mRefCountedEvents);
        return *this;
    }

    void destroy(Renderer *renderer);

    // Check the queue serial and release the events to context if GPU finished. Note that release
    // to context may end up recycle the object instead of destroy. Returns true if it is GPU
    // finished.
    bool releaseIfComplete(Renderer *renderer, RefCountedEventsGarbageRecycler *recycler);

    // Move event to the garbage list
    void add(RefCountedEvent &&event) { mRefCountedEvents.emplace_back(std::move(event)); }

    // Move the vector of events to the garbage list
    void add(RefCountedEventCollector &&events)
    {
        mRefCountedEvents.insert(mRefCountedEvents.end(), events.begin(), events.end());
        ASSERT(events.empty());
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

    size_t size() const { return mRefCountedEvents.size(); }

  private:
    friend class RefCountedEventsGarbageRecycler;
    QueueSerial mQueueSerial;
    RefCountedEventCollector mRefCountedEvents;
};

// Thread safe event recycler, protected by its own lock.
class RefCountedEventRecycler final
{
  public:
    void recycle(RefCountedEvent &&garbageObject)
    {
        ASSERT(garbageObject.valid());
        ASSERT(!garbageObject.mHandle->isReferenced());
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        mFreeStack.recycle(std::move(garbageObject));
    }

    void releaseOrRecycle(Renderer *renderer, RefCountedEventCollector &&eventCollector)
    {
        // Take lock once and then use event's releaseImpl function to directly recycle into the
        // underlying recycling storage.
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        while (!eventCollector.empty())
        {
            eventCollector.back().releaseImpl(renderer, &mFreeStack);
            eventCollector.pop_back();
        }
    }

    bool fetch(RefCountedEvent *outObject)
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        if (mFreeStack.empty())
        {
            return false;
        }
        mFreeStack.fetch(outObject);
        ASSERT(outObject->valid());
        ASSERT(!outObject->mHandle->isReferenced());
        return true;
    }

    void destroy(VkDevice device)
    {
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        mFreeStack.destroy(device);
    }

  private:
    angle::SimpleMutex mMutex;
    Recycler<RefCountedEvent> mFreeStack;
};

// Not thread safe event garbage collection and recycler. Caller must ensure the thread safety. It
// is intended to use by ShareGroupVk which all access should already protected by share context
// lock.
class RefCountedEventsGarbageRecycler final
{
  public:
    RefCountedEventsGarbageRecycler() : mGarbageCount(0) {}
    ~RefCountedEventsGarbageRecycler();

    // Release all garbage and free events.
    void destroy(Renderer *renderer);

    // Walk the garbage list and move completed garbage to free list
    void cleanup(Renderer *renderer);

    void collectGarbage(const QueueSerial &queueSerial, RefCountedEventCollector &&refCountedEvents)
    {
        mGarbageCount += refCountedEvents.size();
        mGarbageQueue.emplace(queueSerial, std::move(refCountedEvents));
    }

    void recycle(RefCountedEvent &&garbageObject)
    {
        ASSERT(garbageObject.valid());
        ASSERT(!garbageObject.mHandle->isReferenced());
        mFreeStack.recycle(std::move(garbageObject));
    }

    bool fetch(RefCountedEvent *outObject);

    size_t getGarbageCount() const { return mGarbageCount; }

  private:
    Recycler<RefCountedEvent> mFreeStack;
    std::queue<RefCountedEventsGarbage> mGarbageQueue;
    size_t mGarbageCount;
};

// This wraps data and API for vkCmdWaitEvent call
class EventBarrier : angle::NonCopyable
{
  public:
    EventBarrier()
        : mSrcStageMask(0),
          mDstStageMask(0),
          mMemoryBarrierSrcAccess(0),
          mMemoryBarrierDstAccess(0),
          mImageMemoryBarrierCount(0),
          mEvent(VK_NULL_HANDLE)
    {}

    EventBarrier(VkPipelineStageFlags srcStageMask,
                 VkPipelineStageFlags dstStageMask,
                 VkAccessFlags srcAccess,
                 VkAccessFlags dstAccess,
                 const VkEvent &event)
        : mSrcStageMask(srcStageMask),
          mDstStageMask(dstStageMask),
          mMemoryBarrierSrcAccess(srcAccess),
          mMemoryBarrierDstAccess(dstAccess),
          mImageMemoryBarrierCount(0),
          mEvent(event)
    {
        ASSERT(mEvent != VK_NULL_HANDLE);
    }

    EventBarrier(VkPipelineStageFlags srcStageMask,
                 VkPipelineStageFlags dstStageMask,
                 const VkEvent &event,
                 const VkImageMemoryBarrier &imageMemoryBarrier)
        : mSrcStageMask(srcStageMask),
          mDstStageMask(dstStageMask),
          mMemoryBarrierSrcAccess(0),
          mMemoryBarrierDstAccess(0),
          mImageMemoryBarrierCount(1),
          mEvent(event),
          mImageMemoryBarrier(imageMemoryBarrier)
    {
        ASSERT(mEvent != VK_NULL_HANDLE);
        ASSERT(mImageMemoryBarrier.image != VK_NULL_HANDLE);
        ASSERT(mImageMemoryBarrier.pNext == nullptr);
    }

    EventBarrier(EventBarrier &&other)
    {
        mSrcStageMask            = other.mSrcStageMask;
        mDstStageMask            = other.mDstStageMask;
        mMemoryBarrierSrcAccess  = other.mMemoryBarrierSrcAccess;
        mMemoryBarrierDstAccess  = other.mMemoryBarrierDstAccess;
        mImageMemoryBarrierCount = other.mImageMemoryBarrierCount;
        std::swap(mEvent, other.mEvent);
        std::swap(mImageMemoryBarrier, other.mImageMemoryBarrier);
        other.mSrcStageMask            = 0;
        other.mDstStageMask            = 0;
        other.mMemoryBarrierSrcAccess  = 0;
        other.mMemoryBarrierDstAccess  = 0;
        other.mImageMemoryBarrierCount = 0;
    }

    ~EventBarrier() {}

    bool isEmpty() const { return mEvent == VK_NULL_HANDLE; }

    bool hasEvent(const VkEvent &event) const { return mEvent == event; }

    void addAdditionalStageAccess(VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccess)
    {
        mDstStageMask |= dstStageMask;
        mMemoryBarrierDstAccess |= dstAccess;
    }

    void execute(PrimaryCommandBuffer *primary);

    void addDiagnosticsString(std::ostringstream &out) const;

  private:
    friend class EventBarrierArray;
    VkPipelineStageFlags mSrcStageMask;
    VkPipelineStageFlags mDstStageMask;
    VkAccessFlags mMemoryBarrierSrcAccess;
    VkAccessFlags mMemoryBarrierDstAccess;
    uint32_t mImageMemoryBarrierCount;
    VkEvent mEvent;
    VkImageMemoryBarrier mImageMemoryBarrier;
};

class EventBarrierArray final
{
  public:
    bool isEmpty() const { return mBarriers.empty(); }

    void execute(Renderer *renderer, PrimaryCommandBuffer *primary);

    // Add the additional stageMask to the existing waitEvent.
    void addAdditionalStageAccess(const RefCountedEvent &waitEvent,
                                  VkPipelineStageFlags dstStageMask,
                                  VkAccessFlags dstAccess);

    void addMemoryEvent(Context *context,
                        const RefCountedEvent &waitEvent,
                        VkPipelineStageFlags dstStageMask,
                        VkAccessFlags dstAccess);

    void addImageEvent(Context *context,
                       const RefCountedEvent &waitEvent,
                       VkPipelineStageFlags dstStageMask,
                       const VkImageMemoryBarrier &imageMemoryBarrier);

    void reset() { ASSERT(mBarriers.empty()); }

    void addDiagnosticsString(std::ostringstream &out) const;

  private:
    std::deque<EventBarrier> mBarriers;
};

VkPipelineStageFlags GetRefCountedEventStageMask(Context *context, const RefCountedEvent &event);
VkPipelineStageFlags GetRefCountedEventStageMask(Context *context,
                                                 const RefCountedEvent &event,
                                                 VkAccessFlags *accessMask);
}  // namespace vk
}  // namespace rx
#endif  // LIBANGLE_RENDERER_VULKAN_REFCOUNTED_EVENT_H_
