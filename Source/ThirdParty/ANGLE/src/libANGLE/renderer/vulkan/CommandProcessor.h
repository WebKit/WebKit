//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CommandProcessor.h:
//    A class to process and submit Vulkan command buffers that can be
//    used in an asynchronous worker thread.
//

#ifndef LIBANGLE_RENDERER_VULKAN_COMMAND_PROCESSOR_H_
#define LIBANGLE_RENDERER_VULKAN_COMMAND_PROCESSOR_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "common/FixedQueue.h"
#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/vulkan/PersistentCommandPool.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class RendererVk;
class CommandProcessor;

namespace vk
{
constexpr size_t kMaxCommandProcessorTasksLimit = 16u;
constexpr size_t kInFlightCommandsLimit         = 50u;

enum class SubmitPolicy
{
    AllowDeferred,
    EnsureSubmitted,
};

class FenceRecycler;
// This is a RAII class manages refcounted vkfence object with auto-release and recycling.
class SharedFence final
{
  public:
    SharedFence();
    SharedFence(const SharedFence &other);
    SharedFence(SharedFence &&other);
    ~SharedFence();
    // Copy assignment will add reference count to the underline object
    SharedFence &operator=(const SharedFence &other);
    // Move assignment will move reference count from other to this object
    SharedFence &operator=(SharedFence &&other);

    // Initialize it with a new vkFence either from recycler or create a new one.
    VkResult init(VkDevice device, FenceRecycler *recycler);
    // Destroy it immediately (will not recycle).
    void destroy(VkDevice device);
    // Release the vkFence (to recycler)
    void release();
    // Return true if underline VkFence is valid
    operator bool() const;
    const Fence &get() const
    {
        ASSERT(mRefCountedFence != nullptr && mRefCountedFence->isReferenced());
        return mRefCountedFence->get();
    }

    // The following three APIs can call without lock. Since fence is refcounted and this object has
    // a refcount to VkFence, No one is able to come in and destroy the VkFence.
    VkResult getStatus(VkDevice device) const;
    VkResult wait(VkDevice device, uint64_t timeout) const;

  private:
    RefCounted<Fence> *mRefCountedFence;
    FenceRecycler *mRecycler;
};

class FenceRecycler
{
  public:
    FenceRecycler() {}
    ~FenceRecycler() {}
    void destroy(Context *context);

    void fetch(VkDevice device, Fence *fenceOut);
    void recycle(Fence &&fence);

  private:
    std::mutex mMutex;
    Recycler<Fence> mRecyler;
};

struct SwapchainStatus
{
    mutable std::mutex mutex;
    mutable std::condition_variable condVar;
    bool isPending = false;

    VkResult lastPresentResult = VK_NOT_READY;
};

enum class CustomTask
{
    Invalid = 0,
    // Flushes wait semaphores
    FlushWaitSemaphores,
    // Process SecondaryCommandBuffer commands into the primary CommandBuffer.
    ProcessOutsideRenderPassCommands,
    ProcessRenderPassCommands,
    // End the current command buffer and submit commands to the queue
    FlushAndQueueSubmit,
    // Submit custom command buffer, excludes some state management
    OneOffQueueSubmit,
    // Execute QueuePresent
    Present,
};

// CommandProcessorTask interface
class CommandProcessorTask
{
  public:
    CommandProcessorTask() { initTask(); }

    void initTask();

    void initOutsideRenderPassProcessCommands(ProtectionType protectionType,
                                              OutsideRenderPassCommandBufferHelper *commandBuffer);

    void initRenderPassProcessCommands(ProtectionType protectionType,
                                       RenderPassCommandBufferHelper *commandBuffer,
                                       const RenderPass *renderPass);

    void initPresent(egl::ContextPriority priority,
                     const VkPresentInfoKHR &presentInfo,
                     SwapchainStatus *swapchainStatus);

    void initFlushWaitSemaphores(ProtectionType protectionType,
                                 std::vector<VkSemaphore> &&waitSemaphores,
                                 std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks);

