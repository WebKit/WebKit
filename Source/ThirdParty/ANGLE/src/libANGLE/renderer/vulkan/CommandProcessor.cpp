//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CommandProcessor.cpp:
//    Implements the class methods for CommandProcessor.
//

#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "common/system_utils.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{
namespace vk
{
namespace
{
constexpr bool kOutputVmaStatsString = false;
// When suballocation garbages is more than this, we may wait for GPU to finish and free up some
// memory for allocation.
constexpr VkDeviceSize kMaxBufferSuballocationGarbageSize = 64 * 1024 * 1024;

void InitializeSubmitInfo(VkSubmitInfo *submitInfo,
                          const PrimaryCommandBuffer &commandBuffer,
                          const std::vector<VkSemaphore> &waitSemaphores,
                          const std::vector<VkPipelineStageFlags> &waitSemaphoreStageMasks,
                          const VkSemaphore &signalSemaphore)
{
    // Verify that the submitInfo has been zero'd out.
    ASSERT(submitInfo->signalSemaphoreCount == 0);
    ASSERT(waitSemaphores.size() == waitSemaphoreStageMasks.size());
    submitInfo->sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo->commandBufferCount = commandBuffer.valid() ? 1 : 0;
    submitInfo->pCommandBuffers    = commandBuffer.ptr();
    submitInfo->waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo->pWaitSemaphores    = waitSemaphores.empty() ? nullptr : waitSemaphores.data();
    submitInfo->pWaitDstStageMask  = waitSemaphoreStageMasks.data();

    if (signalSemaphore != VK_NULL_HANDLE)
    {
        submitInfo->signalSemaphoreCount = 1;
        submitInfo->pSignalSemaphores    = &signalSemaphore;
    }
}

template <typename SecondaryCommandBufferListT>
void ResetSecondaryCommandBuffers(VkDevice device,
                                  CommandPool *commandPool,
                                  SecondaryCommandBufferListT *commandBuffers)
{
    // Nothing to do when using ANGLE secondary command buffers.
}

template <>
[[maybe_unused]] void ResetSecondaryCommandBuffers<std::vector<VulkanSecondaryCommandBuffer>>(
    VkDevice device,
    CommandPool *commandPool,
    std::vector<VulkanSecondaryCommandBuffer> *commandBuffers)
{
    // Note: we currently free the command buffers individually, but we could potentially reset the
    // entire command pool.  https://issuetracker.google.com/issues/166793850
    for (VulkanSecondaryCommandBuffer &secondary : *commandBuffers)
    {
        commandPool->freeCommandBuffers(device, 1, secondary.ptr());
        secondary.releaseHandle();
    }
    commandBuffers->clear();
}

// Count the number of batches with serial <= given serial.  A reference to the fence of the last
// batch with a valid fence is returned for waiting purposes.  Note that due to empty submissions
// being optimized out, there may not be a fence associated with every batch.
template <typename BitSetArrayT>
size_t GetBatchCountUpToSerials(CommandBatchQueue &inFlightCommands,
                                const AtomicQueueSerialFixedArray &lastSubmittedSerials,
                                const AtomicQueueSerialFixedArray &lastCompletedSerials,
                                const Serials &serials)
{
    // First calculate the bitmask of which index we should wait
    BitSetArrayT serialBitMaskToFinish;
    for (SerialIndex i = 0; i < serials.size(); i++)
    {
        ASSERT(serials[i] <= lastSubmittedSerials[i]);
        if (serials[i] > lastCompletedSerials[i])
        {
            serialBitMaskToFinish.set(i);
        }
    }

    if (serialBitMaskToFinish.none())
    {
        return 0;
    }

    Serials serialsWillBeFinished(serials.size());
    size_t batchCountToFinish = 0, batchCount = 0;
    for (const CommandBatch &batch : inFlightCommands)
    {
        batchCount++;
        if (serialBitMaskToFinish[batch.queueSerial.getIndex()])
        {
            // Update what the serial will look like if this batch is completed.
            serialsWillBeFinished[batch.queueSerial.getIndex()] = batch.queueSerial.getSerial();

            // If finish this batch will make completed serial meet the requested serial, we should
            // wait, and we are done with this index.
            if (serialsWillBeFinished[batch.queueSerial.getIndex()] >=
                serials[batch.queueSerial.getIndex()])
            {
                batchCountToFinish = batchCount;

                serialBitMaskToFinish.reset(batch.queueSerial.getIndex());
                if (serialBitMaskToFinish.none())
                {
                    // If nothing left to wait, we are done.
                    break;
                }
            }
        }
    }

    ASSERT(batchCountToFinish > 0);
    return batchCountToFinish;
}
}  // namespace

// SharedFence implementation
SharedFence::SharedFence() : mRefCountedFence(nullptr), mRecycler(nullptr) {}
SharedFence::SharedFence(const SharedFence &other)
    : mRefCountedFence(other.mRefCountedFence), mRecycler(other.mRecycler)
{
    if (mRefCountedFence != nullptr)
    {
        mRefCountedFence->addRef();
    }
}
SharedFence::SharedFence(SharedFence &&other)
    : mRefCountedFence(other.mRefCountedFence), mRecycler(other.mRecycler)
{
    other.mRecycler        = nullptr;
    other.mRefCountedFence = nullptr;
}

SharedFence::~SharedFence()
{
    release();
}

VkResult SharedFence::init(VkDevice device, FenceRecycler *recycler)
{
    ASSERT(mRecycler == nullptr && mRefCountedFence == nullptr);
    Fence fence;

    // First try to fetch from recycler. If that failed, try to create a new VkFence
    recycler->fetch(device, &fence);
    if (!fence.valid())
    {
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags             = 0;
        VkResult result                   = fence.init(device, fenceCreateInfo);
        if (result != VK_SUCCESS)
        {
            return result;
        }
    }

    // Create a new refcounted object to hold onto VkFence
    mRefCountedFence = new RefCounted<Fence>(std::move(fence));
    mRefCountedFence->addRef();
    mRecycler = recycler;

    return VK_SUCCESS;
}

SharedFence &SharedFence::operator=(const SharedFence &other)
{
    release();

    mRecycler = other.mRecycler;
    if (other.mRefCountedFence != nullptr)
    {
        mRefCountedFence = other.mRefCountedFence;
        mRefCountedFence->addRef();
    }
    return *this;
}

SharedFence &SharedFence::operator=(SharedFence &&other)
{
    release();
    mRecycler              = other.mRecycler;
    mRefCountedFence       = other.mRefCountedFence;
    other.mRecycler        = nullptr;
    other.mRefCountedFence = nullptr;
    return *this;
}

void SharedFence::destroy(VkDevice device)
{
    if (mRefCountedFence != nullptr)
    {
        mRefCountedFence->releaseRef();
        if (!mRefCountedFence->isReferenced())
        {
            mRefCountedFence->get().destroy(device);
            SafeDelete(mRefCountedFence);
        }
        else
        {
            mRefCountedFence = nullptr;
        }
        mRecycler = nullptr;
    }
}

void SharedFence::release()
{
    if (mRefCountedFence != nullptr)
    {
        mRefCountedFence->releaseRef();
        if (!mRefCountedFence->isReferenced())
        {
            mRecycler->recycle(std::move(mRefCountedFence->get()));
            ASSERT(!mRefCountedFence->get().valid());
            SafeDelete(mRefCountedFence);
        }
        else
        {
            mRefCountedFence = nullptr;
        }
        mRecycler = nullptr;
    }
}

SharedFence::operator bool() const
{
    ASSERT(mRefCountedFence == nullptr || mRefCountedFence->isReferenced());
    return mRefCountedFence != nullptr;
}

VkResult SharedFence::getStatus(VkDevice device) const
{
    if (mRefCountedFence != nullptr)
    {
        return mRefCountedFence->get().getStatus(device);
    }
    return VK_SUCCESS;
}

VkResult SharedFence::wait(VkDevice device, uint64_t timeout) const
{
    if (mRefCountedFence != nullptr)
    {
        return mRefCountedFence->get().wait(device, timeout);
    }
    return VK_SUCCESS;
}

// FenceRecycler implementation
void FenceRecycler::destroy(Context *context)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mRecyler.destroy(context->getDevice());
}

