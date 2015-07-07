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

static const size_t kVisualStudioThreadNameLimit = 31;

void WorkQueue::platformInitialize(const char* name, QOS)
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
    ref();
    m_socketEventSource.schedule("[WebKit] WorkQueue::SocketEventHandler", [function, closeFunction](GIOCondition condition) {
            if (condition & G_IO_HUP || condition & G_IO_ERR || condition & G_IO_NVAL) {
                closeFunction();
                return GMainLoopSource::Stop;
            }

            if (condition & G_IO_IN) {
                function();
                return GMainLoopSource::Continue;
            }

            ASSERT_NOT_REACHED();
            return GMainLoopSource::Stop;
        }, socket.get(), G_IO_IN,
        [this] { deref(); },
        m_eventContext.get());
}

void WorkQueue::unregisterSocketEventHandler(int)
{
    m_socketEventSource.cancel();
}

void WorkQueue::dispatch(std::function<void ()> function)
{
    ref();
    GMainLoopSource::createAndDeleteOnDestroy().schedule("[WebKit] WorkQueue::dispatch", WTF::move(function), G_PRIORITY_DEFAULT,
        [this] { deref(); }, m_eventContext.get());
}

void WorkQueue::dispatchAfter(std::chrono::nanoseconds duration, std::function<void ()> function)
{
    ref();
    GMainLoopSource::createAndDeleteOnDestroy().scheduleAfterDelay("[WebKit] WorkQueue::dispatchAfter", WTF::move(function),
        std::chrono::duration_cast<std::chrono::milliseconds>(duration), G_PRIORITY_DEFAULT, [this] { deref(); }, m_eventContext.get());
}
