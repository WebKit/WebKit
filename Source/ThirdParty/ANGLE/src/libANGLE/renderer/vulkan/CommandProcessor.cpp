//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CommandProcessor.cpp:
//    Implements the class methods for CommandProcessor.
//

#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{
namespace vk
{
namespace
{
constexpr size_t kInFlightCommandsLimit = 50u;
constexpr bool kOutputVmaStatsString    = false;
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
size_t GetBatchCountUpToSerials(std::vector<CommandBatch> &inFlightCommands,
                                const AtomicQueueSerialFixedArray &lastSubmittedSerials,
                                const AtomicQueueSerialFixedArray &lastCompletedSerials,
                                const Serials &serials,
                                Shared<Fence> **fenceToWaitOnOut)
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
    size_t batchCountToFinish = 0;
    for (size_t i = 0; i < inFlightCommands.size(); i++)
    {
        CommandBatch &batch = inFlightCommands[i];

        if (serialBitMaskToFinish[batch.queueSerial.getIndex()])
        {
            // Update what the serial will look like if this batch is completed.
            serialsWillBeFinished[batch.queueSerial.getIndex()] = batch.queueSerial.getSerial();

            // If finish this batch will make completed serial meet the requested serial, we should
            // wait, and we are done with this index.
            if (serialsWillBeFinished[batch.queueSerial.getIndex()] >=
                serials[batch.queueSerial.getIndex()])
            {
                batchCountToFinish = i + 1;

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
    // Now search for the nearest fence to wait on.
    for (int i = static_cast<int>(batchCountToFinish) - 1; i >= 0; i--)
    {
        if (inFlightCommands[i].fence.isReferenced())
        {
            *fenceToWaitOnOut = &inFlightCommands[i].fence;
            break;
        }
    }

    return batchCountToFinish;
}
}  // namespace

angle::Result FenceRecycler::newSharedFence(Context *context, Shared<Fence> *sharedFenceOut)
{
    bool gotRecycledFence = false;
    Fence fence;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRecyler.empty())
        {
            mRecyler.fetch(&fence);
            gotRecycledFence = true;
        }
    }

    VkDevice device(context->getDevice());
    if (gotRecycledFence)
    {
        ANGLE_VK_TRY(context, fence.reset(device));
    }
    else
    {
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags             = 0;
        ANGLE_VK_TRY(context, fence.init(device, fenceCreateInfo));
    }
    sharedFenceOut->assign(device, std::move(fence));
    return angle::Result::Continue;
}

void FenceRecycler::destroy(Context *context)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mRecyler.destroy(context->getDevice());
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
    mOneOffCommandBufferVk          = VK_NULL_HANDLE;
    mPriority                       = egl::ContextPriority::Medium;
    mHasProtectedContent            = false;
}

void CommandProcessorTask::initOutsideRenderPassProcessCommands(
    bool hasProtectedContent,
    OutsideRenderPassCommandBufferHelper *commandBuffer)
{
    mTask                           = CustomTask::ProcessOutsideRenderPassCommands;
    mOutsideRenderPassCommandBuffer = commandBuffer;
    mHasProtectedContent            = hasProtectedContent;
}

void CommandProcessorTask::initRenderPassProcessCommands(
    bool hasProtectedContent,
    RenderPassCommandBufferHelper *commandBuffer,
    const RenderPass *renderPass)
{
    mTask                    = CustomTask::ProcessRenderPassCommands;
    mRenderPassCommandBuffer = commandBuffer;
    mRenderPass              = renderPass;
    mHasProtectedContent     = hasProtectedContent;
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
                mPresentRegions.pNext          = presentRegions->pNext;
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
            default:
                ERR() << "Unknown sType: " << sType << " in VkPresentInfoKHR.pNext chain";
                UNREACHABLE();
                break;
        }
    }
}

void CommandProcessorTask::initPresent(egl::ContextPriority priority,
                                       const VkPresentInfoKHR &presentInfo)
{
    mTask     = CustomTask::Present;
    mPriority = priority;
    copyPresentInfo(presentInfo);
}