void FenceRecycler::fetch(VkDevice device, Fence *fenceOut)
{
    ASSERT(fenceOut != nullptr && !fenceOut->valid());
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mRecyler.empty())
    {
        mRecyler.fetch(fenceOut);
        fenceOut->reset(device);
    }
}

void FenceRecycler::recycle(Fence &&fence)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mRecyler.recycle(std::move(fence));
}

// CommandProcessorTask implementation
void CommandProcessorTask::initTask()
{
    mTask                           = CustomTask::Invalid;
    mOutsideRenderPassCommandBuffer = nullptr;
    mRenderPassCommandBuffer        = nullptr;
    mRenderPass                     = nullptr;
    mSemaphore                      = VK_NULL_HANDLE;
    mCommandPools                   = nullptr;
    mOneOffWaitSemaphore            = nullptr;
    mOneOffWaitSemaphoreStageMask   = 0;
    mOneOffFence                    = nullptr;
    mPresentInfo                    = {};
    mPresentInfo.pResults           = nullptr;
    mPresentInfo.pSwapchains        = nullptr;
    mPresentInfo.pImageIndices      = nullptr;
    mPresentInfo.pNext              = nullptr;
    mPresentInfo.pWaitSemaphores    = nullptr;
    mPresentFence                   = VK_NULL_HANDLE;
    mSwapchainStatus                = nullptr;
    mOneOffCommandBufferVk          = VK_NULL_HANDLE;
    mPriority                       = egl::ContextPriority::Medium;
    mProtectionType                 = ProtectionType::InvalidEnum;
}

void CommandProcessorTask::initOutsideRenderPassProcessCommands(
    ProtectionType protectionType,
    OutsideRenderPassCommandBufferHelper *commandBuffer)
{
    mTask                           = CustomTask::ProcessOutsideRenderPassCommands;
    mOutsideRenderPassCommandBuffer = commandBuffer;
    mProtectionType                 = protectionType;
}

void CommandProcessorTask::initRenderPassProcessCommands(
    ProtectionType protectionType,
    RenderPassCommandBufferHelper *commandBuffer,
    const RenderPass *renderPass)
{
    mTask                    = CustomTask::ProcessRenderPassCommands;
    mRenderPassCommandBuffer = commandBuffer;
    mRenderPass              = renderPass;
    mProtectionType          = protectionType;
}

void CommandProcessorTask::copyPresentInfo(const VkPresentInfoKHR &other)
{
    if (other.sType == 0)
    {
        return;
    }

    mPresentInfo.sType = other.sType;
    mPresentInfo.pNext = nullptr;

    if (other.swapchainCount > 0)
    {
        ASSERT(other.swapchainCount == 1);
        mPresentInfo.swapchainCount = 1;
        mSwapchain                  = other.pSwapchains[0];
        mPresentInfo.pSwapchains    = &mSwapchain;
        mImageIndex                 = other.pImageIndices[0];
        mPresentInfo.pImageIndices  = &mImageIndex;
    }

    if (other.waitSemaphoreCount > 0)
    {
        ASSERT(other.waitSemaphoreCount == 1);
        mPresentInfo.waitSemaphoreCount = 1;
        mWaitSemaphore                  = other.pWaitSemaphores[0];
        mPresentInfo.pWaitSemaphores    = &mWaitSemaphore;
    }

    mPresentInfo.pResults = other.pResults;

    void *pNext = const_cast<void *>(other.pNext);
    while (pNext != nullptr)
    {
        VkStructureType sType = *reinterpret_cast<VkStructureType *>(pNext);
        switch (sType)
        {
            case VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR:
            {
                const VkPresentRegionsKHR *presentRegions =
                    reinterpret_cast<VkPresentRegionsKHR *>(pNext);
                mPresentRegion = *presentRegions->pRegions;
                mRects.resize(mPresentRegion.rectangleCount);
                for (uint32_t i = 0; i < mPresentRegion.rectangleCount; i++)
                {
                    mRects[i] = presentRegions->pRegions->pRectangles[i];
                }
                mPresentRegion.pRectangles = mRects.data();

                mPresentRegions.sType          = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR;
                mPresentRegions.pNext          = nullptr;
                mPresentRegions.swapchainCount = 1;
                mPresentRegions.pRegions       = &mPresentRegion;
                AddToPNextChain(&mPresentInfo, &mPresentRegions);
                pNext = const_cast<void *>(presentRegions->pNext);
                break;
            }
            case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT:
            {
                const VkSwapchainPresentFenceInfoEXT *presentFenceInfo =
                    reinterpret_cast<VkSwapchainPresentFenceInfoEXT *>(pNext);
                ASSERT(presentFenceInfo->swapchainCount == 1);
                mPresentFence = presentFenceInfo->pFences[0];

                mPresentFenceInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
                mPresentFenceInfo.pNext = nullptr;
                mPresentFenceInfo.swapchainCount = 1;
                mPresentFenceInfo.pFences        = &mPresentFence;
                AddToPNextChain(&mPresentInfo, &mPresentFenceInfo);
                pNext = const_cast<void *>(presentFenceInfo->pNext);
                break;
            }
            case VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT:
            {
                const VkSwapchainPresentModeInfoEXT *presentModeInfo =
                    reinterpret_cast<VkSwapchainPresentModeInfoEXT *>(pNext);
                ASSERT(presentModeInfo->swapchainCount == 1);
                mPresentMode = presentModeInfo->pPresentModes[0];

                mPresentModeInfo.sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT;
                mPresentModeInfo.pNext          = nullptr;
                mPresentModeInfo.swapchainCount = 1;
                mPresentModeInfo.pPresentModes  = &mPresentMode;
                AddToPNextChain(&mPresentInfo, &mPresentModeInfo);
                pNext = const_cast<void *>(presentModeInfo->pNext);
                break;
            }
            default:
                ERR() << "Unknown sType: " << sType << " in VkPresentInfoKHR.pNext chain";
                UNREACHABLE();
                break;
        }
    }
}

void CommandProcessorTask::initPresent(egl::ContextPriority priority,
                                       const VkPresentInfoKHR &presentInfo,
                                       SwapchainStatus *swapchainStatus)
{
    mTask            = CustomTask::Present;
    mPriority        = priority;
    mSwapchainStatus = swapchainStatus;
    copyPresentInfo(presentInfo);
}

void CommandProcessorTask::initFlushWaitSemaphores(
    ProtectionType protectionType,
    std::vector<VkSemaphore> &&waitSemaphores,
    std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks)
{
    mTask                    = CustomTask::FlushWaitSemaphores;
    mProtectionType          = protectionType;
    mWaitSemaphores          = std::move(waitSemaphores);
    mWaitSemaphoreStageMasks = std::move(waitSemaphoreStageMasks);
}

void CommandProcessorTask::initFlushAndQueueSubmit(
    const VkSemaphore semaphore,
    ProtectionType protectionType,
    egl::ContextPriority priority,
    SecondaryCommandPools *commandPools,
    SecondaryCommandBufferList &&commandBuffersToReset,
    const QueueSerial &submitQueueSerial)
{
    mTask                  = CustomTask::FlushAndQueueSubmit;
    mSemaphore             = semaphore;
    mCommandPools          = commandPools;
    mCommandBuffersToReset = std::move(commandBuffersToReset);
    mPriority              = priority;
    mProtectionType        = protectionType;
    mSubmitQueueSerial     = submitQueueSerial;
}

void CommandProcessorTask::initOneOffQueueSubmit(VkCommandBuffer commandBufferHandle,
                                                 ProtectionType protectionType,
                                                 egl::ContextPriority priority,
                                                 const Semaphore *waitSemaphore,
                                                 VkPipelineStageFlags waitSemaphoreStageMask,
                                                 const Fence *fence,
                                                 const QueueSerial &submitQueueSerial)
{
    mTask                         = CustomTask::OneOffQueueSubmit;
    mOneOffCommandBufferVk        = commandBufferHandle;
    mOneOffWaitSemaphore          = waitSemaphore;
    mOneOffWaitSemaphoreStageMask = waitSemaphoreStageMask;
    mOneOffFence                  = fence;
    mPriority                     = priority;
    mProtectionType               = protectionType;
    mSubmitQueueSerial            = submitQueueSerial;
}

