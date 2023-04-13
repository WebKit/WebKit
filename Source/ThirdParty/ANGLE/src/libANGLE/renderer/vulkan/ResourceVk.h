//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ResourceVk:
//    Resource lifetime tracking in the Vulkan back-end.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RESOURCEVK_H_
#define LIBANGLE_RENDERER_VULKAN_RESOURCEVK_H_

#include "libANGLE/HandleAllocator.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#include <queue>

namespace rx
{
namespace vk
{
// We expect almost all reasonable usage case should have at most 4 current contexts now. When
// exceeded, it should still work, but storage will grow.
static constexpr size_t kMaxFastQueueSerials = 4;
// Serials is an array of queue serials, which when paired with the index of the serials in the
// array result in QueueSerials. The array may expand if needed. Since it owned by Resource object
// which is protected by shared lock, it is safe to reallocate storage if needed. When it passes to
// renderer at garbage collection time, we will make a copy. The array size is expected to be small.
// But in future if we run into situation that array size is too big, we can change to packed array
// of QueueSerials.
using Serials = angle::FastVector<Serial, kMaxFastQueueSerials>;

// Tracks how a resource is used by ANGLE and by a VkQueue. The serial indicates the most recent use
// of a resource in the VkQueue. We use the monotonically incrementing serial number to determine if
// a resource is currently in use.
class ResourceUse final
{
  public:
    ResourceUse()  = default;
    ~ResourceUse() = default;

    ResourceUse(const QueueSerial &queueSerial) { setQueueSerial(queueSerial); }
    ResourceUse(const Serials &otherSerials) { mSerials = otherSerials; }

    // Copy constructor
    ResourceUse(const ResourceUse &other) : mSerials(other.mSerials) {}
    ResourceUse &operator=(const ResourceUse &other)
    {
        mSerials = other.mSerials;
        return *this;
    }

    // Move constructor
    ResourceUse(ResourceUse &&other) : mSerials(other.mSerials) { other.mSerials.clear(); }
    ResourceUse &operator=(ResourceUse &&other)
    {
        mSerials = other.mSerials;
        other.mSerials.clear();
        return *this;
    }

    bool valid() const { return mSerials.size() > 0; }

    void reset() { mSerials.clear(); }

    const Serials &getSerials() const { return mSerials; }

    void setSerial(SerialIndex index, Serial serial)
    {
        ASSERT(index != kInvalidQueueSerialIndex);
        if (ANGLE_UNLIKELY(mSerials.size() <= index))
        {
            mSerials.resize(index + 1, kZeroSerial);
        }
        ASSERT(mSerials[index] <= serial);
        mSerials[index] = serial;
    }

    void setQueueSerial(const QueueSerial &queueSerial)
    {
        setSerial(queueSerial.getIndex(), queueSerial.getSerial());
    }

    // Returns true if there is at least one serial is greater than
    bool operator>(const AtomicQueueSerialFixedArray &serials) const
    {
        ASSERT(mSerials.size() <= serials.size());
        for (SerialIndex i = 0; i < mSerials.size(); ++i)
        {
            if (mSerials[i] > serials[i])
            {
                return true;
            }
        }
        return false;
    }

    // Returns true if it contains a serial that is greater than
    bool operator>(const QueueSerial &queuSerial) const
    {
        return mSerials.size() > queuSerial.getIndex() &&
               mSerials[queuSerial.getIndex()] > queuSerial.getSerial();
    }

    // Returns true if all serials are less than or equal
    bool operator<=(const AtomicQueueSerialFixedArray &serials) const
    {
        ASSERT(mSerials.size() <= serials.size());
        for (SerialIndex i = 0; i < mSerials.size(); ++i)
        {
            if (mSerials[i] > serials[i])
            {
                return false;
            }
        }
        return true;
    }

    bool usedByCommandBuffer(const QueueSerial &commandBufferQueueSerial) const
    {
        ASSERT(commandBufferQueueSerial.valid());
        // Return true if we have the exact queue serial in the array.
        return mSerials.size() > commandBufferQueueSerial.getIndex() &&
               mSerials[commandBufferQueueSerial.getIndex()] ==
                   commandBufferQueueSerial.getSerial();
    }

