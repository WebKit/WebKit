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
#include "libANGLE/trace.h"

namespace rx
{
namespace vk
{
namespace
{
constexpr size_t kInFlightCommandsLimit = 100u;
constexpr bool kOutputVmaStatsString    = false;

void InitializeSubmitInfo(VkSubmitInfo *submitInfo,
                          const PrimaryCommandBuffer &commandBuffer,
                          const std::vector<VkSemaphore> &waitSemaphores,
                          std::vector<VkPipelineStageFlags> *waitSemaphoreStageMasks,
                          const Semaphore *signalSemaphore)
{
    // Verify that the submitInfo has been zero'd out.
    ASSERT(submitInfo->signalSemaphoreCount == 0);

    submitInfo->sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo->commandBufferCount = commandBuffer.valid() ? 1 : 0;
    submitInfo->pCommandBuffers    = commandBuffer.ptr();

    if (waitSemaphoreStageMasks->size() < waitSemaphores.size())
    {
        waitSemaphoreStageMasks->resize(waitSemaphores.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    submitInfo->waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo->pWaitSemaphores    = waitSemaphores.data();
    submitInfo->pWaitDstStageMask  = waitSemaphoreStageMasks->data();

    if (signalSemaphore)
    {
        submitInfo->signalSemaphoreCount = 1;
        submitInfo->pSignalSemaphores    = signalSemaphore->ptr();
    }
    else
    {
        submitInfo->signalSemaphoreCount = 0;
        submitInfo->pSignalSemaphores    = nullptr;
    }
}

// TODO(jmadill): De-duplicate. b/172704839
void InitializeSubmitInfo(VkSubmitInfo *submitInfo,
                          const vk::PrimaryCommandBuffer &commandBuffer,
                          const std::vector<VkSemaphore> &waitSemaphores,
                          const std::vector<VkPipelineStageFlags> &waitSemaphoreStageMasks,
                          const vk::Semaphore *signalSemaphore)
{
    // Verify that the submitInfo has been zero'd out.
    ASSERT(submitInfo->signalSemaphoreCount == 0);
    ASSERT(waitSemaphores.size() == waitSemaphoreStageMasks.size());
    submitInfo->sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo->commandBufferCount = commandBuffer.valid() ? 1 : 0;
    submitInfo->pCommandBuffers    = commandBuffer.ptr();
    submitInfo->waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo->pWaitSemaphores    = waitSemaphores.data();
    submitInfo->pWaitDstStageMask  = waitSemaphoreStageMasks.data();

    if (signalSemaphore)
    {
        submitInfo->signalSemaphoreCount = 1;
        submitInfo->pSignalSemaphores    = signalSemaphore->ptr();
    }
}

bool CommandsHaveValidOrdering(const std::vector<vk::CommandBatch> &commands)
{
    Serial currentSerial;
    for (const vk::CommandBatch &commands : commands)
    {
        if (commands.serial <= currentSerial)
        {
            return false;
        }
        currentSerial = commands.serial;
    }

    return true;
}
}  // namespace

void CommandProcessorTask::initTask()
{
    mTask                        = CustomTask::Invalid;
    mContextVk                   = nullptr;
    mRenderPass                  = nullptr;
    mCommandBuffer               = nullptr;
    mSemaphore                   = nullptr;
    mOneOffFence                 = nullptr;
    mPresentInfo                 = {};
    mPresentInfo.pResults        = nullptr;
    mPresentInfo.pSwapchains     = nullptr;
    mPresentInfo.pImageIndices   = nullptr;
    mPresentInfo.pNext           = nullptr;
    mPresentInfo.pWaitSemaphores = nullptr;
    mOneOffCommandBufferVk       = VK_NULL_HANDLE;
}

// CommandProcessorTask implementation
void CommandProcessorTask::initProcessCommands(ContextVk *contextVk,
                                               CommandBufferHelper *commandBuffer,
                                               RenderPass *renderPass)
{
    mTask          = CustomTask::ProcessCommands;
    mContextVk     = contextVk;
    mCommandBuffer = commandBuffer;
    mRenderPass    = renderPass;
}

void CommandProcessorTask::copyPresentInfo(const VkPresentInfoKHR &other)
{
    if (other.sType == VK_NULL_HANDLE)
    {
        return;
    }

    mPresentInfo.sType = other.sType;
    mPresentInfo.pNext = other.pNext;

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
                mPresentInfo.pNext             = &mPresentRegions;
                pNext                          = const_cast<void *>(presentRegions->pNext);
                break;
            }
            default:
                ERR() << "Unknown sType: " << sType << " in VkPresentInfoKHR.pNext chain";
                UNREACHABLE();
                break;
        }
    }
}

void CommandProcessorTask::initPresent(egl::ContextPriority priority, VkPresentInfoKHR &presentInfo)
{
    mTask     = CustomTask::Present;
    mPriority = priority;
    copyPresentInfo(presentInfo);
}

void CommandProcessorTask::initFinishToSerial(Serial serial)
{
    // Note: sometimes the serial is not valid and that's okay, the finish will early exit in the
    // TaskProcessor::finishToSerial
    mTask   = CustomTask::FinishToSerial;
    mSerial = serial;
}

void CommandProcessorTask::initFlushAndQueueSubmit(
    std::vector<VkSemaphore> &&waitSemaphores,
    std::vector<VkPipelineStageFlags> &&waitSemaphoreStageMasks,
    const Semaphore *semaphore,
    egl::ContextPriority priority,
    GarbageList &&currentGarbage,
    ResourceUseList &&currentResources)
{
    mTask                    = CustomTask::FlushAndQueueSubmit;
    mWaitSemaphores          = std::move(waitSemaphores);
    mWaitSemaphoreStageMasks = std::move(waitSemaphoreStageMasks);
    mSemaphore               = semaphore;
    mGarbage                 = std::move(currentGarbage);
    mResourceUseList         = std::move(currentResources);
    mPriority                = priority;
}

void CommandProcessorTask::initOneOffQueueSubmit(VkCommandBuffer oneOffCommandBufferVk,
                                                 egl::ContextPriority priority,
                                                 const Fence *fence)
{
    mTask                  = CustomTask::OneOffQueueSubmit;
    mOneOffCommandBufferVk = oneOffCommandBufferVk;
    mOneOffFence           = fence;
    mPriority              = priority;
}

CommandProcessorTask &CommandProcessorTask::operator=(CommandProcessorTask &&rhs)
{
    if (this == &rhs)
    {
        return *this;
    }

    mContextVk     = rhs.mContextVk;
    mRenderPass    = rhs.mRenderPass;
    mCommandBuffer = rhs.mCommandBuffer;
    std::swap(mTask, rhs.mTask);
    std::swap(mWaitSemaphores, rhs.mWaitSemaphores);
    std::swap(mWaitSemaphoreStageMasks, rhs.mWaitSemaphoreStageMasks);
    mSemaphore   = rhs.mSemaphore;
    mOneOffFence = rhs.mOneOffFence;
    std::swap(mGarbage, rhs.mGarbage);
    std::swap(mSerial, rhs.mSerial);
    std::swap(mPriority, rhs.mPriority);
    std::swap(mResourceUseList, rhs.mResourceUseList);
    mOneOffCommandBufferVk = rhs.mOneOffCommandBufferVk;

    copyPresentInfo(rhs.mPresentInfo);

    // clear rhs now that everything has moved.
    rhs.initTask();

    return *this;
}

// CommandBatch implementation.
CommandBatch::CommandBatch() = default;

CommandBatch::~CommandBatch() = default;

CommandBatch::CommandBatch(CommandBatch &&other)
{
    *this = std::move(other);
}

CommandBatch &CommandBatch::operator=(CommandBatch &&other)
{
    std::swap(primaryCommands, other.primaryCommands);
    std::swap(commandPool, other.commandPool);
    std::swap(fence, other.fence);
    std::swap(serial, other.serial);
    return *this;
}

void CommandBatch::destroy(VkDevice device)
{
    primaryCommands.destroy(device);
    commandPool.destroy(device);
    fence.reset(device);
}

// TaskProcessor implementation.
TaskProcessor::TaskProcessor() = default;

TaskProcessor::~TaskProcessor() = default;

void TaskProcessor::destroy(VkDevice device)
{
    mPrimaryCommandPool.destroy(device);
    ASSERT(mInFlightCommands.empty() && mGarbageQueue.empty());
}

angle::Result TaskProcessor::init(Context *context, std::thread::id threadId)
{
    mThreadId = threadId;

    // Initialize the command pool now that we know the queue family index.
    ANGLE_TRY(mPrimaryCommandPool.init(context, context->getRenderer()->getQueueFamilyIndex()));

    return angle::Result::Continue;
}

angle::Result TaskProcessor::lockAndCheckCompletedCommands(Context *context)
{
    ASSERT(isValidWorkerThread(context));
    std::lock_guard<std::mutex> inFlightLock(mInFlightCommandsMutex);

    return checkCompletedCommandsNoLock(context);
}

VkResult TaskProcessor::getLastAndClearPresentResult(VkSwapchainKHR swapchain)
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

angle::Result TaskProcessor::checkCompletedCommandsNoLock(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::checkCompletedCommandsNoLock");
    VkDevice device        = context->getDevice();
    RendererVk *rendererVk = context->getRenderer();

    int finishedCount = 0;

    for (CommandBatch &batch : mInFlightCommands)
    {
        VkResult result = batch.fence.get().getStatus(device);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(context, result);

        rendererVk->onCompletedSerial(batch.serial);

        rendererVk->resetSharedFence(&batch.fence);

        ANGLE_TRACE_EVENT0("gpu.angle", "command buffer recycling");
        batch.commandPool.destroy(device);
        ANGLE_TRY(releasePrimaryCommandBuffer(context, std::move(batch.primaryCommands)));
        ++finishedCount;
    }

    if (finishedCount > 0)
    {
        auto beginIter = mInFlightCommands.begin();
        mInFlightCommands.erase(beginIter, beginIter + finishedCount);
    }

    Serial lastCompleted = rendererVk->getLastCompletedQueueSerial();

    size_t freeIndex = 0;
    for (; freeIndex < mGarbageQueue.size(); ++freeIndex)
    {
        GarbageAndSerial &garbageList = mGarbageQueue[freeIndex];
        if (garbageList.getSerial() <= lastCompleted)
        {
            for (GarbageObject &garbage : garbageList.get())
            {
                garbage.destroy(rendererVk);
            }
        }
        else
        {
            break;
        }
    }

    // Remove the entries from the garbage list - they should be ready to go.
    if (freeIndex > 0)
    {
        mGarbageQueue.erase(mGarbageQueue.begin(), mGarbageQueue.begin() + freeIndex);
    }

    return angle::Result::Continue;
}

angle::Result TaskProcessor::releaseToCommandBatch(Context *context,
                                                   PrimaryCommandBuffer &&commandBuffer,
                                                   CommandPool *commandPool,
                                                   CommandBatch *batch)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::releaseToCommandBatch");
    batch->primaryCommands = std::move(commandBuffer);