CommandProcessorTask &CommandProcessorTask::operator=(CommandProcessorTask &&rhs)
{
    if (this == &rhs)
    {
        return *this;
    }

    std::swap(mRenderPass, rhs.mRenderPass);
    std::swap(mOutsideRenderPassCommandBuffer, rhs.mOutsideRenderPassCommandBuffer);
    std::swap(mRenderPassCommandBuffer, rhs.mRenderPassCommandBuffer);
    std::swap(mTask, rhs.mTask);
    std::swap(mWaitSemaphores, rhs.mWaitSemaphores);
    std::swap(mWaitSemaphoreStageMasks, rhs.mWaitSemaphoreStageMasks);
    std::swap(mSemaphore, rhs.mSemaphore);
    std::swap(mOneOffWaitSemaphore, rhs.mOneOffWaitSemaphore);
    std::swap(mOneOffWaitSemaphoreStageMask, rhs.mOneOffWaitSemaphoreStageMask);
    std::swap(mOneOffFence, rhs.mOneOffFence);
    std::swap(mCommandPools, rhs.mCommandPools);
    std::swap(mCommandBuffersToReset, rhs.mCommandBuffersToReset);
    std::swap(mSubmitQueueSerial, rhs.mSubmitQueueSerial);
    std::swap(mPriority, rhs.mPriority);
    std::swap(mProtectionType, rhs.mProtectionType);
    std::swap(mOneOffCommandBufferVk, rhs.mOneOffCommandBufferVk);

    copyPresentInfo(rhs.mPresentInfo);
    std::swap(mSwapchainStatus, rhs.mSwapchainStatus);

    // clear rhs now that everything has moved.
    rhs.initTask();

    return *this;
}

// CommandBatch implementation.
CommandBatch::CommandBatch() : commandPools(nullptr), protectionType(ProtectionType::InvalidEnum) {}

CommandBatch::~CommandBatch() = default;

CommandBatch::CommandBatch(CommandBatch &&other) : CommandBatch()
{
    *this = std::move(other);
}

CommandBatch &CommandBatch::operator=(CommandBatch &&other)
{
    std::swap(primaryCommands, other.primaryCommands);
    std::swap(commandPools, other.commandPools);
    std::swap(commandBuffersToReset, other.commandBuffersToReset);
    std::swap(fence, other.fence);
    std::swap(queueSerial, other.queueSerial);
    std::swap(protectionType, other.protectionType);
    return *this;
}

void CommandBatch::destroy(VkDevice device)
{
    primaryCommands.destroy(device);
    fence.destroy(device);
    protectionType = ProtectionType::InvalidEnum;
}

void CommandBatch::resetSecondaryCommandBuffers(VkDevice device)
{
    ResetSecondaryCommandBuffers(device, &commandPools->outsideRenderPassPool,
                                 &commandBuffersToReset.outsideRenderPassCommandBuffers);
    ResetSecondaryCommandBuffers(device, &commandPools->renderPassPool,
                                 &commandBuffersToReset.renderPassCommandBuffers);
}

// CommandProcessor implementation.
void CommandProcessor::handleError(VkResult errorCode,
                                   const char *file,
                                   const char *function,
                                   unsigned int line)
{
    ASSERT(errorCode != VK_SUCCESS);

    std::stringstream errorStream;
    errorStream << "Internal Vulkan error (" << errorCode << "): " << VulkanResultString(errorCode)
                << ".";

    if (errorCode == VK_ERROR_DEVICE_LOST)
    {
        WARN() << errorStream.str();
        handleDeviceLost(mRenderer);
    }

    std::lock_guard<std::mutex> queueLock(mErrorMutex);
    Error error = {errorCode, file, function, line};
    mErrors.emplace(error);
}

CommandProcessor::CommandProcessor(RendererVk *renderer, CommandQueue *commandQueue)
    : Context(renderer), mCommandQueue(commandQueue), mTaskThreadShouldExit(false)
{
    std::lock_guard<std::mutex> queueLock(mErrorMutex);
    while (!mErrors.empty())
    {
        mErrors.pop();
    }
}

CommandProcessor::~CommandProcessor() = default;

angle::Result CommandProcessor::checkAndPopPendingError(Context *errorHandlingContext)
{
    std::lock_guard<std::mutex> queueLock(mErrorMutex);
    if (mErrors.empty())
    {
        return angle::Result::Continue;
    }

    while (!mErrors.empty())
    {
        Error err = mErrors.front();
        mErrors.pop();
        errorHandlingContext->handleError(err.errorCode, err.file, err.function, err.line);
    }
    return angle::Result::Stop;
}

angle::Result CommandProcessor::queueCommand(CommandProcessorTask &&task)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::queueCommand");
    // Take mTaskEnqueueMutex lock. If task queue is full, try to drain one.
    std::unique_lock<std::mutex> enqueueLock(mTaskEnqueueMutex);
    if (mTaskQueue.full())
    {
        std::lock_guard<std::mutex> dequeueLock(mTaskDequeueMutex);
        // Check mTasks again in case someone just drained the mTasks.
        if (mTaskQueue.full())
        {
            CommandProcessorTask frontTask(std::move(mTaskQueue.front()));
            mTaskQueue.pop();
            ANGLE_TRY(processTask(&frontTask));
        }
    }
    mTaskQueue.push(std::move(task));
    mWorkAvailableCondition.notify_one();

    return angle::Result::Continue;
}

void CommandProcessor::processTasks()
{
    while (true)
    {
        bool exitThread      = false;
        angle::Result result = processTasksImpl(&exitThread);
        if (exitThread)
        {
            // We are doing a controlled exit of the thread, break out of the while loop.
            break;
        }
        if (result != angle::Result::Continue)
        {
            // TODO: https://issuetracker.google.com/issues/170311829 - follow-up on error handling
            // ContextVk::commandProcessorSyncErrorsAndQueueCommand and WindowSurfaceVk::destroy
            // do error processing, is anything required here? Don't think so, mostly need to
            // continue the worker thread until it's been told to exit.
            UNREACHABLE();
        }
    }
}

angle::Result CommandProcessor::processTasksImpl(bool *exitThread)
{
    while (true)
    {
        std::unique_lock<std::mutex> enqueueLock(mTaskEnqueueMutex);
        if (mTaskQueue.empty())
        {
            // Only wake if notified and command queue is not empty
            mWorkAvailableCondition.wait(
                enqueueLock, [this] { return !mTaskQueue.empty() || mTaskThreadShouldExit; });
        }

        if (mTaskThreadShouldExit)
        {
            break;
        }
        // Do submission with mTaskEnqueueMutex unlocked so that we still allow enqueue while we
        // process work.
        enqueueLock.unlock();

        // Take submission lock to ensure the submission is in the same order as we received.
        std::lock_guard<std::mutex> dequeueLock(mTaskDequeueMutex);
        if (!mTaskQueue.empty())
        {
            CommandProcessorTask task(std::move(mTaskQueue.front()));
            mTaskQueue.pop();

            // Artificially make the task take longer to catch threading issues.
            if (getFeatures().slowAsyncCommandQueueForTesting.enabled)
            {
                constexpr double kSlowdownTime = 0.005;

                double startTime = angle::GetCurrentSystemTime();
                while (angle::GetCurrentSystemTime() - startTime < kSlowdownTime)
                {
                    // Busy waiting
                }
            }

            ANGLE_TRY(processTask(&task));
        }
    }
    *exitThread = true;
    return angle::Result::Continue;
}