void CommandProcessorTask::initFlushAndQueueSubmit(
    const std::vector<VkSemaphore> &waitSemaphores,
    const std::vector<VkPipelineStageFlags> &waitSemaphoreStageMasks,
    const VkSemaphore semaphore,
    bool hasProtectedContent,
    egl::ContextPriority priority,
    SecondaryCommandPools *commandPools,
    SecondaryCommandBufferList &&commandBuffersToReset,
    const QueueSerial &submitQueueSerial)
{
    mTask                    = CustomTask::FlushAndQueueSubmit;
    mWaitSemaphores          = waitSemaphores;
    mWaitSemaphoreStageMasks = waitSemaphoreStageMasks;
    mSemaphore               = semaphore;
    mCommandPools            = commandPools;
    mCommandBuffersToReset   = std::move(commandBuffersToReset);
    mPriority                = priority;
    mHasProtectedContent     = hasProtectedContent;
    mSubmitQueueSerial       = submitQueueSerial;
}

void CommandProcessorTask::initOneOffQueueSubmit(VkCommandBuffer commandBufferHandle,
                                                 bool hasProtectedContent,
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
    mHasProtectedContent          = hasProtectedContent;
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
    std::swap(mHasProtectedContent, rhs.mHasProtectedContent);
    std::swap(mOneOffCommandBufferVk, rhs.mOneOffCommandBufferVk);

    copyPresentInfo(rhs.mPresentInfo);

    // clear rhs now that everything has moved.
    rhs.initTask();

    return *this;
}

// CommandBatch implementation.
CommandBatch::CommandBatch() : commandPools(nullptr), hasProtectedContent(false) {}

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
    std::swap(hasProtectedContent, other.hasProtectedContent);
    return *this;
}

void CommandBatch::destroy(VkDevice device)
{
    primaryCommands.destroy(device);
    fence.reset(device);
    hasProtectedContent = false;
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

CommandProcessor::CommandProcessor(RendererVk *renderer)
    : Context(renderer), mWorkerThreadIdle(false)
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
    else
    {
        Error err = mErrors.front();
        mErrors.pop();
        errorHandlingContext->handleError(err.errorCode, err.file, err.function, err.line);
        return angle::Result::Stop;
    }
}

void CommandProcessor::queueCommand(CommandProcessorTask &&task)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::queueCommand");
    // Grab the worker mutex so that we put things on the queue in the same order as we give out
    // serials.
    std::lock_guard<std::mutex> queueLock(mWorkerMutex);

    mTasks.emplace(std::move(task));
    mWorkAvailableCondition.notify_one();
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
        std::unique_lock<std::mutex> lock(mWorkerMutex);
        if (mTasks.empty())
        {
            mWorkerThreadIdle = true;
            mWorkerIdleCondition.notify_all();
            // Only wake if notified and command queue is not empty
            mWorkAvailableCondition.wait(lock, [this] { return !mTasks.empty(); });
        }
        mWorkerThreadIdle = false;
        CommandProcessorTask task(std::move(mTasks.front()));
        mTasks.pop();
        lock.unlock();

        ANGLE_TRY(processTask(&task));
        if (task.getTaskCommand() == CustomTask::Exit)
        {

            *exitThread = true;
            lock.lock();
            mWorkerThreadIdle = true;
            mWorkerIdleCondition.notify_one();
            return angle::Result::Continue;
        }
    }

    UNREACHABLE();
    return angle::Result::Stop;
}

angle::Result CommandProcessor::processTask(CommandProcessorTask *task)
{
    switch (task->getTaskCommand())
    {
        case CustomTask::Exit:
        {
            ANGLE_TRY(mCommandQueue.waitIdle(this, mRenderer->getMaxFenceWaitTimeNs()));
            // Shutting down so cleanup
            mCommandQueue.destroy(this);
            break;
        }
        case CustomTask::FlushAndQueueSubmit:
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "processTask::FlushAndQueueSubmit");
            // End command buffer

            // Call submitCommands()
            ANGLE_TRY(mCommandQueue.submitCommands(
                this, task->hasProtectedContent(), task->getPriority(), task->getWaitSemaphores(),
                task->getWaitSemaphoreStageMasks(), task->getSemaphore(),
                std::move(task->getCommandBuffersToReset()), task->getCommandPools(),
                task->getSubmitQueueSerial()));
            break;
        }
        case CustomTask::OneOffQueueSubmit:
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "processTask::OneOffQueueSubmit");

            ANGLE_TRY(mCommandQueue.queueSubmitOneOff(
                this, task->hasProtectedContent(), task->getPriority(),
                task->getOneOffCommandBufferVk(), task->getOneOffWaitSemaphore(),
                task->getOneOffWaitSemaphoreStageMask(), task->getOneOffFence(),
                SubmitPolicy::EnsureSubmitted, task->getSubmitQueueSerial()));
            ANGLE_TRY(mCommandQueue.checkCompletedCommands(this));
            break;
        }
        case CustomTask::Present:
        {
            VkResult result = present(task->getPriority(), task->getPresentInfo());
            if (ANGLE_UNLIKELY(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR))
            {
                // We get to ignore these as they are not fatal
            }
            else if (ANGLE_UNLIKELY(result != VK_SUCCESS))
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
            ANGLE_TRY(mCommandQueue.flushOutsideRPCommands(this, task->hasProtectedContent(),
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
            ANGLE_TRY(mCommandQueue.flushRenderPassCommands(
                this, task->hasProtectedContent(), *task->getRenderPass(), &commandBuffer));

            RenderPassCommandBufferHelper *originalCommandBuffer =
                task->getRenderPassCommandBuffer();
            mRenderer->recycleRenderPassCommandBufferHelper(mRenderer->getDevice(),
                                                            &originalCommandBuffer);
            break;
        }
        case CustomTask::CheckCompletedCommands:
        {
            ANGLE_TRY(mCommandQueue.checkCompletedCommands(this));
            break;
        }
        default:
            UNREACHABLE();
            break;
    }

    return angle::Result::Continue;
}

