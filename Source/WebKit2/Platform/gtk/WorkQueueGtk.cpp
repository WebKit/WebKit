/*
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

#include "NotImplemented.h"
#include "WKBase.h"
#include <glib.h>

// WorkQueue::EventSource
class WorkQueue::EventSource {
public:
    EventSource(GSource* dispatchSource, PassOwnPtr<WorkItem> workItem, WorkQueue* workQueue)
        : m_dispatchSource(dispatchSource)
        , m_workItem(workItem)
        , m_workQueue(workQueue)
    {
    }

    GSource* dispatchSource() { return m_dispatchSource; }

    static gboolean performWorkOnce(EventSource* eventSource)
    {
        ASSERT(eventSource);
        WorkQueue* queue = eventSource->m_workQueue;
        {
            MutexLocker locker(queue->m_isValidMutex);
            if (!queue->m_isValid)
                return FALSE;
        }

        eventSource->m_workItem->execute();
        return FALSE;
    }

    static gboolean performWork(GIOChannel* channel, GIOCondition condition, EventSource* eventSource) 
    {
        ASSERT(eventSource);

        if (!(condition & G_IO_IN) && !(condition & G_IO_HUP) && !(condition & G_IO_ERR))
            return FALSE;

        WorkQueue* queue = eventSource->m_workQueue;
        {
            MutexLocker locker(queue->m_isValidMutex);
            if (!queue->m_isValid)
                return FALSE;
        }

        eventSource->m_workItem->execute();

        if ((condition & G_IO_HUP) || (condition & G_IO_ERR))
            return FALSE;

        return TRUE;
    }
    
    static void deleteEventSource(EventSource* eventSource) 
    {
        ASSERT(eventSource);
        delete eventSource;
    }
   
public:
    GSource* m_dispatchSource;
    PassOwnPtr<WorkItem> m_workItem;
    WorkQueue* m_workQueue;
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
    GIOChannel* channel = g_io_channel_unix_new(fileDescriptor);
    ASSERT(channel);
    GSource* dispatchSource = g_io_create_watch(channel, static_cast<GIOCondition>(condition));
    ASSERT(dispatchSource);
    EventSource* eventSource = new EventSource(dispatchSource, item, this);
    ASSERT(eventSource);

    g_source_set_callback(dispatchSource, reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWork), 
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

    // Attach the event source to the GMainContext under the mutex since this is shared across multiple threads.
    {
        MutexLocker locker(m_eventLoopLock);
        g_source_attach(dispatchSource, m_eventContext);
    }
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
            g_source_destroy(sources[i]->dispatchSource());

        m_eventSources.remove(it);
    }
}

void WorkQueue::scheduleWork(PassOwnPtr<WorkItem> item)
{
    GSource* dispatchSource = g_timeout_source_new(0);
    ASSERT(dispatchSource);
    EventSource* eventSource = new EventSource(dispatchSource, item, this);
    
    g_source_set_callback(dispatchSource, 
                          reinterpret_cast<GSourceFunc>(&WorkQueue::EventSource::performWorkOnce), 
                          eventSource, 
                          reinterpret_cast<GDestroyNotify>(&WorkQueue::EventSource::deleteEventSource));
    {
        MutexLocker locker(m_eventLoopLock);
        g_source_attach(dispatchSource, m_eventContext);
    }
}

void WorkQueue::scheduleWorkAfterDelay(PassOwnPtr<WorkItem>, double)
{
    notImplemented();
}