angle::Result CommandProcessor::processTask(CommandProcessorTask *task)
{
    switch (task->getTaskCommand())
    {
        case CustomTask::FlushWaitSemaphores:
        {
            mCommandQueue->flushWaitSemaphores(task->getProtectionType(),
                                               std::move(task->getWaitSemaphores()),
                                               std::move(task->getWaitSemaphoreStageMasks()));
            break;
        }
        case CustomTask::FlushAndQueueSubmit:
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "processTask::FlushAndQueueSubmit");
            // End command buffer

            // Call submitCommands()
            ANGLE_TRY(mCommandQueue->submitCommands(
                this, task->getProtectionType(), task->getPriority(), task->getSemaphore(),
                std::move(task->getCommandBuffersToReset()), task->getCommandPools(),
                task->getSubmitQueueSerial()));
            break;
        }
        case CustomTask::OneOffQueueSubmit:
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "processTask::OneOffQueueSubmit");

            ANGLE_TRY(mCommandQueue->queueSubmitOneOff(
                this, task->getProtectionType(), task->getPriority(),
                task->getOneOffCommandBufferVk(), task->getOneOffWaitSemaphore(),
                task->getOneOffWaitSemaphoreStageMask(), task->getOneOffFence(),
                SubmitPolicy::EnsureSubmitted, task->getSubmitQueueSerial()));
            ANGLE_TRY(mCommandQueue->checkCompletedCommands(this));
            break;
        }
        case CustomTask::Present:
        {
            present(task->getPriority(), task->getPresentInfo(), task->getSwapchainStatus());

            VkResult result = task->getSwapchainStatus()->lastPresentResult;
            // We get to ignore these as they are not fatal
            if (result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR &&
                result != VK_SUCCESS)
            {
                // Save the error so that we can handle it.
                // Don't leave processing loop, don't consider errors from present to be fatal.
                // TODO: https://issuetracker.google.com/issues/170329600 - This needs to improve to
                // properly parallelize present
                handleError(result, __FILE__, __FUNCTION__, __LINE__);
            }
            break;
        }
        case CustomTask::ProcessOutsideRenderPassCommands:
        {
            OutsideRenderPassCommandBufferHelper *commandBuffer =
                task->getOutsideRenderPassCommandBuffer();
            ANGLE_TRY(mCommandQueue->flushOutsideRPCommands(this, task->getProtectionType(),
                                                            &commandBuffer));

            OutsideRenderPassCommandBufferHelper *originalCommandBuffer =
                task->getOutsideRenderPassCommandBuffer();
            mRenderer->recycleOutsideRenderPassCommandBufferHelper(mRenderer->getDevice(),
                                                                   &originalCommandBuffer);
            break;
        }
        case CustomTask::ProcessRenderPassCommands:
        {
            RenderPassCommandBufferHelper *commandBuffer = task->getRenderPassCommandBuffer();
            ANGLE_TRY(mCommandQueue->flushRenderPassCommands(
                this, task->getProtectionType(), *task->getRenderPass(), &commandBuffer));

            RenderPassCommandBufferHelper *originalCommandBuffer =
                task->getRenderPassCommandBuffer();
            mRenderer->recycleRenderPassCommandBufferHelper(mRenderer->getDevice(),
                                                            &originalCommandBuffer);
            break;
        }
        default:
            UNREACHABLE();
            break;
    }

    return angle::Result::Continue;
}

angle::Result CommandProcessor::waitForAllWorkToBeSubmitted(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::waitForAllWorkToBeSubmitted");
    std::lock_guard<std::mutex> dequeueLock(mTaskDequeueMutex);

    // Sync any errors to the context
    // Do this inside the mutex to prevent new errors adding to the list.
    ANGLE_TRY(checkAndPopPendingError(context));

    while (!mTaskQueue.empty())
    {
        CommandProcessorTask task(std::move(mTaskQueue.front()));
        mTaskQueue.pop();
        ANGLE_TRY(processTask(&task));
    }

    return angle::Result::Continue;
}

angle::Result CommandProcessor::init()
{
    mTaskThread = std::thread(&CommandProcessor::processTasks, this);

    return angle::Result::Continue;
}

void CommandProcessor::destroy(Context *context)
{
    {
        // Request to terminate the worker thread
        std::lock_guard<std::mutex> enqueueLock(mTaskEnqueueMutex);
        mTaskThreadShouldExit = true;
        mWorkAvailableCondition.notify_one();
    }

    (void)waitForAllWorkToBeSubmitted(context);
    if (mTaskThread.joinable())
    {
        mTaskThread.join();
    }
}

void CommandProcessor::handleDeviceLost(RendererVk *renderer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::handleDeviceLost");
    // Take mTaskEnqueueMutex lock so that no one is able to add more work to the queue while we
    // drain it and handle device lost.
    std::lock_guard<std::mutex> enqueueLock(mTaskEnqueueMutex);
    (void)waitForAllWorkToBeSubmitted(this);
    // Worker thread is idle and command queue is empty so good to continue
    mCommandQueue->handleDeviceLost(renderer);
}

void CommandProcessor::present(egl::ContextPriority priority,
                               const VkPresentInfoKHR &presentInfo,
                               SwapchainStatus *swapchainStatus)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "vkQueuePresentKHR");
    // Verify that we are presenting one and only one swapchain
    ASSERT(presentInfo.swapchainCount == 1);
    ASSERT(presentInfo.pResults == nullptr);

    mCommandQueue->queuePresent(priority, presentInfo, swapchainStatus);
    ASSERT(swapchainStatus->isPending);
    // Always make sure update isPending after status has been updated.
    swapchainStatus->isPending = false;
}

angle::Result CommandProcessor::enqueueSubmitCommands(
    Context *context,
    ProtectionType protectionType,
    egl::ContextPriority priority,
    const VkSemaphore signalSemaphore,
    SecondaryCommandBufferList &&commandBuffersToReset,
    SecondaryCommandPools *commandPools,
    const QueueSerial &submitQueueSerial)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    CommandProcessorTask task;
    task.initFlushAndQueueSubmit(signalSemaphore, protectionType, priority, commandPools,
                                 std::move(commandBuffersToReset), submitQueueSerial);

    ANGLE_TRY(queueCommand(std::move(task)));

    mLastEnqueuedSerials.setQueueSerial(submitQueueSerial);

    return angle::Result::Continue;
}

angle::Result CommandProcessor::enqueueSubmitOneOffCommands(
    Context *context,
    ProtectionType protectionType,
    egl::ContextPriority contextPriority,
    VkCommandBuffer commandBufferHandle,
    const Semaphore *waitSemaphore,
    VkPipelineStageFlags waitSemaphoreStageMask,
    const Fence *fence,
    SubmitPolicy submitPolicy,
    const QueueSerial &submitQueueSerial)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    CommandProcessorTask task;
    task.initOneOffQueueSubmit(commandBufferHandle, protectionType, contextPriority, waitSemaphore,
                               waitSemaphoreStageMask, fence, submitQueueSerial);
    ANGLE_TRY(queueCommand(std::move(task)));

    mLastEnqueuedSerials.setQueueSerial(submitQueueSerial);

    if (submitPolicy == SubmitPolicy::EnsureSubmitted)
    {
        // Caller has synchronization requirement to have work in GPU pipe when returning from this
        // function.
        ANGLE_TRY(waitForQueueSerialToBeSubmitted(context, submitQueueSerial));
    }

    return angle::Result::Continue;
}

void CommandProcessor::enqueuePresent(egl::ContextPriority contextPriority,
                                      const VkPresentInfoKHR &presentInfo,
                                      SwapchainStatus *swapchainStatus)
{
    ASSERT(!swapchainStatus->isPending);
    swapchainStatus->isPending = true;
    // Always return with VK_SUCCESS initially. When we call acquireNextImage we'll check the
    // return code again. This allows the app to continue working until we really need to know
    // the return code from present.
    swapchainStatus->lastPresentResult = VK_SUCCESS;

    CommandProcessorTask task;
    task.initPresent(contextPriority, presentInfo, swapchainStatus);
    (void)queueCommand(std::move(task));
}

angle::Result CommandProcessor::enqueueFlushWaitSemaphores(
    ProtectionType protectionType,
    std::vector<VkSemaphore> &&waitSemaphores,
    std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks)
{
    CommandProcessorTask task;
    task.initFlushWaitSemaphores(protectionType, std::move(waitSemaphores),
                                 std::move(waitSemaphoreStageMasks));
    ANGLE_TRY(queueCommand(std::move(task)));

    return angle::Result::Continue;
}