angle::Result CommandProcessor::checkCompletedCommands(Context *context)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    CommandProcessorTask checkCompletedTask;
    checkCompletedTask.initTask(CustomTask::CheckCompletedCommands);
    queueCommand(std::move(checkCompletedTask));

    return angle::Result::Continue;
}

angle::Result CommandProcessor::waitForWorkComplete(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::waitForWorkComplete");
    std::unique_lock<std::mutex> lock(mWorkerMutex);
    mWorkerIdleCondition.wait(lock, [this] { return (mTasks.empty() && mWorkerThreadIdle); });
    // Worker thread is idle and command queue is empty so good to continue

    // Sync any errors to the context
    bool shouldStop = hasPendingError();
    while (hasPendingError())
    {
        (void)checkAndPopPendingError(context);
    }
    return shouldStop ? angle::Result::Stop : angle::Result::Continue;
}

angle::Result CommandProcessor::init(Context *context, const DeviceQueueMap &queueMap)
{
    ANGLE_TRY(mCommandQueue.init(context, queueMap));

    mTaskThread = std::thread(&CommandProcessor::processTasks, this);

    return angle::Result::Continue;
}

void CommandProcessor::destroy(Context *context)
{
    CommandProcessorTask endTask;
    endTask.initTask(CustomTask::Exit);
    queueCommand(std::move(endTask));
    (void)waitForWorkComplete(context);
    if (mTaskThread.joinable())
    {
        mTaskThread.join();
    }
}

bool CommandProcessor::isBusy() const
{
    std::lock_guard<std::mutex> workerLock(mWorkerMutex);
    return !mTasks.empty() || mCommandQueue.isBusy();
}

// Wait until all commands up to and including serial have been processed
angle::Result CommandProcessor::finishQueueSerial(Context *context,
                                                  const QueueSerial &queueSerial,
                                                  uint64_t timeout)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::finishQueueSerial");
    // TODO: Only call waitForWorkComplete if mUse still have inflight commands in processor that
    // references this queueSerial. https://issuetracker.google.com/261098465
    ANGLE_TRY(waitForWorkComplete(context));
    return mCommandQueue.finishQueueSerial(context, queueSerial, timeout);
}

angle::Result CommandProcessor::finishResourceUse(Context *context,
                                                  const ResourceUse &use,
                                                  uint64_t timeout)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::finishResourceUse");
    // TODO: Only call waitForWorkComplete if mUse still have inflight commands in processor that
    // references this queueSerial. https://issuetracker.google.com/261098465
    ANGLE_TRY(waitForWorkComplete(context));
    return mCommandQueue.finishResourceUse(context, use, timeout);
}

angle::Result CommandProcessor::waitIdle(Context *context, uint64_t timeout)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::waitIdle");
    ANGLE_TRY(waitForWorkComplete(context));
    return mCommandQueue.waitIdle(this, mRenderer->getMaxFenceWaitTimeNs());
}

void CommandProcessor::handleDeviceLost(RendererVk *renderer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::handleDeviceLost");
    std::unique_lock<std::mutex> lock(mWorkerMutex);
    mWorkerIdleCondition.wait(lock, [this] { return (mTasks.empty() && mWorkerThreadIdle); });

    // Worker thread is idle and command queue is empty so good to continue
    mCommandQueue.handleDeviceLost(renderer);
}

