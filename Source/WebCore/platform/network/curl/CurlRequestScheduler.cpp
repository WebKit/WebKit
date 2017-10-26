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

/*
 * CurlJobList is used only in background so that no need to manage mutex
 */
class CurlJobList : public CurlMultiHandle {
public:
    bool isEmpty() const { return m_activeJobs.isEmpty(); }

    void startJobs(HashSet<CurlRequestSchedulerClient*>&& jobs)
    {
        auto localJobs = WTFMove(jobs);
        for (auto& client : localJobs) {
            CURL* handle = client->setupTransfer();
            if (!handle)
                return;

            m_activeJobs.add(handle, client);
            addHandle(handle);
        }
    }

    void finishJobs(HashMap<CURL*, CURLcode>&& tickets, WTF::Function<void(CurlRequestSchedulerClient*, CURLcode)> completionHandler)
    {
        auto localTickets = WTFMove(tickets);
        for (auto& ticket : localTickets) {
            if (!m_activeJobs.contains(ticket.key))
                continue;

            CURL* handle = ticket.key;
            CURLcode result = ticket.value;
            CurlRequestSchedulerClient* client = m_activeJobs.inlineGet(handle);

            removeHandle(handle);
            m_activeJobs.remove(handle);
            completionHandler(client, result);

            callOnMainThread([client]() {
                client->release();
            });
        }
    }

private:
    HashMap<CURL*, CurlRequestSchedulerClient*> m_activeJobs;
};

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

    client->retain();

    {
        LockHolder locker(m_mutex);
        m_pendingJobs.add(client);
    }

    startThreadIfNeeded();

    return true;
}

void CurlRequestScheduler::cancel(CurlRequestSchedulerClient* client)
{
    ASSERT(isMainThread());

    if (!client || !client->handle())
        return;

    LockHolder locker(m_mutex);
    m_cancelledJobs.add(client->handle(), CURLE_OK);
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

    LockHolder locker(m_mutex);

    if (m_pendingJobs.isEmpty())
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

void CurlRequestScheduler::updateJobList(CurlJobList& jobs)
{
    ASSERT(!isMainThread());

    HashSet<CurlRequestSchedulerClient*> pendingJobs;
    HashMap<CURL*, CURLcode> cancelledJobs;
    Vector<WTF::Function<void()>> taskQueue;

    {
        LockHolder locker(m_mutex);

        pendingJobs = WTFMove(m_pendingJobs);
        cancelledJobs = WTFMove(m_cancelledJobs);
        taskQueue = WTFMove(m_taskQueue);
    }

    for (auto& task : taskQueue)
        task();

    jobs.startJobs(WTFMove(pendingJobs));

    jobs.finishJobs(WTFMove(cancelledJobs), [](CurlRequestSchedulerClient* client, CURLcode) {
        client->didCancelTransfer();
    });

    jobs.finishJobs(WTFMove(m_finishedJobs), [](CurlRequestSchedulerClient* client, CURLcode result) {
        client->didCompleteTransfer(result);
    });
}

void CurlRequestScheduler::workerThread()
{
    ASSERT(!isMainThread());

    CurlJobList jobs;

    while (m_runThread) {
        updateJobList(jobs);

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

            jobs.getFdSet(fdread, fdwrite, fdexcep, maxfd);

            // When the 3 file descriptors are empty, winsock will return -1
            // and bail out, stopping the file download. So make sure we
            // have valid file descriptors before calling select.
            if (maxfd >= 0)
                rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        } while (rc == -1 && errno == EINTR);

        int activeCount = 0;
        while (jobs.perform(activeCount) == CURLM_CALL_MULTI_PERFORM) { }

        // check the curl messages indicating completed transfers
        // and free their resources
        while (true) {
            int messagesInQueue = 0;
            CURLMsg* msg = jobs.readInfo(messagesInQueue);
            if (!msg)
                break;

            ASSERT(msg->msg == CURLMSG_DONE);
            m_finishedJobs.add(msg->easy_handle, msg->data.result);
        }

        if (jobs.isEmpty())
            stopThreadIfNoMoreJobRunning();
    }
}

}

#endif