angle::Result CommandProcessor::enqueueFlushOutsideRPCommands(
    Context *context,
    ProtectionType protectionType,
    OutsideRenderPassCommandBufferHelper **outsideRPCommands)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    (*outsideRPCommands)->markClosed();

    // Detach functions are only used for ring buffer allocators.
    SecondaryCommandMemoryAllocator *allocator = (*outsideRPCommands)->detachAllocator();

    CommandProcessorTask task;
    task.initOutsideRenderPassProcessCommands(protectionType, *outsideRPCommands);
    ANGLE_TRY(queueCommand(std::move(task)));

    ANGLE_TRY(mRenderer->getOutsideRenderPassCommandBufferHelper(
        context, (*outsideRPCommands)->getCommandPool(), allocator, outsideRPCommands));

    return angle::Result::Continue;
}

angle::Result CommandProcessor::enqueueFlushRenderPassCommands(
    Context *context,
    ProtectionType protectionType,
    const RenderPass &renderPass,
    RenderPassCommandBufferHelper **renderPassCommands)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    (*renderPassCommands)->markClosed();

    // Detach functions are only used for ring buffer allocators.
    SecondaryCommandMemoryAllocator *allocator = (*renderPassCommands)->detachAllocator();

    CommandProcessorTask task;
    task.initRenderPassProcessCommands(protectionType, *renderPassCommands, &renderPass);
    ANGLE_TRY(queueCommand(std::move(task)));

    ANGLE_TRY(mRenderer->getRenderPassCommandBufferHelper(
        context, (*renderPassCommands)->getCommandPool(), allocator, renderPassCommands));

    return angle::Result::Continue;
}

angle::Result CommandProcessor::waitForResourceUseToBeSubmitted(Context *context,
                                                                const ResourceUse &use)
{
    if (mCommandQueue->hasResourceUseSubmitted(use))
    {
        ANGLE_TRY(checkAndPopPendingError(context));
    }
    else
    {
        // We do not hold mTaskEnqueueMutex lock, so that we still allow other context to enqueue
        // work while we are processing them.
        std::lock_guard<std::mutex> dequeueLock(mTaskDequeueMutex);

        // Do this inside the mutex to prevent new errors adding to the list.
        ANGLE_TRY(checkAndPopPendingError(context));

        size_t maxTaskCount = mTaskQueue.size();
        size_t taskCount    = 0;
        while (taskCount < maxTaskCount && !mCommandQueue->hasResourceUseSubmitted(use))
        {
            CommandProcessorTask task(std::move(mTaskQueue.front()));
            mTaskQueue.pop();
            ANGLE_TRY(processTask(&task));
            taskCount++;
        }
    }
    return angle::Result::Continue;
}

angle::Result CommandProcessor::waitForPresentToBeSubmitted(SwapchainStatus *swapchainStatus)
{
    if (!swapchainStatus->isPending)
    {
        return angle::Result::Continue;
    }

    std::lock_guard<std::mutex> dequeueLock(mTaskDequeueMutex);
    size_t maxTaskCount = mTaskQueue.size();
    size_t taskCount    = 0;
    while (taskCount < maxTaskCount && swapchainStatus->isPending)
    {
        CommandProcessorTask task(std::move(mTaskQueue.front()));
        mTaskQueue.pop();
        ANGLE_TRY(processTask(&task));
        taskCount++;
    }
    ASSERT(!swapchainStatus->isPending);
    return angle::Result::Continue;
}

// CommandQueue public API implementation. These must be thread safe and never called from
// CommandQueue class itself.
CommandQueue::CommandQueue() : mPerfCounters{} {}

CommandQueue::~CommandQueue() = default;

void CommandQueue::destroy(Context *context)
{
    std::lock_guard<std::mutex> lock(mMutex);
    // Force all commands to finish by flushing all queues.
    for (VkQueue queue : mQueueMap)
    {
        if (queue != VK_NULL_HANDLE)
        {
            vkQueueWaitIdle(queue);
        }
    }

    RendererVk *renderer = context->getRenderer();

    // Assigns an infinite "last completed" serial to force garbage to delete.
    mLastCompletedSerials.fill(Serial::Infinite());

    for (CommandsState &state : mCommandsStateMap)
    {
        state.waitSemaphores.clear();
        state.waitSemaphoreStageMasks.clear();
        state.primaryCommands.destroy(renderer->getDevice());
        state.primaryCommandPool.destroy(renderer->getDevice());
    }

    mFenceRecycler.destroy(context);

    ASSERT(mInFlightCommands.empty());
}

angle::Result CommandQueue::init(Context *context, const DeviceQueueMap &queueMap)
{
    std::lock_guard<std::mutex> lock(mMutex);
    // In case of RendererVk gets re-initialized, we can't rely on constructor to do initialization
    // for us.
    mLastSubmittedSerials.fill(kZeroSerial);
    mLastCompletedSerials.fill(kZeroSerial);

    // Assign before initializing the command pools in order to get the queue family index.
    mQueueMap = queueMap;

    ANGLE_TRY(initCommandPool(context, ProtectionType::Unprotected));

    if (queueMap.isProtected())
    {
        ANGLE_TRY(initCommandPool(context, ProtectionType::Protected));
    }

    return angle::Result::Continue;
}

void CommandQueue::handleDeviceLost(RendererVk *renderer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::handleDeviceLost");
    std::lock_guard<std::mutex> lock(mMutex);
    VkDevice device = renderer->getDevice();

    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        if (batch.fence)
        {
            VkResult status = batch.fence.wait(device, renderer->getMaxFenceWaitTimeNs());
            // If the wait times out, it is probably not possible to recover from lost device
            ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);

            batch.fence.destroy(device);
        }

        // On device lost, here simply destroy the CommandBuffer, it will fully cleared later
        // by CommandPool::destroy
        if (batch.primaryCommands.valid())
        {
            batch.primaryCommands.destroy(device);
        }

        batch.resetSecondaryCommandBuffers(device);

        mLastCompletedSerials.setQueueSerial(batch.queueSerial);
    }
    mInFlightCommands.clear();
}

angle::Result CommandQueue::postSubmitCheck(Context *context)
{
    std::unique_lock<std::mutex> lock(mMutex);
    RendererVk *renderer = context->getRenderer();

    int finishedCount;
    ANGLE_TRY(checkCompletedCommandCount(context, &finishedCount));
    if (finishedCount > 0)
    {
        ANGLE_TRY(retireFinishedCommandsAndCleanupGarbage(context, finishedCount));
    }

    // CPU should be throttled to avoid accumulating too much memory garbage waiting to be
    // destroyed. This is important to keep peak memory usage at check when game launched and a lot
    // of staging buffers used for textures upload and then gets released. But if there is only one
    // command buffer in flight, we do not wait here to ensure we keep GPU busy.
    VkDeviceSize suballocationGarbageSize = renderer->getSuballocationGarbageSize();
    while (suballocationGarbageSize > kMaxBufferSuballocationGarbageSize &&
           mInFlightCommands.size() > 1)
    {
        ANGLE_TRY(finishOneCommandBatch(context, renderer->getMaxFenceWaitTimeNs()));
        suballocationGarbageSize = renderer->getSuballocationGarbageSize();
    }

    if (kOutputVmaStatsString)
    {
        renderer->outputVmaStatString();
    }

    return angle::Result::Continue;
}

angle::Result CommandQueue::finishResourceUse(Context *context,
                                              const ResourceUse &use,
                                              uint64_t timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    size_t finishCount = getBatchCountUpToSerials(context->getRenderer(), use.getSerials());
    if (finishCount == 0)
    {
        return angle::Result::Continue;
    }

    const SharedFence &sharedFence = getSharedFenceToWait(finishCount);
    // Wait for it finish.
    if (!sharedFence)
    {
        return angle::Result::Continue;
    }
    else
    {
        const SharedFence localSharedFenceToWaitOn = sharedFence;
        lock.unlock();
        ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::finishResourceUse");
        // You can only use the local copy of the sharedFence without lock;
        VkResult status = localSharedFenceToWaitOn.wait(context->getDevice(), timeout);
        ANGLE_VK_TRY(context, status);
        lock.lock();
    }

    // Clean up finished batches. After we unlocked, finishCount may have changed, recheck the
    // mInFlightCommands for all finished commands.
    int finishedCount;
    ANGLE_TRY(checkCompletedCommandCount(context, &finishedCount));
    if (finishedCount > 0)
    {
        ANGLE_TRY(retireFinishedCommandsAndCleanupGarbage(context, finishedCount));
    }
    ASSERT(allInFlightCommandsAreAfterSerials(use.getSerials()));
    ASSERT(hasResourceUseFinished(use));

    return angle::Result::Continue;
}