    if (commandPool->valid())
    {
        batch->commandPool = std::move(*commandPool);
        // Recreate CommandPool
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex        = context->getRenderer()->getQueueFamilyIndex();

        ANGLE_VK_TRY(context, commandPool->init(context->getDevice(), poolInfo));
    }

    return angle::Result::Continue;
}

angle::Result TaskProcessor::allocatePrimaryCommandBuffer(Context *context,
                                                          PrimaryCommandBuffer *commandBufferOut)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::allocatePrimaryCommandBuffer");
    return mPrimaryCommandPool.allocate(context, commandBufferOut);
}

angle::Result TaskProcessor::releasePrimaryCommandBuffer(Context *context,
                                                         PrimaryCommandBuffer &&commandBuffer)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::releasePrimaryCommandBuffer");
    ASSERT(mPrimaryCommandPool.valid());
    return mPrimaryCommandPool.collect(context, std::move(commandBuffer));
}

void TaskProcessor::handleDeviceLost(Context *context)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::handleDeviceLost");
    VkDevice device = context->getDevice();
    std::lock_guard<std::mutex> inFlightLock(mInFlightCommandsMutex);

    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        VkResult status =
            batch.fence.get().wait(device, context->getRenderer()->getMaxFenceWaitTimeNs());
        // If the wait times out, it is probably not possible to recover from lost device
        ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);

        // On device lost, here simply destroy the CommandBuffer, it will be fully cleared later by
        // CommandPool::destroy
        batch.primaryCommands.destroy(device);

        batch.commandPool.destroy(device);
        batch.fence.reset(device);
    }
    mInFlightCommands.clear();
}

