/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * Copyright (C) 2017 NAVER Corp.
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

#include "config.h"
#include "CurlRequestScheduler.h"

#if USE(CURL)

#include "CurlRequestSchedulerClient.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

CurlRequestScheduler& CurlRequestScheduler::singleton()
{
    static NeverDestroyed<CurlRequestScheduler> sharedInstance;
    return sharedInstance;
}

bool CurlRequestScheduler::add(CurlRequestSchedulerClient* client)
{
    ASSERT(isMainThread());

    if (!client)
        return false;

    startTransfer(client);
    startThreadIfNeeded();

    return true;
}

void CurlRequestScheduler::cancel(CurlRequestSchedulerClient* client)
{
    ASSERT(isMainThread());

    if (!client || !client->handle())
        return;

    cancelTransfer(client->handle());
}

void CurlRequestScheduler::callOnWorkerThread(WTF::Function<void()>&& task)
{
    {
        LockHolder locker(m_mutex);
        m_taskQueue.append(WTFMove(task));
    }

    startThreadIfNeeded();
}

void CurlRequestScheduler::startThreadIfNeeded()
{
    ASSERT(isMainThread());

    LockHolder locker(m_mutex);
    if (!m_runThread) {
        if (m_thread)
            m_thread->waitForCompletion();

        m_runThread = true;
        m_thread = Thread::create("curlThread", [this] {
            workerThread();
            m_runThread = false;
        });
    }
}

void CurlRequestScheduler::stopThreadIfNoMoreJobRunning()
{
    ASSERT(!isMainThread());

    if (m_activeJobs.size())
        return;

    LockHolder locker(m_mutex);
    if (m_taskQueue.size())
        return;

    m_runThread = false;
}

void CurlRequestScheduler::stopThread()
{
    m_runThread = false;

    if (m_thread) {
        m_thread->waitForCompletion();
        m_thread = nullptr;
    }
}

void CurlRequestScheduler::executeTasks()
{
    ASSERT(!isMainThread());

    Vector<WTF::Function<void()>> taskQueue;

    {
        LockHolder locker(m_mutex);
        taskQueue = WTFMove(m_taskQueue);
    }

    for (auto& task : taskQueue)
        task();
}

void CurlRequestScheduler::workerThread()
{
    ASSERT(!isMainThread());

    m_curlMultiHandle = std::make_unique<CurlMultiHandle>();

    while (m_runThread) {
        executeTasks();

        // Retry 'select' if it was interrupted by a process signal.
        int rc = 0;
        do {
            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = 0;

            const int selectTimeoutMS = 5;

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = selectTimeoutMS * 1000; // select waits microseconds

            m_curlMultiHandle->getFdSet(fdread, fdwrite, fdexcep, maxfd);

            // When the 3 file descriptors are empty, winsock will return -1
            // and bail out, stopping the file download. So make sure we
            // have valid file descriptors before calling select.
            if (maxfd >= 0)
                rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        } while (rc == -1 && errno == EINTR);

        int activeCount = 0;
        while (m_curlMultiHandle->perform(activeCount) == CURLM_CALL_MULTI_PERFORM) { }

        // check the curl messages indicating completed transfers
        // and free their resources
        while (true) {
            int messagesInQueue = 0;
            CURLMsg* msg = m_curlMultiHandle->readInfo(messagesInQueue);
            if (!msg)
                break;

            ASSERT(msg->msg == CURLMSG_DONE);
            completeTransfer(msg->easy_handle, msg->data.result);
        }

        stopThreadIfNoMoreJobRunning();
    }

    m_curlMultiHandle = nullptr;
}

void CurlRequestScheduler::startTransfer(CurlRequestSchedulerClient* client)
{
    client->retain();

    auto task = [this, client]() {
        CURL* handle = client->setupTransfer();
        if (!handle)
            return;

        m_activeJobs.add(handle, client);
        m_curlMultiHandle->addHandle(handle);
    };

    LockHolder locker(m_mutex);
    m_taskQueue.append(WTFMove(task));
}

void CurlRequestScheduler::completeTransfer(CURL* handle, CURLcode result)
{
    finalizeTransfer(handle, [result](CurlRequestSchedulerClient* client) {
        client->didCompleteTransfer(result);
    });
}

void CurlRequestScheduler::cancelTransfer(CURL* handle)
{
    finalizeTransfer(handle, [](CurlRequestSchedulerClient* client) {
        client->didCancelTransfer();
    });
}

void CurlRequestScheduler::finalizeTransfer(CURL* handle, Function<void(CurlRequestSchedulerClient*)> completionHandler)
{
    auto task = [this, handle, completion = WTFMove(completionHandler)]() {
        if (!m_activeJobs.contains(handle))
            return;

        CurlRequestSchedulerClient* client = m_activeJobs.inlineGet(handle);

        m_curlMultiHandle->removeHandle(handle);
        m_activeJobs.remove(handle);
        completion(client);

        callOnMainThread([client]() {
            client->release();
        });
    };

    LockHolder locker(m_mutex);
    m_taskQueue.append(WTFMove(task));
}

}

#endif