    void initFlushAndQueueSubmit(const VkSemaphore semaphore,
                                 ProtectionType protectionType,
                                 egl::ContextPriority priority,
                                 SecondaryCommandPools *commandPools,
                                 SecondaryCommandBufferList &&commandBuffersToReset,
                                 const QueueSerial &submitQueueSerial);

    void initOneOffQueueSubmit(VkCommandBuffer commandBufferHandle,
                               ProtectionType protectionType,
                               egl::ContextPriority priority,
                               const Semaphore *waitSemaphore,
                               VkPipelineStageFlags waitSemaphoreStageMask,
                               const Fence *fence,
                               const QueueSerial &submitQueueSerial);

    CommandProcessorTask &operator=(CommandProcessorTask &&rhs);

    CommandProcessorTask(CommandProcessorTask &&other) : CommandProcessorTask()
    {
        *this = std::move(other);
    }

    const QueueSerial &getSubmitQueueSerial() const { return mSubmitQueueSerial; }
    CustomTask getTaskCommand() { return mTask; }
    std::vector<VkSemaphore> &getWaitSemaphores() { return mWaitSemaphores; }
    std::vector<VkPipelineStageFlags> &getWaitSemaphoreStageMasks()
    {
        return mWaitSemaphoreStageMasks;
    }
    VkSemaphore getSemaphore() { return mSemaphore; }
    SecondaryCommandBufferList &getCommandBuffersToReset() { return mCommandBuffersToReset; }
    egl::ContextPriority getPriority() const { return mPriority; }
    ProtectionType getProtectionType() const { return mProtectionType; }
    VkCommandBuffer getOneOffCommandBufferVk() const { return mOneOffCommandBufferVk; }
    const Semaphore *getOneOffWaitSemaphore() { return mOneOffWaitSemaphore; }
    VkPipelineStageFlags getOneOffWaitSemaphoreStageMask() { return mOneOffWaitSemaphoreStageMask; }
    const Fence *getOneOffFence() { return mOneOffFence; }
    const VkPresentInfoKHR &getPresentInfo() const { return mPresentInfo; }
    SwapchainStatus *getSwapchainStatus() const { return mSwapchainStatus; }
    const RenderPass *getRenderPass() const { return mRenderPass; }
    OutsideRenderPassCommandBufferHelper *getOutsideRenderPassCommandBuffer() const
    {
        return mOutsideRenderPassCommandBuffer;
    }
    RenderPassCommandBufferHelper *getRenderPassCommandBuffer() const
    {
        return mRenderPassCommandBuffer;
    }
    SecondaryCommandPools *getCommandPools() const { return mCommandPools; }

  private:
    void copyPresentInfo(const VkPresentInfoKHR &other);

    CustomTask mTask;

    // Wait semaphores
    std::vector<VkSemaphore> mWaitSemaphores;
    std::vector<VkPipelineStageFlags> mWaitSemaphoreStageMasks;

    // ProcessCommands
    OutsideRenderPassCommandBufferHelper *mOutsideRenderPassCommandBuffer;
    RenderPassCommandBufferHelper *mRenderPassCommandBuffer;
    const RenderPass *mRenderPass;

    // Flush data
    VkSemaphore mSemaphore;
    SecondaryCommandPools *mCommandPools;
    SecondaryCommandBufferList mCommandBuffersToReset;

    // Flush command data
    QueueSerial mSubmitQueueSerial;

    // Present command data
    VkPresentInfoKHR mPresentInfo;
    VkSwapchainKHR mSwapchain;
    VkSemaphore mWaitSemaphore;
    uint32_t mImageIndex;
    // Used by Present if supportsIncrementalPresent is enabled
    VkPresentRegionKHR mPresentRegion;
    VkPresentRegionsKHR mPresentRegions;
    std::vector<VkRectLayerKHR> mRects;

    VkSwapchainPresentFenceInfoEXT mPresentFenceInfo;
    VkFence mPresentFence;

    VkSwapchainPresentModeInfoEXT mPresentModeInfo;
    VkPresentModeKHR mPresentMode;

    SwapchainStatus *mSwapchainStatus;