// If there are any inflight commands worker will look for fence that corresponds to the request
// serial or the last available fence and wait on that fence. Will then do necessary cleanup work.
// This can cause the worker thread to block.
// TODO: https://issuetracker.google.com/issues/170312581 - A more optimal solution might be to do
// the wait in CommandProcessor rather than the worker thread. That would require protecting access
// to mInFlightCommands
angle::Result TaskProcessor::finishToSerial(Context *context, Serial serial)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::finishToSerial");
    RendererVk *rendererVk = context->getRenderer();
    uint64_t timeout       = rendererVk->getMaxFenceWaitTimeNs();
    std::unique_lock<std::mutex> inFlightLock(mInFlightCommandsMutex);

    if (mInFlightCommands.empty())
    {
        // No outstanding work, nothing to wait for.
        return angle::Result::Continue;
    }

    // Find the first batch with serial equal to or bigger than given serial (note that
    // the batch serials are unique, otherwise upper-bound would have been necessary).
    size_t batchIndex = mInFlightCommands.size() - 1;
    for (size_t i = 0; i < mInFlightCommands.size(); ++i)
    {
        if (mInFlightCommands[i].serial >= serial)
        {
            batchIndex = i;
            break;
        }
    }
    const CommandBatch &batch = mInFlightCommands[batchIndex];

    // Don't need to hold the lock while waiting for the fence
    inFlightLock.unlock();

    // Wait for it finish
    VkDevice device = context->getDevice();
    ANGLE_VK_TRY(context, batch.fence.get().wait(device, timeout));

    // Clean up finished batches.
    return lockAndCheckCompletedCommands(context);
}

VkResult TaskProcessor::present(VkQueue queue, const VkPresentInfoKHR &presentInfo)
{
    std::lock_guard<std::mutex> lock(mSwapchainStatusMutex);
    ANGLE_TRACE_EVENT0("gpu.angle", "vkQueuePresentKHR");
    VkResult result = vkQueuePresentKHR(queue, &presentInfo);

    // Verify that we are presenting one and only one swapchain
    ASSERT(presentInfo.swapchainCount == 1);
    ASSERT(presentInfo.pResults == nullptr);
    mSwapchainStatus[presentInfo.pSwapchains[0]] = result;

    mSwapchainStatusCondition.notify_all();

    return result;
}