angle::Result CommandQueue::finishQueueSerial(Context *context,
                                              const QueueSerial &queueSerial,
                                              uint64_t timeout)
{
    vk::ResourceUse use(queueSerial);
    return finishResourceUse(context, use, timeout);
}

angle::Result CommandQueue::waitIdle(Context *context, uint64_t timeout)
{
    // Fill the local variable with lock
    vk::ResourceUse use;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mInFlightCommands.empty())
        {
            return angle::Result::Continue;
        }
        use.setQueueSerial(mInFlightCommands.back().queueSerial);
    }

    return finishResourceUse(context, use, timeout);
}

angle::Result CommandQueue::waitForResourceUseToFinishWithUserTimeout(Context *context,
                                                                      const ResourceUse &use,
                                                                      uint64_t timeout,
                                                                      VkResult *result)
{
    std::unique_lock<std::mutex> lock(mMutex);
    size_t finishCount = getBatchCountUpToSerials(context->getRenderer(), use.getSerials());
    // The serial is already complete if there is no in-flight work (i.e. mInFlightCommands is
    // empty).
    if (finishCount == 0)
    {
        *result = VK_SUCCESS;
        return angle::Result::Continue;
    }
    // The serial is already complete if every batch up to this serial is a garbage-clean-up-only
    // batch (i.e. empty submission that's optimized out)
    const SharedFence &sharedFence = getSharedFenceToWait(finishCount);
    if (!sharedFence)
    {
        *result = VK_SUCCESS;
        return angle::Result::Continue;
    }

    // Serial is not yet submitted. This is undefined behaviour, so we can do anything.
    if (!hasResourceUseSubmitted(use))
    {
        WARN() << "Waiting on an unsubmitted serial.";
        *result = VK_TIMEOUT;
        return angle::Result::Continue;
    }

    // Make a local copy of SharedFence with lock being held. Since SharedFence is refCounted, this
    // local copy of SharedFence will ensure underline VkFence will not go away.
    SharedFence localSharedFenceToWaitOn = sharedFence;
    lock.unlock();
    // You can only use the local copy of the sharedFence without lock;
    *result = localSharedFenceToWaitOn.wait(context->getDevice(), timeout);
    lock.lock();
    // Don't trigger an error on timeout.
    if (*result != VK_TIMEOUT)
    {
        ANGLE_VK_TRY(context, *result);
    }

    return angle::Result::Continue;
}

bool CommandQueue::isBusy(RendererVk *renderer) const
{
    // No lock is needed here since we are accessing atomic variables only.
    size_t maxIndex = renderer->getLargestQueueSerialIndexEverAllocated();
    for (SerialIndex i = 0; i <= maxIndex; ++i)
    {
        if (mLastSubmittedSerials[i] > mLastCompletedSerials[i])
        {
            return true;
        }
    }
    return false;
}

void CommandQueue::flushWaitSemaphores(ProtectionType protectionType,
                                       std::vector<VkSemaphore> &&waitSemaphores,
                                       std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks)
{
    ASSERT(!waitSemaphores.empty());
    ASSERT(waitSemaphores.size() == waitSemaphoreStageMasks.size());
    std::lock_guard<std::mutex> lock(mMutex);

    CommandsState &state = mCommandsStateMap[protectionType];

    state.waitSemaphores.insert(state.waitSemaphores.end(), waitSemaphores.begin(),
                                waitSemaphores.end());
    state.waitSemaphoreStageMasks.insert(state.waitSemaphoreStageMasks.end(),
                                         waitSemaphoreStageMasks.begin(),
                                         waitSemaphoreStageMasks.end());

    waitSemaphores.clear();
    waitSemaphoreStageMasks.clear();
}

angle::Result CommandQueue::flushOutsideRPCommands(
    Context *context,
    ProtectionType protectionType,
    OutsideRenderPassCommandBufferHelper **outsideRPCommands)
{
    std::lock_guard<std::mutex> lock(mMutex);
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context, protectionType));
    PrimaryCommandBuffer &commandBuffer = mCommandsStateMap[protectionType].primaryCommands;
    return (*outsideRPCommands)->flushToPrimary(context, &commandBuffer);
}

angle::Result CommandQueue::flushRenderPassCommands(
    Context *context,
    ProtectionType protectionType,
    const RenderPass &renderPass,
    RenderPassCommandBufferHelper **renderPassCommands)
{
    std::lock_guard<std::mutex> lock(mMutex);
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context, protectionType));
    PrimaryCommandBuffer &commandBuffer = mCommandsStateMap[protectionType].primaryCommands;
    return (*renderPassCommands)->flushToPrimary(context, &commandBuffer, &renderPass);
}

angle::Result CommandQueue::submitCommands(Context *context,
                                           ProtectionType protectionType,
                                           egl::ContextPriority priority,
                                           const VkSemaphore signalSemaphore,
                                           SecondaryCommandBufferList &&commandBuffersToReset,
                                           SecondaryCommandPools *commandPools,
                                           const QueueSerial &submitQueueSerial)
{
    ANGLE_TRY(submitCommandsImpl(context, protectionType, priority, signalSemaphore,
                                 std::move(commandBuffersToReset), commandPools,
                                 submitQueueSerial));
    return postSubmitCheck(context);
}

angle::Result CommandQueue::submitCommandsImpl(Context *context,
                                               ProtectionType protectionType,
                                               egl::ContextPriority priority,
                                               const VkSemaphore signalSemaphore,
                                               SecondaryCommandBufferList &&commandBuffersToReset,
                                               SecondaryCommandPools *commandPools,
                                               const QueueSerial &submitQueueSerial)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::submitCommands");
    std::unique_lock<std::mutex> lock(mMutex);
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    ++mPerfCounters.commandQueueSubmitCallsTotal;
    ++mPerfCounters.commandQueueSubmitCallsPerFrame;

    DeviceScoped<CommandBatch> scopedBatch(device);
    CommandBatch &batch = scopedBatch.get();

    batch.queueSerial           = submitQueueSerial;
    batch.protectionType        = protectionType;
    batch.commandPools          = commandPools;
    batch.commandBuffersToReset = std::move(commandBuffersToReset);

    CommandsState &state = mCommandsStateMap[protectionType];
    // Store the primary CommandBuffer in the in-flight list.
    batch.primaryCommands = std::move(state.primaryCommands);

    // Move to local copy of vectors since queueSubmit will release the lock.
    std::vector<VkSemaphore> waitSemaphores = std::move(state.waitSemaphores);
    std::vector<VkPipelineStageFlags> waitSemaphoreStageMasks =
        std::move(state.waitSemaphoreStageMasks);

    // Don't make a submission if there is nothing to submit.
    const bool needsQueueSubmit = batch.primaryCommands.valid() ||
                                  signalSemaphore != VK_NULL_HANDLE || !waitSemaphores.empty();
    VkSubmitInfo submitInfo                   = {};
    VkProtectedSubmitInfo protectedSubmitInfo = {};
    const Fence *fence                        = nullptr;

    if (needsQueueSubmit)
    {
        if (batch.primaryCommands.valid())
        {
            ANGLE_VK_TRY(context, batch.primaryCommands.end());
        }

        InitializeSubmitInfo(&submitInfo, batch.primaryCommands, waitSemaphores,
                             waitSemaphoreStageMasks, signalSemaphore);

        if (protectionType == ProtectionType::Protected)
        {
            protectedSubmitInfo.sType           = VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO;
            protectedSubmitInfo.pNext           = nullptr;
            protectedSubmitInfo.protectedSubmit = true;
            submitInfo.pNext                    = &protectedSubmitInfo;
        }

        ANGLE_VK_TRY(context, batch.fence.init(context->getDevice(), &mFenceRecycler));
        fence = &batch.fence.get();

        ++mPerfCounters.vkQueueSubmitCallsTotal;
        ++mPerfCounters.vkQueueSubmitCallsPerFrame;
    }

    // Note queueSubmit will release the lock.
    ANGLE_TRY(queueSubmit(context, std::move(lock), priority, submitInfo, fence, scopedBatch,
                          submitQueueSerial));

    // Clear local vector without lock.
    waitSemaphores.clear();
    waitSemaphoreStageMasks.clear();

    return angle::Result::Continue;
}