    // Used by OneOffQueueSubmit
    VkCommandBuffer mOneOffCommandBufferVk;
    const Semaphore *mOneOffWaitSemaphore;
    VkPipelineStageFlags mOneOffWaitSemaphoreStageMask;
    const Fence *mOneOffFence;

    // Flush, Present & QueueWaitIdle data
    egl::ContextPriority mPriority;
    ProtectionType mProtectionType;
};
using CommandProcessorTaskQueue =
    angle::FixedQueue<CommandProcessorTask, kMaxCommandProcessorTasksLimit>;

struct CommandBatch final : angle::NonCopyable
{
    CommandBatch();
    ~CommandBatch();
    CommandBatch(CommandBatch &&other);
    CommandBatch &operator=(CommandBatch &&other);

    void destroy(VkDevice device);
    void resetSecondaryCommandBuffers(VkDevice device);

    PrimaryCommandBuffer primaryCommands;
    // commandPools is for secondary CommandBuffer allocation
    SecondaryCommandPools *commandPools;
    SecondaryCommandBufferList commandBuffersToReset;
    SharedFence fence;
    QueueSerial queueSerial;
    ProtectionType protectionType;
};
using CommandBatchQueue = angle::FixedQueue<CommandBatch, kInFlightCommandsLimit>;

class DeviceQueueMap;

class QueueFamily final : angle::NonCopyable
{
  public:
    static const uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

    static uint32_t FindIndex(const std::vector<VkQueueFamilyProperties> &queueFamilyProperties,
                              VkQueueFlags flags,
                              int32_t matchNumber,  // 0 = first match, 1 = second match ...
                              uint32_t *matchCount);
    static const uint32_t kQueueCount = static_cast<uint32_t>(egl::ContextPriority::EnumCount);
    static const float kQueuePriorities[static_cast<uint32_t>(egl::ContextPriority::EnumCount)];

    QueueFamily() : mProperties{}, mIndex(kInvalidIndex) {}
    ~QueueFamily() {}

    void initialize(const VkQueueFamilyProperties &queueFamilyProperties, uint32_t index);
    bool valid() const { return (mIndex != kInvalidIndex); }
    uint32_t getIndex() const { return mIndex; }
    const VkQueueFamilyProperties *getProperties() const { return &mProperties; }
    bool isGraphics() const { return ((mProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0); }
    bool isCompute() const { return ((mProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) > 0); }
    bool supportsProtected() const
    {
        return ((mProperties.queueFlags & VK_QUEUE_PROTECTED_BIT) > 0);
    }
    uint32_t getDeviceQueueCount() const { return mProperties.queueCount; }

    DeviceQueueMap initializeQueueMap(VkDevice device,
                                      bool makeProtected,
                                      uint32_t queueIndex,
                                      uint32_t queueCount);

  private:
    VkQueueFamilyProperties mProperties;
    uint32_t mIndex;

    void getDeviceQueue(VkDevice device, bool makeProtected, uint32_t queueIndex, VkQueue *queue);
};

class DeviceQueueMap : public angle::PackedEnumMap<egl::ContextPriority, VkQueue>
{
    friend QueueFamily;

  public:
    DeviceQueueMap() : mIndex(QueueFamily::kInvalidIndex), mIsProtected(false) {}
    DeviceQueueMap(uint32_t queueFamilyIndex, bool isProtected)
        : mIndex(queueFamilyIndex), mIsProtected(isProtected)
    {}
    DeviceQueueMap(const DeviceQueueMap &other) = default;
    ~DeviceQueueMap();
    DeviceQueueMap &operator=(const DeviceQueueMap &other);

    bool valid() const { return (mIndex != QueueFamily::kInvalidIndex); }
    uint32_t getIndex() const { return mIndex; }
    bool isProtected() const { return mIsProtected; }
    egl::ContextPriority getDevicePriority(egl::ContextPriority priority) const;