angle::Result TaskProcessor::submitFrame(Context *context,
                                         VkQueue queue,
                                         const VkSubmitInfo &submitInfo,
                                         const Shared<Fence> &sharedFence,
                                         GarbageList *currentGarbage,
                                         CommandPool *commandPool,
                                         PrimaryCommandBuffer &&commandBuffer,
                                         const Serial &queueSerial)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::submitFrame");

    VkDevice device = context->getDevice();

    DeviceScoped<CommandBatch> scopedBatch(device);
    CommandBatch &batch = scopedBatch.get();
    batch.fence.copy(device, sharedFence);
    batch.serial = queueSerial;

    ANGLE_TRY(queueSubmit(context, queue, submitInfo, &batch.fence.get()));

    if (!currentGarbage->empty())
    {
        mGarbageQueue.emplace_back(std::move(*currentGarbage), queueSerial);
    }

    // Store the primary CommandBuffer and command pool used for secondary CommandBuffers
    // in the in-flight list.
    ANGLE_TRY(releaseToCommandBatch(context, std::move(commandBuffer), commandPool, &batch));

    std::unique_lock<std::mutex> inFlightLock(mInFlightCommandsMutex);
    mInFlightCommands.emplace_back(scopedBatch.release());

    ANGLE_TRY(checkCompletedCommandsNoLock(context));

    // CPU should be throttled to avoid mInFlightCommands from growing too fast. Important for
    // off-screen scenarios.
    if (mInFlightCommands.size() > kInFlightCommandsLimit)
    {
        size_t numCommandsToFinish = mInFlightCommands.size() - kInFlightCommandsLimit;
        Serial finishSerial        = mInFlightCommands[numCommandsToFinish].serial;
        inFlightLock.unlock();
        return finishToSerial(context, finishSerial);
    }

    return angle::Result::Continue;
}

Shared<Fence> TaskProcessor::getLastSubmittedFenceWithLock(VkDevice device) const
{
    Shared<Fence> fence;
    std::lock_guard<std::mutex> inFlightLock(mInFlightCommandsMutex);

    if (!mInFlightCommands.empty())
    {
        fence.copy(device, mInFlightCommands.back().fence);
    }

    return fence;
}

angle::Result TaskProcessor::queueSubmit(Context *context,
                                         VkQueue queue,
                                         const VkSubmitInfo &submitInfo,
                                         const Fence *fence)
{
    ASSERT(isValidWorkerThread(context));
    ANGLE_TRACE_EVENT0("gpu.angle", "TaskProcessor::queueSubmit");
    ASSERT((context->getRenderer()->getFeatures().asynchronousCommandProcessing.enabled == false) ||
           std::this_thread::get_id() == mThreadId);
    if (kOutputVmaStatsString)
    {
        context->getRenderer()->outputVmaStatString();
    }

    // Don't need a QueueMutex since all queue accesses are serialized through the worker.
    VkFence handle = fence ? fence->getHandle() : VK_NULL_HANDLE;
    ANGLE_VK_TRY(context, vkQueueSubmit(queue, 1, &submitInfo, handle));

    // Now that we've submitted work, clean up RendererVk garbage
    return context->getRenderer()->cleanupGarbage(false);
}

bool TaskProcessor::isValidWorkerThread(Context *context) const
{
    return (context->getRenderer()->getFeatures().asynchronousCommandProcessing.enabled == false) ||
           std::this_thread::get_id() == mThreadId;
}

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
        handleDeviceLost();
    }

    std::lock_guard<std::mutex> queueLock(mErrorMutex);
    Error error = {errorCode, file, function, line};
    mErrors.emplace(error);
}

CommandProcessor::CommandProcessor(RendererVk *renderer)
    : Context(renderer),
      mWorkerThreadIdle(false),
      mCommandProcessorLastSubmittedSerial(mQueueSerialFactory.generate()),
      mCommandProcessorCurrentQueueSerial(mQueueSerialFactory.generate())
{
    std::lock_guard<std::mutex> queueLock(mErrorMutex);
    while (!mErrors.empty())
    {
        mErrors.pop();
    }
}

CommandProcessor::~CommandProcessor() = default;

Error CommandProcessor::getAndClearPendingError()
{
    std::lock_guard<std::mutex> queueLock(mErrorMutex);
    Error tmpError({VK_SUCCESS, nullptr, nullptr, 0});
    if (!mErrors.empty())
    {
        tmpError = mErrors.front();
        mErrors.pop();
    }
    return tmpError;
}

