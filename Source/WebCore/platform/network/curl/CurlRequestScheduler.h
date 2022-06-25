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

#pragma once

#include "CurlContext.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>

namespace WebCore {

class CurlRequestSchedulerClient;

class CurlRequestScheduler {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CurlRequestScheduler);
    friend NeverDestroyed<CurlRequestScheduler>;
public:
    CurlRequestScheduler(long maxConnects, long maxTotalConnections, long maxHostConnections);
    ~CurlRequestScheduler() { stopThread(); }

    bool add(CurlRequestSchedulerClient*);
    void cancel(CurlRequestSchedulerClient*);

    void callOnWorkerThread(Function<void()>&&);

private:
    void startOrWakeUpThread();
    void wakeUpThreadIfPossible();
    void stopThreadIfNoMoreJobRunning();
    void stopThread();

    void executeTasks();

    void workerThread();

    void startTransfer(CurlRequestSchedulerClient*);
    void completeTransfer(CurlRequestSchedulerClient*, CURLcode);
    void cancelTransfer(CurlRequestSchedulerClient*);
    void finalizeTransfer(CurlRequestSchedulerClient*, Function<void()>);

    Lock m_mutex;
    RefPtr<Thread> m_thread;
    bool m_runThread { false };

    Vector<Function<void()>> m_taskQueue;
    HashSet<CurlRequestSchedulerClient*> m_activeJobs;
    HashMap<CURL*, CurlRequestSchedulerClient*> m_clientMaps;

    Lock m_multiHandleMutex;
    std::optional<CurlMultiHandle> m_curlMultiHandle;

    long m_maxConnects;
    long m_maxTotalConnections;
    long m_maxHostConnections;
};

} // namespace WebCore
