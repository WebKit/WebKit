/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "ContextDestructionObserver.h"
#include "EventLoop.h"
#include "ScriptExecutionContext.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class Lock;
};

namespace WebCore {

template <typename T>
class TaskDispatcher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TaskDispatcher(T* context)
        : m_context(context)
    {
    }

    void postTask(Function<void()>&& function)
    {
        ASSERT(m_context);
        m_context->enqueueTaskForDispatcher(WTFMove(function));
    }

private:
    T* m_context;
};

template<>
class TaskDispatcher<Timer> : public CanMakeWeakPtr<TaskDispatcher<Timer>, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TaskDispatcher();
    void postTask(Function<void()>&&);

private:
    static Timer& sharedTimer();
    static void sharedTimerFired();
    static Deque<WeakPtr<TaskDispatcher<Timer>>>& pendingDispatchers() WTF_REQUIRES_LOCK(s_sharedLock);

    void dispatchOneTask();

    static Lock s_sharedLock;
    Deque<Function<void()>> m_pendingTasks WTF_GUARDED_BY_LOCK(s_sharedLock);
};

class GenericTaskQueueBase : public CanMakeWeakPtr<GenericTaskQueueBase> {
public:
    bool hasPendingTasks() const { return m_pendingTasks; }
    bool isClosed() const { return m_isClosed; }

    void close()
    {
        cancelAllTasks();
        m_isClosed = true;
    }

    void cancelAllTasks()
    {
        weakPtrFactory().revokeAll();
        m_pendingTasks = 0;
    }

protected:
    ~GenericTaskQueueBase() = default;
    void incrementPendingTasks() { ++m_pendingTasks; }
    void decrementPendingTasks() { ASSERT(m_pendingTasks); --m_pendingTasks; }

private:
    unsigned m_pendingTasks { 0 };
    bool m_isClosed { false };
};

template <typename T>
class GenericTaskQueue : public GenericTaskQueueBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GenericTaskQueue()
        : m_dispatcher(makeUniqueRef<TaskDispatcher<T>>())
    {
        ASSERT(isMainThread());
    }

    explicit GenericTaskQueue(T& t)
        : m_dispatcher(makeUniqueRef<TaskDispatcher<T>>(&t))
    {
        ASSERT(isMainThread());
    }

    explicit GenericTaskQueue(T* t)
        : m_dispatcher(makeUniqueRef<TaskDispatcher<T>>(t))
    {
        ASSERT(isMainThread());
        if (!t)
            close();
    }

    ~GenericTaskQueue()
    {
        if (!isMainThread())
            m_dispatcher->postTask([dispatcher = WTFMove(m_dispatcher)] { });
    }

    void enqueueTask(Function<void()>&& task)
    {
        if (isClosed())
            return;

        incrementPendingTasks();
        m_dispatcher->postTask([weakThis = makeWeakPtr(*this), task = WTFMove(task)] {
            if (!weakThis)
                return;
            weakThis->decrementPendingTasks();
            task();
        });
    }

private:
    UniqueRef<TaskDispatcher<T>> m_dispatcher;
};

// Similar to GenericTaskQueue but based on the HTML event loop.
class EventLoopTaskQueue : public GenericTaskQueueBase, private ContextDestructionObserver {
public:
    EventLoopTaskQueue(ScriptExecutionContext* context)
        : ContextDestructionObserver(context)
    { }

    // FIXME: Pass a TaskSource instead of assuming TaskSource::MediaElement.
    void enqueueTask(Function<void()>&& task)
    {
        if (isClosed() || !scriptExecutionContext())
            return;

        incrementPendingTasks();
        scriptExecutionContext()->eventLoop().queueTask(TaskSource::MediaElement, [weakThis = makeWeakPtr(*this), task = WTFMove(task)] {
            if (!weakThis)
                return;
            weakThis->decrementPendingTasks();
            task();
        });
    }
};

}