void CommandProcessor::queueCommand(Context *context, CommandProcessorTask *task)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::queueCommand");
    // Grab the worker mutex so that we put things on the queue in the same order as we give out
    // serials.
    std::lock_guard<std::mutex> queueLock(mWorkerMutex);

    if (task->getTaskCommand() == CustomTask::FlushAndQueueSubmit ||
        task->getTaskCommand() == CustomTask::OneOffQueueSubmit)
    {
        std::lock_guard<std::mutex> lock(mCommandProcessorQueueSerialMutex);
        // Flush submits work, so give it the current serial and generate a new one.
        Serial queueSerial = mCommandProcessorCurrentQueueSerial;
        task->setQueueSerial(queueSerial);
        mCommandProcessorLastSubmittedSerial = mCommandProcessorCurrentQueueSerial;
        mCommandProcessorCurrentQueueSerial  = mQueueSerialFactory.generate();

        task->getResourceUseList().releaseResourceUsesAndUpdateSerials(queueSerial);
    }

    if (context->getRenderer()->getFeatures().asynchronousCommandProcessing.enabled)
    {
        mTasks.emplace(std::move(*task));
        mWorkAvailableCondition.notify_one();
    }
    else
    {
        angle::Result result = processTask(context, task);
        if (ANGLE_UNLIKELY(IsError(result)))
        {
            // TODO: Ignore error, similar to ANGLE_CONTEXT_TRY.
            // Vulkan errors will get passed back to the calling context. We are still in the
            // context's thread so no mutex needed.
            return;
        }
    }
}

angle::Result CommandProcessor::initTaskProcessor(Context *context)
{
    // Initialization prior to work thread loop
    ANGLE_TRY(mTaskProcessor.init(context, std::this_thread::get_id()));
    // Allocate and begin primary command buffer
    ANGLE_TRY(mTaskProcessor.allocatePrimaryCommandBuffer(context, &mPrimaryCommandBuffer));
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;

    ANGLE_VK_TRY(context, mPrimaryCommandBuffer.begin(beginInfo));

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
    ANGLE_TRY(initTaskProcessor(this));

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

        ANGLE_TRY(processTask(this, &task));
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

angle::Result CommandProcessor::processTask(Context *context, CommandProcessorTask *task)
{
    switch (task->getTaskCommand())
    {
        case CustomTask::Exit:
        {
            ANGLE_TRY(mTaskProcessor.finishToSerial(context, Serial::Infinite()));
            // Shutting down so cleanup
            mTaskProcessor.destroy(mRenderer->getDevice());
            mCommandPool.destroy(mRenderer->getDevice());
            mPrimaryCommandBuffer.destroy(mRenderer->getDevice());
            break;
        }
        case CustomTask::FlushAndQueueSubmit:
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "processTask::FlushAndQueueSubmit");
            // End command buffer
            ANGLE_VK_TRY(context, mPrimaryCommandBuffer.end());
            // 1. Create submitInfo
            VkSubmitInfo submitInfo = {};
            InitializeSubmitInfo(&submitInfo, mPrimaryCommandBuffer, task->getWaitSemaphores(),
                                 &task->getWaitSemaphoreStageMasks(), task->getSemaphore());

            // 2. Get shared submit fence. It's possible there are other users of this fence that
            // must wait for the work to be submitted before waiting on the fence. Reset the fence
            // immediately so we are sure to get a fresh one next time.
            Shared<Fence> fence;
            ANGLE_TRY(mRenderer->getNextSubmitFence(&fence, true));

            // 3. Call submitFrame()
            ANGLE_TRY(mTaskProcessor.submitFrame(
                context, getRenderer()->getVkQueue(task->getPriority()), submitInfo, fence,
                &task->getGarbage(), &mCommandPool, std::move(mPrimaryCommandBuffer),
                task->getQueueSerial()));
            // 4. Allocate & begin new primary command buffer
            ANGLE_TRY(mTaskProcessor.allocatePrimaryCommandBuffer(context, &mPrimaryCommandBuffer));

            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo         = nullptr;
            ANGLE_VK_TRY(context, mPrimaryCommandBuffer.begin(beginInfo));

            // Free this local reference
            getRenderer()->resetSharedFence(&fence);

            ASSERT(task->getGarbage().empty());
            break;
        }
        case CustomTask::OneOffQueueSubmit:
        {
            ANGLE_TRACE_EVENT0("gpu.angle", "processTask::OneOffQueueSubmit");
            VkSubmitInfo submitInfo = {};
            submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            if (task->getOneOffCommandBufferVk() != VK_NULL_HANDLE)
            {
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers    = &task->getOneOffCommandBufferVk();
            }

            // TODO: https://issuetracker.google.com/issues/170328907 - vkQueueSubmit should be
            // owned by TaskProcessor to ensure proper synchronization
            ANGLE_TRY(mTaskProcessor.queueSubmit(context,
                                                 getRenderer()->getVkQueue(task->getPriority()),
                                                 submitInfo, task->getOneOffFence()));
            ANGLE_TRY(mTaskProcessor.lockAndCheckCompletedCommands(context));
            break;
        }
        case CustomTask::FinishToSerial:
        {
            ANGLE_TRY(mTaskProcessor.finishToSerial(context, task->getQueueSerial()));
            break;
        }
        case CustomTask::Present:
        {
            VkResult result = mTaskProcessor.present(getRenderer()->getVkQueue(task->getPriority()),
                                                     task->getPresentInfo());
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
                context->handleError(result, __FILE__, __FUNCTION__, __LINE__);
            }
            break;
        }
        case CustomTask::ProcessCommands:
        {
            ASSERT(!task->getCommandBuffer()->empty());
            ANGLE_TRY(task->getCommandBuffer()->flushToPrimary(
                getRenderer()->getFeatures(), &mPrimaryCommandBuffer, task->getRenderPass()));
            ASSERT(task->getCommandBuffer()->empty());
            task->getCommandBuffer()->releaseToContextQueue(task->getContextVk());
            break;
        }
        case CustomTask::CheckCompletedCommands:
        {
            ANGLE_TRY(mTaskProcessor.lockAndCheckCompletedCommands(this));
            break;
        }
        default:
            UNREACHABLE();
            break;
    }

    return angle::Result::Continue;
}

