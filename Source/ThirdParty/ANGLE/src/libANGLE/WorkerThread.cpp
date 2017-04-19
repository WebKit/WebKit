//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WorkerThread:
//   Task running thread for ANGLE, similar to a TaskRunner in Chromium.
//   Might be implemented differently depending on platform.
//

#include "libANGLE/WorkerThread.h"

namespace angle
{

namespace priv
{
// SingleThreadedWorkerPool implementation.
SingleThreadedWorkerPool::SingleThreadedWorkerPool(size_t maxThreads)
    : WorkerThreadPoolBase(maxThreads)
{
}

SingleThreadedWorkerPool::~SingleThreadedWorkerPool()
{
}

SingleThreadedWaitableEvent SingleThreadedWorkerPool::postWorkerTaskImpl(Closure *task)
{
    (*task)();
    return SingleThreadedWaitableEvent(EventResetPolicy::Automatic, EventInitialState::Signaled);
}

// SingleThreadedWaitableEvent implementation.
SingleThreadedWaitableEvent::SingleThreadedWaitableEvent()
    : SingleThreadedWaitableEvent(EventResetPolicy::Automatic, EventInitialState::NonSignaled)
{
}

SingleThreadedWaitableEvent::SingleThreadedWaitableEvent(EventResetPolicy resetPolicy,
                                                         EventInitialState initialState)
    : WaitableEventBase(resetPolicy, initialState)
{
}

SingleThreadedWaitableEvent::~SingleThreadedWaitableEvent()
{
}

SingleThreadedWaitableEvent::SingleThreadedWaitableEvent(SingleThreadedWaitableEvent &&other)
    : WaitableEventBase(std::move(other))
{
}

SingleThreadedWaitableEvent &SingleThreadedWaitableEvent::operator=(
    SingleThreadedWaitableEvent &&other)
{
    return copyBase(std::move(other));
}

void SingleThreadedWaitableEvent::resetImpl()
{
    mSignaled = false;
}

void SingleThreadedWaitableEvent::waitImpl()
{
}

void SingleThreadedWaitableEvent::signalImpl()
{
    mSignaled = true;
}

#if (ANGLE_STD_ASYNC_WORKERS == ANGLE_ENABLED)
// AsyncWorkerPool implementation.
AsyncWorkerPool::AsyncWorkerPool(size_t maxThreads) : WorkerThreadPoolBase(maxThreads)
{
}

AsyncWorkerPool::~AsyncWorkerPool()
{
}

AsyncWaitableEvent AsyncWorkerPool::postWorkerTaskImpl(Closure *task)
{
    auto future = std::async(std::launch::async, [task] { (*task)(); });

    AsyncWaitableEvent waitable(EventResetPolicy::Automatic, EventInitialState::NonSignaled);

    waitable.setFuture(std::move(future));

    return waitable;
}

// AsyncWaitableEvent implementation.
AsyncWaitableEvent::AsyncWaitableEvent()
    : AsyncWaitableEvent(EventResetPolicy::Automatic, EventInitialState::NonSignaled)
{
}

AsyncWaitableEvent::AsyncWaitableEvent(EventResetPolicy resetPolicy, EventInitialState initialState)
    : WaitableEventBase(resetPolicy, initialState)
{
}

AsyncWaitableEvent::~AsyncWaitableEvent()
{
}

AsyncWaitableEvent::AsyncWaitableEvent(AsyncWaitableEvent &&other)
    : WaitableEventBase(std::move(other)), mFuture(std::move(other.mFuture))
{
}

AsyncWaitableEvent &AsyncWaitableEvent::operator=(AsyncWaitableEvent &&other)
{
    std::swap(mFuture, other.mFuture);
    return copyBase(std::move(other));
}

void AsyncWaitableEvent::setFuture(std::future<void> &&future)
{
    mFuture = std::move(future);
}

void AsyncWaitableEvent::resetImpl()
{
    mSignaled = false;
    mFuture   = std::future<void>();
}

void AsyncWaitableEvent::waitImpl()
{
    if (mSignaled || !mFuture.valid())
    {
        return;
    }

    mFuture.wait();
    signal();
}

void AsyncWaitableEvent::signalImpl()
{
    mSignaled = true;

    if (mResetPolicy == EventResetPolicy::Automatic)
    {
        reset();
    }
}
#endif  // (ANGLE_STD_ASYNC_WORKERS == ANGLE_ENABLED)

}  // namespace priv

}  // namespace angle