angle::Result CommandQueue::queueSubmitOneOff(Context *context,
                                              ProtectionType protectionType,
                                              egl::ContextPriority contextPriority,
                                              VkCommandBuffer commandBufferHandle,
                                              const Semaphore *waitSemaphore,
                                              VkPipelineStageFlags waitSemaphoreStageMask,
                                              const Fence *fence,
                                              SubmitPolicy submitPolicy,
                                              const QueueSerial &submitQueueSerial)
{
    std::unique_lock<std::mutex> lock(mMutex);
    DeviceScoped<CommandBatch> scopedBatch(context->getDevice());
    CommandBatch &batch  = scopedBatch.get();
    batch.queueSerial    = submitQueueSerial;
    batch.protectionType = protectionType;

    // If caller passed in a fence, use it. Otherwise create an internal fence. That way if we can
    // go through normal waitForQueueSerial code path to wait for it to finish.
    if (fence == nullptr)
    {
        ANGLE_VK_TRY(context, batch.fence.init(context->getDevice(), &mFenceRecycler));
        fence = &batch.fence.get();
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkProtectedSubmitInfo protectedSubmitInfo = {};
    ASSERT(protectionType == ProtectionType::Unprotected ||
           protectionType == ProtectionType::Protected);
    if (protectionType == ProtectionType::Protected)
    {
        protectedSubmitInfo.sType           = VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO;
        protectedSubmitInfo.pNext           = nullptr;
        protectedSubmitInfo.protectedSubmit = true;
        submitInfo.pNext                    = &protectedSubmitInfo;
    }

    if (commandBufferHandle != VK_NULL_HANDLE)
    {
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBufferHandle;
    }

    if (waitSemaphore != nullptr)
    {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores    = waitSemaphore->ptr();
        submitInfo.pWaitDstStageMask  = &waitSemaphoreStageMask;
    }

    ++mPerfCounters.vkQueueSubmitCallsTotal;
    ++mPerfCounters.vkQueueSubmitCallsPerFrame;

    // Note queueSubmit will release the lock.
    return queueSubmit(context, std::move(lock), contextPriority, submitInfo, fence, scopedBatch,
                       submitQueueSerial);
}

angle::Result CommandQueue::queueSubmit(Context *context,
                                        std::unique_lock<std::mutex> &&dequeueLock,
                                        egl::ContextPriority contextPriority,
                                        const VkSubmitInfo &submitInfo,
                                        const Fence *fence,
                                        DeviceScoped<CommandBatch> &commandBatch,
                                        const QueueSerial &submitQueueSerial)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::queueSubmit");
    RendererVk *renderer = context->getRenderer();

    // Lock relay to ensure the ordering of submission strictly follow the context's submission
    // order. This lock relay (first take mMutex and then mQueueSubmitMutex, and then release
    // mMutex) ensures we always have a lock covering the entire call which ensures the strict
    // submission order.
    std::lock_guard<std::mutex> queueSubmitLock(mQueueSubmitMutex);
    // CPU should be throttled to avoid mInFlightCommands from growing too fast. Important for
    // off-screen scenarios.
    if (mInFlightCommands.full())
    {
        ANGLE_TRY(finishOneCommandBatch(context, renderer->getMaxFenceWaitTimeNs()));
    }
    // Release the dequeue lock while doing potentially lengthy vkQueueSubmit call.
    // Note: after this point, you can not reference anything that required mMutex lock.
    dequeueLock.unlock();

    if (submitInfo.sType == VK_STRUCTURE_TYPE_SUBMIT_INFO)
    {
        VkQueue queue       = getQueue(contextPriority);
        VkFence fenceHandle = fence ? fence->getHandle() : VK_NULL_HANDLE;
        ANGLE_VK_TRY(context, vkQueueSubmit(queue, 1, &submitInfo, fenceHandle));
    }

    mInFlightCommands.push(commandBatch.release());

    // This must set last so that when this submission appears submitted, it actually already
    // submitted and enqueued to mInFlightCommands.
    mLastSubmittedSerials.setQueueSerial(submitQueueSerial);
    return angle::Result::Continue;
}

void CommandQueue::queuePresent(egl::ContextPriority contextPriority,
                                const VkPresentInfoKHR &presentInfo,
                                SwapchainStatus *swapchainStatus)
{
    std::lock_guard<std::mutex> queueSubmitLock(mQueueSubmitMutex);
    VkQueue queue                      = getQueue(contextPriority);
    swapchainStatus->lastPresentResult = vkQueuePresentKHR(queue, &presentInfo);
}

const angle::VulkanPerfCounters CommandQueue::getPerfCounters() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mPerfCounters;
}

void CommandQueue::resetPerFramePerfCounters()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mPerfCounters.commandQueueSubmitCallsPerFrame = 0;
    mPerfCounters.vkQueueSubmitCallsPerFrame      = 0;
}

angle::Result CommandQueue::checkCompletedCommands(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::checkCompletedCommands");
    std::lock_guard<std::mutex> lock(mMutex);
    int finishedCount;
    ANGLE_TRY(checkCompletedCommandCount(context, &finishedCount));
    if (finishedCount > 0)
    {
        ANGLE_TRY(retireFinishedCommandsAndCleanupGarbage(context, finishedCount));
    }
    return angle::Result::Continue;
}

// CommandQueue private API implementation. These are called by public API, so lock already held.
angle::Result CommandQueue::finishOneCommandBatch(Context *context, uint64_t timeout)
{
    ASSERT(!mInFlightCommands.empty());
    int finishedCount = 0;
    for (const CommandBatch &batch : mInFlightCommands)
    {
        finishedCount++;
        if (batch.fence)
        {
            VkResult status = batch.fence.wait(context->getDevice(), timeout);
            ANGLE_VK_TRY(context, status);
            break;
        }
    }

    if (finishedCount > 0)
    {
        // Clean up finished batches.
        ANGLE_TRY(retireFinishedCommandsAndCleanupGarbage(context, finishedCount));
    }
    return angle::Result::Continue;
}

angle::Result CommandQueue::checkCompletedCommandCount(Context *context, int *finishedCountOut)
{
    VkDevice device = context->getDevice();

    *finishedCountOut = 0;
    for (CommandBatch &batch : mInFlightCommands)
    {
        // For empty submissions, fence is not set but there may be garbage to be collected.  In
        // such a case, the empty submission is "completed" at the same time as the last submission
        // that actually happened.
        if (batch.fence)
        {
            VkResult result = batch.fence.getStatus(device);
            if (result == VK_NOT_READY)
            {
                break;
            }
            ANGLE_VK_TRY(context, result);
        }
        ++(*finishedCountOut);
    }
    return angle::Result::Continue;
}

angle::Result CommandQueue::retireFinishedCommands(Context *context, size_t finishedCount)
{
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    // First store the last completed queue serial value into a local variable and then update
    // mLastCompletedQueueSerial once in the end.
    angle::FastMap<Serial, kMaxFastQueueSerials> lastCompletedQueueSerials;
    for (size_t commandIndex = 0; commandIndex < finishedCount; ++commandIndex)
    {
        CommandBatch &batch = mInFlightCommands.front();

        lastCompletedQueueSerials[batch.queueSerial.getIndex()] = batch.queueSerial.getSerial();

        if (batch.fence)
        {
            batch.fence.release();
        }
        if (batch.primaryCommands.valid())
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "Primary command buffer recycling");
            PersistentCommandPool &commandPool =
                mCommandsStateMap[batch.protectionType].primaryCommandPool;
            ANGLE_TRY(commandPool.collect(context, std::move(batch.primaryCommands)));
        }

        if (batch.commandPools)
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "Secondary command buffer recycling");
            batch.resetSecondaryCommandBuffers(device);
        }
        mInFlightCommands.pop();
    }

    for (SerialIndex index = 0; index < lastCompletedQueueSerials.size(); index++)
    {
        // Set mLastCompletedSerials only if there is a lastCompletedQueueSerials in the index.
        if (lastCompletedQueueSerials[index] != kZeroSerial)
        {
            mLastCompletedSerials.setQueueSerial(index, lastCompletedQueueSerials[index]);
        }
    }
    return angle::Result::Continue;
}