VkResult CommandProcessor::getLastAndClearPresentResult(VkSwapchainKHR swapchain)
{
    std::unique_lock<std::mutex> lock(mSwapchainStatusMutex);
    if (mSwapchainStatus.find(swapchain) == mSwapchainStatus.end())
    {
        // Wake when required swapchain status becomes available
        mSwapchainStatusCondition.wait(lock, [this, swapchain] {
            return mSwapchainStatus.find(swapchain) != mSwapchainStatus.end();
        });
    }
    VkResult result = mSwapchainStatus[swapchain];
    mSwapchainStatus.erase(swapchain);
    return result;
}

VkResult CommandProcessor::present(egl::ContextPriority priority,
                                   const VkPresentInfoKHR &presentInfo)
{
    std::lock_guard<std::mutex> lock(mSwapchainStatusMutex);
    ANGLE_TRACE_EVENT0("gpu.angle", "vkQueuePresentKHR");
    VkResult result = mCommandQueue.queuePresent(priority, presentInfo);

    // Verify that we are presenting one and only one swapchain
    ASSERT(presentInfo.swapchainCount == 1);
    ASSERT(presentInfo.pResults == nullptr);
    mSwapchainStatus[presentInfo.pSwapchains[0]] = result;

    mSwapchainStatusCondition.notify_all();

    return result;
}

angle::Result CommandProcessor::submitCommands(
    Context *context,
    bool hasProtectedContent,
    egl::ContextPriority priority,
    const std::vector<VkSemaphore> &waitSemaphores,
    const std::vector<VkPipelineStageFlags> &waitSemaphoreStageMasks,
    const VkSemaphore signalSemaphore,
    SecondaryCommandBufferList &&commandBuffersToReset,
    SecondaryCommandPools *commandPools,
    const QueueSerial &submitQueueSerial)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    CommandProcessorTask task;
    task.initFlushAndQueueSubmit(waitSemaphores, waitSemaphoreStageMasks, signalSemaphore,
                                 hasProtectedContent, priority, commandPools,
                                 std::move(commandBuffersToReset), submitQueueSerial);

    queueCommand(std::move(task));

    mLastSubmittedSerials.setQueueSerial(submitQueueSerial);

    return angle::Result::Continue;
}

angle::Result CommandProcessor::queueSubmitOneOff(Context *context,
                                                  bool hasProtectedContent,
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
    task.initOneOffQueueSubmit(commandBufferHandle, hasProtectedContent, contextPriority,
                               waitSemaphore, waitSemaphoreStageMask, fence, submitQueueSerial);
    queueCommand(std::move(task));

    mLastSubmittedSerials.setQueueSerial(submitQueueSerial);

    if (submitPolicy == SubmitPolicy::EnsureSubmitted)
    {
        // Caller has synchronization requirement to have work in GPU pipe when returning from this
        // function.
        ANGLE_TRY(waitForWorkComplete(context));
    }

    return angle::Result::Continue;
}

VkResult CommandProcessor::queuePresent(egl::ContextPriority contextPriority,
                                        const VkPresentInfoKHR &presentInfo)
{
    CommandProcessorTask task;
    task.initPresent(contextPriority, presentInfo);

    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::queuePresent");
    queueCommand(std::move(task));

    // Always return success, when we call acquireNextImage we'll check the return code. This
    // allows the app to continue working until we really need to know the return code from
    // present.
    return VK_SUCCESS;
}

angle::Result CommandProcessor::waitForResourceUseToFinishWithUserTimeout(Context *context,
                                                                          const ResourceUse &use,
                                                                          uint64_t timeout,
                                                                          VkResult *result)
{
    // If finishResourceUse times out we generate an error. Therefore we a large timeout.
    // TODO: https://issuetracker.google.com/170312581 - Wait with timeout.
    return finishResourceUse(context, use, mRenderer->getMaxFenceWaitTimeNs());
}

angle::Result CommandProcessor::flushOutsideRPCommands(
    Context *context,
    bool hasProtectedContent,
    OutsideRenderPassCommandBufferHelper **outsideRPCommands)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    (*outsideRPCommands)->markClosed();

    // Detach functions are only used for ring buffer allocators.
    SecondaryCommandMemoryAllocator *allocator = (*outsideRPCommands)->detachAllocator();

    CommandProcessorTask task;
    task.initOutsideRenderPassProcessCommands(hasProtectedContent, *outsideRPCommands);
    queueCommand(std::move(task));

    ANGLE_TRY(mRenderer->getOutsideRenderPassCommandBufferHelper(
        context, (*outsideRPCommands)->getCommandPool(), allocator, outsideRPCommands));

    return angle::Result::Continue;
}

