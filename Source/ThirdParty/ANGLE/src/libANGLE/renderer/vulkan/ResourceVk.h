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
// Estimated maximum command buffers in a normal session. Beyond this we will lose performance.
constexpr size_t kMaxFastCommandBuffers = 64;

struct CommandBufferID
{
    GLuint value;
};

inline bool operator==(CommandBufferID lhs, CommandBufferID rhs)
{
    return lhs.value == rhs.value;
}

using CommandBufferHandleAllocator = gl::HandleAllocator;

// Tracks open command buffers for a resource.
// We could optimize this with a hybrid packed bitset/list for applications that use many contexts.
using ResourceCommandBuffers = angle::FlatUnorderedSet<CommandBufferID, kMaxFastCommandBuffers>;

// Tracks how a resource is used by ANGLE and by a VkQueue. The reference count indicates the number
// of times a resource is retained by ANGLE. The serial indicates the most recent use of a resource
// in the VkQueue. The reference count and serial together can determine if a resource is currently
// in use.
struct ResourceUse
{
    ResourceUse() = default;

    // The number of times a resource is retained by a Context/ShareGroup/Renderer.
    uint32_t counter = 0;

    // Open command buffers using this resource.
    ResourceCommandBuffers commandBuffers;

    // The most recent time of use in a VkQueue.
    Serial serial;
};

class SharedResourceUse final : angle::NonCopyable
{
  public:
    SharedResourceUse() : mUse(nullptr) {}
    ~SharedResourceUse() { ASSERT(!valid()); }
    SharedResourceUse(SharedResourceUse &&rhs) : mUse(rhs.mUse) { rhs.mUse = nullptr; }
    SharedResourceUse &operator=(SharedResourceUse &&rhs)
    {
        std::swap(mUse, rhs.mUse);
        return *this;
    }

    ANGLE_INLINE bool valid() const { return mUse != nullptr; }

    void init()
    {
        ASSERT(!mUse);
        mUse = new ResourceUse;
        mUse->counter++;
    }

    // Specifically for use with command buffers that are used as one-offs.
    void updateSerialOneOff(Serial serial) { mUse->serial = serial; }

    ANGLE_INLINE void release()
    {
        ASSERT(valid());
        ASSERT(mUse->counter > 0);
        if (--mUse->counter == 0)
        {
            delete mUse;
        }
        mUse = nullptr;
    }

    ANGLE_INLINE void releaseAndUpdateSerial(Serial serial)
    {
        ASSERT(valid());
        ASSERT(mUse->counter > 0);
        ASSERT(mUse->serial <= serial);
        mUse->serial = serial;
        release();
    }

    ANGLE_INLINE void set(const SharedResourceUse &rhs)
    {
        ASSERT(rhs.valid());
        ASSERT(!valid());
        ASSERT(rhs.mUse->counter < std::numeric_limits<uint32_t>::max());
        mUse = rhs.mUse;
        mUse->counter++;
    }

    // The base counter value for a live resource is "1". Any value greater than one indicates
    // the resource is in use by a command buffer.
    ANGLE_INLINE bool usedInRecordedCommands() const
    {
        ASSERT(valid());
        return mUse->counter > 1;
    }

    ANGLE_INLINE bool usedInRunningCommands(Serial lastCompletedSerial) const
    {
        ASSERT(valid());
        return mUse->serial > lastCompletedSerial;
    }

    ANGLE_INLINE bool isCurrentlyInUse(Serial lastCompletedSerial) const
    {
        return usedInRecordedCommands() || usedInRunningCommands(lastCompletedSerial);
    }

    ANGLE_INLINE Serial getSerial() const
    {
        ASSERT(valid());
        return mUse->serial;
    }

    ANGLE_INLINE void setCommandBuffer(CommandBufferID commandBufferID)
    {
        if (!mUse->commandBuffers.contains(commandBufferID))
        {
            mUse->commandBuffers.insert(commandBufferID);
        }
    }

    ANGLE_INLINE void clearCommandBuffer(CommandBufferID commandBufferID)
    {
        if (mUse->commandBuffers.contains(commandBufferID))
        {
            mUse->commandBuffers.remove(commandBufferID);
        }
    }

    ANGLE_INLINE bool usedByCommandBuffer(CommandBufferID commandBufferID) const
    {
        return mUse->commandBuffers.contains(commandBufferID);
    }

