/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "CurlStreamScheduler.h"

#if USE(CURL)

namespace WebCore {

CurlStreamScheduler::CurlStreamScheduler()
{
    ASSERT(isMainThread());
}

CurlStreamScheduler::~CurlStreamScheduler()
{
    ASSERT(isMainThread());
}

CurlStreamID CurlStreamScheduler::createStream(const URL& url, CurlStream::Client& client)
{
    ASSERT(isMainThread());

    do {
        m_currentStreamID = (m_currentStreamID + 1 != invalidCurlStreamID) ? m_currentStreamID + 1 : 1;
    } while (m_clientList.contains(m_currentStreamID));

    auto streamID = m_currentStreamID;
    m_clientList.add(streamID, &client);

    callOnWorkerThread([this, streamID, url = url.isolatedCopy()]() mutable {
        m_streamList.add(streamID, CurlStream::create(*this, streamID, WTFMove(url)));
    });

    return streamID;
}

void CurlStreamScheduler::destroyStream(CurlStreamID streamID)
{
    ASSERT(isMainThread());

    if (m_clientList.contains(streamID))
        m_clientList.remove(streamID);

    callOnWorkerThread([this, streamID]() {
        if (m_streamList.contains(streamID))
            m_streamList.remove(streamID);
    });
}

void CurlStreamScheduler::send(CurlStreamID streamID, UniqueArray<uint8_t>&& data, size_t length)
{
    ASSERT(isMainThread());

    callOnWorkerThread([this, streamID, data = WTFMove(data), length]() mutable {
        if (auto stream = m_streamList.get(streamID))
            stream->send(WTFMove(data), length);
    });
}

void CurlStreamScheduler::callOnWorkerThread(WTF::Function<void()>&& task)
{
    ASSERT(isMainThread());

    {
        auto locker = holdLock(m_mutex);
        m_taskQueue.append(WTFMove(task));
    }

    startThreadIfNeeded();
}

void CurlStreamScheduler::callClientOnMainThread(CurlStreamID streamID, WTF::Function<void(CurlStream::Client&)>&& task)
{
    ASSERT(!isMainThread());

    callOnMainThread([this, streamID, task = WTFMove(task)]() {
        if (auto client = m_clientList.get(streamID))
            task(*client);
    });
}

void CurlStreamScheduler::startThreadIfNeeded()
{
    {
        auto locker = holdLock(m_mutex);
        if (m_runThread)
            return;
    }

    if (m_thread)
        m_thread->waitForCompletion();

    m_runThread = true;

    m_thread = Thread::create("curlStreamThread", [this] {
        workerThread();
    }, ThreadType::Network);
}

void CurlStreamScheduler::stopThreadIfNoMoreJobRunning()
{
    ASSERT(!isMainThread());

    if (m_streamList.size())
        return;

    auto locker = holdLock(m_mutex);
    if (m_taskQueue.size())
        return;

    m_runThread = false;
}

void CurlStreamScheduler::executeTasks()
{
    ASSERT(!isMainThread());

    Vector<WTF::Function<void()>> taskQueue;

    {
        auto locker = holdLock(m_mutex);
        taskQueue = WTFMove(m_taskQueue);
    }

    for (auto& task : taskQueue)
        task();
}

void CurlStreamScheduler::workerThread()
{
    ASSERT(!isMainThread());
    static const int selectTimeoutMS = 20;
    struct timeval timeout { 0, selectTimeoutMS * 1000};

    while (m_runThread) {
        executeTasks();

        int rc = 0;
        fd_set readfds;
        fd_set writefds;
        fd_set exceptfds;

        do {
            int maxfd = -1;

            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);

            for (auto& stream : m_streamList.values())
                stream->appendMonitoringFd(readfds, writefds, exceptfds, maxfd);

            if (maxfd >= 0)
                rc = ::select(maxfd + 1, &readfds, &writefds, &exceptfds, &timeout);
        } while (rc == -1 && errno == EINTR);

        for (auto& stream : m_streamList.values())
            stream->tryToTransfer(readfds, writefds, exceptfds);

        stopThreadIfNoMoreJobRunning();
    }
}

}

#endif