angle::Result CommandProcessor::flushRenderPassCommands(
    Context *context,
    bool hasProtectedContent,
    const RenderPass &renderPass,
    RenderPassCommandBufferHelper **renderPassCommands)
{
    ANGLE_TRY(checkAndPopPendingError(context));

    (*renderPassCommands)->markClosed();

    // Detach functions are only used for ring buffer allocators.
    SecondaryCommandMemoryAllocator *allocator = (*renderPassCommands)->detachAllocator();

    CommandProcessorTask task;
    task.initRenderPassProcessCommands(hasProtectedContent, *renderPassCommands, &renderPass);
    queueCommand(std::move(task));

    ANGLE_TRY(mRenderer->getRenderPassCommandBufferHelper(
        context, (*renderPassCommands)->getCommandPool(), allocator, renderPassCommands));

    return angle::Result::Continue;
}

angle::Result CommandProcessor::ensureNoPendingWork(Context *context)
{
    return waitForWorkComplete(context);
}

bool CommandProcessor::hasUnsubmittedUse(const vk::ResourceUse &use) const
{
    const Serials &serials = use.getSerials();
    for (SerialIndex i = 0; i < serials.size(); ++i)
    {
        if (serials[i] > mLastSubmittedSerials[i])
        {
            return true;
        }
    }
    return false;
}

// CommandQueue implementation.
CommandQueue::CommandQueue() : mPerfCounters{} {}

CommandQueue::~CommandQueue() = default;

void CommandQueue::destroy(Context *context)
{
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

    mPrimaryCommands.destroy(renderer->getDevice());
    mPrimaryCommandPool.destroy(renderer->getDevice());

    if (mProtectedPrimaryCommandPool.valid())
    {
        mProtectedPrimaryCommands.destroy(renderer->getDevice());
        mProtectedPrimaryCommandPool.destroy(renderer->getDevice());
    }

    mFenceRecycler.destroy(context);

    ASSERT(mInFlightCommands.empty());
}

angle::Result CommandQueue::init(Context *context, const DeviceQueueMap &queueMap)
{
    // In case of RendererVk gets re-initialized, we can't rely on constructor to do initialization
    // for us.
    mLastSubmittedSerials.fill(kZeroSerial);
    mLastCompletedSerials.fill(kZeroSerial);

    // Initialize the command pool now that we know the queue family index.
    ANGLE_TRY(mPrimaryCommandPool.init(context, false, queueMap.getIndex()));
    mQueueMap = queueMap;

    if (queueMap.isProtected())
    {
        ANGLE_TRY(mProtectedPrimaryCommandPool.init(context, true, queueMap.getIndex()));
    }

    return angle::Result::Continue;
}

angle::Result CommandQueue::checkCompletedCommands(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::checkCompletedCommandsNoLock");
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    int finishedCount = 0;

    for (CommandBatch &batch : mInFlightCommands)
    {
        // For empty submissions, fence is not set but there may be garbage to be collected.  In
        // such a case, the empty submission is "completed" at the same time as the last submission
        // that actually happened.
        if (batch.fence.isReferenced())
        {
            VkResult result = batch.fence.get().getStatus(device);
            if (result == VK_NOT_READY)
            {
                break;
            }
            ANGLE_VK_TRY(context, result);
        }
        ++finishedCount;
    }

    if (finishedCount == 0)
    {
        return angle::Result::Continue;
    }

    return retireFinishedCommandsAndCleanupGarbage(context, finishedCount);
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
        CommandBatch &batch = mInFlightCommands[commandIndex];

        lastCompletedQueueSerials[batch.queueSerial.getIndex()] = batch.queueSerial.getSerial();

        if (batch.fence.isReferenced())
        {
            mFenceRecycler.resetSharedFence(&batch.fence);
        }
        if (batch.primaryCommands.valid())
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "Primary command buffer recycling");
            PersistentCommandPool &commandPool = getCommandPool(batch.hasProtectedContent);
            ANGLE_TRY(commandPool.collect(context, std::move(batch.primaryCommands)));
        }

        if (batch.commandPools)
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "Secondary command buffer recycling");
            batch.resetSecondaryCommandBuffers(device);
        }
    }

    for (SerialIndex index = 0; index < lastCompletedQueueSerials.size(); index++)
    {
        // Set mLastCompletedSerials only if there is a lastCompletedQueueSerials in the index.
        if (lastCompletedQueueSerials[index] != kZeroSerial)
        {
            mLastCompletedSerials.setQueueSerial(index, lastCompletedQueueSerials[index]);
        }
    }

    auto beginIter = mInFlightCommands.begin();
    mInFlightCommands.erase(beginIter, beginIter + finishedCount);

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

