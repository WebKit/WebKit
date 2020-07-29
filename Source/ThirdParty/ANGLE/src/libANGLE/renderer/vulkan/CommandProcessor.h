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

#include "common/vulkan/vk_headers.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
namespace vk
{
// CommandProcessorTask is used to queue a task to the worker thread when
//  enableCommandProcessingThread feature is true.
// The typical task includes pointers in all values and the worker thread will
//  process the SecondaryCommandBuffer commands in cbh into the primaryCB.
// There is a special task in which all of the pointers are null that will trigger
//  the worker thread to exit, and is sent when the renderer instance shuts down.
struct CommandProcessorTask
{
    ContextVk *contextVk;
    // TODO: b/153666475 Removed primaryCB in threading phase2.
    vk::PrimaryCommandBuffer *primaryCB;
    CommandBufferHelper *commandBuffer;
};

static const CommandProcessorTask kEndCommandProcessorThread = {nullptr, nullptr, nullptr};
}  // namespace vk

class CommandProcessor : angle::NonCopyable
{
  public:
    CommandProcessor();
    ~CommandProcessor() = default;

    // Main worker loop that should be launched in its own thread. The
    //  loop waits for work to be submitted from a separate thread.
    void processCommandProcessorTasks();
    // Called asynchronously from workLoop() thread to queue work that is
    //  then processed by the workLoop() thread
    void queueCommands(const vk::CommandProcessorTask &commands);
    // Used by separate thread to wait for worker thread to complete all
    //  outstanding work.
    void waitForWorkComplete();
    // Stop the command processor loop
    void shutdown(std::thread *commandProcessorThread);

  private:
    std::queue<vk::CommandProcessorTask> mCommandsQueue;
    std::mutex mWorkerMutex;
    // Signal worker thread when work is available
    std::condition_variable mWorkAvailableCondition;
    // Signal main thread when all work completed
    std::condition_variable mWorkerIdleCondition;
    // Track worker thread Idle state for assertion purposes
    bool mWorkerThreadIdle;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_COMMAND_PROCESSOR_H_
