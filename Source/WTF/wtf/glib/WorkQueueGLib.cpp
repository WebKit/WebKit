/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "config.h"
#include "WorkQueue.h"

#include <glib.h>
#include <string.h>

namespace WTF {

static const size_t kVisualStudioThreadNameLimit = 31;

void WorkQueue::platformInitialize(const char* name, Type, QOS)
{
    // This name can be com.apple.WebKit.ProcessLauncher or com.apple.CoreIPC.ReceiveQueue.
    // We are using those names for the thread name, but both are longer than 31 characters,
    // which is the limit of Visual Studio for thread names.
    // When log is enabled createThread() will assert instead of truncate the name, so we need
    // to make sure we don't use a name longer than 31 characters.
    const char* threadName = g_strrstr(name, ".");
    if (threadName)
        threadName++;
    else
        threadName = name;
    if (strlen(threadName) > kVisualStudioThreadNameLimit)
        threadName += strlen(threadName) - kVisualStudioThreadNameLimit;

    LockHolder locker(m_initializeRunLoopConditionMutex);
    m_workQueueThread = createThread(threadName, [this] {
        {
            LockHolder locker(m_initializeRunLoopConditionMutex);
            m_runLoop = &RunLoop::current();
            m_initializeRunLoopCondition.notifyOne();
        }
        m_runLoop->run();
        {
            LockHolder locker(m_terminateRunLoopConditionMutex);
            m_runLoop = nullptr;
            m_terminateRunLoopCondition.notifyOne();
        }
    });
    m_initializeRunLoopCondition.wait(m_initializeRunLoopConditionMutex);
}

void WorkQueue::platformInvalidate()
{
    {
        LockHolder locker(m_terminateRunLoopConditionMutex);
        if (m_runLoop) {
            m_runLoop->stop();
            m_terminateRunLoopCondition.wait(m_terminateRunLoopConditionMutex);
        }
    }

    if (m_workQueueThread) {
        detachThread(m_workQueueThread);
        m_workQueueThread = 0;
    }
}

void WorkQueue::dispatch(std::function<void ()> function)
{
    RefPtr<WorkQueue> protector(this);
    m_runLoop->dispatch([protector, function] { function(); });
}

class DispatchAfterContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DispatchAfterContext(WorkQueue& queue, std::function<void ()> function)
        : m_queue(&queue)
        , m_function(WTFMove(function))
    {
    }

    ~DispatchAfterContext()
    {
    }

    void dispatch()
    {
        m_function();
    }

private:
    RefPtr<WorkQueue> m_queue;
    std::function<void ()> m_function;
};

void WorkQueue::dispatchAfter(std::chrono::nanoseconds duration, std::function<void ()> function)
{
    GRefPtr<GSource> source = adoptGRef(g_timeout_source_new(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()));
    g_source_set_name(source.get(), "[WebKit] WorkQueue dispatchAfter");

    std::unique_ptr<DispatchAfterContext> context = std::make_unique<DispatchAfterContext>(*this, WTFMove(function));
    g_source_set_callback(source.get(), [](gpointer userData) -> gboolean {
        std::unique_ptr<DispatchAfterContext> context(static_cast<DispatchAfterContext*>(userData));
        context->dispatch();
        return G_SOURCE_REMOVE;
    }, context.release(), nullptr);
    g_source_attach(source.get(), m_runLoop->mainContext());
}

}