  private:
    uint32_t mIndex;
    bool mIsProtected;
    angle::PackedEnumMap<egl::ContextPriority, egl::ContextPriority> mPriorities;
};

// Note all public APIs of CommandQueue class must be thread safe.
class CommandQueue : angle::NonCopyable
{
  public:
    CommandQueue();
    ~CommandQueue();

    angle::Result init(Context *context, const DeviceQueueMap &queueMap);
    void destroy(Context *context);

    void handleDeviceLost(RendererVk *renderer);

    // These public APIs are inherently thread safe. Thread unsafe methods must be protected methods
    // that are only accessed via ThreadSafeCommandQueue API.
    egl::ContextPriority getDriverPriority(egl::ContextPriority priority) const
    {
        return mQueueMap.getDevicePriority(priority);
    }
    uint32_t getDeviceQueueIndex() const { return mQueueMap.getIndex(); }

    VkQueue getQueue(egl::ContextPriority priority) const { return mQueueMap[priority]; }

    // The ResourceUse still have unfinished queue serial by ANGLE or vulkan.
    bool hasUnfinishedUse(const ResourceUse &use) const { return use > mLastCompletedSerials; }
    // The ResourceUse still have queue serial not yet submitted to vulkan.
    bool hasUnsubmittedUse(const ResourceUse &use) const { return use > mLastSubmittedSerials; }
    Serial getLastSubmittedSerial(SerialIndex index) const { return mLastSubmittedSerials[index]; }

    // Wait until the desired serial has been completed.
    angle::Result finishResourceUse(Context *context, const ResourceUse &use, uint64_t timeout);
    angle::Result finishQueueSerial(Context *context,
                                    const QueueSerial &queueSerial,
                                    uint64_t timeout);
    angle::Result waitIdle(Context *context, uint64_t timeout);
    angle::Result waitForResourceUseToFinishWithUserTimeout(Context *context,
                                                            const ResourceUse &use,
                                                            uint64_t timeout,
                                                            VkResult *result);
    bool isBusy(RendererVk *renderer) const;

    angle::Result submitCommands(Context *context,
                                 ProtectionType protectionType,
                                 egl::ContextPriority priority,
                                 const VkSemaphore signalSemaphore,
                                 SecondaryCommandBufferList &&commandBuffersToReset,
                                 SecondaryCommandPools *commandPools,
                                 const QueueSerial &submitQueueSerial);

    angle::Result queueSubmitOneOff(Context *context,
                                    ProtectionType protectionType,
                                    egl::ContextPriority contextPriority,
                                    VkCommandBuffer commandBufferHandle,
                                    const Semaphore *waitSemaphore,
                                    VkPipelineStageFlags waitSemaphoreStageMask,
                                    const Fence *fence,
                                    SubmitPolicy submitPolicy,
                                    const QueueSerial &submitQueueSerial);

    VkResult queuePresent(egl::ContextPriority contextPriority,
                          const VkPresentInfoKHR &presentInfo,
                          SwapchainStatus *swapchainStatus);

    // Check to see which batches have finished completion (forward progress for
    // the last completed serial, for example for when the application busy waits on a query
    // result). It would be nice if we didn't have to expose this for QueryVk::getResult.
    angle::Result checkCompletedCommands(Context *context);

    void flushWaitSemaphores(ProtectionType protectionType,
                             std::vector<VkSemaphore> &&waitSemaphores,
                             std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks);
    angle::Result flushOutsideRPCommands(Context *context,
                                         ProtectionType protectionType,
                                         OutsideRenderPassCommandBufferHelper **outsideRPCommands);
    angle::Result flushRenderPassCommands(Context *context,
                                          ProtectionType protectionType,
                                          const RenderPass &renderPass,
                                          RenderPassCommandBufferHelper **renderPassCommands);

    const angle::VulkanPerfCounters getPerfCounters() const;
    void resetPerFramePerfCounters();

  private:
    // All these private APIs are called with mutex locked, so we must not take lock again.
    angle::Result checkCompletedCommandCount(Context *context, int *finishedCountOut);
    angle::Result finishOneCommandBatch(Context *context, uint64_t timeout);