  private:
    ResourceUse *mUse;
};

class SharedBufferSuballocationGarbage
{
  public:
    SharedBufferSuballocationGarbage() = default;
    SharedBufferSuballocationGarbage(SharedBufferSuballocationGarbage &&other)
        : mLifetime(std::move(other.mLifetime)),
          mSuballocation(std::move(other.mSuballocation)),
          mBuffer(std::move(other.mBuffer))
    {}
    SharedBufferSuballocationGarbage(SharedResourceUse &&use,
                                     BufferSuballocation &&suballocation,
                                     Buffer &&buffer)
        : mLifetime(std::move(use)),
          mSuballocation(std::move(suballocation)),
          mBuffer(std::move(buffer))
    {}
    ~SharedBufferSuballocationGarbage() = default;

    bool destroyIfComplete(RendererVk *renderer, Serial completedSerial);
    bool usedInRecordedCommands() const { return mLifetime.usedInRecordedCommands(); }
    VkDeviceSize getSize() const { return mSuballocation.getSize(); }
    bool isSuballocated() const { return mSuballocation.isSuballocated(); }

  private:
    SharedResourceUse mLifetime;
    BufferSuballocation mSuballocation;
    Buffer mBuffer;
};
using SharedBufferSuballocationGarbageList = std::queue<SharedBufferSuballocationGarbage>;

class SharedGarbage
{
  public:
    SharedGarbage();
    SharedGarbage(SharedGarbage &&other);
    SharedGarbage(SharedResourceUse &&use, std::vector<GarbageObject> &&garbage);
    ~SharedGarbage();
    SharedGarbage &operator=(SharedGarbage &&rhs);

    bool destroyIfComplete(RendererVk *renderer, Serial completedSerial);
    bool usedInRecordedCommands() const { return mLifetime.usedInRecordedCommands(); }

  private:
    SharedResourceUse mLifetime;
    std::vector<GarbageObject> mGarbage;
};

using SharedGarbageList = std::queue<SharedGarbage>;

// Mixin to abstract away the resource use tracking.
class ResourceUseList final : angle::NonCopyable
{
  public:
    ResourceUseList();
    ResourceUseList(ResourceUseList &&other);
    virtual ~ResourceUseList();
    ResourceUseList &operator=(ResourceUseList &&rhs);

    void add(const SharedResourceUse &resourceUse);

    void releaseResourceUses();
    void releaseResourceUsesAndUpdateSerials(Serial serial);

    void clearCommandBuffer(CommandBufferID commandBufferID);

    bool empty() { return mResourceUses.empty(); }

  private:
    std::vector<SharedResourceUse> mResourceUses;
};

ANGLE_INLINE void ResourceUseList::add(const SharedResourceUse &resourceUse)
{
    SharedResourceUse newUse;
    newUse.set(resourceUse);
    mResourceUses.emplace_back(std::move(newUse));
}

// This is a helper class for back-end objects used in Vk command buffers. They keep a record
// of their use in ANGLE and VkQueues via SharedResourceUse.
class Resource : angle::NonCopyable
{
  public:
    virtual ~Resource();

    // Returns true if the resource is used by ANGLE in an unflushed command buffer.
    bool usedInRecordedCommands() const { return mUse.usedInRecordedCommands(); }

    // Determine if the driver has finished execution with this resource.
    bool usedInRunningCommands(Serial lastCompletedSerial) const
    {
        return mUse.usedInRunningCommands(lastCompletedSerial);
    }

    // Returns true if the resource is in use by ANGLE or the driver.
    bool isCurrentlyInUse(Serial lastCompletedSerial) const
    {
        return mUse.isCurrentlyInUse(lastCompletedSerial);
    }

    // Ensures the driver is caught up to this resource and it is only in use by ANGLE.
    angle::Result finishRunningCommands(ContextVk *contextVk);

    // Complete all recorded and in-flight commands involving this resource
    angle::Result waitForIdle(ContextVk *contextVk,
                              const char *debugMessage,
                              RenderPassClosureReason reason);

    // Adds the resource to a resource use list.
    void retain(ResourceUseList *resourceUseList);

    // Adds the resource to the list and also records command buffer use.
    void retainCommands(CommandBufferID commandBufferID, ResourceUseList *resourceUseList);

