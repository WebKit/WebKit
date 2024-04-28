//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEventVk.cpp: Implements the class methods for CLEventVk.

#include "libANGLE/renderer/vulkan/CLEventVk.h"
#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"

#include "libANGLE/cl_utils.h"

namespace rx
{

CLEventVk::CLEventVk(const cl::Event &event)
    : CLEventImpl(event), mStatus(isUserEvent() ? CL_SUBMITTED : CL_QUEUED)
{}

CLEventVk::~CLEventVk() {}

angle::Result CLEventVk::getCommandExecutionStatus(cl_int &executionStatus)
{
    executionStatus = *mStatus;
    return angle::Result::Continue;
}

angle::Result CLEventVk::setUserEventStatus(cl_int executionStatus)
{
    ASSERT(isUserEvent());

    // Not much to do here other than storing the user supplied state.
    // Error checking and single call enforcement is responsibility of the front end.
    ANGLE_TRY(setStatusAndExecuteCallback(executionStatus));

    // User event set and callback(s) finished - notify those waiting
    mUserEventCondition.notify_all();

    return angle::Result::Continue;
}

angle::Result CLEventVk::setCallback(cl::Event &event, cl_int commandExecCallbackType)
{
    ASSERT(commandExecCallbackType >= CL_COMPLETE);
    ASSERT(commandExecCallbackType < CL_QUEUED);

    // Not much to do, acknowledge the presence of callback and returns
    mHaveCallbacks->at(commandExecCallbackType) = true;

    return angle::Result::Continue;
}

angle::Result CLEventVk::getProfilingInfo(cl::ProfilingInfo name,
                                          size_t valueSize,
                                          void *value,
                                          size_t *valueSizeRet)
{
    UNIMPLEMENTED();
    ANGLE_CL_RETURN_ERROR(CL_OUT_OF_RESOURCES);
}

angle::Result CLEventVk::waitForUserEventStatus()
{
    ASSERT(isUserEvent());

    cl_int status = CL_QUEUED;
    std::unique_lock<std::mutex> ul(mUserEventMutex);
    ANGLE_TRY(getCommandExecutionStatus(status));
    if (status > CL_COMPLETE)
    {
        // User is responsible for setting the user-event object, we need to wait for that event
        // (We dont care what the outcome is, just need to wait until that event triggers)
        INFO() << "Waiting for user-event (" << &mEvent
               << ") to be set! (aka clSetUserEventStatus)";
        mUserEventCondition.wait(ul);
    }

    return angle::Result::Continue;
}

angle::Result CLEventVk::setStatusAndExecuteCallback(cl_int status)
{
    *mStatus = status;

    if (status >= CL_COMPLETE && status < CL_QUEUED && mHaveCallbacks->at(status))
    {
        auto haveCallbacks = mHaveCallbacks.synchronize();

        // Sanity check, callback(s) only from this exec status should be outstanding
        ASSERT(std::count(haveCallbacks->begin() + status, haveCallbacks->end(), true) == 1);

        getFrontendObject().callback(status);
        haveCallbacks->at(status) = false;
    }

    return angle::Result::Continue;
}

}  // namespace rx
