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

constexpr VkPipelineStageFlags kPreFragmentStageFlags =
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;

constexpr VkPipelineStageFlags kAllShadersPipelineStageFlags =
    kPreFragmentStageFlags | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

constexpr VkPipelineStageFlags kAllDepthStencilPipelineStageFlags =
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

constexpr VkPipelineStageFlags kFragmentAndAttachmentPipelineStageFlags =
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

// We group VK_PIPELINE_STAGE_*_BITs into different groups. The expectation is that execution within
// Fragment/PreFragment/Compute will not overlap. This information is used to optimize the usage of
// VkEvent where we try to not use it when we know that it will not provide benefits over
// pipelineBarriers.
enum class PipelineStageGroup : uint8_t
{
    Other,
    PreFragmentOnly,
    FragmentOnly,
    ComputeOnly,

    InvalidEnum,
    EnumCount = InvalidEnum,
};

class PipelineStageAccessHeuristic final
{
  public:
    constexpr PipelineStageAccessHeuristic() = default;
    constexpr PipelineStageAccessHeuristic(PipelineStageGroup pipelineStageGroup)
    {
        for (size_t i = 0; i < kHeuristicWindowSize; i++)
        {
            mHeuristicBits <<= kPipelineStageGroupBitShift;
            mHeuristicBits |= ToUnderlying(pipelineStageGroup);
        }
    }
    void onAccess(PipelineStageGroup pipelineStageGroup)
    {
        mHeuristicBits <<= kPipelineStageGroupBitShift;
        mHeuristicBits |= ToUnderlying(pipelineStageGroup);
    }
    constexpr bool operator==(const PipelineStageAccessHeuristic &other) const
    {
        return mHeuristicBits == other.mHeuristicBits;
    }

  private:
    static constexpr size_t kPipelineStageGroupBitShift = 2;
    static_assert(ToUnderlying(PipelineStageGroup::EnumCount) <=
                  (1 << kPipelineStageGroupBitShift));
    static constexpr size_t kHeuristicWindowSize = 8;
    angle::BitSet16<kHeuristicWindowSize * kPipelineStageGroupBitShift> mHeuristicBits;
};
static constexpr PipelineStageAccessHeuristic kPipelineStageAccessFragmentOnly =
    PipelineStageAccessHeuristic(PipelineStageGroup::FragmentOnly);
static constexpr PipelineStageAccessHeuristic kPipelineStageAccessComputeOnly =
    PipelineStageAccessHeuristic(PipelineStageGroup::ComputeOnly);
static constexpr PipelineStageAccessHeuristic kPipelineStageAccessPreFragmentOnly =
    PipelineStageAccessHeuristic(PipelineStageGroup::PreFragmentOnly);

// Enum for predefined VkPipelineStageFlags set that VkEvent will be using. Because VkEvent has
// strict rules that waitEvent and setEvent must have matching VkPipelineStageFlags, it is desirable
// to keep VkEvent per VkPipelineStageFlags combination. This enum table enumerates all possible
// pipeline stage combinations that VkEvent used with. The enum maps to VkPipelineStageFlags via
// Renderer::getPipelineStageMask call.
enum class EventStage : uint32_t
{
    Transfer                                          = 0,
    VertexShader                                      = 1,
    FragmentShader                                    = 2,
    ComputeShader                                     = 3,
    AllShaders                                        = 4,
    PreFragmentShaders                                = 5,
    FragmentShadingRate                               = 6,
    ColorAttachmentOutput                             = 7,
    ColorAttachmentOutputAndFragmentShader            = 8,
    ColorAttachmentOutputAndFragmentShaderAndTransfer = 9,
    ColorAttachmentOutputAndAllShaders                = 10,
    AllFragmentTest                                   = 11,
    AllFragmentTestAndFragmentShader                  = 12,
    AllFragmentTestAndAllShaders                      = 13,
    TransferAndComputeShader                          = 14,
    InvalidEnum                                       = 15,
    EnumCount                                         = InvalidEnum,
};