void CommandProcessor::checkCompletedCommands(Context *context)
{
    CommandProcessorTask checkCompletedTask;
    checkCompletedTask.initTask(CustomTask::CheckCompletedCommands);
    queueCommand(this, &checkCompletedTask);
}

void CommandProcessor::waitForWorkComplete(Context *context)
{
    ASSERT(getRenderer()->getFeatures().asynchronousCommandProcessing.enabled);
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::waitForWorkComplete");
    std::unique_lock<std::mutex> lock(mWorkerMutex);
    mWorkerIdleCondition.wait(lock, [this] { return (mTasks.empty() && mWorkerThreadIdle); });
    // Worker thread is idle and command queue is empty so good to continue

    if (!context)
    {
        return;
    }

    // Sync any errors to the context
    while (hasPendingError())
    {
        Error workerError = getAndClearPendingError();
        if (workerError.mErrorCode != VK_SUCCESS)
        {
            context->handleError(workerError.mErrorCode, workerError.mFile, workerError.mFunction,
                                 workerError.mLine);
        }
    }
}

// TODO: https://issuetracker.google.com/170311829 - Add vk::Context so that queueCommand has
// someplace to send errors.
void CommandProcessor::shutdown(std::thread *commandProcessorThread)
{
    CommandProcessorTask endTask;
    endTask.initTask(CustomTask::Exit);
    queueCommand(this, &endTask);
    if (this->getRenderer()->getFeatures().asynchronousCommandProcessing.enabled)
    {
        waitForWorkComplete(nullptr);
        if (commandProcessorThread->joinable())
        {
            commandProcessorThread->join();
        }
    }
}

// Return the fence for the last submit. This may mean waiting on the worker to process tasks to
// actually get to the last submit
Shared<Fence> CommandProcessor::getLastSubmittedFence(const Context *context) const
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::getLastSubmittedFence");
    std::unique_lock<std::mutex> lock(mWorkerMutex);
    if (context->getRenderer()->getFeatures().asynchronousCommandProcessing.enabled)
    {
        mWorkerIdleCondition.wait(lock, [this] { return (mTasks.empty() && mWorkerThreadIdle); });
    }
    // Worker thread is idle and command queue is empty so good to continue

    return mTaskProcessor.getLastSubmittedFenceWithLock(getDevice());
}

Serial CommandProcessor::getLastSubmittedSerial()
{
    std::lock_guard<std::mutex> lock(mCommandProcessorQueueSerialMutex);
    return mCommandProcessorLastSubmittedSerial;
}

Serial CommandProcessor::getCurrentQueueSerial()
{
    std::lock_guard<std::mutex> lock(mCommandProcessorQueueSerialMutex);
    return mCommandProcessorCurrentQueueSerial;
}

// Wait until all commands up to and including serial have been processed
void CommandProcessor::finishToSerial(Context *context, Serial serial)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::finishToSerial");
    CommandProcessorTask finishToSerial;
    finishToSerial.initFinishToSerial(serial);
    queueCommand(context, &finishToSerial);

    // Wait until the worker is idle. At that point we know that the finishToSerial command has
    // completed executing, including any associated state cleanup.
    if (context->getRenderer()->getFeatures().asynchronousCommandProcessing.enabled)
    {
        waitForWorkComplete(context);
    }
}

void CommandProcessor::handleDeviceLost()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::handleDeviceLost");
    std::unique_lock<std::mutex> lock(mWorkerMutex);
    if (getRenderer()->getFeatures().asynchronousCommandProcessing.enabled)
    {
        mWorkerIdleCondition.wait(lock, [this] { return (mTasks.empty() && mWorkerThreadIdle); });
    }

    // Worker thread is idle and command queue is empty so good to continue
    mTaskProcessor.handleDeviceLost(this);
}

void CommandProcessor::finishAllWork(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::finishAllWork");
    // Wait for GPU work to finish
    finishToSerial(context, Serial::Infinite());
}

// CommandQueue implementation.
CommandQueue::CommandQueue()  = default;
CommandQueue::~CommandQueue() = default;

void CommandQueue::destroy(VkDevice device)
{
    mPrimaryCommands.destroy(device);
    mPrimaryCommandPool.destroy(device);
    ASSERT(mInFlightCommands.empty() && mGarbageQueue.empty());
}