void CommandQueue::releaseToCommandBatch(bool hasProtectedContent,
                                         PrimaryCommandBuffer &&commandBuffer,
                                         SecondaryCommandPools *commandPools,
                                         CommandBatch *batch)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::releaseToCommandBatch");

    batch->primaryCommands     = std::move(commandBuffer);
    batch->commandPools        = commandPools;
    batch->hasProtectedContent = hasProtectedContent;
}

void CommandQueue::handleDeviceLost(RendererVk *renderer)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::handleDeviceLost");

    VkDevice device = renderer->getDevice();

    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        if (batch.fence.isReferenced())
        {
            VkResult status = batch.fence.get().wait(device, renderer->getMaxFenceWaitTimeNs());
            // If the wait times out, it is probably not possible to recover from lost device
            ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);

            batch.fence.reset(device);
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

angle::Result CommandQueue::finishQueueSerial(Context *context,
                                              const QueueSerial &queueSerial,
                                              uint64_t timeout)
{
    vk::ResourceUse use(queueSerial);
    return finishResourceUse(context, use, timeout);
}

angle::Result CommandQueue::finishResourceUse(Context *context,
                                              const ResourceUse &use,
                                              uint64_t timeout)
{
    if (mInFlightCommands.empty())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::finishResourceUse");

    Shared<Fence> *fenceToWaitOn = nullptr;
    size_t finishCount =
        getBatchCountUpToSerials(context->getRenderer(), use.getSerials(), &fenceToWaitOn);

    if (finishCount == 0)
    {
        return angle::Result::Continue;
    }

    // Wait for it finish.  If no fence, the serial is already finished, it might just have garbage
    // to clean up.
    if (fenceToWaitOn != nullptr)
    {
        VkDevice device = context->getDevice();
        VkResult status = fenceToWaitOn->get().wait(device, timeout);

        ANGLE_VK_TRY(context, status);
    }

    // Clean up finished batches.
    ANGLE_TRY(retireFinishedCommandsAndCleanupGarbage(context, finishCount));
    ASSERT(allInFlightCommandsAreAfterSerials(use.getSerials()));

    return angle::Result::Continue;
}

angle::Result CommandQueue::waitIdle(Context *context, uint64_t timeout)
{
    if (mInFlightCommands.empty())
    {
        return angle::Result::Continue;
    }

    return finishQueueSerial(context, mInFlightCommands.back().queueSerial, timeout);
}