    // Merge other's serials into this object.
    void merge(const ResourceUse &other)
    {
        if (mSerials.size() < other.mSerials.size())
        {
            mSerials.resize(other.mSerials.size(), kZeroSerial);
        }

        for (SerialIndex i = 0; i < other.mSerials.size(); ++i)
        {
            if (mSerials[i] < other.mSerials[i])
            {
                mSerials[i] = other.mSerials[i];
            }
        }
    }

  private:
    // The most recent time of use in a VkQueue.
    Serials mSerials;
};
std::ostream &operator<<(std::ostream &os, const ResourceUse &use);

class SharedGarbage
{
  public:
    SharedGarbage();
    SharedGarbage(SharedGarbage &&other);
    SharedGarbage(const ResourceUse &use, GarbageList &&garbage);
    ~SharedGarbage();
    SharedGarbage &operator=(SharedGarbage &&rhs);

    bool destroyIfComplete(RendererVk *renderer);
    bool hasResourceUseSubmitted(RendererVk *renderer) const;

  private:
    ResourceUse mLifetime;
    GarbageList mGarbage;
};

using SharedGarbageList = std::queue<SharedGarbage>;

// This is a helper class for back-end objects used in Vk command buffers. They keep a record
// of their use in ANGLE and VkQueues via ResourceUse.
class Resource : angle::NonCopyable
{
  public:
    virtual ~Resource() {}

    // Complete all recorded and in-flight commands involving this resource
    angle::Result waitForIdle(ContextVk *contextVk,
                              const char *debugMessage,
                              RenderPassClosureReason reason);

    void setSerial(SerialIndex index, Serial serial) { mUse.setSerial(index, serial); }

    void setQueueSerial(const QueueSerial &queueSerial)
    {
        mUse.setSerial(queueSerial.getIndex(), queueSerial.getSerial());
    }

    void mergeResourceUse(const ResourceUse &use) { mUse.merge(use); }

    // Check if this resource is used by a command buffer.
    bool usedByCommandBuffer(const QueueSerial &commandBufferQueueSerial) const
    {
        return mUse.usedByCommandBuffer(commandBufferQueueSerial);
    }

    const ResourceUse &getResourceUse() const { return mUse; }

  protected:
    Resource() {}
    Resource(Resource &&other) : Resource() { mUse = std::move(other.mUse); }
    Resource &operator=(Resource &&rhs)
    {
        std::swap(mUse, rhs.mUse);
        return *this;
    }

    // Current resource lifetime.
    ResourceUse mUse;
};

// Similar to |Resource| above, this tracks object usage. This includes additional granularity to
// track whether an object is used for read-only or read/write access.
class ReadWriteResource : public Resource
{
  public:
    virtual ~ReadWriteResource() override {}

    // Complete all recorded and in-flight commands involving this resource
    angle::Result waitForIdle(ContextVk *contextVk,
                              const char *debugMessage,
                              RenderPassClosureReason reason)
    {
        return Resource::waitForIdle(contextVk, debugMessage, reason);
    }

    void setWriteQueueSerial(const QueueSerial &writeQueueSerial)
    {
        mUse.setQueueSerial(writeQueueSerial);
        mWriteUse.setQueueSerial(writeQueueSerial);
    }

    // Check if this resource is used by a command buffer.
    bool usedByCommandBuffer(const QueueSerial &commandBufferQueueSerial) const
    {
        return mUse.usedByCommandBuffer(commandBufferQueueSerial);
    }
    bool writtenByCommandBuffer(const QueueSerial &commandBufferQueueSerial) const
    {
        return mWriteUse.usedByCommandBuffer(commandBufferQueueSerial);
    }

    const ResourceUse &getWriteResourceUse() const { return mWriteUse; }

  protected:
    ReadWriteResource() {}
    ReadWriteResource(ReadWriteResource &&other) { *this = std::move(other); }
    ReadWriteResource &operator=(ReadWriteResource &&other)
    {
        Resource::operator=(std::move(other));
        mWriteUse = std::move(other.mWriteUse);
        return *this;
    }

    // Track write use of the object. Only updated for setWriteQueueSerial().
    ResourceUse mWriteUse;
};

// Adds "void release(RendererVk *)" method for collecting garbage.
// Enables RendererScoped<> for classes that support DeviceScoped<>.
template <class T>
class ReleasableResource final : public Resource
{
  public:
    // Calls collectGarbage() on the object.
    void release(RendererVk *renderer);

    const T &get() const { return mObject; }
    T &get() { return mObject; }

  private:
    T mObject;
};
}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RESOURCEVK_H_
