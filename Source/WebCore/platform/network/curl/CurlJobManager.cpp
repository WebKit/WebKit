/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#if USE(CURL)
#include "CurlJobManager.h"

#include <iterator>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebCore {

/*
 * CurlJobList is used only in background so that no need to manage mutex
 */
class CurlJobList : public CurlMultiHandle {
    using Predicate = WTF::Function<bool(const CurlJob&)>;
    using Action = WTF::Function<void(CurlJob&)>;
    using JobMap = HashMap<CurlJobTicket, CurlJob>;

public:
    void append(CurlJob& job)
    {
        CurlHandle* curl = job.curlHandle();

        CURLMcode retval = addHandle(curl->handle());
            // @FIXME error logging
        if (retval == CURLM_OK)
            m_jobs.set(job.ticket(), WTFMove(job));
    }

    void cancel(CurlJobTicket ticket)
    {
        complete(ticket, [](auto&& job) { job.cancel(); });
    }

    void complete(CurlJobTicket ticket, Action action)
    {
        auto found = m_jobs.find(ticket);
        if (found != m_jobs.end()) {
            auto job = WTFMove(found->value);

            removeHandle(job.curlHandle()->handle());
            action(job);

            m_jobs.remove(found);
        }
    }

    bool isEmpty() const { return m_jobs.isEmpty(); }

    bool withJob(CurlJobTicket ticket, WTF::Function<void(JobMap::iterator)> callback)
    {
        auto found = m_jobs.find(ticket);
        if (found == m_jobs.end())
            return false;
        
        callback(found);
        return true;
    }

    bool withCurlHandle(CurlJobTicket ticket, WTF::Function<void(CurlHandle&)> callback)
    {
        return withJob(ticket, [&callback](JobMap::iterator it) {
            callback(*it->value.curlHandle());
        });
    }

private:
    JobMap m_jobs;
};

void CurlJob::invoke(CurlJobResult result)
{
    callOnMainThread([job = WTFMove(m_job), result] {
        job(result);
    });
}

CurlJobTicket CurlJobManager::add(CurlHandle& curl, Callback callback)
{
    ASSERT(isMainThread());

    CurlJobTicket ticket = static_cast<CurlJobTicket>(curl.handle());

    {
        LockHolder locker(m_mutex);

        if (isActiveJob(ticket))
            return ticket;


        m_pendingJobs.append(CurlJob { &curl, WTFMove(callback) });
        m_activeJobs.add(ticket);
    }

    startThreadIfNeeded();

    return ticket;
}

bool CurlJobManager::cancel(CurlJobTicket job)
{
    ASSERT(isMainThread());

    if (m_runThread) {
        LockHolder locker(m_mutex);

        if (!isActiveJob(job))
            return false;
        
        m_cancelledTickets.append(job);
        m_activeJobs.remove(job);
    }

    return true;
}

void CurlJobManager::callOnJobThread(WTF::Function<void()>&& callback)
{
    LockHolder locker { m_mutex };

    m_taskQueue.append(std::make_unique<WTF::Function<void()>>(WTFMove(callback)));
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

bool CurlJobManager::updateJobs(CurlJobList& jobs)
{
    ASSERT(!isMainThread());

    Vector<CurlJob> pendingJobs;
    Vector<CurlJobTicket> cancelledTickets;
    {
        LockHolder locker(m_mutex);
        if (!m_runThread)
            return false;

        pendingJobs = WTFMove(m_pendingJobs);
        cancelledTickets = WTFMove(m_cancelledTickets);
    }

    for (auto& job : pendingJobs)
        jobs.append(job);

    for (auto& ticket : cancelledTickets)
        jobs.cancel(ticket);

    for (auto& callback : m_taskQueue.takeAllMessages()) {
        (*callback)();
    }

    return true;
}

void CurlJobManager::workerThread()
{
    ASSERT(!isMainThread());

    CurlJobList jobs;

    while (updateJobs(jobs)) {
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

            if (msg->msg == CURLMSG_DONE) {
                auto ticket = static_cast<CurlJobTicket>(msg->easy_handle);
                CURLcode result = msg->data.result;

                {
                    LockHolder locker(m_mutex);
                    m_activeJobs.remove(ticket);
                }

                jobs.complete(ticket, [result](auto& job) {
                    job.curlHandle()->setErrorCode(result);

                    bool done = result == CURLE_OK;
                    if (done)
                        job.finished();
                    else {
                        URL url = job.curlHandle()->getEffectiveURL();
#ifndef NDEBUG
                        fprintf(stderr, "Curl ERROR for url='%s', error: '%s'\n", url.string().utf8().data(), job.curlHandle()->errorDescription().utf8().data());
#endif
                        job.error();
                    }
                });
            } else
                ASSERT_NOT_REACHED();
        }

        if (jobs.isEmpty()) {
            stopThreadIfNoMoreJobRunning();
            if (!m_runThread)
                break;
        }
    }
}

}

#endif