    // Check if this resource is used by a command buffer.
    bool usedByCommandBuffer(CommandBufferID commandBufferID) const
    {
        return mUse.usedByCommandBuffer(commandBufferID);
    }

  protected:
    Resource();
    Resource(Resource &&other);
    Resource &operator=(Resource &&rhs);

    // Current resource lifetime.
    SharedResourceUse mUse;
};

ANGLE_INLINE void Resource::retain(ResourceUseList *resourceUseList)
{
    // Store reference in resource list.
    resourceUseList->add(mUse);
}

ANGLE_INLINE void Resource::retainCommands(CommandBufferID commandBufferID,
                                           ResourceUseList *resourceUseList)
{
    mUse.setCommandBuffer(commandBufferID);

    // Store reference in resource list.
    resourceUseList->add(mUse);
}

// Similar to |Resource| above, this tracks object usage. This includes additional granularity to
// track whether an object is used for read-only or read/write access.
class ReadWriteResource : public angle::NonCopyable
{
  public:
    virtual ~ReadWriteResource();

    // Returns true if the resource is used by ANGLE in an unflushed command buffer.
    bool usedInRecordedCommands() const { return mReadOnlyUse.usedInRecordedCommands(); }

    // Determine if the driver has finished execution with this resource.
    bool usedInRunningCommands(Serial lastCompletedSerial) const
    {
        return mReadOnlyUse.usedInRunningCommands(lastCompletedSerial);
    }

    // Returns true if the resource is in use by ANGLE or the driver.
    bool isCurrentlyInUse(Serial lastCompletedSerial) const
    {
        return mReadOnlyUse.isCurrentlyInUse(lastCompletedSerial);
    }
    bool isCurrentlyInUseForWrite(Serial lastCompletedSerial) const
    {
        return mReadWriteUse.isCurrentlyInUse(lastCompletedSerial);
    }

    // Ensures the driver is caught up to this resource and it is only in use by ANGLE.
    angle::Result finishRunningCommands(ContextVk *contextVk);

    // Ensures the GPU write commands is completed.
    angle::Result finishGPUWriteCommands(ContextVk *contextVk);

    // Complete all recorded and in-flight commands involving this resource
    angle::Result waitForIdle(ContextVk *contextVk,
                              const char *debugMessage,
                              RenderPassClosureReason reason);

    // Adds the resource to a resource use list.
    void retainReadOnly(CommandBufferID commandBufferID, ResourceUseList *resourceUseList);
    void retainReadWrite(CommandBufferID commandBufferID, ResourceUseList *resourceUseList);

    // Retain to a one-shot resource use list.
    void retainReadOnlyOneOff(ResourceUseList *resourceUseList);

    // Check if this resource is used by a command buffer.
    bool usedByCommandBuffer(CommandBufferID commandBufferID) const
    {
        return mReadOnlyUse.usedByCommandBuffer(commandBufferID);
    }

    bool writtenByCommandBuffer(CommandBufferID commandBufferID) const
    {
        return mReadWriteUse.usedByCommandBuffer(commandBufferID);
    }

  protected:
    ReadWriteResource();
    ReadWriteResource(ReadWriteResource &&other);
    ReadWriteResource &operator=(ReadWriteResource &&other);

    // Track any use of the object. Always updated on every retain call.
    SharedResourceUse mReadOnlyUse;
    // Track read/write use of the object. Only updated for retainReadWrite().
    SharedResourceUse mReadWriteUse;
};

ANGLE_INLINE void ReadWriteResource::retainReadOnly(CommandBufferID commandBufferID,
                                                    ResourceUseList *resourceUseList)
{
    // Store reference in resource list.
    resourceUseList->add(mReadOnlyUse);
    mReadOnlyUse.setCommandBuffer(commandBufferID);
}

ANGLE_INLINE void ReadWriteResource::retainReadWrite(CommandBufferID commandBufferID,
                                                     ResourceUseList *resourceUseList)
{
    // Store reference in resource list.
    resourceUseList->add(mReadOnlyUse);
    resourceUseList->add(mReadWriteUse);
    mReadOnlyUse.setCommandBuffer(commandBufferID);
    mReadWriteUse.setCommandBuffer(commandBufferID);
}

ANGLE_INLINE void ReadWriteResource::retainReadOnlyOneOff(ResourceUseList *resourceUseList)
{
    resourceUseList->add(mReadOnlyUse);
}

}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RESOURCEVK_H_
