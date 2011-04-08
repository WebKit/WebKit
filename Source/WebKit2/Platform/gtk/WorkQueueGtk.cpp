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

#include "WKBase.h"
#include <WebCore/NotImplemented.h>
#include <gio/gio.h>
#include <glib.h>
#include <wtf/gobject/GRefPtr.h>

// WorkQueue::EventSource
class WorkQueue::EventSource {
public:
    EventSource(PassOwnPtr<WorkItem> workItem, WorkQueue* workQueue, GCancellable* cancellable)
        : m_workItem(workItem)
        , m_workQueue(workQueue)
        , m_cancellable(cancellable)
    {
    }

    void cancel()
    {
        if (!m_cancellable)
            return;
        g_cancellable_cancel(m_cancellable);
    }

    static void executeEventSource(EventSource* eventSource)
    {
        ASSERT(eventSource);
        WorkQueue* queue = eventSource->m_workQueue;
        {
            MutexLocker locker(queue->m_isValidMutex);
            if (!queue->m_isValid)
                return;
        }

        eventSource->m_workItem->execute();
    }

    static gboolean performWorkOnce(EventSource* eventSource)
    {
        executeEventSource(eventSource);
        return FALSE;
    }

    static gboolean performWork(GSocket* socket, GIOCondition condition, EventSource* eventSource)
    {
        if (!(condition & G_IO_IN) && !(condition & G_IO_HUP) && !(condition & G_IO_ERR)) {
            // EventSource has been cancelled, return FALSE to destroy the source.
            return FALSE;
        }

        executeEventSource(eventSource);
        return TRUE;
    }

    static gboolean performWorkOnTermination(GPid, gint, EventSource* eventSource)
    {
        executeEventSource(eventSource);
        return FALSE;
    }

    static void deleteEventSource(EventSource* eventSource) 
    {
        ASSERT(eventSource);
        delete eventSource;
    }
   
public:
    PassOwnPtr<WorkItem> m_workItem;
    WorkQueue* m_workQueue;
    GCancellable* m_cancellable;
};

// WorkQueue
void WorkQueue::platformInitialize(const char* name)
{
    m_eventContext = g_main_context_new();
    ASSERT(m_eventContext);
    m_eventLoop = g_main_loop_new(m_eventContext, FALSE);
    ASSERT(m_eventLoop);
    m_workQueueThread = createThread(reinterpret_cast<WTF::ThreadFunction>(&WorkQueue::startWorkQueueThread), this, name);
}

void WorkQueue::platformInvalidate()
{
    MutexLocker locker(m_eventLoopLock);

    if (m_eventLoop) {
        if (g_main_loop_is_running(m_eventLoop))
            g_main_loop_quit(m_eventLoop);

        g_main_loop_unref(m_eventLoop);
        m_eventLoop = 0;
    }

    if (m_eventContext) {
        g_main_context_unref(m_eventContext);
        m_eventContext = 0;
    }
}

void* WorkQueue::startWorkQueueThread(WorkQueue* workQueue)
{
    workQueue->workQueueThreadBody();
    return 0;
}

void WorkQueue::workQueueThreadBody()
{
    g_main_loop_run(m_eventLoop);
}

void WorkQueue::registerEventSourceHandler(int fileDescriptor, int condition, PassOwnPtr<WorkItem> item)
{
    GRefPtr<GSocket> socket = adoptGRef(g_socket_new_from_fd(fileDescriptor, 0));
    ASSERT(socket);
    GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());
    GRefPtr<GSource> dispatchSource = adoptGRef(g_socket_create_source(socket.get(), static_cast<GIOCondition>(condition), cancellable.get()));
    ASSERT(dispatchSource);
    EventSource* eventSource = new EventSource(item, this, cancellable.get());
    ASSERT(eventSource);

    g_source_set_callback(dispatchSource.get(), reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWork),
        eventSource, reinterpret_cast<GDestroyNotify>(&WorkQueue::EventSource::deleteEventSource));

    // Set up the event sources under the mutex since this is shared across multiple threads.
    {
        MutexLocker locker(m_eventSourcesLock);
        Vector<EventSource*> sources;
        EventSourceIterator it = m_eventSources.find(fileDescriptor);
        if (it != m_eventSources.end()) 
            sources = it->second;

        sources.append(eventSource);
        m_eventSources.set(fileDescriptor, sources);
    }

    g_source_attach(dispatchSource.get(), m_eventContext);
}

void WorkQueue::unregisterEventSourceHandler(int fileDescriptor)
{
    ASSERT(fileDescriptor);
    
    MutexLocker locker(m_eventSourcesLock);
    
    EventSourceIterator it = m_eventSources.find(fileDescriptor);
    ASSERT(it != m_eventSources.end());
    ASSERT(m_eventSources.contains(fileDescriptor));

    if (it != m_eventSources.end()) {
        Vector<EventSource*> sources = it->second;
        for (unsigned i = 0; i < sources.size(); i++)
            sources[i]->cancel();

        m_eventSources.remove(it);
    }
}

void WorkQueue::scheduleWorkOnSource(GSource* dispatchSource, PassOwnPtr<WorkItem> item, GSourceFunc sourceCallback)
{
    EventSource* eventSource = new EventSource(item, this, 0);

    g_source_set_callback(dispatchSource, sourceCallback, eventSource,
                          reinterpret_cast<GDestroyNotify>(&WorkQueue::EventSource::deleteEventSource));

    g_source_attach(dispatchSource, m_eventContext);
}

void WorkQueue::scheduleWork(PassOwnPtr<WorkItem> item)
{
    GRefPtr<GSource> dispatchSource = adoptGRef(g_idle_source_new());
    ASSERT(dispatchSource);
    g_source_set_priority(dispatchSource.get(), G_PRIORITY_DEFAULT);

    scheduleWorkOnSource(dispatchSource.get(), item, reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWorkOnce));
}

void WorkQueue::scheduleWorkAfterDelay(PassOwnPtr<WorkItem> item, double delay)
{
    GRefPtr<GSource> dispatchSource = adoptGRef(g_timeout_source_new(static_cast<guint>(delay * 1000)));
    ASSERT(dispatchSource);

    scheduleWorkOnSource(dispatchSource.get(), item, reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWorkOnce));
}

void WorkQueue::scheduleWorkOnTermination(WebKit::PlatformProcessIdentifier process, PassOwnPtr<WorkItem> item)
{
    GRefPtr<GSource> dispatchSource = adoptGRef(g_child_watch_source_new(process));
    ASSERT(dispatchSource);

    scheduleWorkOnSource(dispatchSource.get(), item, reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWorkOnTermination));
}
