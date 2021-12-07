//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEvent.cpp: Implements the cl::Event class.

#include "libANGLE/CLEvent.h"

#include "libANGLE/CLCommandQueue.h"
#include "libANGLE/CLContext.h"

#include <cstring>

namespace cl
{

cl_int Event::setUserEventStatus(cl_int executionStatus)
{
    const cl_int errorCode = mImpl->setUserEventStatus(executionStatus);
    if (errorCode == CL_SUCCESS)
    {
        mStatusWasChanged = true;
    }
    return errorCode;
}

cl_int Event::getInfo(EventInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const
{
    cl_int execStatus     = 0;
    cl_uint valUInt       = 0u;
    void *valPointer      = nullptr;
    const void *copyValue = nullptr;
    size_t copySize       = 0u;

    switch (name)
    {
        case EventInfo::CommandQueue:
            valPointer = CommandQueue::CastNative(mCommandQueue.get());
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        case EventInfo::CommandType:
            copyValue = &mCommandType;
            copySize  = sizeof(mCommandType);
            break;
        case EventInfo::ReferenceCount:
            valUInt   = getRefCount();
            copyValue = &valUInt;
            copySize  = sizeof(valUInt);
            break;
        case EventInfo::CommandExecutionStatus:
        {
            const cl_int errorCode = mImpl->getCommandExecutionStatus(execStatus);
            if (errorCode != CL_SUCCESS)
            {
                return errorCode;
            }
            copyValue = &execStatus;
            copySize  = sizeof(execStatus);
            break;
        }
        case EventInfo::Context:
            valPointer = mContext->getNative();
            copyValue  = &valPointer;
            copySize   = sizeof(valPointer);
            break;
        default:
            return CL_INVALID_VALUE;
    }

    if (value != nullptr)
    {
        // CL_INVALID_VALUE if size in bytes specified by param_value_size is < size of return type
        // as described in the Event Queries table and param_value is not NULL.
        if (valueSize < copySize)
        {
            return CL_INVALID_VALUE;
        }
        if (copyValue != nullptr)
        {
            std::memcpy(value, copyValue, copySize);
        }
    }
    if (valueSizeRet != nullptr)
    {
        *valueSizeRet = copySize;
    }
    return CL_SUCCESS;
}

cl_int Event::setCallback(cl_int commandExecCallbackType, EventCB pfnNotify, void *userData)
{
    auto callbacks = mCallbacks.synchronize();
    // Only when required register a single callback with the back end for each callback type.
    if ((*callbacks)[commandExecCallbackType].empty())
    {
        ANGLE_CL_TRY(mImpl->setCallback(*this, commandExecCallbackType));
        // This event has to be retained until the callback is called.
        retain();
    }
    (*callbacks)[commandExecCallbackType].emplace_back(pfnNotify, userData);
    return CL_SUCCESS;
}

cl_int Event::getProfilingInfo(ProfilingInfo name,
                               size_t valueSize,
                               void *value,
                               size_t *valueSizeRet)
{
    return mImpl->getProfilingInfo(name, valueSize, value, valueSizeRet);
}

Event::~Event() = default;

void Event::callback(cl_int commandStatus)
{
    ASSERT(commandStatus >= 0 && commandStatus < 3);
    const Callbacks callbacks = std::move(mCallbacks->at(commandStatus));
    for (const CallbackData &data : callbacks)
    {
        data.first(this, commandStatus, data.second);
    }
    // This event can be released after the callback was called.
    if (release())
    {
        delete this;
    }
}

EventPtrs Event::Cast(cl_uint numEvents, const cl_event *eventList)
{
    EventPtrs events;
    events.reserve(numEvents);
    while (numEvents-- != 0u)
    {
        events.emplace_back(&(*eventList++)->cast<Event>());
    }
    return events;
}

Event::Event(Context &context, cl_int &errorCode)
    : mContext(&context),
      mCommandType(CL_COMMAND_USER),
      mImpl(context.getImpl().createUserEvent(*this, errorCode))
{}

Event::Event(CommandQueue &queue,
             cl_command_type commandType,
             const rx::CLEventImpl::CreateFunc &createFunc,
             cl_int &errorCode)
    : mContext(&queue.getContext()),
      mCommandQueue(&queue),
      mCommandType(commandType),
      mImpl(createFunc(*this))
{}

}  // namespace cl