// Initialize EventStage to VkPipelineStageFlags mapping table.
void InitializeEventAndPipelineStagesMap(
    angle::PackedEnumMap<EventStage, VkPipelineStageFlags> *mapping,
    VkPipelineStageFlags supportedVulkanPipelineStageMask);

// VkCmdWaitEvents requires srcStageMask must be the bitwise OR of the stageMask parameter used in
// previous calls to vkCmdSetEvent (See VUID-vkCmdWaitEvents-srcStageMask-01158). This mean we must
// keep the record of what stageMask each event has been used in VkCmdSetEvent call so that we can
// retrieve that information when we need to wait for the event. Instead of keeping just stageMask
// here, we keep the ImageLayout for now which gives us more information for debugging.
struct EventAndStage
{
    bool valid() const { return event.valid(); }
    Event event;
    EventStage eventStage;
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
    bool init(Context *context, EventStage eventStage);

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

    // Only intended for assertion in recycler
    bool validAndNoReference() const { return mHandle != nullptr && !mHandle->isReferenced(); }

    // Returns the underlying Event object
    const Event &getEvent() const
    {
        ASSERT(valid());
        return mHandle->get().event;
    }

    EventStage getEventStage() const
    {
        ASSERT(mHandle != nullptr);
        return mHandle->get().eventStage;
    }

  private:
    // Release one reference count to the underline Event object and destroy or recycle the handle
    // to the provided recycler if this is the very last reference.
    friend class RefCountedEventsGarbage;
    template <typename RecyclerT>
    void releaseImpl(Renderer *renderer, RecyclerT *recycler);

    RefCounted<EventAndStage> *mHandle;
};
using RefCountedEventCollector = std::deque<RefCountedEvent>;

// Tracks a list of RefCountedEvents per EventStage.
struct EventMaps
{
    angle::PackedEnumMap<EventStage, RefCountedEvent> map;
    // The mask is used to accelerate the loop of map
    angle::PackedEnumBitSet<EventStage, uint64_t> mask;
    // Only used by RenderPassCommandBufferHelper
    angle::PackedEnumMap<EventStage, VkEvent> vkEvents;
};

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
    RefCountedEventsGarbage() = default;
    ~RefCountedEventsGarbage() { ASSERT(mRefCountedEvents.empty()); }

    RefCountedEventsGarbage(const QueueSerial &queueSerial,
                            RefCountedEventCollector &&refCountedEvents)
        : mQueueSerial(queueSerial), mRefCountedEvents(std::move(refCountedEvents))
    {
        ASSERT(!mRefCountedEvents.empty());
    }

    void destroy(Renderer *renderer);

    // Check the queue serial and release the events to recycler if GPU finished.
    bool releaseIfComplete(Renderer *renderer, RefCountedEventsGarbageRecycler *recycler);

    // Check the queue serial and move all events to releasedBucket if GPU finished. This is only
    // used by RefCountedEventRecycler.
    bool moveIfComplete(Renderer *renderer, std::deque<RefCountedEventCollector> *releasedBucket);

    bool empty() const { return mRefCountedEvents.empty(); }

    size_t size() const { return mRefCountedEvents.size(); }

  private:
    QueueSerial mQueueSerial;
    RefCountedEventCollector mRefCountedEvents;
};

