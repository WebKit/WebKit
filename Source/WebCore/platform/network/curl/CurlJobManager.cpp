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
#include "CurlJobManager.h"

#if USE(CURL)

#include "ResourceHandleCurlDelegate.h"
#include <iterator>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebCore {

enum class CurlJobResult { Done, Error, Cancelled };

/*
 * CurlJobList is used only in background so that no need to manage mutex
 */
class CurlJobList : public CurlMultiHandle {
public:
    bool isEmpty() const { return m_activeJobs.isEmpty(); }

    void startJobs(HashMap<CurlJobTicket, CurlJobClient*>&& jobs)
    {
        auto localJobs = WTFMove(jobs);
        for (auto& job : localJobs) {
            job.value->setupRequest();
            m_activeJobs.add(job.key, job.value);
            addHandle(job.key);
        }
    }

    void finishJobs(HashSet<CurlJobTicket>&& tickets, CurlJobResult result)
    {
        auto localTickets = WTFMove(tickets);
        for (auto& ticket : localTickets) {
            if (!m_activeJobs.contains(ticket))
                continue;
            removeHandle(ticket);
            notifyResult(m_activeJobs.fastGet(ticket), result);
            m_activeJobs.remove(ticket);
        }
    }

private:
    void notifyResult(CurlJobClient* client, CurlJobResult result)
    {
        switch (result) {
        case CurlJobResult::Done:
            client->notifyFinish();
            break;
        case CurlJobResult::Error:
            client->notifyFail();
            break;
        case CurlJobResult::Cancelled:
            break;
        }

        client->release();
    }

    HashMap<CurlJobTicket, CurlJobClient*> m_activeJobs;
};

CurlJobTicket CurlJobManager::add(CurlHandle& curl, CurlJobClient& client)
{
    ASSERT(isMainThread());

    client.retain();

    CurlJobTicket ticket = static_cast<CurlJobTicket>(curl.handle());

    {
        LockHolder locker(m_mutex);
        m_cancelledTickets.remove(ticket);
        m_pendingJobs.add(ticket, &client);
    }

    startThreadIfNeeded();

    return ticket;
}

void CurlJobManager::cancel(CurlJobTicket job)
{
    ASSERT(isMainThread());

    LockHolder locker(m_mutex);
    m_cancelledTickets.add(job);
}

void CurlJobManager::callOnJobThread(WTF::Function<void()>&& task)
{
    LockHolder locker(m_mutex);
    m_taskQueue.append(WTFMove(task));
}

void CurlJobManager::startThreadIfNeeded()
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

void CurlJobManager::stopThreadIfNoMoreJobRunning()
{
    ASSERT(!isMainThread());

    LockHolder locker(m_mutex);

    if (m_pendingJobs.isEmpty())
        m_runThread = false;
}

void CurlJobManager::stopThread()
{
    m_runThread = false;

    if (m_thread) {
        m_thread->waitForCompletion();
        m_thread = nullptr;
    }
}

void CurlJobManager::updateJobList(CurlJobList& jobs)
{
    ASSERT(!isMainThread());

    HashMap<CurlJobTicket, CurlJobClient*> pendingJobs;
    HashSet<CurlJobTicket> cancelledTickets;
    Vector<WTF::Function<void()>> taskQueue;

    {
        LockHolder locker(m_mutex);

        pendingJobs = WTFMove(m_pendingJobs);
        cancelledTickets = WTFMove(m_cancelledTickets);
        taskQueue = WTFMove(m_taskQueue);
    }

    jobs.startJobs(WTFMove(pendingJobs));
    jobs.finishJobs(WTFMove(cancelledTickets), CurlJobResult::Cancelled);
    jobs.finishJobs(WTFMove(m_finishedTickets), CurlJobResult::Done);
    jobs.finishJobs(WTFMove(m_failedTickets), CurlJobResult::Error);

    for (auto& task : taskQueue)
        task();
}

void CurlJobManager::workerThread()
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
            auto ticket = static_cast<CurlJobTicket>(msg->easy_handle);
            if (msg->data.result == CURLE_OK)
                m_finishedTickets.add(ticket);
            else
                m_failedTickets.add(ticket);
        }

        if (jobs.isEmpty())
            stopThreadIfNoMoreJobRunning();
    }
}

}

#endif
