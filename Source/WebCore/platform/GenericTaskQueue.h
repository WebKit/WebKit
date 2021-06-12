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
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class TaskQueueBase : public CanMakeWeakPtr<TaskQueueBase> {
    WTF_MAKE_FAST_ALLOCATED;
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
    ~TaskQueueBase() = default;
    void incrementPendingTasks() { ++m_pendingTasks; }
    void decrementPendingTasks() { ASSERT(m_pendingTasks); --m_pendingTasks; }

private:
    unsigned m_pendingTasks { 0 };
    bool m_isClosed { false };
};

// Relies on a shared Timer, only safe to use on the MainThread. Please use EventLoopTaskQueue
// (or dispatch directly to the HTML event loop) whenever possible.
class MainThreadTaskQueue : public TaskQueueBase {
public:
    MainThreadTaskQueue()
    {
        ASSERT(isMainThread());
    }

    void enqueueTask(Function<void()>&& task)
    {
        if (isClosed())
            return;

        incrementPendingTasks();
        callOnMainThread([weakThis = makeWeakPtr(*this), task = WTFMove(task)] {
            if (!weakThis)
                return;
            weakThis->decrementPendingTasks();
            task();
        });
    }
};

// Similar to MainThreadTaskQueue but based on the HTML event loop.
class EventLoopTaskQueue : public TaskQueueBase, private ContextDestructionObserver {
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
