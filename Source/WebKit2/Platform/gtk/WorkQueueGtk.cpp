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

#include <gio/gio.h>
#include <glib.h>
#include <wtf/gobject/GRefPtr.h>

// WorkQueue::EventSource
class WorkQueue::EventSource {
public:
    EventSource(std::function<void()> function, WorkQueue* workQueue)
        : m_function(std::move(function))
        , m_workQueue(workQueue)
    {
        ASSERT(workQueue);
    }

    virtual ~EventSource() { }

    void performWork()
    {
        m_function();
    }

    static gboolean performWorkOnce(EventSource* eventSource)
    {
        ASSERT(eventSource);
        eventSource->performWork();
        return FALSE;
    }

    static void deleteEventSource(EventSource* eventSource)
    {
        ASSERT(eventSource);
        delete eventSource;
    }

private:
    std::function<void ()> m_function;
    RefPtr<WorkQueue> m_workQueue;
};

class WorkQueue::SocketEventSource : public WorkQueue::EventSource {
public:
    SocketEventSource(std::function<void ()> function, WorkQueue* workQueue, GCancellable* cancellable, std::function<void ()> closeFunction)
        : EventSource(std::move(function), workQueue)
        , m_cancellable(cancellable)
        , m_closeFunction(std::move(closeFunction))
    {
        ASSERT(cancellable);
    }

    void cancel()
    {
        g_cancellable_cancel(m_cancellable);
    }

    void didClose()
    {
        m_closeFunction();
    }

    bool isCancelled() const
    {
        return g_cancellable_is_cancelled(m_cancellable);
    }

    static gboolean eventCallback(GSocket*, GIOCondition condition, SocketEventSource* eventSource)
    {
        ASSERT(eventSource);

        if (eventSource->isCancelled()) {
            // EventSource has been cancelled, return FALSE to destroy the source.
            return FALSE;
        }

        if (condition & G_IO_HUP || condition & G_IO_ERR || condition & G_IO_NVAL) {
            eventSource->didClose();
            return FALSE;
        }

        if (condition & G_IO_IN) {
            eventSource->performWork();
            return TRUE;
        }

        ASSERT_NOT_REACHED();
        return FALSE;
    }

private:
    GCancellable* m_cancellable;
    std::function<void ()> m_closeFunction;
};

// WorkQueue
static const size_t kVisualStudioThreadNameLimit = 31;

void WorkQueue::platformInitialize(const char* name)
{
    m_eventContext = adoptGRef(g_main_context_new());
    ASSERT(m_eventContext);
    m_eventLoop = adoptGRef(g_main_loop_new(m_eventContext.get(), FALSE));
    ASSERT(m_eventLoop);

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

    m_workQueueThread = createThread(reinterpret_cast<WTF::ThreadFunction>(&WorkQueue::startWorkQueueThread), this, threadName);
}

void WorkQueue::platformInvalidate()
{
    MutexLocker locker(m_eventLoopLock);

    if (m_eventLoop) {
        if (g_main_loop_is_running(m_eventLoop.get()))
            g_main_loop_quit(m_eventLoop.get());
        m_eventLoop.clear();
    }

    m_eventContext.clear();
}

void WorkQueue::startWorkQueueThread(WorkQueue* workQueue)
{
    workQueue->workQueueThreadBody();
}

void WorkQueue::workQueueThreadBody()
{
    g_main_loop_run(m_eventLoop.get());
}

void WorkQueue::registerSocketEventHandler(int fileDescriptor, std::function<void ()> function, std::function<void ()> closeFunction)
{
    GRefPtr<GSocket> socket = adoptGRef(g_socket_new_from_fd(fileDescriptor, 0));
    ASSERT(socket);
    GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());
    GRefPtr<GSource> dispatchSource = adoptGRef(g_socket_create_source(socket.get(), G_IO_IN, cancellable.get()));
    ASSERT(dispatchSource);
    SocketEventSource* eventSource = new SocketEventSource(std::move(function), this,
        cancellable.get(), std::move(closeFunction));

    g_source_set_callback(dispatchSource.get(), reinterpret_cast<GSourceFunc>(&WorkQueue::SocketEventSource::eventCallback),
        eventSource, reinterpret_cast<GDestroyNotify>(&WorkQueue::EventSource::deleteEventSource));

    // Set up the event sources under the mutex since this is shared across multiple threads.
    {
        MutexLocker locker(m_eventSourcesLock);
        Vector<SocketEventSource*> sources;
        auto it = m_eventSources.find(fileDescriptor);
        if (it != m_eventSources.end())
            sources = it->value;

        sources.append(eventSource);
        m_eventSources.set(fileDescriptor, sources);
    }

    g_source_attach(dispatchSource.get(), m_eventContext.get());
}

void WorkQueue::unregisterSocketEventHandler(int fileDescriptor)
{
    ASSERT(fileDescriptor);

    MutexLocker locker(m_eventSourcesLock);

    ASSERT(m_eventSources.contains(fileDescriptor));
    auto it = m_eventSources.find(fileDescriptor);

    if (it != m_eventSources.end()) {
        Vector<SocketEventSource*> sources = it->value;
        for (unsigned i = 0; i < sources.size(); i++)
            sources[i]->cancel();

        m_eventSources.remove(it);
    }
}

void WorkQueue::dispatchOnSource(GSource* dispatchSource, std::function<void ()> function, GSourceFunc sourceCallback)
{
    g_source_set_callback(dispatchSource, sourceCallback, new EventSource(std::move(function), this),
        reinterpret_cast<GDestroyNotify>(&WorkQueue::EventSource::deleteEventSource));

    g_source_attach(dispatchSource, m_eventContext.get());
}

void WorkQueue::dispatch(std::function<void ()> function)
{
    GRefPtr<GSource> dispatchSource = adoptGRef(g_idle_source_new());
    g_source_set_priority(dispatchSource.get(), G_PRIORITY_DEFAULT);
    dispatchOnSource(dispatchSource.get(), std::move(function),
        reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWorkOnce));
}

void WorkQueue::dispatchAfter(std::chrono::nanoseconds duration, std::function<void ()> function)
{
    GRefPtr<GSource> dispatchSource = adoptGRef(g_timeout_source_new(
        static_cast<guint>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count())));
    dispatchOnSource(dispatchSource.get(), std::move(function),
        reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWorkOnce));
}