angle::Result CommandQueue::submitCommands(
    Context *context,
    bool hasProtectedContent,
    egl::ContextPriority priority,
    const std::vector<VkSemaphore> &waitSemaphores,
    const std::vector<VkPipelineStageFlags> &waitSemaphoreStageMasks,
    const VkSemaphore signalSemaphore,
    SecondaryCommandBufferList &&commandBuffersToReset,
    SecondaryCommandPools *commandPools,
    const QueueSerial &submitQueueSerial)
{
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    ++mPerfCounters.commandQueueSubmitCallsTotal;
    ++mPerfCounters.commandQueueSubmitCallsPerFrame;

    DeviceScoped<CommandBatch> scopedBatch(device);
    CommandBatch &batch = scopedBatch.get();

    batch.queueSerial           = submitQueueSerial;
    batch.hasProtectedContent   = hasProtectedContent;
    batch.commandBuffersToReset = std::move(commandBuffersToReset);

    // Don't make a submission if there is nothing to submit.
    PrimaryCommandBuffer &commandBuffer = getCommandBuffer(hasProtectedContent);
    const bool hasAnyPendingCommands    = commandBuffer.valid();
    if (hasAnyPendingCommands || signalSemaphore != VK_NULL_HANDLE || !waitSemaphores.empty())
    {
        if (commandBuffer.valid())
        {
            ANGLE_VK_TRY(context, commandBuffer.end());
        }

        VkSubmitInfo submitInfo = {};
        InitializeSubmitInfo(&submitInfo, commandBuffer, waitSemaphores, waitSemaphoreStageMasks,
                             signalSemaphore);

        VkProtectedSubmitInfo protectedSubmitInfo = {};
        if (hasProtectedContent)
        {
            protectedSubmitInfo.sType           = VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO;
            protectedSubmitInfo.pNext           = nullptr;
            protectedSubmitInfo.protectedSubmit = true;
            submitInfo.pNext                    = &protectedSubmitInfo;
        }

        ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::submitCommands");

        ANGLE_TRY(mFenceRecycler.newSharedFence(context, &batch.fence));
        ANGLE_TRY(
            queueSubmit(context, priority, submitInfo, &batch.fence.get(), batch.queueSerial));
    }
    else
    {
        mLastSubmittedSerials.setQueueSerial(submitQueueSerial);
    }

    // Store the primary CommandBuffer and command pool used for secondary CommandBuffers
    // in the in-flight list.
    if (hasProtectedContent)
    {
        releaseToCommandBatch(hasProtectedContent, std::move(mProtectedPrimaryCommands),
                              commandPools, &batch);
    }
    else
    {
        releaseToCommandBatch(hasProtectedContent, std::move(mPrimaryCommands), commandPools,
                              &batch);
    }
    mInFlightCommands.emplace_back(scopedBatch.release());

    ANGLE_TRY(checkCompletedCommands(context));

    // CPU should be throttled to avoid mInFlightCommands from growing too fast. Important for
    // off-screen scenarios.
    if (mInFlightCommands.size() > kInFlightCommandsLimit)
    {
        size_t numCommandsToFinish      = mInFlightCommands.size() - kInFlightCommandsLimit;
        const QueueSerial &finishSerial = mInFlightCommands[numCommandsToFinish].queueSerial;
        ANGLE_TRY(finishQueueSerial(context, finishSerial, renderer->getMaxFenceWaitTimeNs()));
    }

    // CPU should be throttled to avoid accumulating too much memory garbage waiting to be
    // destroyed. This is important to keep peak memory usage at check when game launched and a lot
    // of staging buffers used for textures upload and then gets released. But if there is only one
    // command buffer in flight, we do not wait here to ensure we keep GPU busy.
    VkDeviceSize suballocationGarbageSize = renderer->getSuballocationGarbageSize();
    while (suballocationGarbageSize > kMaxBufferSuballocationGarbageSize &&
           mInFlightCommands.size() > 1)
    {
        const QueueSerial &finishSerial = mInFlightCommands.back().queueSerial;
        ANGLE_TRY(finishQueueSerial(context, finishSerial, renderer->getMaxFenceWaitTimeNs()));
        suballocationGarbageSize = renderer->getSuballocationGarbageSize();
    }

    return angle::Result::Continue;
}

angle::Result CommandQueue::waitForResourceUseToFinishWithUserTimeout(Context *context,
                                                                      const ResourceUse &use,
                                                                      uint64_t timeout,
                                                                      VkResult *result)
{
    Shared<Fence> *fenceToWaitOn = nullptr;
    size_t finishCount =
        getBatchCountUpToSerials(context->getRenderer(), use.getSerials(), &fenceToWaitOn);

    // The serial is already complete if:
    //
    // - There is no in-flight work (i.e. mInFlightCommands is empty), or
    // - The given serial is smaller than the smallest serial, or
    // - Every batch up to this serial is a garbage-clean-up-only batch (i.e. empty submission
    //   that's optimized out)
    if (finishCount == 0 || fenceToWaitOn == nullptr)
    {
        *result = VK_SUCCESS;
        return angle::Result::Continue;
    }

    // Serial is not yet submitted. This is undefined behaviour, so we can do anything.
    if (hasUnsubmittedUse(use))
    {
        WARN() << "Waiting on an unsubmitted serial.";
        *result = VK_TIMEOUT;
        return angle::Result::Continue;
    }

    *result = fenceToWaitOn->get().wait(context->getDevice(), timeout);

    // Don't trigger an error on timeout.
    if (*result != VK_TIMEOUT)
    {
        ANGLE_VK_TRY(context, *result);
    }

    return angle::Result::Continue;
}

angle::Result CommandQueue::ensurePrimaryCommandBufferValid(Context *context,
                                                            bool hasProtectedContent)
{
    PersistentCommandPool &commandPool  = getCommandPool(hasProtectedContent);
    PrimaryCommandBuffer &commandBuffer = getCommandBuffer(hasProtectedContent);

    if (commandBuffer.valid())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRY(commandPool.allocate(context, &commandBuffer));
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;
    ANGLE_VK_TRY(context, commandBuffer.begin(beginInfo));

    return angle::Result::Continue;
}

angle::Result CommandQueue::flushOutsideRPCommands(
    Context *context,
    bool hasProtectedContent,
    OutsideRenderPassCommandBufferHelper **outsideRPCommands)
{
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context, hasProtectedContent));
    PrimaryCommandBuffer &commandBuffer = getCommandBuffer(hasProtectedContent);
    return (*outsideRPCommands)->flushToPrimary(context, &commandBuffer);
}