angle::Result CommandQueue::init(Context *context)
{
    RendererVk *renderer = context->getRenderer();

    // Initialize the command pool now that we know the queue family index.
    uint32_t queueFamilyIndex = renderer->getQueueFamilyIndex();
    ANGLE_TRY(mPrimaryCommandPool.init(context, queueFamilyIndex));

    return angle::Result::Continue;
}

angle::Result CommandQueue::checkCompletedCommands(Context *context)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::checkCompletedCommandsNoLock");
    ASSERT(!context->getRenderer()->getFeatures().commandProcessor.enabled);
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    int finishedCount = 0;

    for (CommandBatch &batch : mInFlightCommands)
    {
        VkResult result = batch.fence.get().getStatus(device);
        if (result == VK_NOT_READY)
        {
            break;
        }
        ANGLE_VK_TRY(context, result);
        ++finishedCount;
    }

    if (finishedCount == 0)
    {
        return angle::Result::Continue;
    }

    return retireFinishedCommands(context, finishedCount);
}

angle::Result CommandQueue::retireFinishedCommands(Context *context, size_t finishedCount)
{
    ASSERT(finishedCount > 0);

    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    for (size_t commandIndex = 0; commandIndex < finishedCount; ++commandIndex)
    {
        CommandBatch &batch = mInFlightCommands[commandIndex];

        renderer->onCompletedSerial(batch.serial);
        renderer->resetSharedFence(&batch.fence);
        ANGLE_TRACE_EVENT0("gpu.angle", "command buffer recycling");
        batch.commandPool.destroy(device);
        ANGLE_TRY(releasePrimaryCommandBuffer(context, std::move(batch.primaryCommands)));
    }

    if (finishedCount > 0)
    {
        auto beginIter = mInFlightCommands.begin();
        mInFlightCommands.erase(beginIter, beginIter + finishedCount);
    }

    Serial lastCompleted = renderer->getLastCompletedQueueSerial();

    size_t freeIndex = 0;
    for (; freeIndex < mGarbageQueue.size(); ++freeIndex)
    {
        GarbageAndSerial &garbageList = mGarbageQueue[freeIndex];
        if (garbageList.getSerial() < lastCompleted)
        {
            for (GarbageObject &garbage : garbageList.get())
            {
                garbage.destroy(renderer);
            }
        }
        else
        {
            break;
        }
    }

    // Remove the entries from the garbage list - they should be ready to go.
    if (freeIndex > 0)
    {
        mGarbageQueue.erase(mGarbageQueue.begin(), mGarbageQueue.begin() + freeIndex);
    }

    return angle::Result::Continue;
}

angle::Result CommandQueue::releaseToCommandBatch(Context *context,
                                                  PrimaryCommandBuffer &&commandBuffer,
                                                  CommandPool *commandPool,
                                                  CommandBatch *batch)
{
    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    batch->primaryCommands = std::move(commandBuffer);

    if (commandPool->valid())
    {
        batch->commandPool = std::move(*commandPool);
        // Recreate CommandPool
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex        = renderer->getQueueFamilyIndex();

        ANGLE_VK_TRY(context, commandPool->init(device, poolInfo));
    }

    return angle::Result::Continue;
}

void CommandQueue::clearAllGarbage(RendererVk *renderer)
{
    for (GarbageAndSerial &garbageList : mGarbageQueue)
    {
        for (GarbageObject &garbage : garbageList.get())
        {
            garbage.destroy(renderer);
        }
    }
    mGarbageQueue.clear();
}

angle::Result CommandQueue::allocatePrimaryCommandBuffer(Context *context,
                                                         PrimaryCommandBuffer *commandBufferOut)
{
    return mPrimaryCommandPool.allocate(context, commandBufferOut);
}

angle::Result CommandQueue::releasePrimaryCommandBuffer(Context *context,
                                                        PrimaryCommandBuffer &&commandBuffer)
{
    ASSERT(!context->getRenderer()->getFeatures().commandProcessor.enabled);
    ASSERT(mPrimaryCommandPool.valid());
    ANGLE_TRY(mPrimaryCommandPool.collect(context, std::move(commandBuffer)));

    return angle::Result::Continue;
}

void CommandQueue::handleDeviceLost(RendererVk *renderer)
{
    VkDevice device = renderer->getDevice();

    for (CommandBatch &batch : mInFlightCommands)
    {
        // On device loss we need to wait for fence to be signaled before destroying it
        VkResult status = batch.fence.get().wait(device, renderer->getMaxFenceWaitTimeNs());
        // If the wait times out, it is probably not possible to recover from lost device
        ASSERT(status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST);

        // On device lost, here simply destroy the CommandBuffer, it will fully cleared later
        // by CommandPool::destroy
        batch.primaryCommands.destroy(device);

        batch.commandPool.destroy(device);
        batch.fence.reset(device);
    }
    mInFlightCommands.clear();
}

bool CommandQueue::hasInFlightCommands() const
{
    return !mInFlightCommands.empty();
}