    angle::Result submitCommandsImpl(Context *context,
                                     ProtectionType protectionType,
                                     egl::ContextPriority priority,
                                     const VkSemaphore signalSemaphore,
                                     SecondaryCommandBufferList &&commandBuffersToReset,
                                     SecondaryCommandPools *commandPools,
                                     const QueueSerial &submitQueueSerial);
    angle::Result queueSubmit(Context *context,
                              std::unique_lock<std::mutex> &&dequeueLock,
                              egl::ContextPriority contextPriority,
                              const VkSubmitInfo &submitInfo,
                              const Fence *fence,
                              DeviceScoped<CommandBatch> &commandBatch,
                              const QueueSerial &submitQueueSerial);
    angle::Result postSubmitCheck(Context *context);

    angle::Result retireFinishedCommands(Context *context, size_t finishedCount);
    angle::Result retireFinishedCommandsAndCleanupGarbage(Context *context, size_t finishedCount);
    angle::Result ensurePrimaryCommandBufferValid(Context *context, ProtectionType protectionType);
    // Returns number of CommandBatchs that are smaller than serials
    size_t getBatchCountUpToSerials(RendererVk *renderer, const Serials &serials);
    // Returns the last valid SharedFence of the first "count" CommandBatchs in mInflightCommands.
    const SharedFence &getSharedFenceToWait(size_t count);

    // For validation only. Should only be called with ASSERT macro.
    bool allInFlightCommandsAreAfterSerials(const Serials &serials);

    struct CommandsState
    {
        std::vector<VkSemaphore> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitSemaphoreStageMasks;
        PrimaryCommandBuffer primaryCommands;
        // Keeps a free list of reusable primary command buffers.
        PersistentCommandPool primaryCommandPool;
    };

    angle::Result initCommandPool(Context *context, ProtectionType protectionType)
    {
        PersistentCommandPool &commandPool = mCommandsStateMap[protectionType].primaryCommandPool;
        return commandPool.init(context, protectionType, mQueueMap.getIndex());
    }

    // Protect multi-thread access to mInFlightCommands.pop and ensure ordering of submission.
    mutable std::mutex mMutex;
    // Protect multi-thread access to mInFlightCommands.push as well as does lock relay for mMutex
    // so that we can release mMutex while doing potential lengthy vkQueueSubmit and vkQueuePresent
    // call.
    std::mutex mQueueSubmitMutex;
    CommandBatchQueue mInFlightCommands;

    angle::PackedEnumMap<ProtectionType, CommandsState> mCommandsStateMap;

    // Queue serial management.
    AtomicQueueSerialFixedArray mLastSubmittedSerials;
    // This queue serial can be read/write from different threads, so we need to use atomic
    // operations to access the underline value. Since we only do load/store on this value, it
    // should be just a normal uint64_t load/store on most platforms.
    AtomicQueueSerialFixedArray mLastCompletedSerials;

    // QueueMap
    DeviceQueueMap mQueueMap;

    FenceRecycler mFenceRecycler;

    angle::VulkanPerfCounters mPerfCounters;
};

// CommandProcessor is used to dispatch work to the GPU when the asyncCommandQueue feature is
// enabled. Issuing the |destroy| command will cause the worker thread to clean up it's resources
// and shut down. This command is sent when the renderer instance shuts down. Tasks are defined by
// the CommandQueue interface.

class CommandProcessor : public Context
{
  public:
    CommandProcessor(RendererVk *renderer, CommandQueue *commandQueue);
    ~CommandProcessor() override;

    // Context
    void handleError(VkResult result,
                     const char *file,
                     const char *function,
                     unsigned int line) override;

    angle::Result init();

    void destroy(Context *context);

    void handleDeviceLost(RendererVk *renderer);

    angle::Result submitCommands(Context *context,
                                 ProtectionType protectionType,
                                 egl::ContextPriority priority,
                                 const VkSemaphore signalSemaphore,
                                 SecondaryCommandBufferList &&commandBuffersToReset,
                                 SecondaryCommandPools *commandPools,
                                 const QueueSerial &submitQueueSerial);

