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

#pragma once

#include "CurlContext.h"

#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MessageQueue.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if OS(WINDOWS)
#include <windows.h>
#include <winsock2.h>
#endif


namespace WebCore {

enum class CurlJobResult { Done, Error, Cancelled };
using CurlJobTicket = void*;
using CurlJobCallback = WTF::Function<void(CurlJobResult)>;
using CurlJobTask = WTF::Function<void(CurlHandle&)>;

class CurlJobList;

class CurlJob {
    WTF_MAKE_NONCOPYABLE(CurlJob);
public:
    CurlJob() { }
    CurlJob(CurlHandle* curl, CurlJobCallback job)
        : m_curl { curl }, m_job { WTFMove(job) } { }
    CurlJob(CurlJob&& other)
    {
        m_curl = other.m_curl;
        other.m_curl = nullptr;
        m_job = WTFMove(other.m_job);
    }
    ~CurlJob() { }

    CurlJob& operator=(CurlJob&& other)
    {
        m_curl = other.m_curl;
        other.m_curl = nullptr;
        m_job = WTFMove(other.m_job);
        return *this;
    }

    CurlHandle* curlHandle() const { return m_curl; }
    CurlJobTicket ticket() const { return static_cast<CurlJobTicket>(m_curl->handle()); }

    void finished() { invoke(CurlJobResult::Done); }
    void error() { invoke(CurlJobResult::Error); }
    void cancel() { invoke(CurlJobResult::Cancelled); }

private:
    CurlHandle* m_curl;
    CurlJobCallback m_job;

    void invoke(CurlJobResult);
};


class CurlJobManager {
    WTF_MAKE_NONCOPYABLE(CurlJobManager);
    using Callback = CurlJobCallback;

public:

    static CurlJobManager& singleton()
    {
        static CurlJobManager shared;
        return shared;
    }

    CurlJobManager() = default;
    ~CurlJobManager() { stopThread(); }

    CurlJobTicket add(CurlHandle&, Callback);
    bool cancel(CurlJobTicket);
    void callOnJobThread(WTF::Function<void()>&&);

private:
    void startThreadIfNeeded();
    void stopThreadIfNoMoreJobRunning();
    void stopThread();

    bool updateJobs(CurlJobList& jobs);
    bool isActiveJob(CurlJobTicket job) const { return m_activeJobs.contains(job); }

    void workerThread();

    RefPtr<Thread> m_thread;
    Vector<CurlJob> m_pendingJobs;
    HashSet<CurlJobTicket> m_activeJobs;
    Vector<CurlJobTicket> m_cancelledTickets;
    MessageQueue<WTF::Function<void()>> m_taskQueue;
    mutable Lock m_mutex;
    bool m_runThread { };
};

}