angle::Result CommandQueue::flushRenderPassCommands(
    Context *context,
    bool hasProtectedContent,
    const RenderPass &renderPass,
    RenderPassCommandBufferHelper **renderPassCommands)
{
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context, hasProtectedContent));
    PrimaryCommandBuffer &commandBuffer = getCommandBuffer(hasProtectedContent);
    return (*renderPassCommands)->flushToPrimary(context, &commandBuffer, &renderPass);
}

angle::Result CommandQueue::queueSubmitOneOff(Context *context,
                                              bool hasProtectedContent,
                                              egl::ContextPriority contextPriority,
                                              VkCommandBuffer commandBufferHandle,
                                              const Semaphore *waitSemaphore,
                                              VkPipelineStageFlags waitSemaphoreStageMask,
                                              const Fence *fence,
                                              SubmitPolicy submitPolicy,
                                              const QueueSerial &submitQueueSerial)
{
    DeviceScoped<CommandBatch> scopedBatch(context->getDevice());
    CommandBatch &batch       = scopedBatch.get();
    batch.queueSerial         = submitQueueSerial;
    batch.hasProtectedContent = hasProtectedContent;

    // If caller passed in a fence, use it. Otherwise create an internal fence. That way if we can
    // go through normal waitForQueueSerial code path to wait for it to finish.
    if (fence == nullptr)
    {
        ANGLE_TRY(mFenceRecycler.newSharedFence(context, &batch.fence));
        fence = &batch.fence.get();
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkProtectedSubmitInfo protectedSubmitInfo = {};
    if (hasProtectedContent)
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

    ANGLE_TRY(queueSubmit(context, contextPriority, submitInfo, fence, submitQueueSerial));

    mInFlightCommands.emplace_back(scopedBatch.release());
    return angle::Result::Continue;
}

angle::Result CommandQueue::queueSubmit(Context *context,
                                        egl::ContextPriority contextPriority,
                                        const VkSubmitInfo &submitInfo,
                                        const Fence *fence,
                                        const QueueSerial &submitQueueSerial)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::queueSubmit");

    RendererVk *renderer = context->getRenderer();

    if (kOutputVmaStatsString)
    {
        renderer->outputVmaStatString();
    }

    VkFence fenceHandle = fence ? fence->getHandle() : VK_NULL_HANDLE;
    VkQueue queue       = getQueue(contextPriority);
    ANGLE_VK_TRY(context, vkQueueSubmit(queue, 1, &submitInfo, fenceHandle));

    mLastSubmittedSerials.setQueueSerial(submitQueueSerial);

    ++mPerfCounters.vkQueueSubmitCallsTotal;
    ++mPerfCounters.vkQueueSubmitCallsPerFrame;
    return angle::Result::Continue;
}

void CommandQueue::resetPerFramePerfCounters()
{
    mPerfCounters.commandQueueSubmitCallsPerFrame = 0;
    mPerfCounters.vkQueueSubmitCallsPerFrame      = 0;
}

VkResult CommandQueue::queuePresent(egl::ContextPriority contextPriority,
                                    const VkPresentInfoKHR &presentInfo)
{
    VkQueue queue = getQueue(contextPriority);
    return vkQueuePresentKHR(queue, &presentInfo);
}

bool CommandQueue::isBusy() const
{
    for (SerialIndex i = 0; i < mLastSubmittedSerials.size(); ++i)
    {
        if (mLastSubmittedSerials[i] > mLastCompletedSerials[i])
        {
            return true;
        }
    }
    return false;
}

bool CommandQueue::hasUnfinishedUse(const vk::ResourceUse &use) const
{
    return use > mLastCompletedSerials;
}

bool CommandQueue::hasUnsubmittedUse(const vk::ResourceUse &use) const
{
    return use > mLastSubmittedSerials;
}

size_t CommandQueue::getBatchCountUpToSerials(RendererVk *renderer,
                                              const Serials &serials,
                                              Shared<Fence> **fenceToWaitOnOut)
{
    if (renderer->getLargestQueueSerialIndexEverAllocated() < 64)
    {
        return GetBatchCountUpToSerials<angle::BitSet64<64>>(
            mInFlightCommands, mLastSubmittedSerials, mLastCompletedSerials, serials,
            fenceToWaitOnOut);
    }
    else
    {
        return GetBatchCountUpToSerials<angle::BitSetArray<kMaxQueueSerialIndexCount>>(
            mInFlightCommands, mLastSubmittedSerials, mLastCompletedSerials, serials,
            fenceToWaitOnOut);
    }
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