angle::Result CommandQueue::retireFinishedCommandsAndCleanupGarbage(Context *context,
                                                                    size_t finishedCount)
{
    ASSERT(finishedCount > 0);
    RendererVk *renderer = context->getRenderer();

    ANGLE_TRY(retireFinishedCommands(context, finishedCount));

    // Now clean up RendererVk garbage
    renderer->cleanupGarbage();

    return angle::Result::Continue;
}

bool CommandQueue::allInFlightCommandsAreAfterSerials(const Serials &serials)
{
    for (const CommandBatch &batch : mInFlightCommands)
    {
        if (batch.queueSerial.getIndex() < serials.size() &&
            serials[batch.queueSerial.getIndex()] != kZeroSerial &&
            batch.queueSerial.getSerial() <= serials[batch.queueSerial.getIndex()])
        {
            return false;
        }
    }
    return true;
}

angle::Result CommandQueue::ensurePrimaryCommandBufferValid(Context *context,
                                                            ProtectionType protectionType)
{
    CommandsState &state = mCommandsStateMap[protectionType];

    if (state.primaryCommands.valid())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRY(state.primaryCommandPool.allocate(context, &state.primaryCommands));
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;
    ANGLE_VK_TRY(context, state.primaryCommands.begin(beginInfo));

    return angle::Result::Continue;
}

size_t CommandQueue::getBatchCountUpToSerials(RendererVk *renderer, const Serials &serials)
{
    if (mInFlightCommands.empty())
    {
        return 0;
    }

    if (renderer->getLargestQueueSerialIndexEverAllocated() < 64)
    {
        return GetBatchCountUpToSerials<angle::BitSet64<64>>(
            mInFlightCommands, mLastSubmittedSerials, mLastCompletedSerials, serials);
    }
    else
    {
        return GetBatchCountUpToSerials<angle::BitSetArray<kMaxQueueSerialIndexCount>>(
            mInFlightCommands, mLastSubmittedSerials, mLastCompletedSerials, serials);
    }
}

const SharedFence &CommandQueue::getSharedFenceToWait(size_t finishCount)
{
    ASSERT(finishCount > 0);
    // Because some submission maybe empty submission, we have to search for the closest non-empty
    // submission for the fence to wait on
    for (int i = static_cast<int>(finishCount) - 1; i >= 0; i--)
    {
        if (mInFlightCommands[i].fence)
        {
            return mInFlightCommands[i].fence;
        }
    }
    // If everything is finished, we just return the first one (which is also finished).
    return mInFlightCommands[0].fence;
}

// QueuePriorities:
constexpr float kVulkanQueuePriorityLow    = 0.0;
constexpr float kVulkanQueuePriorityMedium = 0.4;
constexpr float kVulkanQueuePriorityHigh   = 1.0;

const float QueueFamily::kQueuePriorities[static_cast<uint32_t>(egl::ContextPriority::EnumCount)] =
    {kVulkanQueuePriorityMedium, kVulkanQueuePriorityHigh, kVulkanQueuePriorityLow};

egl::ContextPriority DeviceQueueMap::getDevicePriority(egl::ContextPriority priority) const
{
    return mPriorities[priority];
}

DeviceQueueMap::~DeviceQueueMap() {}

DeviceQueueMap &DeviceQueueMap::operator=(const DeviceQueueMap &other)
{
    ASSERT(this != &other);
    if ((this != &other) && other.valid())
    {
        mIndex                                    = other.mIndex;
        mIsProtected                              = other.mIsProtected;
        mPriorities[egl::ContextPriority::Low]    = other.mPriorities[egl::ContextPriority::Low];
        mPriorities[egl::ContextPriority::Medium] = other.mPriorities[egl::ContextPriority::Medium];
        mPriorities[egl::ContextPriority::High]   = other.mPriorities[egl::ContextPriority::High];
        *static_cast<angle::PackedEnumMap<egl::ContextPriority, VkQueue> *>(this) = other;
    }
    return *this;
}

void QueueFamily::getDeviceQueue(VkDevice device,
                                 bool makeProtected,
                                 uint32_t queueIndex,
                                 VkQueue *queue)
{
    if (makeProtected)
    {
        VkDeviceQueueInfo2 queueInfo2 = {};
        queueInfo2.sType              = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
        queueInfo2.flags              = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
        queueInfo2.queueFamilyIndex   = mIndex;
        queueInfo2.queueIndex         = queueIndex;

        vkGetDeviceQueue2(device, &queueInfo2, queue);
    }
    else
    {
        vkGetDeviceQueue(device, mIndex, queueIndex, queue);
    }
}

DeviceQueueMap QueueFamily::initializeQueueMap(VkDevice device,
                                               bool makeProtected,
                                               uint32_t queueIndex,
                                               uint32_t queueCount)
{
    // QueueIndexing:
    constexpr uint32_t kQueueIndexMedium = 0;
    constexpr uint32_t kQueueIndexHigh   = 1;
    constexpr uint32_t kQueueIndexLow    = 2;

    ASSERT(queueCount);
    ASSERT((queueIndex + queueCount) <= mProperties.queueCount);
    DeviceQueueMap queueMap(mIndex, makeProtected);

    getDeviceQueue(device, makeProtected, queueIndex + kQueueIndexMedium,
                   &queueMap[egl::ContextPriority::Medium]);
    queueMap.mPriorities[egl::ContextPriority::Medium] = egl::ContextPriority::Medium;

    // If at least 2 queues, High has its own queue
    if (queueCount > 1)
    {
        getDeviceQueue(device, makeProtected, queueIndex + kQueueIndexHigh,
                       &queueMap[egl::ContextPriority::High]);
        queueMap.mPriorities[egl::ContextPriority::High] = egl::ContextPriority::High;
    }
    else
    {
        queueMap[egl::ContextPriority::High]             = queueMap[egl::ContextPriority::Medium];
        queueMap.mPriorities[egl::ContextPriority::High] = egl::ContextPriority::Medium;
    }
    // If at least 3 queues, Low has its own queue. Adjust Low priority.
    if (queueCount > 2)
    {
        getDeviceQueue(device, makeProtected, queueIndex + kQueueIndexLow,
                       &queueMap[egl::ContextPriority::Low]);
        queueMap.mPriorities[egl::ContextPriority::Low] = egl::ContextPriority::Low;
    }
    else
    {
        queueMap[egl::ContextPriority::Low]             = queueMap[egl::ContextPriority::Medium];
        queueMap.mPriorities[egl::ContextPriority::Low] = egl::ContextPriority::Medium;
    }
    return queueMap;
}

void QueueFamily::initialize(const VkQueueFamilyProperties &queueFamilyProperties, uint32_t index)
{
    mProperties = queueFamilyProperties;
    mIndex      = index;
}

uint32_t QueueFamily::FindIndex(const std::vector<VkQueueFamilyProperties> &queueFamilyProperties,
                                VkQueueFlags flags,
                                int32_t matchNumber,
                                uint32_t *matchCount)
{
    uint32_t index = QueueFamily::kInvalidIndex;
    uint32_t count = 0;

    for (uint32_t familyIndex = 0; familyIndex < queueFamilyProperties.size(); ++familyIndex)
    {
        const auto &queueInfo = queueFamilyProperties[familyIndex];
        if ((queueInfo.queueFlags & flags) == flags)
        {
            ASSERT(queueInfo.queueCount > 0);
            count++;
            if ((index == QueueFamily::kInvalidIndex) && (matchNumber-- == 0))
            {
                index = familyIndex;
            }
        }
    }
    if (matchCount)
    {
        *matchCount = count;
    }

    return index;
}

}  // namespace vk
}  // namespace rx