angle::Result CommandQueue::finishToSerial(Context *context, Serial finishSerial, uint64_t timeout)
{
    ASSERT(!context->getRenderer()->getFeatures().commandProcessor.enabled);

    if (mInFlightCommands.empty())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::finishToSerial");

    // Find the serial in the the list. The serials should be in order.
    ASSERT(CommandsHaveValidOrdering(mInFlightCommands));

    size_t finishedCount = 0;
    while (finishedCount < mInFlightCommands.size() &&
           mInFlightCommands[finishedCount].serial < finishSerial)
    {
        finishedCount++;
    }

    // This heuristic attempts to increase chances of success for shared resource scenarios.
    // Ultimately as long as fences are managed by ContextVk there are edge case bugs here.
    // TODO: http://anglebug.com/5217: fix this bug by moving it submit into RendererVk.
    if (finishedCount < mInFlightCommands.size())
    {
        finishedCount++;
    }

    if (finishedCount == 0)
    {
        return angle::Result::Continue;
    }

    const CommandBatch &batch = mInFlightCommands[finishedCount - 1];

    // Wait for it finish
    VkDevice device = context->getDevice();
    VkResult status = batch.fence.get().wait(device, timeout);

    ANGLE_VK_TRY(context, status);

    // Clean up finished batches.
    return retireFinishedCommands(context, finishedCount);
}

angle::Result CommandQueue::submitFrame(
    Context *context,
    egl::ContextPriority priority,
    const std::vector<VkSemaphore> &waitSemaphores,
    const std::vector<VkPipelineStageFlags> &waitSemaphoreStageMasks,
    const Semaphore *signalSemaphore,
    const Shared<Fence> &sharedFence,
    ResourceUseList *resourceList,
    GarbageList *currentGarbage,
    CommandPool *commandPool)
{
    // Start an empty primary buffer if we have an empty submit.
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context));
    ANGLE_VK_TRY(context, mPrimaryCommands.end());

    VkSubmitInfo submitInfo = {};
    InitializeSubmitInfo(&submitInfo, mPrimaryCommands, waitSemaphores, waitSemaphoreStageMasks,
                         signalSemaphore);

    ANGLE_TRACE_EVENT0("gpu.angle", "CommandQueue::submitFrame");
    ASSERT(!context->getRenderer()->getFeatures().commandProcessor.enabled);

    RendererVk *renderer = context->getRenderer();
    VkDevice device      = renderer->getDevice();

    DeviceScoped<CommandBatch> scopedBatch(device);
    CommandBatch &batch = scopedBatch.get();
    batch.fence.copy(device, sharedFence);

    ANGLE_TRY(renderer->queueSubmit(context, priority, submitInfo, resourceList, &batch.fence.get(),
                                    &batch.serial));

    if (!currentGarbage->empty())
    {
        mGarbageQueue.emplace_back(std::move(*currentGarbage), batch.serial);
    }

    // Store the primary CommandBuffer and command pool used for secondary CommandBuffers
    // in the in-flight list.
    ANGLE_TRY(releaseToCommandBatch(context, std::move(mPrimaryCommands), commandPool, &batch));

    mInFlightCommands.emplace_back(scopedBatch.release());

    ANGLE_TRY(checkCompletedCommands(context));

    // CPU should be throttled to avoid mInFlightCommands from growing too fast. Important for
    // off-screen scenarios.
    while (mInFlightCommands.size() > kInFlightCommandsLimit)
    {
        ANGLE_TRY(finishToSerial(context, mInFlightCommands[0].serial,
                                 renderer->getMaxFenceWaitTimeNs()));
    }

    return angle::Result::Continue;
}

Shared<Fence> CommandQueue::getLastSubmittedFence(const Context *context) const
{
    ASSERT(!context->getRenderer()->getFeatures().commandProcessor.enabled);

    Shared<Fence> fence;
    if (!mInFlightCommands.empty())
    {
        fence.copy(context->getDevice(), mInFlightCommands.back().fence);
    }

    return fence;
}

angle::Result CommandQueue::ensurePrimaryCommandBufferValid(Context *context)
{
    if (mPrimaryCommands.valid())
    {
        return angle::Result::Continue;
    }

    ANGLE_TRY(allocatePrimaryCommandBuffer(context, &mPrimaryCommands));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo         = nullptr;
    ANGLE_VK_TRY(context, mPrimaryCommands.begin(beginInfo));

    return angle::Result::Continue;
}

angle::Result CommandQueue::flushOutsideRPCommands(Context *context,
                                                   CommandBufferHelper *outsideRPCommands)
{
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context));
    return outsideRPCommands->flushToPrimary(context->getRenderer()->getFeatures(),
                                             &mPrimaryCommands, nullptr);
}

angle::Result CommandQueue::flushRenderPassCommands(Context *context,
                                                    const RenderPass &renderPass,
                                                    CommandBufferHelper *renderPassCommands)
{
    ANGLE_TRY(ensurePrimaryCommandBufferValid(context));
    return renderPassCommands->flushToPrimary(context->getRenderer()->getFeatures(),
                                              &mPrimaryCommands, &renderPass);
}
}  // namespace vk
}  // namespace rx
