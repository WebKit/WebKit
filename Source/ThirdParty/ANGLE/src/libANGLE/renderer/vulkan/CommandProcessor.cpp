//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CommandProcessor.cpp:
//    Implements the class methods for CommandProcessor.
//

#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "libANGLE/trace.h"

namespace rx
{

CommandProcessor::CommandProcessor() : mWorkerThreadIdle(true) {}

void CommandProcessor::queueCommands(const vk::CommandProcessorTask &commands)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "RendererVk::queueCommands");
    std::lock_guard<std::mutex> queueLock(mWorkerMutex);
    ASSERT(commands.commandBuffer == nullptr || !commands.commandBuffer->empty());
    mCommandsQueue.push(commands);
    mWorkAvailableCondition.notify_one();
}

void CommandProcessor::processCommandProcessorTasks()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mWorkerMutex);
        mWorkerIdleCondition.notify_one();
        mWorkerThreadIdle = true;
        // Only wake if notified and command queue is not empty
        mWorkAvailableCondition.wait(lock, [this] { return !mCommandsQueue.empty(); });
        mWorkerThreadIdle             = false;
        vk::CommandProcessorTask task = mCommandsQueue.front();
        mCommandsQueue.pop();
        lock.unlock();
        // Either both ptrs should be null or non-null
        ASSERT((task.commandBuffer != nullptr && task.contextVk != nullptr) ||
               (task.commandBuffer == nullptr && task.contextVk == nullptr));
        // A work block with null ptrs signals worker thread to exit
        if (task.commandBuffer == nullptr && task.contextVk == nullptr)
        {
            break;
        }

        ASSERT(!task.commandBuffer->empty());
        // TODO: Will need some way to synchronize error reporting between threads
        (void)(task.commandBuffer->flushToPrimary(task.contextVk, task.primaryCB));
        ASSERT(task.commandBuffer->empty());
        task.commandBuffer->releaseToContextQueue(task.contextVk);
    }
}

void CommandProcessor::waitForWorkComplete()
{
    ANGLE_TRACE_EVENT0("gpu.angle", "CommandProcessor::waitForWorkerThreadIdle");
    std::unique_lock<std::mutex> lock(mWorkerMutex);
    mWorkerIdleCondition.wait(lock,
                              [this] { return (mCommandsQueue.empty() && mWorkerThreadIdle); });
    // Worker thread is idle and command queue is empty so good to continue
    lock.unlock();
}

void CommandProcessor::shutdown(std::thread *commandProcessorThread)
{
    waitForWorkComplete();
    const vk::CommandProcessorTask endTask = vk::kEndCommandProcessorThread;
    queueCommands(endTask);
    if (commandProcessorThread->joinable())
    {
        commandProcessorThread->join();
    }
}
}  // namespace rx
