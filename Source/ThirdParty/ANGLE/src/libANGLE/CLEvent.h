//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEvent.h: Defines the cl::Event class, which can be used to track the execution status of an
// OpenCL command.

#ifndef LIBANGLE_CLEVENT_H_
#define LIBANGLE_CLEVENT_H_

#include "libANGLE/CLObject.h"
#include "libANGLE/renderer/CLEventImpl.h"

#include "common/SynchronizedValue.h"

#include <array>

namespace cl
{

class Event final : public _cl_event, public Object
{
  public:
    // Front end entry functions, only called from OpenCL entry points

    cl_int setUserEventStatus(cl_int executionStatus);

    cl_int getInfo(EventInfo name, size_t valueSize, void *value, size_t *valueSizeRet) const;

    cl_int setCallback(cl_int commandExecCallbackType, EventCB pfnNotify, void *userData);

    cl_int getProfilingInfo(ProfilingInfo name,
                            size_t valueSize,
                            void *value,
                            size_t *valueSizeRet);

  public:
    ~Event() override;

    Context &getContext();
    const Context &getContext() const;
    const CommandQueuePtr &getCommandQueue() const;
    cl_command_type getCommandType() const;
    bool wasStatusChanged() const;

    template <typename T = rx::CLEventImpl>
    T &getImpl() const;

    void callback(cl_int commandStatus);

    static EventPtrs Cast(cl_uint numEvents, const cl_event *eventList);

  private:
    using CallbackData = std::pair<EventCB, void *>;
    using Callbacks    = std::vector<CallbackData>;

    Event(Context &context, cl_int &errorCode);

    Event(CommandQueue &queue,
          cl_command_type commandType,
          const rx::CLEventImpl::CreateFunc &createFunc,
          cl_int &errorCode);

    const ContextPtr mContext;
    const CommandQueuePtr mCommandQueue;
    const cl_command_type mCommandType;
    const rx::CLEventImpl::Ptr mImpl;

    bool mStatusWasChanged = false;

    // Create separate storage for each possible callback type.
    static_assert(CL_COMPLETE == 0 && CL_RUNNING == 1 && CL_SUBMITTED == 2,
                  "OpenCL command execution status values are not as assumed");
    angle::SynchronizedValue<std::array<Callbacks, 3u>> mCallbacks;

    friend class Object;
};

inline Context &Event::getContext()
{
    return *mContext;
}

inline const Context &Event::getContext() const
{
    return *mContext;
}

inline const CommandQueuePtr &Event::getCommandQueue() const
{
    return mCommandQueue;
}

inline cl_command_type Event::getCommandType() const
{
    return mCommandType;
}

inline bool Event::wasStatusChanged() const
{
    return mStatusWasChanged;
}

template <typename T>
inline T &Event::getImpl() const
{
    return static_cast<T &>(*mImpl);
}

}  // namespace cl

#endif  // LIBANGLE_CLEVENT_H_