    angle::Result queueSubmitOneOff(Context *context,
                                    ProtectionType protectionType,
                                    egl::ContextPriority contextPriority,
                                    VkCommandBuffer commandBufferHandle,
                                    const Semaphore *waitSemaphore,
                                    VkPipelineStageFlags waitSemaphoreStageMask,
                                    const Fence *fence,
                                    SubmitPolicy submitPolicy,
                                    const QueueSerial &submitQueueSerial);
    VkResult queuePresent(egl::ContextPriority contextPriority,
                          const VkPresentInfoKHR &presentInfo,
                          SwapchainStatus *swapchainStatus);

    angle::Result flushWaitSemaphores(ProtectionType protectionType,
                                      std::vector<VkSemaphore> &&waitSemaphores,
                                      std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks);
    angle::Result flushOutsideRPCommands(Context *context,
                                         ProtectionType protectionType,
                                         OutsideRenderPassCommandBufferHelper **outsideRPCommands);
    angle::Result flushRenderPassCommands(Context *context,
                                          ProtectionType protectionType,
                                          const RenderPass &renderPass,
                                          RenderPassCommandBufferHelper **renderPassCommands);

    // Wait until the desired serial has been submitted.
    angle::Result waitForQueueSerialToBeSubmitted(vk::Context *context,
                                                  const QueueSerial &queueSerial)
    {
        const ResourceUse use(queueSerial);
        return waitForResourceUseToBeSubmitted(context, use);
    }
    angle::Result waitForResourceUseToBeSubmitted(vk::Context *context, const ResourceUse &use);
    // Wait for worker thread to submit all outstanding work.
    angle::Result waitForAllWorkToBeSubmitted(Context *context);

    bool isBusy(RendererVk *renderer) const
    {
        std::lock_guard<std::mutex> workerLock(mWorkerMutex);
        return !mTasks.empty() || mCommandQueue->isBusy(renderer);
    }

    bool hasUnsubmittedUse(const ResourceUse &use) const;
    Serial getLastSubmittedSerial(SerialIndex index) const { return mLastSubmittedSerials[index]; }

  protected:
    bool hasPendingError() const
    {
        std::lock_guard<std::mutex> queueLock(mErrorMutex);
        return !mErrors.empty();
    }
    angle::Result checkAndPopPendingError(Context *errorHandlingContext);

    // Entry point for command processor thread, calls processTasksImpl to do the
    // work. called by RendererVk::initializeDevice on main thread
    void processTasks();

    // Called asynchronously from main thread to queue work that is then processed by the worker
    // thread
    angle::Result queueCommand(CommandProcessorTask &&task);

    // Command processor thread, called by processTasks. The loop waits for work to
    // be submitted from a separate thread.
    angle::Result processTasksImpl(bool *exitThread);

    // Command processor thread, process a task
    angle::Result processTask(CommandProcessorTask *task);

    VkResult present(egl::ContextPriority priority,
                     const VkPresentInfoKHR &presentInfo,
                     SwapchainStatus *swapchainStatus);
    void updateSwapchainStatus(SwapchainStatus *swapchainStatus, VkResult presentResult);

    // The mutex lock that serializes dequeue from mTask and submit to mCommandQueue so that only
    // one mTasks consumer at a time
    std::mutex mSubmissionMutex;

    CommandProcessorTaskQueue mTasks;
    mutable std::mutex mWorkerMutex;
    // Signal worker thread when work is available
    std::condition_variable mWorkAvailableCondition;
    CommandQueue *const mCommandQueue;

    // Tracks last serial that was submitted to command processor. Note: this maybe different from
    // mLastSubmittedQueueSerial in CommandQueue since submission from CommandProcessor to
    // CommandQueue occur in a separate thread.
    AtomicQueueSerialFixedArray mLastSubmittedSerials;

    mutable std::mutex mErrorMutex;
    std::queue<Error> mErrors;

    // Command queue worker thread.
    std::thread mTaskThread;
    bool mTaskThreadShouldExit;
};
}  // namespace vk

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_COMMAND_PROCESSOR_H_