// Two levels of RefCountedEvents recycle system: For the performance reason, we have two levels of
// events recycler system. The first level is per ShareGroupVk, which owns RefCountedEventRecycler.
// RefCountedEvent garbage is added to it without any lock. Once GPU complete, the refCount is
// decremented. When the last refCount goes away, it goes into mEventsToReset. Note that since
// ShareGoupVk access is already protected by context share lock at the API level, so no lock is
// taken and reference counting is not atomic. At RefCountedEventsGarbageRecycler::cleanup time, the
// entire mEventsToReset is added into renderer's list. The renderer owns RefCountedEventRecycler
// list, and all access to it is protected with simple mutex lock. When any context calls
// OutsideRenderPassCommandBufferHelper::flushToPrimary, mEventsToReset is retrieved from renderer
// and the reset commands is added to the command buffer. The events are then moved to the
// renderer's garbage list. They are checked and along with renderer's garbage cleanup and if
// completed, they get moved to renderer's mEventsToReuse list. When a RefCountedEvent is needed, we
// always dip into ShareGroupVk's mEventsToReuse list. If its empty, it then dip into renderer's
// mEventsToReuse and grab a collector of events and try to reuse. That way the traffic into
// renderer is minimized as most of calls will be contained in SHareGroupVk.

// Thread safe event recycler, protected by its own lock.
class RefCountedEventRecycler final
{
  public:
    RefCountedEventRecycler() {}
    ~RefCountedEventRecycler()
    {
        ASSERT(mEventsToReset.empty());
        ASSERT(mResettingQueue.empty());
        ASSERT(mEventsToReuse.empty());
    }

    void destroy(VkDevice device);

    // Add single event to the toReset list
    void recycle(RefCountedEvent &&garbageObject)
    {
        ASSERT(garbageObject.validAndNoReference());
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        if (mEventsToReset.empty())
        {
            mEventsToReset.emplace_back();
        }
        mEventsToReset.back().emplace_back(std::move(garbageObject));
    }

    // Add a list of events to the toReset list
    void recycle(RefCountedEventCollector &&garbageObjects)
    {
        ASSERT(!garbageObjects.empty());
        for (const RefCountedEvent &event : garbageObjects)
        {
            ASSERT(event.validAndNoReference());
        }
        std::lock_guard<angle::SimpleMutex> lock(mMutex);
        mEventsToReset.emplace_back(std::move(garbageObjects));
    }

    // Reset all events in the toReset list and move them to the toReuse list
    void resetEvents(Context *context,
                     const QueueSerial queueSerial,
                     PrimaryCommandBuffer *commandbuffer);

    // Clean up the resetting event list and move completed events to the toReuse list.
    void cleanupResettingEvents(Renderer *renderer);

    // Fetch a list of events that are ready to be reused. Returns true if eventsToReuseOut is
    // returned.
    bool fetchEventsToReuse(RefCountedEventCollector *eventsToReuseOut);

  private:
    angle::SimpleMutex mMutex;
    // RefCountedEvent list that has been released, needs to be reset.
    std::deque<RefCountedEventCollector> mEventsToReset;
    // RefCountedEvent list that is currently resetting.
    std::queue<RefCountedEventsGarbage> mResettingQueue;
    // RefCountedEvent list that already has been reset. Ready to be reused.
    std::deque<RefCountedEventCollector> mEventsToReuse;
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
        ASSERT(garbageObject.validAndNoReference());
        mEventsToReset.emplace_back(std::move(garbageObject));
    }

    bool fetch(Renderer *renderer, RefCountedEvent *outObject);

    size_t getGarbageCount() const { return mGarbageCount; }

  private:
    RefCountedEventCollector mEventsToReset;
    std::queue<RefCountedEventsGarbage> mGarbageQueue;
    Recycler<RefCountedEvent> mEventsToReuse;
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

    void addMemoryEvent(Renderer *renderer,
                        const RefCountedEvent &waitEvent,
                        VkPipelineStageFlags dstStageMask,
                        VkAccessFlags dstAccess);

    void addImageEvent(Renderer *renderer,
                       const RefCountedEvent &waitEvent,
                       VkPipelineStageFlags dstStageMask,
                       const VkImageMemoryBarrier &imageMemoryBarrier);

    void reset() { ASSERT(mBarriers.empty()); }

    void addDiagnosticsString(std::ostringstream &out) const;

  private:
    std::deque<EventBarrier> mBarriers;
};
}  // namespace vk
}  // namespace rx
#endif  // LIBANGLE_RENDERER_VULKAN_REFCOUNTED_EVENT_H_
