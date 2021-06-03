/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ContextDestructionObserver.h"
#include "EventLoop.h"
#include "GenericTaskQueue.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

class DeferrableTaskBase : public CanMakeWeakPtr<DeferrableTaskBase> {
public:
    void close()
    {
        cancelTask();
        m_isClosed = true;
    }

    void cancelTask()
    {
        weakPtrFactory().revokeAll();
        m_isPending = false;
    }

    bool isPending() const { return m_isPending; }

protected:
    ~DeferrableTaskBase() = default;
    bool isClosed() const { return m_isClosed; }
    void setIsPending(bool isPending) { m_isPending = isPending; }

private:
    bool m_isPending { false };
    bool m_isClosed { false };
};

template <typename T>
class DeferrableTask : public DeferrableTaskBase {
public:
    DeferrableTask()
        : m_dispatcher()
    { }

    DeferrableTask(T& t)
        : m_dispatcher(&t)
    { }

    void scheduleTask(Function<void()>&& task)
    {
        if (isClosed())
            return;

        cancelTask();

        setIsPending(true);
        m_dispatcher.postTask([weakThis = makeWeakPtr(*this), task = WTFMove(task)] {
            if (!weakThis)
                return;
            ASSERT(weakThis->isPending());
            weakThis->setIsPending(false);
            task();
        });
    }

private:
    TaskDispatcher<T> m_dispatcher;
};

// Similar to DeferrableTask but based on the HTML event loop.
class EventLoopDeferrableTask : public DeferrableTaskBase, private ContextDestructionObserver {
public:
    EventLoopDeferrableTask(ScriptExecutionContext* context)
        : ContextDestructionObserver(context)
    { }

    // FIXME: Pass TaskSource instead of assuming TaskSource::MediaElement.
    void scheduleTask(Function<void()>&& task)
    {
        if (isClosed() || !scriptExecutionContext())
            return;

        cancelTask();

        setIsPending(true);
        scriptExecutionContext()->eventLoop().queueTask(TaskSource::MediaElement, [weakThis = makeWeakPtr(*this), task = WTFMove(task)] {
            if (!weakThis)
                return;
            ASSERT(weakThis->isPending());
            weakThis->setIsPending(false);
            task();
        });
    }
};

}
