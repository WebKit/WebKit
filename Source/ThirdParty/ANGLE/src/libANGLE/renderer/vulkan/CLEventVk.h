//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEventVk.h: Defines the class interface for CLEventVk, implementing CLEventImpl.

#ifndef LIBANGLE_RENDERER_VULKAN_CLEVENTVK_H_
#define LIBANGLE_RENDERER_VULKAN_CLEVENTVK_H_

#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"
#include "libANGLE/renderer/vulkan/CLContextVk.h"
#include "libANGLE/renderer/vulkan/cl_types.h"
#include "libANGLE/renderer/vulkan/vk_resource.h"

#include "libANGLE/renderer/CLEventImpl.h"

#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"
#include "libANGLE/CLEvent.h"

namespace rx
{

class CLEventVk : public CLEventImpl, public vk::Resource
{
  public:
    CLEventVk(const cl::Event &event, const cl::EventPtrs &depEvents);
    ~CLEventVk() override;

    CLCommandQueueVk &getCommandQueue() const
    {
        return mEvent.getCommandQueue()->getImpl<CLCommandQueueVk>();
    }
    CLContextVk &getContext() const { return mEvent.getContext().getImpl<CLContextVk>(); }
    cl_int getCommandType() const { return mEvent.getCommandType(); }
    bool isUserEvent() const { return getCommandType() == CL_COMMAND_USER; }

    angle::Result getCommandExecutionStatus(cl_int &executionStatus) override
    {
        executionStatus = mStatus;
        return angle::Result::Continue;
    }

    angle::Result setUserEventStatus(cl_int executionStatus) override;

    angle::Result setCallback(cl::Event &event, cl_int commandExecCallbackType) override;

    angle::Result getProfilingInfo(cl::ProfilingInfo name,
                                   size_t valueSize,
                                   void *value,
                                   size_t *valueSizeRet) override;

  private:
    // In the case of clFlush (non-blocking submit), any of the cmd event statuses in submission
    // batch can be updated in the submission thread.
    std::atomic<cl_int> mStatus;

    cl::EventPtrs mDepEvents;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_CLEVENTVK_H_
